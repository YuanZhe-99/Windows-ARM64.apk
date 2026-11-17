// elf_loader.h -- N0 ELF loader and P3 managed module graphs on the Windows host.
//
// Maps an Android arm64-v8a shared object (ELF64, AArch64, ET_DYN) from its program headers
// into memory the host reserves, applies its relocations, binds its imports to host symbols,
// screens its code and sets the final page protections. Nothing of the module runs while it
// loads through Load: no constructor, no IFUNC resolver. Profile N0 (native-profiles.md) admits only a
// leaf library: no dependencies, no TLS, no constructors, no x18/TPIDR_EL0/svc instructions.
// ModuleSet additionally supports dependency/constructor transactions with process-lifetime
// pinning. Host code: only explicit guest layouts and audited signatures cross the ABI.
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace winapk::guest {

/// The stable error categories a refusal carries (PLAN.md §14.2; native-profiles.md).
enum class Category {
    ElfMalformed,           ///< ELF_MALFORMED: the file is not a well-formed ELF
    ElfUnsupported,         ///< ELF_UNSUPPORTED: well formed, but a kind this host does not load
    RelocationUnsupported,  ///< ELF_RELOCATION_UNSUPPORTED
    SymbolUnresolved,       ///< GUEST_SYMBOL_UNRESOLVED
    AbiUnsupported,         ///< GUEST_ABI_UNSUPPORTED: outside the profile (N0)
    HostMemory,             ///< HOST_MEMORY_FAILED: reserving, committing or protecting failed
};

/// Names a category the way logs and tests print it ("ELF_MALFORMED", ...).
const char* CategoryName(Category c);

/// Why a module was refused: the category, where in the file or image, and the reason.
struct LoadError {
    Category category = Category::ElfMalformed;
    std::string where;   ///< "file offset 0x40", "vaddr 0x4b44", or "" for the whole file
    std::string reason;  ///< one line, for the person reading the log
};

/// An explicitly supplied function or variable that a guest import of the same name binds to.
/// Notes: host addresses require a proven direct signature (abi-compatibility.md). A guest-ABI
/// helper address remains a guest-to-guest call; never bind a host variadic function in its place.
struct HostSymbol {
    const char* name;
    void* address;
};

/// Declares one runtime-owned logical library and its exact audited exports/version.
/// Notes: symbol addresses and names outlive the namespace; no host DLL search is performed.
struct BuiltinLibrary {
    std::string name;
    std::vector<HostSymbol> symbols;
    std::string version;
};

/// What loading found, for reports.
struct LoadStats {
    std::string segments;     ///< the PT_LOAD permissions in order, "R, RX, RW"
    std::string relocations;  ///< counts by type, "RELR 5, GLOB_DAT 4, JUMP_SLOT 2"
    std::string hash;         ///< "GNU" or "SysV": the table symbol lookup uses
    uint32_t symbols = 0;           ///< dynamic symbols
    uint64_t codeBytes = 0;         ///< bytes of executable segments screened
    uint32_t instructions = 0;      ///< of those, words decoded as instructions
    uint32_t dataWords = 0;         ///< words that do not decode (literal data, padding)
    uint64_t relroBytes = 0;        ///< bytes made read-only after relocation
};

/// A mapped guest image; Load returns it relocated/protected without executing guest code.
/// Side effects: owns its address range and fault attribution; unregisters before unmapping.
class Module {
public:
    ~Module();
    Module(const Module&) = delete;
    Module& operator=(const Module&) = delete;

    /// Finds a symbol the module defines and exports (global or weak, default or protected).
    /// Returns: its address in this process, or nullptr.
    void* Find(std::string_view name) const;

    /// Returns: the start of the module's reserved range in this process.
    uintptr_t Base() const { return base_; }
    /// Returns: the size of the module's reserved range in bytes.
    size_t Size() const { return size_; }
    /// Returns: the file name the module was loaded from (UTF-8, without folder).
    const std::string& Name() const { return name_; }
    /// Returns: what loading found.
    const LoadStats& Stats() const { return stats_; }
    /// Returns declared dependency basenames.
    const std::vector<std::string>& Needed() const { return needed_; }
    /// Returns SONAME, or an empty string if the image does not declare one.
    const std::string& Soname() const { return soname_; }
    /// Returns validated initializers after managed linking, in DT_INIT/array order.
    std::span<void (*const)()> Initializers() const { return initializers_; }
    /// Reports whether an aligned address belongs to this image's executable segments.
    bool Executable(uintptr_t address) const;
    /// Converts a PC in this image's executable segments to a guest virtual address.
    bool FaultAddress(uintptr_t pc, uint64_t* address) const;

private:
    friend class Loader;
    Module() = default;

    std::string name_;
    uintptr_t base_ = 0;
    size_t size_ = 0;
    int64_t bias_ = 0;  // run-time address minus ELF vaddr
    // Into the mapped image (read-only after load).
    const uint8_t* symtab_ = nullptr;
    const char* strtab_ = nullptr;
    uint64_t strsz_ = 0;
    uint32_t nsyms_ = 0;
    const uint32_t* gnuHash_ = nullptr;
    const uint32_t* sysvHash_ = nullptr;
    LoadStats stats_;
    std::vector<std::string> needed_;
    std::string soname_;
    std::vector<std::pair<uintptr_t, size_t>> executable_;
    std::vector<void (*)()> initializers_;
    bool tracked_ = false;
};

/// Loads an ELF shared object from `path` within profile N0.
/// Inputs: `imports` -- the host symbols the module's undefined symbols may bind to.
/// Returns: the module, or nullptr with *error set. Every check runs before the module is
/// returned, and none of its code runs; a refused module leaves nothing mapped (rollback).
/// Side effects: reserves, commits and protects memory with VirtualAlloc/VirtualProtect and
/// flushes the instruction cache over the module.
std::unique_ptr<Module> Load(const std::wstring& path, std::span<const HostSymbol> imports,
                             LoadError* error);

/// Owns a restricted C-library graph with transactional linking and process-lifetime pinning.
/// Notes: JNI lifecycle belongs to the JNI bridge, not this graph; no ELF TLS or dynamic unload.
/// The caller keeps this object alive until all guest threads/callbacks end and must enter
/// a managed guest context before Open, because dependency constructors may call shims.
class ModuleSet {
public:
    /// Creates a private namespace with audited imports and optional reserved logical libraries.
    ModuleSet(std::wstring directory, std::span<const HostSymbol> imports,
              std::span<const BuiltinLibrary> libraries = {});
    /// Releases mapped images after the caller has stopped every guest execution path.
    ~ModuleSet();
    ModuleSet(const ModuleSet&) = delete;
    ModuleSet& operator=(const ModuleSet&) = delete;
    /// Opens a portable .so basename, initializes dependency-first once, and adds one reference.
    /// Returns: an opaque handle, or 0 and error; failures before initialization roll back.
    uint64_t Open(std::string_view name, LoadError* error);
    /// Decrements an explicit open reference; mappings stay pinned. Invalid/closed handles fail.
    bool Close(uint64_t handle);
    /// Looks up an export in an open handle and its breadth-first dependency scope.
    void* Find(uint64_t handle, std::string_view symbol) const;
    /// Looks up only this open image/provider, excluding dependencies (for its own JNI_OnLoad).
    void* FindOwn(uint64_t handle, std::string_view symbol) const;
    /// Checks whether an address belongs to a retained, screened guest executable image.
    bool Executable(uintptr_t address) const;
    /// Returns retained ELF images, including closed-but-pinned dependencies, excluding logical records.
    size_t Mapped() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace winapk::guest
