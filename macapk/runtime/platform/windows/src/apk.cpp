// apk.cpp -- the APK reader: ZIP (with ZIP64), inflate (zlib) and binary XML.
#include "apk.h"

#include <windows.h>
#include <zlib.h>

#include <algorithm>
#include <cstring>
#include <set>

namespace winapk {
namespace {

constexpr size_t kMaxEntries = 1u << 20;  // more entries than any real APK carries

/// A read-only view of a whole file.
class MappedFile {
public:
    explicit MappedFile(const std::wstring& path) {
        file_ = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file_ == INVALID_HANDLE_VALUE) { error_ = "cannot open the file (error " + std::to_string(GetLastError()) + ")"; return; }
        LARGE_INTEGER sz;
        if (!GetFileSizeEx(file_, &sz) || sz.QuadPart == 0) { error_ = "the file is empty"; return; }
        size_ = static_cast<uint64_t>(sz.QuadPart);
        map_ = CreateFileMappingW(file_, nullptr, PAGE_READONLY, 0, 0, nullptr);
        if (map_) data_ = static_cast<const uint8_t*>(MapViewOfFile(map_, FILE_MAP_READ, 0, 0, 0));
        if (!data_) error_ = "cannot map the file (error " + std::to_string(GetLastError()) + ")";
    }
    ~MappedFile() {
        if (data_) UnmapViewOfFile(data_);
        if (map_) CloseHandle(map_);
        if (file_ != INVALID_HANDLE_VALUE) CloseHandle(file_);
    }
    MappedFile(const MappedFile&) = delete;
    MappedFile& operator=(const MappedFile&) = delete;

    const std::string& error() const { return error_; }
    uint64_t size() const { return size_; }
    /// Returns `n` bytes at `off`, or null when they are not all inside the file.
    const uint8_t* at(uint64_t off, uint64_t n) const {
        return off <= size_ && n <= size_ - off ? data_ + off : nullptr;
    }

private:
    HANDLE file_ = INVALID_HANDLE_VALUE;
    HANDLE map_ = nullptr;
    const uint8_t* data_ = nullptr;
    uint64_t size_ = 0;
    std::string error_;
};

uint16_t U16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | p[1] << 8); }
uint32_t U32(const uint8_t* p) { return p[0] | p[1] << 8 | p[2] << 16 | static_cast<uint32_t>(p[3]) << 24; }
uint64_t U64(const uint8_t* p) { return U32(p) | static_cast<uint64_t>(U32(p + 4)) << 32; }

