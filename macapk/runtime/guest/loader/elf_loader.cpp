// elf_loader.cpp -- N0 ELF loading and managed dependency transactions (elf_loader.h).
//
// Order (PLAN.md §9.2): read the file; check the ELF header and program headers; plan the
// pages; reserve and commit read-write; copy the segments; read PT_DYNAMIC and refuse what N0
// does not admit; screen the executable segments; check every relocation and import, then
// apply them; set the final protections (RELRO read-only) and flush the instruction cache.
// Any refusal destroys the half-built Module, which releases its range: nothing half-loaded
// is ever returned. The ELF record layouts come from upstream's reader
// (macapk/elf64_reader.h); upstream's parser itself is not used, because it needs section
// headers and POSIX file calls (port-audit.md).
// ModuleSet prepares/links/protects an entire new graph before dependency-first constructors;
// its close operation preserves mappings until teardown, unlike a standalone N0 Module.

#include "elf_loader.h"
#include "faults.h"

#include "macapk/elf64_reader.h"
#include "macapk/elf_reader.h"

#include <windows.h>

#include <capstone/capstone.h>

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

namespace winapk::guest {

namespace {

using macapk::translator::Elf64Ehdr;
using macapk::translator::Elf64Phdr;
using macapk::translator::Elf64Rela;
using macapk::translator::Elf64Sym;

constexpr uint32_t PT_INTERP = 3, PT_NOTE = 4, PT_PHDR = 6, PT_TLS = 7;
constexpr uint32_t PT_GNU_EH_FRAME = 0x6474e550, PT_GNU_STACK = 0x6474e551,
                   PT_GNU_RELRO = 0x6474e552, PT_GNU_PROPERTY = 0x6474e553;
constexpr uint32_t PF_W = 2, PF_R = 4;

constexpr int64_t DT_NULL = 0, DT_NEEDED = 1, DT_PLTRELSZ = 2, DT_PLTGOT = 3, DT_HASH = 4,
                  DT_STRTAB = 5, DT_SYMTAB = 6, DT_RELA = 7, DT_RELASZ = 8, DT_RELAENT = 9,
                  DT_STRSZ = 10, DT_SYMENT = 11, DT_INIT = 12, DT_FINI = 13, DT_SONAME = 14,
                  DT_RPATH = 15, DT_SYMBOLIC = 16, DT_REL = 17, DT_RELSZ = 18, DT_RELENT = 19,
                  DT_PLTREL = 20, DT_DEBUG = 21, DT_TEXTREL = 22, DT_JMPREL = 23,
                  DT_BIND_NOW = 24, DT_INIT_ARRAY = 25, DT_FINI_ARRAY = 26,
                  DT_INIT_ARRAYSZ = 27, DT_FINI_ARRAYSZ = 28, DT_RUNPATH = 29, DT_FLAGS = 30,
                  DT_PREINIT_ARRAY = 32, DT_PREINIT_ARRAYSZ = 33, DT_RELRSZ = 35, DT_RELR = 36,
                  DT_RELRENT = 37;
constexpr int64_t DT_ANDROID_REL = 0x6000000f, DT_ANDROID_RELSZ = 0x60000010,
                  DT_ANDROID_RELA = 0x60000011, DT_ANDROID_RELASZ = 0x60000012,
                  DT_ANDROID_RELR = 0x6fffe000, DT_ANDROID_RELRSZ = 0x6fffe001,
                  DT_ANDROID_RELRENT = 0x6fffe003, DT_ANDROID_RELRCOUNT = 0x6fffe005,
                  DT_GNU_HASH = 0x6ffffef5, DT_VERSYM = 0x6ffffff0, DT_RELACOUNT = 0x6ffffff9,
                  DT_RELCOUNT = 0x6ffffffa, DT_FLAGS_1 = 0x6ffffffb, DT_VERDEF = 0x6ffffffc,
                  DT_VERDEFNUM = 0x6ffffffd, DT_VERNEED = 0x6ffffffe, DT_VERNEEDNUM = 0x6fffffff,
                  DT_AARCH64_BTI_PLT = 0x70000001, DT_AARCH64_PAC_PLT = 0x70000003,
                  DT_AARCH64_VARIANT_PCS = 0x70000005;
constexpr uint64_t DF_TEXTREL = 0x4, DF_STATIC_TLS = 0x10;

constexpr uint32_t R_AARCH64_NONE = 0, R_AARCH64_ABS64 = 257, R_AARCH64_COPY = 1024,
                   R_AARCH64_GLOB_DAT = 1025, R_AARCH64_JUMP_SLOT = 1026,
                   R_AARCH64_RELATIVE = 1027, R_AARCH64_TLS_DTPMOD = 1028,
                   R_AARCH64_TLS_DTPREL = 1029, R_AARCH64_TLS_TPREL = 1030,
                   R_AARCH64_TLSDESC = 1031, R_AARCH64_IRELATIVE = 1032;

constexpr uint16_t SHN_UNDEF = 0, SHN_ABS = 0xfff1;
constexpr uint8_t STT_TLS = 6, STT_GNU_IFUNC = 10, STO_AARCH64_VARIANT_PCS = 0x80;

struct Elf64Dyn {
    int64_t tag;
    uint64_t val;
};

/// Formats like printf into a std::string.
std::string Fmt(const char* f, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, f);
    std::vsnprintf(buf, sizeof buf, f, ap);
    va_end(ap);
    return buf;
}

std::string Hex(uint64_t v) { return Fmt("0x%llx", static_cast<unsigned long long>(v)); }

/// Names the relocation types the loader knows of, applied or refused.
const char* RelocName(uint32_t t) {
    switch (t) {
    case R_AARCH64_NONE: return "NONE";
    case R_AARCH64_ABS64: return "ABS64";
    case R_AARCH64_COPY: return "COPY";
    case R_AARCH64_GLOB_DAT: return "GLOB_DAT";
    case R_AARCH64_JUMP_SLOT: return "JUMP_SLOT";
    case R_AARCH64_RELATIVE: return "RELATIVE";
    case R_AARCH64_TLS_DTPMOD: return "TLS_DTPMOD64";
    case R_AARCH64_TLS_DTPREL: return "TLS_DTPREL64";
    case R_AARCH64_TLS_TPREL: return "TLS_TPREL64";
    case R_AARCH64_TLSDESC: return "TLSDESC";
    case R_AARCH64_IRELATIVE: return "IRELATIVE";
    default: return nullptr;
    }
}

/// The Windows protection for ELF segment flags, or 0 when Windows cannot express them
/// (write without read) or N0 refuses them (write with execute).
DWORD Protection(uint32_t flags) {
    const bool r = flags & PF_R, w = flags & PF_W, x = flags & macapk::translator::PF_X;
    if (w && (x || !r)) return 0;
    if (x) return r ? PAGE_EXECUTE_READ : PAGE_EXECUTE;
    if (w) return PAGE_READWRITE;
    return r ? PAGE_READONLY : PAGE_NOACCESS;
}

std::string PermName(uint32_t flags) {
    std::string s;
    if (flags & PF_R) s += 'R';
    if (flags & PF_W) s += 'W';
    if (flags & macapk::translator::PF_X) s += 'X';
    return s.empty() ? "-" : s;
}

uint64_t Down(uint64_t v, uint64_t page) { return v & ~(page - 1); }
uint64_t Up(uint64_t v, uint64_t page) { return (v + page - 1) & ~(page - 1); }

}  // namespace

