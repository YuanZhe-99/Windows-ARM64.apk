#pragma once

#include <string>

/// Selects one compact group of audited T08 Bionic cases.
enum class BionicSuite { Threads, Sync, Memory, Io };

/// Runs the selected T08 cases with cleanup and platform-reserved host-state assertions.
/// Returns: 0 on success, 2 for a refused ELF, 3 for failed assertions.
int ProbeBionic(const std::wstring& path, BionicSuite suite, const std::wstring& androidRoot = {});