/// Parses the central directory of an open file.
std::string ParseZip(const MappedFile& f, std::vector<ZipEntry>& entries) {
    // The end-of-central-directory record: in the last 64 KiB + 22 bytes.
    const uint64_t scan = std::min<uint64_t>(f.size(), 0xFFFF + 22);
    const uint8_t* tail = f.at(f.size() - scan, scan);
    int64_t eocd = -1;
    for (int64_t i = static_cast<int64_t>(scan) - 22; i >= 0; --i) {
        if (U32(tail + i) == 0x06054b50) { eocd = static_cast<int64_t>(f.size() - scan) + i; break; }
    }
    if (eocd < 0) return "not a ZIP archive (no end of central directory)";
    const uint8_t* e = f.at(eocd, 22);
    uint64_t count = U16(e + 10), cdSize = U32(e + 12), cdOffset = U32(e + 16);
    if (count == 0xFFFF || cdSize == 0xFFFFFFFF || cdOffset == 0xFFFFFFFF) {
        // ZIP64: the locator sits just before the end record.
        const uint8_t* loc = eocd >= 20 ? f.at(eocd - 20, 20) : nullptr;
        if (!loc || U32(loc) != 0x07064b50) return "ZIP64 archive without a ZIP64 locator";
        const uint8_t* z = f.at(U64(loc + 8), 56);
        if (!z || U32(z) != 0x06064b50) return "ZIP64 end record not found";
        count = U64(z + 32);
        cdSize = U64(z + 40);
        cdOffset = U64(z + 48);
    }
    if (count > kMaxEntries) return "too many entries (" + std::to_string(count) + ")";
    const uint8_t* cd = f.at(cdOffset, cdSize);
    if (!cd) return "the central directory lies outside the file";
    entries.clear();
    entries.reserve(static_cast<size_t>(count));
    uint64_t pos = 0;
    for (uint64_t i = 0; i < count; ++i) {
        if (cdSize - pos < 46 || U32(cd + pos) != 0x02014b50) return "malformed central directory entry " + std::to_string(i);
        const uint8_t* h = cd + pos;
        const uint16_t nameLen = U16(h + 28), extraLen = U16(h + 30), commentLen = U16(h + 32);
        if (cdSize - pos < 46ull + nameLen + extraLen + commentLen) return "truncated central directory entry " + std::to_string(i);
        ZipEntry z;
        z.method = U16(h + 10);
        z.crc = U32(h + 16);
        z.compressedSize = U32(h + 20);
        z.size = U32(h + 24);
        z.localHeader = U32(h + 42);
        z.name.assign(reinterpret_cast<const char*>(h + 46), nameLen);
        // ZIP64 extended information: the fields that were 0xFFFFFFFF, in order.
        const uint8_t* x = h + 46 + nameLen;
        for (uint32_t xp = 0; xp + 4 <= extraLen;) {
            const uint16_t id = U16(x + xp), len = U16(x + xp + 2);
            if (xp + 4ull + len > extraLen) break;
            if (id == 0x0001) {
                const uint8_t* v = x + xp + 4;
                uint32_t vp = 0;
                auto take = [&](uint64_t& field) {
                    if (field == 0xFFFFFFFF && vp + 8 <= len) { field = U64(v + vp); vp += 8; }
                };
                take(z.size);
                take(z.compressedSize);
                take(z.localHeader);
            }
            xp += 4 + len;
        }
        entries.push_back(std::move(z));
        pos += 46ull + nameLen + extraLen + commentLen;
    }
    return {};
}

/// Says whether `name` would be unsafe or impossible as a Windows path.
bool UnsafeName(const std::string& name) {
    if (name.empty() || name[0] == '/' || name.find('\\') != std::string::npos || name.find(':') != std::string::npos) return true;
    for (unsigned char c : name) {
        if (c < 0x20 || std::strchr("<>\"|?*", c)) return true;
    }
    size_t start = 0;
    while (start <= name.size()) {
        size_t end = name.find('/', start);
        if (end == std::string::npos) end = name.size();
        std::string seg = name.substr(start, end - start);
        if (seg == "..") return true;
        if (!seg.empty() && (seg.back() == '.' || seg.back() == ' ') && seg != ".") return true;
        std::string stem = seg.substr(0, seg.find('.'));
        std::transform(stem.begin(), stem.end(), stem.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        static const char* kReserved[] = {"CON", "PRN", "AUX", "NUL", "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
                                          "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"};
        for (const char* r : kReserved) if (stem == r) return true;
        start = end + 1;
    }
    return false;
}

/// The string pool of a binary XML document.
struct StringPool {
    std::vector<std::string> strings;
};

/// Decodes the string pool chunk at `p` (`n` bytes). Returns false when malformed.
bool ParsePool(const uint8_t* p, uint32_t n, StringPool& pool) {
    if (n < 28) return false;
    const uint32_t headerSize = U16(p + 2), count = U32(p + 8), flags = U32(p + 16), stringsStart = U32(p + 20);
    const bool utf8 = flags & 0x100;
    if (headerSize < 28 || count > n / 4 || headerSize + 4ull * count > n || stringsStart > n) return false;
    pool.strings.resize(count);
    for (uint32_t i = 0; i < count; ++i) {
        uint64_t off = static_cast<uint64_t>(stringsStart) + U32(p + headerSize + 4 * i);
        if (off >= n) return false;
        const uint8_t* s = p + off;
        const uint64_t room = n - off;
        if (utf8) {
            // UTF-16 length then UTF-8 length, each 1 or 2 bytes.
            uint64_t k = (s[0] & 0x80) ? 2 : 1;
            if (k + 2 > room) return false;
            uint32_t len = s[k];
            if (len & 0x80) { len = (len & 0x7F) << 8 | s[k + 1]; k += 2; } else { k += 1; }
            if (k + len > room) return false;
            pool.strings[i].assign(reinterpret_cast<const char*>(s + k), len);
        } else {
            if (room < 2) return false;
            uint32_t len = U16(s), k = 2;
            if (len & 0x8000) { if (room < 4) return false; len = (len & 0x7FFF) << 16 | U16(s + 2); k = 4; }
            if (k + 2ull * len > room) return false;
            std::wstring w(len, L'\0');
            for (uint32_t c = 0; c < len; ++c) w[c] = static_cast<wchar_t>(U16(s + k + 2 * c));
            const int bytes = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(len), nullptr, 0, nullptr, nullptr);
            pool.strings[i].resize(bytes);
            WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(len), pool.strings[i].data(), bytes, nullptr, nullptr);
        }
    }
    return true;
}

