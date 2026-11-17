#pragma once
#include <string>

/// Runs T07's normally linked NDK/library-version and CRT lifetime contracts under a managed runtime.
int ProbeLibraries(const std::wstring& cases, const std::wstring& helper);