const char* CategoryName(Category c) {
    switch (c) {
    case Category::ElfMalformed: return "ELF_MALFORMED";
    case Category::ElfUnsupported: return "ELF_UNSUPPORTED";
    case Category::RelocationUnsupported: return "ELF_RELOCATION_UNSUPPORTED";
    case Category::SymbolUnresolved: return "GUEST_SYMBOL_UNRESOLVED";
    case Category::AbiUnsupported: return "GUEST_ABI_UNSUPPORTED";
    case Category::HostMemory: return "HOST_MEMORY_FAILED";
    }
    return "?";
}

Module::~Module() {
    if (tracked_) UntrackModule(this);
    if (base_) VirtualFree(reinterpret_cast<void*>(base_), 0, MEM_RELEASE);
}

bool Module::Executable(uintptr_t address) const {
    if (address & 3) return false;
    for (const auto& [start, length] : executable_)
        if (address >= start && address - start < length) return true;
    return false;
}

bool Module::FaultAddress(uintptr_t pc, uint64_t* address) const {
    if (!Executable(pc)) return false;
    *address = pc - static_cast<uintptr_t>(bias_);
    return true;
}

namespace {

/// The GNU hash of a symbol name (dl_new_hash).
uint32_t GnuHash(std::string_view s) {
    uint32_t h = 5381;
    for (unsigned char c : s) h = h * 33 + c;
    return h;
}

/// The System V hash of a symbol name (elf_hash).
uint32_t SysvHash(std::string_view s) {
    uint32_t h = 0;
    for (unsigned char c : s) {
        h = (h << 4) + c;
        const uint32_t g = h & 0xf0000000;
        if (g) h ^= g >> 24;
        h &= ~g;
    }
    return h;
}

}  // namespace

void* Module::Find(std::string_view name) const {
    auto match = [&](uint32_t i) -> void* {
        Elf64Sym s;
        std::memcpy(&s, symtab_ + size_t{i} * sizeof s, sizeof s);
        if (s.st_shndx == SHN_UNDEF || s.st_name >= strsz_) return nullptr;
        const uint8_t bind = s.st_info >> 4, vis = s.st_other & 3;
        if (bind != macapk::translator::STB_GLOBAL && bind != macapk::translator::STB_WEAK) return nullptr;
        if (vis != 0 && vis != 3) return nullptr;  // hidden or internal
        if (name != std::string_view(strtab_ + s.st_name)) return nullptr;
        const uint64_t v = s.st_shndx == SHN_ABS ? s.st_value : s.st_value + bias_;
        return reinterpret_cast<void*>(v);
    };
    if (gnuHash_) {
        const uint32_t nbuckets = gnuHash_[0], symoffset = gnuHash_[1], bloomWords = gnuHash_[2];
        const uint32_t* buckets = gnuHash_ + 4 + size_t{bloomWords} * 2;
        const uint32_t* chain = buckets + nbuckets;
        const uint32_t h = GnuHash(name);
        for (uint32_t i = buckets[h % nbuckets]; i >= symoffset && i < nsyms_; i++) {
            const uint32_t c = chain[i - symoffset];
            if ((c | 1) == (h | 1)) {
                if (void* p = match(i)) return p;
            }
            if (c & 1) break;
        }
        return nullptr;
    }
    if (sysvHash_) {
        const uint32_t nbucket = sysvHash_[0];
        const uint32_t* bucket = sysvHash_ + 2;
        const uint32_t* chain = bucket + nbucket;
        for (uint32_t i = bucket[SysvHash(name) % nbucket], n = 0; i != 0 && i < nsyms_ && n < nsyms_;
             i = chain[i], n++) {
            if (void* p = match(i)) return p;
        }
    }
    return nullptr;
}

/// One load: the checks and steps of Load, sharing the file, the headers and the image.
class Loader {
public:
    Loader(std::span<const HostSymbol> imports, LoadError* error) : imports_(imports), error_(error) {}

    std::unique_ptr<Module> Run(const std::wstring& path) {
        if (!Prepare(path, false) || !Finish({})) return nullptr;
        return Take();
    }

    /// Prepares and screens an image without resolving imports or running initializers.
    bool Prepare(const std::wstring& path, bool managed) {
        managed_ = managed;
        mod_.reset(new Module);
        const std::u8string name = std::filesystem::path(path).filename().u8string();
        mod_->name_.assign(name.begin(), name.end());
        if (!ReadFile(path) || !CheckHeaders() || !PlanPages() || !Map() || !ReadDynamic() ||
            !CheckSymbols() || !Screen()) {
            mod_.reset();  // releases whatever was reserved
            return false;
        }
        return true;
    }

    /// Resolves relocations, validates initialization, protects pages and registers fault ranges.
    bool Finish(std::function<void*(std::string_view)> resolver) {
        resolver_ = std::move(resolver);
        if (!Relocate(false) || !Relocate(true) || !Initializers() || !Protect()) return false;
        if (!TrackModule(mod_.get())) return Fail(Category::HostMemory, "fault boundary", "vectored handler registration failed");
        mod_->tracked_ = true;
        return true;
    }

    /// Returns the prepared image for dependency/export discovery, never execution before Finish.
    const Module& Image() const { return *mod_; }
    /// Transfers the fully linked image after Finish succeeds.
    std::unique_ptr<Module> Take() { return std::move(mod_); }

private:
    bool Fail(Category c, std::string where, std::string reason) {
        error_->category = c;
        error_->where = std::move(where);
        error_->reason = std::move(reason);
        return false;
    }
    bool Malformed(uint64_t fileOffset, std::string reason) {
        return Fail(Category::ElfMalformed, "file offset " + Hex(fileOffset), std::move(reason));
    }

    bool InFile(uint64_t off, uint64_t len) const { return off <= file_.size() && len <= file_.size() - off; }

    /// The loaded segment holding [vaddr, vaddr + len), or nullptr.
    const Elf64Phdr* SegmentOf(uint64_t vaddr, uint64_t len) const {
        for (const Elf64Phdr& p : loads_) {
            if (vaddr >= p.p_vaddr && len <= p.p_memsz && vaddr - p.p_vaddr <= p.p_memsz - len) return &p;
        }
        return nullptr;
    }
    uint8_t* At(uint64_t vaddr) const { return reinterpret_cast<uint8_t*>(vaddr + mod_->bias_); }

