// host_services.h -- Windows ARM64 implementations of NativeOs, MacApkHostPasteboard,
// DirectWrite host font lookup, /proc synthesis and gated host URL opening.
#pragma once

#include <jni.h>
#include <string>

namespace winapk::bridge {

/// Registers NativeOs and ClipboardManager$MacApkHostPasteboard on the JVM `env` belongs to.
/// Returns: true when NativeOs bound cleanly; optional clipboard class absence is logged.
bool RegisterHostServices(JNIEnv* env);

/// Resolves the local font file path for an exact PostScript face name via DirectWrite, or null.
jstring JNICALL HostFontFile(JNIEnv* env, jclass, jstring postScriptName);

/// Records the materialized Android /system/fonts directory for guest/host inspection.
void JNICALL SetSystemFontsDir(JNIEnv* env, jclass, jstring dir);

/// Mirrors one Android system property into the host/guest property table.
void JNICALL SetSystemProperty(JNIEnv* env, jclass, jstring key, jstring value);

/// Records the canonical shared storage root directory.
void JNICALL SetSharedStorageRoot(JNIEnv* env, jclass, jstring dir);

/// Synthesizes a host file backing a supported /proc path (/proc/meminfo, /proc/self/status), or null.
jstring JNICALL RemapProcPath(JNIEnv* env, jclass, jstring procPath);

/// Opens an allowlisted http/https/mailto URI on the Windows host (never arbitrary files or commands).
jboolean JNICALL OpenHostUrl(JNIEnv* env, jclass, jstring url);

/// Looks up a mutable guest environment variable set via NativeOs.setenv or inherited at start-up.
const char* GuestEnvGet(const char* name);

}  // namespace winapk::bridge
