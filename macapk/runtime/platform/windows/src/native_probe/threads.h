#pragma once

#include <string>

/// Runs T08's thread/TLS cases, or its synchronization cases when sync, with cleanup assertions.
/// Returns: 0 on success, 2 for a refused ELF, 3 for failed assertions.
int ProbeThreads(const std::wstring& path, bool sync = false);