    bool ReadFile(const std::wstring& path) {
        // One allocation of the file's size, one read.
        std::error_code ec;
        const uintmax_t size = std::filesystem::file_size(std::filesystem::path(path), ec);
        if (ec || size > (1ull << 31)) return Fail(Category::ElfMalformed, "", "cannot read the file, or it is over 2 GiB");
        std::ifstream f(std::filesystem::path(path), std::ios::binary);
        file_.resize(static_cast<size_t>(size));
        if (!f.read(reinterpret_cast<char*>(file_.data()), static_cast<std::streamsize>(size))) {
            return Fail(Category::ElfMalformed, "", "cannot read the file");
        }
        return true;
    }

    bool CheckHeaders() {
        if (file_.size() < sizeof(Elf64Ehdr)) {
            return Malformed(0, Fmt("%zu bytes: shorter than an ELF header", file_.size()));
        }
        std::memcpy(&eh_, file_.data(), sizeof eh_);
        if (std::memcmp(eh_.e_ident, "\x7f" "ELF", 4) != 0) return Malformed(0, "not an ELF file (bad magic)");
        if (eh_.e_ident[4] != macapk::translator::ELFCLASS64) {
            return Fail(Category::ElfUnsupported, "file offset 0x4",
                        eh_.e_ident[4] == macapk::translator::ELFCLASS32
                            ? "32-bit ELF (armeabi-v7a/x86): only arm64-v8a is supported"
                            : Fmt("ELF class %u unknown", eh_.e_ident[4]));
        }
        if (eh_.e_ident[5] != macapk::translator::ELFDATA2LSB) {
            return Fail(Category::ElfUnsupported, "file offset 0x5", "big-endian ELF");
        }
        if (eh_.e_ident[6] != 1 || eh_.e_version != 1) return Malformed(6, "ELF version is not 1");
        if (eh_.e_machine != macapk::translator::EM_AARCH64) {
            const char* m = eh_.e_machine == 62 ? " (x86-64)" : eh_.e_machine == 3 ? " (x86)"
                          : eh_.e_machine == 40 ? " (ARM)" : eh_.e_machine == 243 ? " (RISC-V)" : "";
            return Fail(Category::ElfUnsupported, "file offset 0x12",
                        Fmt("machine %u%s, not AArch64", eh_.e_machine, m));
        }
        if (eh_.e_type != macapk::translator::ET_DYN) {
            return Fail(Category::ElfUnsupported, "file offset 0x10",
                        Fmt("ELF type %u: only shared objects (ET_DYN) load", eh_.e_type));
        }
        if (eh_.e_phentsize != sizeof(Elf64Phdr) || eh_.e_phnum == 0 ||
            !InFile(eh_.e_phoff, uint64_t{eh_.e_phnum} * sizeof(Elf64Phdr))) {
            return Malformed(0x20, Fmt("%u program headers of %u bytes at %s: past the end of the file",
                                       eh_.e_phnum, eh_.e_phentsize, Hex(eh_.e_phoff).c_str()));
        }
        bool haveDynamic = false;
        for (uint32_t i = 0; i < eh_.e_phnum; i++) {
            const uint64_t at = eh_.e_phoff + uint64_t{i} * sizeof(Elf64Phdr);
            Elf64Phdr p;
            std::memcpy(&p, file_.data() + at, sizeof p);
            switch (p.p_type) {
            case macapk::translator::PT_LOAD: {
                if (p.p_filesz > p.p_memsz || !InFile(p.p_offset, p.p_filesz)) {
                    return Malformed(at, Fmt("PT_LOAD %zu: %s file bytes at %s run past the end of the file",
                                             loads_.size(), Hex(p.p_filesz).c_str(), Hex(p.p_offset).c_str()));
                }
                if (p.p_memsz == 0 || p.p_vaddr > (1ull << 40) || p.p_memsz > (1ull << 32)) {
                    return Malformed(at, Fmt("PT_LOAD %zu: address or size out of range", loads_.size()));
                }
                if (p.p_align > 1 && ((p.p_align & (p.p_align - 1)) != 0 ||
                                      p.p_vaddr % p.p_align != p.p_offset % p.p_align)) {
                    return Malformed(at, Fmt("PT_LOAD %zu: alignment %s does not match its offset",
                                             loads_.size(), Hex(p.p_align).c_str()));
                }
                if (!loads_.empty() && p.p_vaddr < loads_.back().p_vaddr + loads_.back().p_memsz) {
                    return Malformed(at, Fmt("PT_LOAD %zu at vaddr %s overlaps PT_LOAD %zu or is out of order",
                                             loads_.size(), Hex(p.p_vaddr).c_str(), loads_.size() - 1));
                }
                if (Protection(p.p_flags) == 0) {
                    return Fail(Category::ElfUnsupported, "file offset " + Hex(at),
                                Fmt("PT_LOAD %zu is %s: no long-lived writable-and-executable or write-only pages",
                                    loads_.size(), PermName(p.p_flags).c_str()));
                }
                loads_.push_back(p);
                break;
            }
            case macapk::translator::PT_DYNAMIC:
                if (haveDynamic || p.p_memsz % sizeof(Elf64Dyn) != 0) return Malformed(at, "a second or misshapen PT_DYNAMIC");
                haveDynamic = true;
                dynamic_ = p;
                break;
            case PT_INTERP:
                return Fail(Category::ElfUnsupported, "file offset " + Hex(at), "PT_INTERP: an executable, not a library");
            case PT_TLS:
                return Fail(Category::AbiUnsupported, "file offset " + Hex(at),
                            "PT_TLS: thread-local storage is not in profile N0");
            case PT_GNU_STACK:
                if (p.p_flags & macapk::translator::PF_X) {
                    return Fail(Category::ElfUnsupported, "file offset " + Hex(at),
                                "PT_GNU_STACK asks for an executable stack");
                }
                break;
            case PT_GNU_RELRO:
                relro_ = p;
                break;
            case 0: case PT_NOTE: case PT_PHDR: case PT_GNU_EH_FRAME: case PT_GNU_PROPERTY:
                break;
            default:
                return Fail(Category::ElfUnsupported, "file offset " + Hex(at),
                            Fmt("program header type %s unknown", Hex(p.p_type).c_str()));
            }
        }
        if (loads_.empty()) return Malformed(eh_.e_phoff, "no PT_LOAD");
        if (!haveDynamic) return Malformed(eh_.e_phoff, "no PT_DYNAMIC");
        return true;
    }

