// run.h -- `winapk run` and `winapk inspect` (PLAN.md §18).
#pragma once

#include <string>

namespace winapk {

/// The arguments of `winapk run`.
struct RunArgs {
    std::wstring apk;        ///< absolute path of the APK
    std::wstring native = L"off";  ///< guest native mode; only "off" runs before P2
    std::wstring dataDir;    ///< "" for %LOCALAPPDATA%\Windows-ARM64.apk\Apps
    std::wstring javaHome;   ///< "" to search as doctor does
    int width = 0, height = 0;  ///< the app's size in points; 0 to fit the screen
};

/// Runs an APK's Java code in this process until its window closes.
/// Returns: the process exit code: 0 after a normal close; 64 usage (unsupported mode),
/// 65 invalid APK, 69 a dependency is missing, 70 DEX conversion failed, 74 file I/O failed,
/// 1 the app's boot threw. Side effects: creates the data folders, converts and caches code,
/// starts the JVM (once per process) and shows the app's window.
int Run(const RunArgs& args);

/// Prints what an APK declares and carries, as JSON (`json`) or text, without running it.
/// Returns: 0, or 65 when the file is not a readable APK.
int Inspect(const std::wstring& apk, bool json);

}  // namespace winapk
