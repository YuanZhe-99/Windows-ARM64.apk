#pragma once

#include <string>

/// Runs T08's guest pthread/errno/emutls cases, with bounded repetition and cleanup assertions.
/// Returns: 0 on success, 2 for a refused ELF, 3 for failed assertions.
int ProbeThreads(const std::wstring& path);