    // Segments may share a host page only with the same permissions: a page has one.
    bool PlanPages() {
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        page_ = si.dwPageSize;
        std::string segs;
        for (size_t i = 0; i < loads_.size(); i++) {
            segs += (i ? ", " : "") + PermName(loads_[i].p_flags);
            if (i == 0) continue;
            const Elf64Phdr& a = loads_[i - 1];
            const Elf64Phdr& b = loads_[i];
            if (Down(b.p_vaddr, page_) < Up(a.p_vaddr + a.p_memsz, page_) && a.p_flags != b.p_flags) {
                return Fail(Category::ElfUnsupported, "vaddr " + Hex(b.p_vaddr),
                            Fmt("PT_LOAD %zu (%s) and %zu (%s) share a host page of %s bytes; aligned to %s, "
                                "they cannot get their own permissions",
                                i - 1, PermName(a.p_flags).c_str(), i, PermName(b.p_flags).c_str(),
                                Hex(page_).c_str(), Hex(b.p_align).c_str()));
            }
        }
        mod_->stats_.segments = segs;
        lo_ = Down(loads_.front().p_vaddr, page_);
        hi_ = Up(loads_.back().p_vaddr + loads_.back().p_memsz, page_);
        return true;
    }

    bool Map() {
        void* base = VirtualAlloc(nullptr, hi_ - lo_, MEM_RESERVE, PAGE_NOACCESS);
        if (!base) {
            return Fail(Category::HostMemory, "", Fmt("reserving %s bytes failed (error %lu)",
                                                      Hex(hi_ - lo_).c_str(), GetLastError()));
        }
        mod_->base_ = reinterpret_cast<uintptr_t>(base);
        mod_->size_ = hi_ - lo_;
        mod_->bias_ = static_cast<int64_t>(mod_->base_ - lo_);
        for (const Elf64Phdr& p : loads_) {
            const uint64_t start = Down(p.p_vaddr, page_), end = Up(p.p_vaddr + p.p_memsz, page_);
            if (!VirtualAlloc(At(start), end - start, MEM_COMMIT, PAGE_READWRITE)) {
                return Fail(Category::HostMemory, "vaddr " + Hex(p.p_vaddr),
                            Fmt("committing %s bytes failed (error %lu)", Hex(end - start).c_str(), GetLastError()));
            }
            // Fresh pages are zero: BSS past p_filesz needs nothing more.
            std::memcpy(At(p.p_vaddr), file_.data() + p.p_offset, p.p_filesz);
            if (p.p_flags & macapk::translator::PF_X)
                mod_->executable_.push_back({reinterpret_cast<uintptr_t>(At(p.p_vaddr)), static_cast<size_t>(p.p_memsz)});
        }
        return true;
    }

