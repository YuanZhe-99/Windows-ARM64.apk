#pragma once
#include "../loader/elf_loader.h"

namespace winapk::guest {

/// Owns the guest formatting image and the audited libc/libdl/liblog namespace declarations.
/// Notes: keep alive after the namespace, every managed thread and all exit callbacks.
class BionicLibraries {
public:
    /// Loads the screened project helper and declares its exact fixed/guest-ABI export providers.
    /// Returns: the owner, or nullptr with a stable loader error; never runs guest initialization.
    static std::unique_ptr<BionicLibraries> Load(const std::wstring& helper, LoadError* error);
    /// Returns audited imports for N0 experiments and explicitly supplied unversioned callbacks.
    std::span<const HostSymbol> Imports() const { return imports_; }
    /// Returns logical system libraries; their names are reserved within the caller's namespace.
    std::span<const BuiltinLibrary> Libraries() const { return libraries_; }

private:
    std::unique_ptr<Module> helper_;
    std::vector<HostSymbol> imports_;
    std::vector<BuiltinLibrary> libraries_;
};

}  // namespace winapk::guest
