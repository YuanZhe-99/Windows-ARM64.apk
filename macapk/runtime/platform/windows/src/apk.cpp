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

/// Manifest resource references resolved after AndroidManifest.xml is read.
struct ManifestRefs {
    uint32_t labelRes = 0;
    uint32_t iconRes = 0;
    std::string iconPath;
};

/// Reads the manifest's package, label, icon, permissions, features and SDK levels from binary XML.
std::string ParseManifest(const std::vector<uint8_t>& x, ApkInfo& info, ManifestRefs& refs) {
    if (x.size() < 8 || U16(x.data()) != 0x0003) return "AndroidManifest.xml is not binary XML";
    const uint32_t total = std::min<uint64_t>(U32(x.data() + 4), x.size());
    StringPool pool;
    std::vector<uint32_t> resMap;
    std::set<std::string> seenPerms;
    uint32_t bestGles = 0;
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
            std::string featName;
            uint32_t featGles = 0;
            bool featRequired = true;
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
                } else if (el == "application" || el == "activity" || el == "activity-alias") {
                    const bool app = el == "application";
                    if (id == 0x01010001 || name == "label") {
                        if (const std::string s = str(); !s.empty() && (app || info.label.empty())) info.label = s;
                        else if (dataType == 0x01 && (app || refs.labelRes == 0)) refs.labelRes = data;
                    } else if (id == 0x01010002 || name == "icon" || id == 0x0101052c || name == "roundIcon") {
                        const bool primary = id == 0x01010002 || name == "icon";
                        if (const std::string s = str(); !s.empty() && (refs.iconPath.empty() || (app && primary))) refs.iconPath = s;
                        else if (dataType == 0x01 && (refs.iconRes == 0 || (app && primary))) refs.iconRes = data;
                    }
                } else if (el == "uses-permission" || el == "uses-permission-sdk-23") {
                    if ((id == 0x01010003 || name == "name") && info.permissions.size() < 128) {
                        if (const std::string p = str(); !p.empty() && seenPerms.insert(p).second) info.permissions.push_back(p);
                    }
                } else if (el == "uses-feature") {
                    if (id == 0x01010003 || name == "name") featName = str();
                    else if (id == 0x01010281 || name == "glEsVersion") featGles = data;
                    else if ((id == 0x0101028e || name == "required") && dataType == 0x12) featRequired = data != 0;
                }
            }
            if (el == "uses-feature") {
                if (featGles > bestGles) bestGles = featGles;
                if (featName.rfind("android.hardware.vulkan.", 0) == 0) {
                    info.vulkan = true;
                    if (featRequired) info.vulkanRequired = true;
                }
            }
            sawManifest |= el == "manifest";
        }
        pos += size;
    }
    if (bestGles >= 0x00010000) {
        info.glesVersion = std::to_string((bestGles >> 16) & 0xffff) + "." + std::to_string(bestGles & 0xffff);
    }
    if (!sawManifest) return "AndroidManifest.xml has no <manifest> element";
    if (info.packageName.empty()) return "AndroidManifest.xml names no package";
    return {};
}