    bool ReadDynamic() {
        if (!SegmentOf(dynamic_.p_vaddr, dynamic_.p_memsz)) {
            return Malformed(dynamic_.p_offset, "PT_DYNAMIC lies outside every PT_LOAD");
        }
        // Two passes: the string table first, so a DT_NEEDED can be named wherever it is.
        std::vector<Elf64Dyn> entries;
        uint64_t strtab = 0, strsz = 0;
        for (size_t i = 0; i < dynamic_.p_memsz / sizeof(Elf64Dyn); i++) {
            Elf64Dyn e;
            std::memcpy(&e, At(dynamic_.p_vaddr + i * sizeof e), sizeof e);
            if (e.tag == DT_NULL) break;
            if (e.tag == DT_STRTAB) strtab = e.val;
            if (e.tag == DT_STRSZ) strsz = e.val;
            entries.push_back(e);
        }
        if (!strtab || !strsz || !SegmentOf(strtab, strsz) || At(strtab)[strsz - 1] != 0) {
            return Malformed(dynamic_.p_offset, "DT_STRTAB is missing, outside the image or not terminated");
        }
        mod_->strtab_ = reinterpret_cast<const char*>(At(strtab));
        mod_->strsz_ = strsz;
        for (const Elf64Dyn& e : entries) {
            if (e.tag == DT_NEEDED) {
                if (!managed_) return Fail(Category::AbiUnsupported, "",
                                          Fmt("DT_NEEDED %s: profile N0 has no library dependencies", Str(e.val)));
                if (e.val >= mod_->strsz_) return Malformed(dynamic_.p_offset, "DT_NEEDED string outside DT_STRTAB");
                mod_->needed_.emplace_back(Str(e.val));
            }
        }
        std::map<int64_t, uint64_t> d;
        for (size_t i = 0; i < entries.size(); i++) {
            const Elf64Dyn& e = entries[i];
            const std::string where = "vaddr " + Hex(dynamic_.p_vaddr + i * sizeof e);
            switch (e.tag) {
            case DT_INIT: case DT_INIT_ARRAY:
                if (!managed_) return Fail(Category::AbiUnsupported, where,
                                          "constructors (DT_INIT, DT_INIT_ARRAY): profile N0 runs no guest code while loading");
                d[e.tag] = e.val;
                continue;
            case DT_PREINIT_ARRAY: case DT_PREINIT_ARRAYSZ:
                return Fail(Category::AbiUnsupported, where, "pre-initializers are not supported in shared libraries");
            case DT_FINI: case DT_FINI_ARRAY:
                if (!managed_) return Fail(Category::AbiUnsupported, where, "destructors (DT_FINI, DT_FINI_ARRAY) are not in profile N0");
                d[e.tag] = e.val;
                continue;
            case DT_VERNEED:
                return Fail(Category::AbiUnsupported, where, "versioned imports (DT_VERNEED) need libraries, not in profile N0");
            case DT_VERDEF: case DT_VERSYM:
                if (managed_) return Fail(Category::AbiUnsupported, where, "symbol version tables are not supported in this module profile");
                continue;
            case DT_RPATH: case DT_RUNPATH:
                if (managed_) return Fail(Category::AbiUnsupported, where, "library search paths are not supported in this module profile");
                continue;
            case DT_TEXTREL:
                return Fail(Category::ElfUnsupported, where, "text relocations (DT_TEXTREL), which Android refuses too");
            case DT_REL: case DT_ANDROID_REL:
                return Fail(Category::RelocationUnsupported, where, "REL relocations: AArch64 uses RELA");
            case DT_ANDROID_RELA:
                return Fail(Category::RelocationUnsupported, where,
                            "Android packed relocations (DT_ANDROID_RELA, APS2) are not implemented");
            case DT_AARCH64_VARIANT_PCS:
                return Fail(Category::AbiUnsupported, where,
                            "functions with a variant calling convention (DT_AARCH64_VARIANT_PCS) are not in profile N0");
            case DT_FLAGS:
                if (e.val & DF_TEXTREL) return Fail(Category::ElfUnsupported, where, "text relocations (DF_TEXTREL)");
                if (e.val & DF_STATIC_TLS) return Fail(Category::AbiUnsupported, where, "static TLS (DF_STATIC_TLS) is not in profile N0");
                continue;
            case DT_STRTAB: case DT_STRSZ: case DT_NEEDED:
                continue;
            case DT_PLTRELSZ: case DT_HASH: case DT_SYMTAB: case DT_RELA: case DT_RELASZ:
            case DT_RELAENT: case DT_SYMENT: case DT_PLTREL: case DT_JMPREL: case DT_RELR:
            case DT_RELRSZ: case DT_RELRENT: case DT_ANDROID_RELR: case DT_ANDROID_RELRSZ:
            case DT_ANDROID_RELRENT: case DT_GNU_HASH: case DT_SONAME:
            case DT_INIT_ARRAYSZ: case DT_FINI_ARRAYSZ:
                d[e.tag] = e.val;
                continue;
            // Known and harmless here: eager binding is the only binding; versions of the
            // module's own definitions are not checked (it has no importers but the host).
            case DT_PLTGOT: case DT_SYMBOLIC: case DT_DEBUG: case DT_BIND_NOW:
            case DT_RELSZ: case DT_RELENT:
            case DT_ANDROID_RELSZ: case DT_ANDROID_RELASZ: case DT_ANDROID_RELRCOUNT:
            case DT_RELACOUNT: case DT_RELCOUNT: case DT_FLAGS_1: case DT_VERDEFNUM:
            case DT_VERNEEDNUM: case DT_AARCH64_BTI_PLT: case DT_AARCH64_PAC_PLT:
                continue;
            default:
                return Fail(Category::ElfUnsupported, where, Fmt("dynamic tag %s unknown", Hex(e.tag).c_str()));
            }
        }
        if (d.count(DT_SONAME)) {
            if (d[DT_SONAME] >= mod_->strsz_) return Malformed(dynamic_.p_offset, "DT_SONAME string outside DT_STRTAB");
            mod_->soname_ = Str(d[DT_SONAME]);
        }
        init_ = d[DT_INIT];
        fini_ = d[DT_FINI];
        initArray_ = d[DT_INIT_ARRAY]; initSize_ = d[DT_INIT_ARRAYSZ];
        finiArray_ = d[DT_FINI_ARRAY]; finiSize_ = d[DT_FINI_ARRAYSZ];
        if ((initSize_ && !initArray_) || (finiSize_ && !finiArray_))
            return Malformed(dynamic_.p_offset, "initializer/finalizer size without an array");
        if (!d.count(DT_SYMTAB)) return Malformed(dynamic_.p_offset, "PT_DYNAMIC lacks DT_SYMTAB");
        if (d.count(DT_SYMENT) && d[DT_SYMENT] != sizeof(Elf64Sym)) return Malformed(dynamic_.p_offset, "DT_SYMENT is not 24");
        if (d.count(DT_RELAENT) && d[DT_RELAENT] != sizeof(Elf64Rela)) return Malformed(dynamic_.p_offset, "DT_RELAENT is not 24");
        if (d.count(DT_PLTREL) && d[DT_PLTREL] != DT_RELA) {
            return Fail(Category::RelocationUnsupported, "", "PLT relocations are not RELA");
        }

        // The symbol count: a hash table says how many symbols there are.
        const uint64_t symtab = d[DT_SYMTAB];
        if (d.count(DT_GNU_HASH)) {
            const uint64_t h = d[DT_GNU_HASH];
            if (!SegmentOf(h, 16)) return Malformed(dynamic_.p_offset, "DT_GNU_HASH lies outside the image");
            const uint32_t* t = reinterpret_cast<const uint32_t*>(At(h));
            const uint64_t nb = t[0], symoffset = t[1], bloom = t[2];
            const uint64_t chainAt = h + 16 + bloom * 8 + nb * 4;
            if (nb == 0 || !SegmentOf(h, 16 + bloom * 8 + nb * 4)) return Malformed(dynamic_.p_offset, "DT_GNU_HASH is misshapen");
            uint64_t count = symoffset;
            for (uint64_t b = 0; b < nb; b++) {
                uint64_t i = t[4 + bloom * 2 + b];
                if (i == 0) continue;
                if (i < symoffset) return Malformed(dynamic_.p_offset, "DT_GNU_HASH bucket before its first symbol");
                for (;; i++) {
                    if (!SegmentOf(chainAt + (i - symoffset) * 4, 4)) return Malformed(dynamic_.p_offset, "DT_GNU_HASH chain runs out of the image");
                    if (i + 1 > count) count = i + 1;
                    if (*reinterpret_cast<const uint32_t*>(At(chainAt + (i - symoffset) * 4)) & 1) break;
                }
            }
            mod_->gnuHash_ = t;
            mod_->nsyms_ = static_cast<uint32_t>(count);
            mod_->stats_.hash = "GNU";
        } else if (d.count(DT_HASH)) {
            const uint64_t h = d[DT_HASH];
            if (!SegmentOf(h, 8)) return Malformed(dynamic_.p_offset, "DT_HASH lies outside the image");
            const uint32_t* t = reinterpret_cast<const uint32_t*>(At(h));
            if (t[0] == 0 || !SegmentOf(h, 8 + (uint64_t{t[0]} + t[1]) * 4)) return Malformed(dynamic_.p_offset, "DT_HASH is misshapen");
            mod_->sysvHash_ = t;
            mod_->nsyms_ = t[1];
            mod_->stats_.hash = "SysV";
        } else {
            return Malformed(dynamic_.p_offset, "no DT_GNU_HASH or DT_HASH: the symbols cannot be counted or found");
        }
        if (!SegmentOf(symtab, uint64_t{mod_->nsyms_} * sizeof(Elf64Sym))) return Malformed(dynamic_.p_offset, "DT_SYMTAB lies outside the image");
        mod_->symtab_ = At(symtab);
        mod_->stats_.symbols = mod_->nsyms_;

        tables_.clear();
        auto table = [&](int64_t at, int64_t size, char kind) -> bool {
            if (!d.count(at)) return true;
            const uint64_t sz = d.count(size) ? d[size] : 0;
            const uint64_t ent = kind == 'r' ? 8 : sizeof(Elf64Rela);
            if (sz % ent != 0 || !SegmentOf(d[at], sz)) return Malformed(dynamic_.p_offset, "a relocation table lies outside the image");
            tables_.push_back({d[at], sz, kind});
            return true;
        };
        return table(DT_RELR, DT_RELRSZ, 'r') && table(DT_ANDROID_RELR, DT_ANDROID_RELRSZ, 'r') &&
               table(DT_RELA, DT_RELASZ, 'a') && table(DT_JMPREL, DT_PLTRELSZ, 'a');
    }

    const char* Str(uint64_t off) const { return off < mod_->strsz_ ? mod_->strtab_ + off : "?"; }

    Elf64Sym Sym(uint32_t i) const {
        Elf64Sym s;
        std::memcpy(&s, mod_->symtab_ + size_t{i} * sizeof s, sizeof s);
        return s;
    }