/// Reads the manifest's package, version and SDK levels from binary XML.
std::string ParseManifest(const std::vector<uint8_t>& x, ApkInfo& info) {
    if (x.size() < 8 || U16(x.data()) != 0x0003) return "AndroidManifest.xml is not binary XML";
    const uint32_t total = std::min<uint64_t>(U32(x.data() + 4), x.size());
    StringPool pool;
    std::vector<uint32_t> resMap;
    bool sawManifest = false;
    for (uint32_t pos = U16(x.data() + 2); pos + 8 <= total;) {
        const uint8_t* c = x.data() + pos;
        const uint16_t type = U16(c), headerSize = U16(c + 2);
        const uint32_t size = U32(c + 4);
        if (size < 8 || size > total - pos) return "malformed binary XML chunk";
        if (type == 0x0001) {
            if (!ParsePool(c, size, pool)) return "malformed string pool";
        } else if (type == 0x0180) {
            for (uint32_t i = 8; i + 4 <= size; i += 4) resMap.push_back(U32(c + i));
        } else if (type == 0x0102 && size >= 36 && headerSize >= 16) {
            const uint8_t* ext = c + headerSize;
            if (headerSize + 20ull > size) return "malformed element";
            const uint32_t nameIdx = U32(ext + 4);
            const uint16_t attrStart = U16(ext + 8), attrSize = U16(ext + 10), attrCount = U16(ext + 12);
            if (nameIdx >= pool.strings.size() || attrSize < 20 || headerSize + attrStart + uint64_t(attrSize) * attrCount > size)
                return "malformed element";
            const std::string& el = pool.strings[nameIdx];
            for (uint16_t a = 0; a < attrCount; ++a) {
                const uint8_t* at = ext + attrStart + attrSize * a;
                const uint32_t an = U32(at + 4), raw = U32(at + 8), data = U32(at + 16);
                const uint8_t dataType = at[15];
                const std::string name = an < pool.strings.size() ? pool.strings[an] : "";
                const uint32_t id = an < resMap.size() ? resMap[an] : 0;
                auto str = [&]() -> std::string {
                    if (raw < pool.strings.size()) return pool.strings[raw];
                    return dataType == 0x03 && data < pool.strings.size() ? pool.strings[data] : "";
                };
                auto num = [&]() -> int64_t { return (dataType >= 0x10 && dataType <= 0x1f) ? static_cast<int32_t>(data) : 0; };
                if (el == "manifest") {
                    if (name == "package" && id == 0) info.packageName = str();
                    else if (id == 0x0101021b || name == "versionCode") info.versionCode = num();
                    else if (id == 0x0101021c || name == "versionName") info.versionName = str();
                } else if (el == "uses-sdk") {
                    if (id == 0x0101020c || name == "minSdkVersion") info.minSdk = static_cast<int>(num());
                    else if (id == 0x01010270 || name == "targetSdkVersion") info.targetSdk = static_cast<int>(num());
                }
            }
            sawManifest |= el == "manifest";
        }
        pos += size;
    }
    if (!sawManifest) return "AndroidManifest.xml has no <manifest> element";
    if (info.packageName.empty()) return "AndroidManifest.xml names no package";
    return {};
}

}  // namespace

