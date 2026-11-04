// apk.h -- reads an APK without running any of it: the ZIP directory, entry bytes and the
// binary AndroidManifest.xml (PLAN.md §8.5, §18 `inspect`).
//
// Host-domain code. An APK is untrusted input: every offset, length and count is checked
// before use, entries are never written to disk by their own names, and sizes are capped.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace winapk {

/// One entry of an APK's ZIP central directory.
struct ZipEntry {
    std::string name;            ///< as stored (UTF-8, case-sensitive, '/'-separated)
    uint16_t method = 0;         ///< 0 stored, 8 deflate; anything else is unsupported
    uint32_t crc = 0;
    uint64_t compressedSize = 0;
    uint64_t size = 0;
    uint64_t localHeader = 0;    ///< offset of the local file header
};

/// What `inspect` and `run` learn from an APK's manifest and entries.
struct ApkInfo {
    std::string packageName;
    std::string versionName;
    int64_t versionCode = 0;
    int minSdk = 0, targetSdk = 0;
    int dexCount = 0;                       ///< classes.dex, classes2.dex, ...
    std::vector<std::string> nativeAbis;    ///< lib/<abi>/ folders holding .so files
    std::vector<std::string> nativeLibs;    ///< lib/<abi>/<name>.so entries
    /// Entry names that would be unsafe as Windows paths (absolute, "..", drive letters,
    /// reserved device names, characters Windows forbids) or that collide when case is
    /// ignored. The runtime never maps entries to files by name, so they are reported only.
    std::vector<std::string> unsafeNames;
};

/// Reads an APK's central directory.
/// Returns: "" on success with `entries` filled, else why the file is not a readable APK.
std::string ReadZip(const std::wstring& path, std::vector<ZipEntry>& entries);

/// Reads one entry's bytes, inflating it when deflated.
/// Returns: "" on success, else the reason. Notes: refuses entries larger than `limit`
/// bytes and checks the CRC-32.
std::string ReadEntry(const std::wstring& path, const ZipEntry& e, std::vector<uint8_t>& out,
                      uint64_t limit = 64ull << 20);

/// Reads an APK's manifest and entries into `info`.
/// Returns: "" on success, else the reason (no manifest, malformed binary XML, ...).
std::string InspectApk(const std::wstring& path, ApkInfo& info);

}  // namespace winapk