    // Every symbol the module defines: inside the image, and of a kind N0 admits.
    bool CheckSymbols() {
        for (uint32_t i = 1; i < mod_->nsyms_; i++) {
            const Elf64Sym s = Sym(i);
            const uint8_t type = s.st_info & 0xf;
            if (s.st_name >= mod_->strsz_) return Malformed(dynamic_.p_offset, Fmt("symbol %u has no name", i));
            const std::string where = "symbol " + std::string(Str(s.st_name));
            if (type == STT_TLS) return Fail(Category::AbiUnsupported, where, "a thread-local symbol: TLS is not in profile N0");
            if (s.st_other & STO_AARCH64_VARIANT_PCS) {
                return Fail(Category::AbiUnsupported, where, "a variant calling convention (vector or SVE PCS) is not in profile N0");
            }
            if (s.st_shndx == SHN_UNDEF) continue;
            if (type == STT_GNU_IFUNC) {
                return Fail(Category::AbiUnsupported, where, "an IFUNC: its resolver would run guest code while loading");
            }
            if (s.st_shndx != SHN_ABS && !SegmentOf(s.st_value, s.st_size ? s.st_size : 1)) {
                return Malformed(dynamic_.p_offset, where + " lies outside the image");
            }
        }
        return true;
    }

    // Screens every executable word: a raw Linux system call, the Android thread pointer
    // and x18 are refused (PLAN.md §5.1, §9.3). A screen, not a proof (native-profiles.md).
    bool Screen() {
        csh cs;
        if (cs_open(CS_ARCH_ARM64, CS_MODE_ARM, &cs) != CS_ERR_OK) {
            return Fail(Category::HostMemory, "", "the disassembler (Capstone) did not open");
        }
        cs_option(cs, CS_OPT_DETAIL, CS_OPT_ON);
        cs_insn* insn = cs_malloc(cs);
        bool ok = true;
        for (const Elf64Phdr& p : loads_) {
            if (!(p.p_flags & macapk::translator::PF_X) || !ok) continue;
            const uint64_t start = Up(p.p_vaddr, 4), end = Down(p.p_vaddr + p.p_filesz, 4);
            mod_->stats_.codeBytes += end > start ? end - start : 0;
            for (uint64_t va = start; ok && va < end; va += 4) {
                uint32_t w;
                std::memcpy(&w, At(va), 4);
                const uint8_t* code = At(va);
                size_t left = 4;
                uint64_t addr = va;
                const bool decoded = cs_disasm_iter(cs, &code, &left, &addr, insn);
                // The message is only built for the word that is refused.
                auto refuse = [&](const char* why) {
                    const std::string text = decoded ? Fmt("%s %s", insn->mnemonic, insn->op_str) : Fmt(".word %s", Hex(w).c_str());
                    return Fail(Category::AbiUnsupported, "vaddr " + Hex(va), text + why);
                };
                if ((w & 0xFFE0001F) == 0xD4000001 || (w & 0xFFE0001F) == 0xD4000002 || (w & 0xFFE0001F) == 0xD4000003) {
                    ok = refuse(": a raw system call, never passed to Windows");
                } else if ((w & 0xFFDFFFE0) == 0xD51BD040) {
                    ok = refuse(": TPIDR_EL0 is the host's, not Android's thread pointer");
                } else if (!decoded) {
                    mod_->stats_.dataWords++;
                } else {
                    mod_->stats_.instructions++;
                    if (UsesX18(insn)) ok = refuse(": x18 holds the Windows TEB");
                }
            }
        }
        cs_free(insn, 1);
        cs_close(&cs);
        return ok;
    }

    static bool UsesX18(const cs_insn* insn) {
        auto is18 = [](unsigned r) { return r == ARM64_REG_X18 || r == ARM64_REG_W18; };
        const cs_detail* d = insn->detail;
        for (uint8_t i = 0; i < d->regs_read_count; i++) if (is18(d->regs_read[i])) return true;
        for (uint8_t i = 0; i < d->regs_write_count; i++) if (is18(d->regs_write[i])) return true;
        for (uint8_t i = 0; i < d->arm64.op_count; i++) {
            const cs_arm64_op& op = d->arm64.operands[i];
            if (op.type == ARM64_OP_REG && is18(op.reg)) return true;
            if (op.type == ARM64_OP_MEM && (is18(op.mem.base) || is18(op.mem.index))) return true;
        }
        return false;
    }

    // Pass `apply` false checks every relocation and import and lists all missing symbols;
    // pass true writes them, so nothing is written to a module that is then refused.
    bool Relocate(bool apply) {
        std::map<std::string, uint32_t> counts;
        std::vector<std::string> missing;
        std::string missingWhere;
        for (const Table& t : tables_) {
            if (t.kind == 'r') {
                uint64_t addr = 0;
                for (uint64_t off = 0; off < t.size; off += 8) {
                    uint64_t e;
                    std::memcpy(&e, At(t.vaddr + off), 8);
                    if ((e & 1) == 0) {
                        if (!Relative(e, apply)) return false;
                        addr = e + 8;
                        counts["RELR"]++;
                    } else {
                        for (uint64_t bit = 1; bit < 64; bit++) {
                            if ((e >> bit) & 1) {
                                if (!Relative(addr + (bit - 1) * 8, apply)) return false;
                                counts["RELR"]++;
                            }
                        }
                        addr += 63 * 8;
                    }
                }
                continue;
            }
            for (uint64_t off = 0; off < t.size; off += sizeof(Elf64Rela)) {
                Elf64Rela r;
                std::memcpy(&r, At(t.vaddr + off), sizeof r);
                const uint32_t type = static_cast<uint32_t>(r.r_info), symi = static_cast<uint32_t>(r.r_info >> 32);
                const std::string where = "vaddr " + Hex(r.r_offset);
                const char* name = RelocName(type);
                if (type == R_AARCH64_NONE) continue;
                if (type == R_AARCH64_TLS_DTPMOD || type == R_AARCH64_TLS_DTPREL || type == R_AARCH64_TLS_TPREL ||
                    type == R_AARCH64_TLSDESC) {
                    return Fail(Category::RelocationUnsupported, where,
                                Fmt("R_AARCH64_%s: TLS relocations are not in profile N0", name));
                }
                if (type == R_AARCH64_IRELATIVE) {
                    return Fail(Category::RelocationUnsupported, where,
                                "R_AARCH64_IRELATIVE: an IFUNC resolver would run guest code while loading");
                }
                if (type == R_AARCH64_COPY) {
                    return Fail(Category::RelocationUnsupported, where, "R_AARCH64_COPY belongs to executables");
                }
                if (!name) return Fail(Category::RelocationUnsupported, where, Fmt("relocation type %u unknown", type));
                if (!Writable(r.r_offset)) return Malformed(t.vaddr + off, where + ": relocation target outside the writable image");
                if (symi >= mod_->nsyms_) return Malformed(t.vaddr + off, Fmt("relocation symbol %u out of range", symi));
                uint64_t value;
                if (type == R_AARCH64_RELATIVE) {
                    value = static_cast<uint64_t>(mod_->bias_ + r.r_addend);
                } else {
                    const Elf64Sym s = Sym(symi);
                    uint64_t sv = 0;
                    if (s.st_shndx == SHN_ABS) {
                        sv = s.st_value;
                    } else if (s.st_shndx != SHN_UNDEF) {
                        sv = s.st_value + mod_->bias_;  // the module's own definition wins
                    } else if (void* h = Import(Str(s.st_name))) {
                        sv = reinterpret_cast<uint64_t>(h);
                    } else if ((s.st_info >> 4) != macapk::translator::STB_WEAK) {
                        if (missingWhere.empty()) missingWhere = where;
                        if (std::find(missing.begin(), missing.end(), Str(s.st_name)) == missing.end()) {
                            missing.push_back(Str(s.st_name));
                        }
                    }  // an unresolved weak import is 0, as on Android
                    value = sv + r.r_addend;
                }
                if (apply) std::memcpy(At(r.r_offset), &value, 8);
                counts[name]++;
            }
        }
        if (!missing.empty()) {
            std::string list;
            for (const std::string& m : missing) list += (list.empty() ? "" : ", ") + m;
            return Fail(Category::SymbolUnresolved, missingWhere,
                        Fmt("%zu import%s the host does not provide: %s", missing.size(), missing.size() == 1 ? "" : "s", list.c_str()));
        }
        if (apply) {
            std::string s;
            for (const char* k : {"RELR", "RELATIVE", "ABS64", "GLOB_DAT", "JUMP_SLOT"}) {
                if (counts.count(k)) s += (s.empty() ? "" : ", ") + Fmt("%s %u", k, counts[k]);
            }
            mod_->stats_.relocations = s.empty() ? "none" : s;
        }
        return true;
    }