std::string ReadZip(const std::wstring& path, std::vector<ZipEntry>& entries) {
    MappedFile f(path);
    if (!f.error().empty()) return f.error();
    return ParseZip(f, entries);
}

std::string ReadEntry(const std::wstring& path, const ZipEntry& e, std::vector<uint8_t>& out, uint64_t limit) {
    if (e.size > limit) return e.name + " is larger than " + std::to_string(limit) + " bytes";
    MappedFile f(path);
    if (!f.error().empty()) return f.error();
    const uint8_t* lh = f.at(e.localHeader, 30);
    if (!lh || U32(lh) != 0x04034b50) return e.name + ": bad local header";
    const uint8_t* data = f.at(e.localHeader + 30 + U16(lh + 26) + U16(lh + 28), e.compressedSize);
    if (!data) return e.name + ": data lies outside the file";
    out.assign(static_cast<size_t>(e.size), 0);
    if (e.method == 0) {
        if (e.compressedSize != e.size) return e.name + ": stored sizes disagree";
        std::memcpy(out.data(), data, static_cast<size_t>(e.size));
    } else if (e.method == 8) {
        z_stream z{};
        if (inflateInit2(&z, -MAX_WBITS) != Z_OK) return "inflateInit2 failed";
        z.next_in = const_cast<Bytef*>(data);
        z.avail_in = static_cast<uInt>(e.compressedSize);
        z.next_out = out.data();
        z.avail_out = static_cast<uInt>(e.size);
        const int rc = inflate(&z, Z_FINISH);
        const uLong produced = z.total_out;
        inflateEnd(&z);
        if (rc != Z_STREAM_END || produced != e.size) return e.name + ": inflate failed";
    } else {
        return e.name + ": unsupported compression method " + std::to_string(e.method);
    }
    if (crc32(0, out.data(), static_cast<uInt>(out.size())) != e.crc) return e.name + ": CRC mismatch";
    return {};
}

std::string InspectApk(const std::wstring& path, ApkInfo& info) {
    std::vector<ZipEntry> entries;
    if (std::string err = ReadZip(path, entries); !err.empty()) return err;
    const ZipEntry* manifest = nullptr;
    std::set<std::string> lower, abis;
    for (const ZipEntry& e : entries) {
        if (e.name == "AndroidManifest.xml") manifest = &e;
        if (e.name == "classes.dex" || (e.name.rfind("classes", 0) == 0 && e.name.size() > 11 &&
                                        e.name.find('/') == std::string::npos && e.name.ends_with(".dex"))) {
            ++info.dexCount;
        }
        if (e.name.rfind("lib/", 0) == 0 && e.name.ends_with(".so")) {
            const size_t slash = e.name.find('/', 4);
            if (slash != std::string::npos) {
                abis.insert(e.name.substr(4, slash - 4));
                info.nativeLibs.push_back(e.name);
            }
        }
        std::string l = e.name;
        std::transform(l.begin(), l.end(), l.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (UnsafeName(e.name) || !lower.insert(l).second) info.unsafeNames.push_back(e.name);
    }
    info.nativeAbis.assign(abis.begin(), abis.end());
    if (!manifest) return "no AndroidManifest.xml";
    std::vector<uint8_t> x;
    if (std::string err = ReadEntry(path, *manifest, x, 16ull << 20); !err.empty()) return err;
    return ParseManifest(x, info);
}

}  // namespace winapk