/// Resolves `@string` label and raster `.png` icon references in `resources.arsc` without running code.
void ResolveResources(const std::vector<uint8_t>& arsc, ManifestRefs& refs, std::string& label) {
    if (arsc.size() < 12 || U16(arsc.data()) != 0x0002) return;
    const uint32_t total = std::min<uint64_t>(U32(arsc.data() + 4), arsc.size());
    const uint16_t rootHeader = U16(arsc.data() + 2);
    if (rootHeader < 12 || rootHeader > total) return;

    StringPool values;
    bool haveValues = false;
    for (uint32_t pos = rootHeader; pos + 8 <= total;) {
        const uint8_t* c = arsc.data() + pos;
        const uint16_t type = U16(c);
        const uint32_t size = U32(c + 4);
        if (size < 8 || size > total - pos) return;
        if (type == 0x0001 && !haveValues) haveValues = ParsePool(c, size, values);
        pos += size;
    }
    if (!haveValues) return;

    auto readEntry = [&](const uint8_t* tc, uint32_t tsize, uint16_t entryIdx,
                         uint8_t& outType, uint32_t& outData) -> bool {
        const uint16_t thdr = U16(tc + 2);
        if (thdr < 20 || thdr > tsize) return false;
        const uint8_t flags = tc[9];
        const uint32_t count = U32(tc + 12), entriesStart = U32(tc + 16);
        if (entriesStart < thdr || entriesStart > tsize) return false;
        uint64_t rel = UINT64_MAX;
        if (flags & 0x01) {  // FLAG_SPARSE: pairs of (uint16 idx, uint16 offset_div_4)
            if (thdr + 4ull * count > entriesStart) return false;
            for (uint32_t i = 0; i < count; ++i) {
                const uint8_t* p = tc + thdr + 4ull * i;
                if (U16(p) == entryIdx) { rel = uint64_t(U16(p + 2)) * 4; break; }
            }
        } else if (flags & 0x02) {  // FLAG_OFFSET16
            if (entryIdx >= count || thdr + 2ull * count > entriesStart) return false;
            const uint16_t off16 = U16(tc + thdr + 2ull * entryIdx);
            if (off16 != 0xffffu) rel = uint64_t(off16) * 4;
        } else {
            if (entryIdx >= count || thdr + 4ull * count > entriesStart) return false;
            const uint32_t off32 = U32(tc + thdr + 4ull * entryIdx);
            if (off32 != 0xffffffffu) rel = off32;
        }
        if (rel == UINT64_MAX || entriesStart + rel + 8 > tsize) return false;
        const uint8_t* e = tc + entriesStart + rel;
        const uint16_t esize = U16(e), eflags = U16(e + 2);
        if (eflags & 0x0008) {  // FLAG_COMPACT (aapt2 compact entry)
            outType = static_cast<uint8_t>((eflags >> 8) & 0xff);
            outData = U32(e + 4);
            return true;
        }
        if ((eflags & 0x0001) || esize < 8 || entriesStart + rel + esize + 8 > tsize) return false;
        const uint8_t* v = e + esize;
        outType = v[3];
        outData = U32(v + 4);
        return true;
    };

    auto resolve = [&](uint32_t targetId, bool wantPng) -> std::string {
        std::string best;
        int bestScore = -1;
        for (int depth = 0; depth < 8 && targetId != 0; ++depth) {
            uint32_t nextRef = 0;
            const uint32_t wantPkg = (targetId >> 24) & 0xff, wantType = (targetId >> 16) & 0xff;
            const uint16_t wantIdx = static_cast<uint16_t>(targetId & 0xffff);
            for (uint32_t pos = rootHeader; pos + 8 <= total;) {
                const uint8_t* c = arsc.data() + pos;
                const uint16_t type = U16(c), phdr = U16(c + 2);
                const uint32_t psize = U32(c + 4);
                if (psize < 8 || psize > total - pos) break;
                if (type == 0x0200 && phdr >= 12 && psize >= phdr && U32(c + 8) == wantPkg) {
                    for (uint32_t tpos = phdr; tpos + 8 <= psize;) {
                        const uint8_t* tc = c + tpos;
                        const uint16_t ttype = U16(tc), thdr = U16(tc + 2);
                        const uint32_t tsize = U32(tc + 4);
                        if (tsize < 8 || tsize > psize - tpos) break;
                        if (ttype == 0x0201 && thdr >= 20 && tc[8] == wantType) {
                            uint8_t vtype = 0;
                            uint32_t vdata = 0;
                            if (readEntry(tc, tsize, wantIdx, vtype, vdata)) {
                                if (vtype == 0x01 && vdata != targetId && nextRef == 0) {
                                    nextRef = vdata;
                                } else if (vtype == 0x03 && vdata < values.strings.size()) {
                                    const std::string& s = values.strings[vdata];
                                    const uint32_t cfgSize = thdr >= 24 ? U32(tc + 20) : 0;
                                    const uint16_t lang = (cfgSize >= 12 && thdr >= 30) ? U16(tc + 28) : 0;
                                    const uint16_t density = (cfgSize >= 20 && thdr >= 38) ? U16(tc + 36) : 0;
                                    if (wantPng) {
                                        std::string low = s;
                                        std::transform(low.begin(), low.end(), low.begin(),
                                                       [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
                                        if (!low.ends_with(".png")) { tpos += tsize; continue; }
                                        const int d = (density == 0 || density >= 0xfffe) ? 160 : density;
                                        const int diff = d > 320 ? d - 320 : 320 - d;
                                        const int score = 10000 - diff;
                                        if (score > bestScore) { bestScore = score; best = s; }
                                    } else if (!s.empty()) {
                                        int score = 10;
                                        if (lang == 0) score = 100;
                                        else if (tc[28] == 'e' && tc[29] == 'n') score = 90;
                                        if (score > bestScore) { bestScore = score; best = s; }
                                    }
                                }
                            }
                        }
                        tpos += tsize;
                    }
                }
                pos += psize;
            }
            if (!best.empty()) return best;
            targetId = nextRef;
        }
        return best;
    };

    if (label.empty() && refs.labelRes != 0) label = resolve(refs.labelRes, false);
    if ((refs.iconPath.empty() || refs.iconPath.ends_with(".xml")) && refs.iconRes != 0) {
        if (std::string png = resolve(refs.iconRes, true); !png.empty()) refs.iconPath = std::move(png);
    }
}

/// Inspects an arm64-v8a ELF64 shared object's dynamic table without mapping or executing it.
/// Returns: 0 for pure leaf N0, 1 for Bionic/JNI N1, 2 for graphics/window N2; sets gles/vulkan flags.
int InspectArm64Elf(const std::vector<uint8_t>& b, ApkInfo& info) {
    if (b.size() < 64 || std::memcmp(b.data(), "\x7f""ELF", 4) != 0 || b[4] != 2 || b[5] != 1 || U16(b.data() + 18) != 183)
        return 1;
    const uint64_t phoff = U64(b.data() + 32);
    const uint16_t phentsize = U16(b.data() + 54), phnum = U16(b.data() + 56);
    if (phentsize < 56 || phoff > b.size() || uint64_t(phentsize) * phnum > b.size() - phoff) return 1;

    struct Seg { uint64_t off, vaddr, filesz; };
    std::vector<Seg> loads;
    uint64_t dynOff = 0, dynSz = 0;
    for (uint16_t i = 0; i < phnum; ++i) {
        const uint8_t* ph = b.data() + phoff + uint64_t(phentsize) * i;
        const uint32_t ptype = U32(ph);
        if (ptype == 1) loads.push_back({U64(ph + 8), U64(ph + 16), U64(ph + 32)});
        else if (ptype == 2) { dynOff = U64(ph + 8); dynSz = U64(ph + 32); }
    }
    auto mapVaddr = [&](uint64_t va, uint64_t len) -> const uint8_t* {
        for (const Seg& s : loads) {
            if (va >= s.vaddr && va - s.vaddr <= s.filesz && len <= s.filesz - (va - s.vaddr)) {
                const uint64_t foff = s.off + (va - s.vaddr);
                if (foff <= b.size() && len <= b.size() - foff) return b.data() + foff;
            }
        }
        return nullptr;
    };
    if (dynOff == 0 || dynOff > b.size() || dynSz > b.size() - dynOff) return 0;

    uint64_t strva = 0, strsz = 0;
    std::vector<uint64_t> neededOffs;
    for (uint64_t off = 0; off + 16 <= dynSz; off += 16) {
        const int64_t tag = static_cast<int64_t>(U64(b.data() + dynOff + off));
        const uint64_t val = U64(b.data() + dynOff + off + 8);
        if (tag == 0) break;
        if (tag == 1) neededOffs.push_back(val);
        else if (tag == 5) strva = val;
        else if (tag == 10) strsz = val;
    }
    int level = neededOffs.empty() ? 0 : 1;
    if (const uint8_t* strtab = (strva && strsz) ? mapVaddr(strva, strsz) : nullptr) {
        for (uint64_t noff : neededOffs) {
            if (noff >= strsz) continue;
            const char* start = reinterpret_cast<const char*>(strtab + noff);
            const size_t maxLen = static_cast<size_t>(strsz - noff);
            const size_t len = strnlen(start, maxLen);
            const std::string dep(start, len);
            if (dep == "libvulkan.so") {
                info.vulkan = true;
                info.vulkanRequired = true;
                level = 2;
            } else if (dep == "libEGL.so" || dep == "libGLESv1_CM.so" || dep == "libGLESv2.so" || dep == "libGLESv3.so") {
                if (info.glesVersion.empty()) info.glesVersion = dep == "libGLESv3.so" ? "3.0" : "2.0";
                level = 2;
            } else if (dep == "libandroid.so") {
                level = 2;
            }
        }
    }
    if (level == 0) {
        // Check section headers for undefined imports or JNI_OnLoad if no DT_NEEDED was present.
        const uint64_t shoff = U64(b.data() + 40);
        const uint16_t shentsize = U16(b.data() + 58), shnum = U16(b.data() + 60);
        if (shentsize >= 64 && shoff <= b.size() && uint64_t(shentsize) * shnum <= b.size() - shoff) {
            for (uint16_t i = 0; i < shnum && level == 0; ++i) {
                const uint8_t* sh = b.data() + shoff + uint64_t(shentsize) * i;
                if (U32(sh + 4) != 11) continue;  // SHT_DYNSYM
                const uint64_t soff = U64(sh + 24), ssz = U64(sh + 32);
                const uint32_t link = U32(sh + 40);
                if (soff > b.size() || ssz > b.size() - soff || link >= shnum) continue;
                const uint8_t* strsh = b.data() + shoff + uint64_t(shentsize) * link;
                const uint64_t stroff = U64(strsh + 24), strsize = U64(strsh + 32);
                if (stroff > b.size() || strsize > b.size() - stroff) continue;
                for (uint64_t k = 24; k + 24 <= ssz; k += 24) {
                    const uint8_t* sym = b.data() + soff + k;
                    const uint32_t stName = U32(sym);
                    const uint16_t stShndx = U16(sym + 6);
                    if (stName >= strsize) continue;
                    const char* sname = reinterpret_cast<const char*>(b.data() + stroff + stName);
                    const std::string symStr(sname, strnlen(sname, static_cast<size_t>(strsize - stName)));
                    if ((stShndx == 0 && !symStr.empty()) || symStr == "JNI_OnLoad") { level = 1; break; }
                }
            }
        }
    }
    return level;
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
    MappedFile f(path);
    if (!f.error().empty()) return f.error();
    info.fileSize = f.size();
    std::vector<ZipEntry> entries;
    if (std::string err = ParseZip(f, entries); !err.empty()) return err;
    const ZipEntry* manifest = nullptr;
    const ZipEntry* arsc = nullptr;
    std::vector<const ZipEntry*> arm64Libs;
    std::set<std::string> lower, abis;
    for (const ZipEntry& e : entries) {
        if (e.name == "AndroidManifest.xml") manifest = &e;
        else if (e.name == "resources.arsc") arsc = &e;
        if (e.name == "classes.dex" || (e.name.rfind("classes", 0) == 0 && e.name.size() > 11 &&
                                        e.name.find('/') == std::string::npos && e.name.ends_with(".dex"))) {
            ++info.dexCount;
        }
        if (e.name.rfind("lib/", 0) == 0 && e.name.ends_with(".so")) {
            const size_t slash = e.name.find('/', 4);
            if (slash != std::string::npos) {
                const std::string abi = e.name.substr(4, slash - 4);
                abis.insert(abi);
                info.nativeLibs.push_back(e.name);
                if (abi == "arm64-v8a" && e.name.find('/', slash + 1) == std::string::npos) arm64Libs.push_back(&e);
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
    ManifestRefs refs;
    if (std::string err = ParseManifest(x, info, refs); !err.empty()) return err;

    if (arsc && (info.label.empty() || refs.iconPath.empty() || refs.iconPath.ends_with(".xml"))) {
        std::vector<uint8_t> arscBytes;
        if (ReadEntry(path, *arsc, arscBytes, 64ull << 20).empty()) ResolveResources(arscBytes, refs, info.label);
    }
    if (info.label.empty()) {
        const size_t dot = info.packageName.find_last_of('.');
        info.label = dot == std::string::npos ? info.packageName : info.packageName.substr(dot + 1);
    }
    if (!refs.iconPath.empty()) {
        for (const ZipEntry& e : entries) {
            if (e.name == refs.iconPath && e.size <= (2ull << 20)) {
                std::vector<uint8_t> png;
                if (ReadEntry(path, e, png, 2ull << 20).empty() && png.size() >= 8 &&
                    std::memcmp(png.data(), "\x89PNG\r\n\x1a\n", 8) == 0) {
                    info.iconPng = std::move(png);
                }
                break;
            }
        }
    }

    const bool hasArm64 = abis.contains("arm64-v8a");
    if (!info.nativeLibs.empty() && !hasArm64) {
        info.verdict = "unsupported";
    } else if (!arm64Libs.empty()) {
        int maxLevel = 0;
        for (const ZipEntry* le : arm64Libs) {
            std::vector<uint8_t> elf;
            if (ReadEntry(path, *le, elf, 64ull << 20).empty()) maxLevel = std::max(maxLevel, InspectArm64Elf(elf, info));
            else maxLevel = std::max(maxLevel, 1);
        }
        info.profile = maxLevel >= 2 ? "N2" : (maxLevel == 1 ? "N1" : "N0");
        info.verdict = info.vulkanRequired ? "not-yet" : "needs-native";
    } else {
        info.verdict = info.vulkanRequired ? "not-yet" : "can-run";
    }
    return {};
}

}  // namespace winapk
