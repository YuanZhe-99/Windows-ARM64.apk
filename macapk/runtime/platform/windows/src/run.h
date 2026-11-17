// run.h -- `winapk run` and `winapk inspect` (PLAN.md §18).
#pragma once

#include <string>

namespace winapk {

/// The arguments of `winapk run`.
struct RunArgs {
    std::wstring apk;        ///< absolute path of the APK
    std::wstring native = L"off";  ///< guest native mode: off (default) or restricted APK namespace
    std::wstring graphics = L"off";  ///< optional Windows ANGLE Java GLSurfaceView backend
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

/// Converts an APK's DEX to a JVM jar with com.macapk.dex in a JVM of this process: the
/// step `winapk run` starts in a child `winapk dex`, so the converter's large heap is gone
/// before the app's JVM starts.
/// Returns: the converter's exit code (0 ok, 1 bad input, 3 no DEX, 4 conversion failed),
/// or 69 when no ARM64 JVM or framework jar is found.
/// Notes: the paths reach Java as UTF-16 strings, never through a code page, so any Unicode
/// path works (a java.exe child would get its arguments in the ANSI code page).
int ConvertDex(const std::wstring& apk, const std::wstring& out, const std::wstring& javaHome);

/// Prints what an APK declares and carries, as JSON (`json`) or text, without running it.
/// Returns: 0, or 65 when the file is not a readable APK.
int Inspect(const std::wstring& apk, bool json);

}  // namespace winapk
