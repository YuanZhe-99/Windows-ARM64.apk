// bridge.h -- winapk-bridge.dll: the Windows implementations of the runtime's host natives
// (ADR-0001: explicit RegisterNatives from a host bridge DLL, usable by winapk.exe's embedded
// JVM and by a java.exe test JVM alike).
//
// Host-domain code only (Windows ARM64, JNI). A Java native with no Windows implementation
// is left unregistered: calling it throws UnsatisfiedLinkError naming it, never a stand-in
// result (AGENTS.md, "No fake success").
#pragma once

#include <jni.h>

#include <string>

namespace winapk::bridge {

/// Registers `methods` on the class `className` ("com/macapk/loader/NativeLog").
/// Returns: false, with the Java exception cleared and the failure logged, when the class
/// or a method is missing. Notes: a missing class is not an error for optional classes;
/// the caller decides.
bool Bind(JNIEnv* env, const char* className, const JNINativeMethod* methods, int count);

/// Converts a Java string to UTF-8 ("" for null).
std::string Utf8(JNIEnv* env, jstring s);

/// Writes one line to the runtime log (stderr), whole, from any thread.
void LogLine(const std::string& line);

/// Registers NativeLog (log.cpp).
bool RegisterLog(JNIEnv* env);

/// Registers NativeCocoa's window, presentation and input natives (window.cpp).
bool RegisterWindow(JNIEnv* env);

/// Registers the host-controlled Windows guest ELF/JNI loading service (guest.cpp).
bool RegisterGuest(JNIEnv* env);

/// Registers NativeOs and host clipboard services (host_services.cpp).
bool RegisterHostServices(JNIEnv* env);

}  // namespace winapk::bridge

/// Registers every Windows host native on the JVM `env` belongs to: NativeLog, NativeCocoa
/// and upstream's NativeSqlite (native_sqlite.cpp, against vcpkg's SQLite).
/// Returns: JNI_TRUE when every required class bound. Notes: exported so winapk.exe can
/// call it after JNI_CreateJavaVM; JNI_OnLoad calls it when java.exe loads the DLL.
extern "C" __declspec(dllexport) jboolean winapk_bridge_register(JNIEnv* env);

/// Sets the app's size in points before Java asks for it (winapk run, from --size or the
/// screen). Notes: NativeCocoa.setPhoneSize changes it later from Java.
extern "C" __declspec(dllexport) void winapk_bridge_set_phone_size(int w, int h);