    bool Relative(uint64_t vaddr, bool apply) {
        if (!Writable(vaddr)) return Malformed(dynamic_.p_offset, "RELR target " + Hex(vaddr) + " outside the writable image");
        if (apply) {
            uint64_t v;
            std::memcpy(&v, At(vaddr), 8);
            v += static_cast<uint64_t>(mod_->bias_);
            std::memcpy(At(vaddr), &v, 8);
        }
        return true;
    }

    bool Writable(uint64_t vaddr) const {
        const Elf64Phdr* p = SegmentOf(vaddr, 8);
        return p && (p->p_flags & PF_W);
    }

    void* Import(const char* name) const {
        if (resolver_) if (void* address = resolver_(name)) return address;
        for (const HostSymbol& h : imports_) if (std::strcmp(h.name, name) == 0) return h.address;
        return nullptr;
    }

    /// Validates every initializer/finalizer against the image's executable segments before any call.
    bool Initializers() {
        auto target = [&](uint64_t address, bool keep) {
            if (!address || address == UINT64_MAX) return true;
            if (!mod_->Executable(static_cast<uintptr_t>(address)))
                return Fail(Category::AbiUnsupported, "initializer/finalizer", "target is not aligned executable code in its image");
            if (keep) mod_->initializers_.push_back(reinterpret_cast<void (*)()>(address));
            return true;
        };
        if (init_ && !target(reinterpret_cast<uintptr_t>(At(init_)), true)) return false;
        if (fini_ && !target(reinterpret_cast<uintptr_t>(At(fini_)), false)) return false;
        for (const bool keep : {true, false}) {
            const uint64_t array = keep ? initArray_ : finiArray_, size = keep ? initSize_ : finiSize_;
            if (!array && !size) continue;
            if (size % 8 || !SegmentOf(array, size))
                return Malformed(dynamic_.p_offset, "initializer/finalizer array outside the image or mis-sized");
            for (uint64_t offset = 0; offset < size; offset += 8) {
                uint64_t address;
                std::memcpy(&address, At(array + offset), 8);
                if (!target(address, keep)) return false;
            }
        }
        return true;
    }

    // Final permissions, then RELRO read-only over whole pages inside it.
    bool Protect() {
        DWORD old;
        for (const Elf64Phdr& p : loads_) {
            const uint64_t start = Down(p.p_vaddr, page_), end = Up(p.p_vaddr + p.p_memsz, page_);
            if (!VirtualProtect(At(start), end - start, Protection(p.p_flags), &old)) {
                return Fail(Category::HostMemory, "vaddr " + Hex(p.p_vaddr),
                            Fmt("setting %s on %s bytes failed (error %lu)", PermName(p.p_flags).c_str(),
                                Hex(end - start).c_str(), GetLastError()));
            }
        }
        if (relro_.p_memsz) {
            const Elf64Phdr* seg = SegmentOf(relro_.p_vaddr, relro_.p_memsz);
            if (!seg) return Malformed(relro_.p_offset, "PT_GNU_RELRO lies outside every PT_LOAD");
            // The page below RELRO's start is its own only when RELRO starts its segment.
            const uint64_t start = relro_.p_vaddr == seg->p_vaddr ? Down(relro_.p_vaddr, page_) : Up(relro_.p_vaddr, page_);
            const uint64_t end = Down(relro_.p_vaddr + relro_.p_memsz, page_);
            if (end > start) {
                if (!VirtualProtect(At(start), end - start, PAGE_READONLY, &old)) {
                    return Fail(Category::HostMemory, "vaddr " + Hex(start), Fmt("RELRO protection failed (error %lu)", GetLastError()));
                }
                mod_->stats_.relroBytes = end - start;
            }
        }
        FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(mod_->base_), mod_->size_);
        return true;
    }

    struct Table {
        uint64_t vaddr, size;
        char kind;  // 'r' RELR, 'a' RELA
    };

    std::span<const HostSymbol> imports_;
    LoadError* error_;
    std::unique_ptr<Module> mod_;
    std::vector<uint8_t> file_;
    Elf64Ehdr eh_{};
    std::vector<Elf64Phdr> loads_;
    Elf64Phdr dynamic_{}, relro_{};
    std::vector<Table> tables_;
    uint64_t page_ = 4096, lo_ = 0, hi_ = 0;
    bool managed_ = false;
    uint64_t init_ = 0, fini_ = 0, initArray_ = 0, initSize_ = 0, finiArray_ = 0, finiSize_ = 0;
    std::function<void*(std::string_view)> resolver_;
};

/// Retains one graph node independently of explicit open references until process teardown.
struct LibraryRecord {
    std::string name;
    uint64_t handle = 0, references = 0;
    std::unique_ptr<Loader> preparation;
    std::unique_ptr<Module> module;
    std::vector<LibraryRecord*> dependencies;
    bool initializing = false, initialized = false;

    /// Returns the image in either transactional preparation or committed state.
    const Module& Image() const { return preparation ? preparation->Image() : *module; }
};

/// Implements one bounded library namespace and its initialization transaction.
struct ModuleSet::Impl {
    std::filesystem::path directory;
    std::vector<HostSymbol> imports;
    mutable std::recursive_mutex mutex;
    std::map<std::string, std::unique_ptr<LibraryRecord>, std::less<>> records;
    uint64_t nextHandle = 1;
    std::thread::id initializingThread;
    size_t initializationDepth = 0;

