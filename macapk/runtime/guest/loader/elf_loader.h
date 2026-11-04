// elf_loader.h -- the guest ELF loader on the Windows host (PLAN.md §9.2), profile N0.
//
// Maps an Android arm64-v8a shared object (ELF64, AArch64, ET_DYN) from its program headers
// into memory the host reserves, applies its relocations, binds its imports to host symbols,
// screens its code and sets the final page protections. Nothing of the module runs while it
// loads: no constructor, no IFUNC resolver. Profile N0 (native-profiles.md) admits only a
// leaf library: no dependencies, no TLS, no constructors, no x18/TPIDR_EL0/svc instructions.
// Host code: it never shares a type with the guest beyond the ELF file format itself.
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>

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

/// A host function or variable that a guest import of the same name binds to.
/// Notes: the address is used as is; its signature must be one abi-compatibility.md
/// lists as direct for every guest that imports it.
struct HostSymbol {
    const char* name;
    void* address;
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

/// A loaded module: mapped, relocated and protected; none of its code has run.
/// Side effects: owns its address range, released when the Module is destroyed.
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
};

/// Loads an ELF shared object from `path` within profile N0.
/// Inputs: `imports` -- the host symbols the module's undefined symbols may bind to.
/// Returns: the module, or nullptr with *error set. Every check runs before the module is
/// returned, and none of its code runs; a refused module leaves nothing mapped (rollback).
/// Side effects: reserves, commits and protects memory with VirtualAlloc/VirtualProtect and
/// flushes the instruction cache over the module.
std::unique_ptr<Module> Load(const std::wstring& path, std::span<const HostSymbol> imports,
                             LoadError* error);

}  // namespace winapk::guest