    /// Restricts dependency names to portable basenames, avoiding host search paths and aliases.
    static bool Name(std::string_view name) {
        if (name.size() < 4 || name.size() > 255 || !name.ends_with(".so") || name.front() == '.') return false;
        for (unsigned char c : name)
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                  c == '_' || c == '-' || c == '+' || c == '.')) return false;
        return true;
    }

    /// Stores a stable load error scoped to the responsible module.
    static uint64_t Error(LoadError* error, Category category, std::string name, std::string reason) {
        *error = {category, "module " + name, std::move(reason)};
        return 0;
    }

    /// Looks up an export in self then breadth-first dependency order; no host export fallback.
    static void* Lookup(const LibraryRecord& root, std::string_view symbol) {
        std::vector<const LibraryRecord*> queue{&root};
        std::set<const LibraryRecord*> seen;
        for (size_t i = 0; i < queue.size(); i++) {
            const auto* record = queue[i];
            if (!seen.insert(record).second) continue;
            if (void* result = record->Image().Find(symbol)) return result;
            for (const auto* dependency : record->dependencies) queue.push_back(dependency);
        }
        return nullptr;
    }

    /// Finds a retained handle without interpreting guest data as a host pointer.
    LibraryRecord* Handle(uint64_t handle) const {
        if (!handle) return nullptr;
        for (const auto& [name, record] : records) {
            (void)name;
            if (record->handle == handle) return record.get();
        }
        return nullptr;
    }

    /// Prepares a whole new graph, rolls back on failure, then commits and initializes it.
    uint64_t Open(std::string_view name, LoadError* error, bool* committed) {
        std::unique_lock lock(mutex);
        if (!Name(name)) return Error(error, Category::AbiUnsupported, std::string(name), "expected a portable .so basename");
        if (initializationDepth && initializingThread != std::this_thread::get_id())
            return Error(error, Category::AbiUnsupported, std::string(name), "concurrent module initialization is not supported");
        if (auto found = records.find(name); found != records.end()) {
            if (found->second->references == UINT64_MAX)
                return Error(error, Category::AbiUnsupported, std::string(name), "library reference limit reached");
            found->second->references++;
            return found->second->handle;
        }
        // This map has the same allocator/type as records, so merge commits without allocating.
        decltype(records) staged;
        std::set<std::string, std::less<>> visiting;
        std::vector<LibraryRecord*> order;
        std::function<LibraryRecord*(std::string_view)> prepare = [&](std::string_view next) -> LibraryRecord* {
            if (!Name(next)) {
                Error(error, Category::AbiUnsupported, std::string(next), "dependency is not a portable .so basename");
                return nullptr;
            }
            if (visiting.contains(next)) {
                Error(error, Category::AbiUnsupported, std::string(next), "dependency cycles are not supported");
                return nullptr;
            }
            if (auto found = records.find(next); found != records.end()) return found->second.get();
            if (auto found = staged.find(next); found != staged.end()) return found->second.get();
            if (records.size() + staged.size() >= 256 || nextHandle == UINT64_MAX) {
                Error(error, Category::AbiUnsupported, std::string(next), "library graph limit reached");
                return nullptr;
            }
            const auto path = directory / std::filesystem::path(std::string(next));
            std::error_code ec;
            const auto status = std::filesystem::symlink_status(path, ec);
            if (ec || !std::filesystem::is_regular_file(status)) {
                Error(error, Category::SymbolUnresolved, std::string(next), "dependency file missing or not a regular library");
                return nullptr;
            }
            auto record = std::make_unique<LibraryRecord>();
            record->name = next;
            record->handle = nextHandle++;
            record->preparation = std::make_unique<Loader>(imports, error);
            if (!record->preparation->Prepare(path.wstring(), true)) {
                error->where = "module " + record->name + " " + error->where;
                return nullptr;
            }
            if (!record->Image().Soname().empty() && record->Image().Soname() != next) {
                Error(error, Category::AbiUnsupported, std::string(next), "SONAME does not match the requested basename");
                return nullptr;
            }
            auto* result = record.get();
            staged.emplace(record->name, std::move(record));
            visiting.emplace(next);
            for (const auto& needed : result->Image().Needed()) {
                auto* dependency = prepare(needed);
                if (!dependency) return nullptr;
                result->dependencies.push_back(dependency);
            }
            visiting.erase(std::string(next));
            order.push_back(result);
            return result;
        };
        LibraryRecord* root = prepare(name);
        if (!root) return 0;
        for (auto* record : order) {
            if (!record->preparation->Finish([record](std::string_view symbol) { return Lookup(*record, symbol); })) {
                error->where = "module " + record->name + " " + error->where;
                return 0;
            }
        }
        for (auto* record : order) {
            record->module = record->preparation->Take();
            record->preparation.reset();
        }
        records.merge(staged);
        *committed = true;
        root->references++;
        initializingThread = std::this_thread::get_id();
        initializationDepth++;
        // Never hold the registry lock across arbitrary guest callbacks.
        lock.unlock();
        for (auto* record : order) {
            {
                std::lock_guard guard(mutex);
                if (record->initialized || record->initializing) continue;
                record->initializing = true;
            }
            for (auto initializer : record->module->Initializers()) initializer();
            {
                std::lock_guard guard(mutex);
                record->initializing = false;
                record->initialized = true;
            }
        }
        lock.lock();
        if (--initializationDepth == 0) initializingThread = {};
        return root->handle;
    }
};

ModuleSet::ModuleSet(std::wstring directory, std::span<const HostSymbol> imports) : impl_(std::make_unique<Impl>()) {
    impl_->directory = std::move(directory);
    impl_->imports.assign(imports.begin(), imports.end());
}

ModuleSet::~ModuleSet() = default;

uint64_t ModuleSet::Open(std::string_view name, LoadError* error) {
    bool committed = false;
    try {
        return impl_->Open(name, error, &committed);
    } catch (const std::bad_alloc&) {
        if (committed) {
            std::fputs("HOST_MEMORY_FAILED module initialization: allocation failed after commit\n", stderr);
            std::fflush(stderr);
            TerminateProcess(GetCurrentProcess(), 3);
            std::abort();
        }
        return Impl::Error(error, Category::HostMemory, std::string(name), "library transaction allocation failed");
    }
}

bool ModuleSet::Close(uint64_t handle) {
    std::lock_guard lock(impl_->mutex);
    auto* record = impl_->Handle(handle);
    if (!record || !record->references) return false;
    record->references--;
    return true;
}

void* ModuleSet::Find(uint64_t handle, std::string_view symbol) const {
    std::lock_guard lock(impl_->mutex);
    auto* record = impl_->Handle(handle);
    if (!record || !record->references ||
        (impl_->initializationDepth && impl_->initializingThread != std::this_thread::get_id())) return nullptr;
    return Impl::Lookup(*record, symbol);
}

size_t ModuleSet::Mapped() const {
    std::lock_guard lock(impl_->mutex);
    return impl_->records.size();
}

std::unique_ptr<Module> Load(const std::wstring& path, std::span<const HostSymbol> imports, LoadError* error) {
    return Loader(imports, error).Run(path);
}

}  // namespace winapk::guest
