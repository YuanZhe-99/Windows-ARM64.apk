// host_services.cpp -- Windows ARM64 host services: NativeOs (file descriptors, memfd, flock,
// statvfs, guest environment, liblzma .xz decompression), Win32/test-isolated clipboard
// (MacApkHostPasteboard), DirectWrite PostScript font lookup, /proc synthesis and URL opening.
#include "host_services.h"
#include "bridge.h"
#include "embed.h"

#include <windows.h>
#include <dwrite.h>
#include <psapi.h>
#include <shellapi.h>
#include <wrl/client.h>

#include <lzma.h>

#include <fcntl.h>
#include <io.h>
#include <share.h>
#include <sys/stat.h>
#ifdef _DEBUG
#include <crtdbg.h>
#endif

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace winapk::bridge {
namespace {

using Microsoft::WRL::ComPtr;

/// Suppresses the Windows UCRT invalid-parameter termination (and Debug _CRT_ASSERT modal dialog) on raw fd checks.
class SilentInvalidParam {
public:
    SilentInvalidParam()
        : prev_(_set_thread_local_invalid_parameter_handler(&SilentInvalidParam::Ignore)) {
#ifdef _DEBUG
        prevAssertMode_ = _CrtSetReportMode(_CRT_ASSERT, 0);
#endif
    }
    ~SilentInvalidParam() {
#ifdef _DEBUG
        if (prevAssertMode_ != -1) _CrtSetReportMode(_CRT_ASSERT, prevAssertMode_);
#endif
        _set_thread_local_invalid_parameter_handler(prev_);
    }

private:
    static void __cdecl Ignore(const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, uintptr_t) {}
    _invalid_parameter_handler prev_;
#ifdef _DEBUG
    int prevAssertMode_ = -1;
#endif
};

bool ValidFdRange(jint fd) { return fd >= 0 && fd < 8192; }

/// Converts UTF-8 to UTF-16.
std::wstring ToWide(const std::string& s) {
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(n > 0 ? n : 0, L'\0');
    if (n > 0) MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
    return w;
}

/// Converts UTF-16 to UTF-8.
std::string FromWide(const wchar_t* w, size_t len) {
    if (!w || len == 0) return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, w, static_cast<int>(len), nullptr, 0, nullptr, nullptr);
    std::string s(n > 0 ? n : 0, '\0');
    if (n > 0) WideCharToMultiByte(CP_UTF8, 0, w, static_cast<int>(len), s.data(), n, nullptr, nullptr);
    return s;
}

/// Creates a Java String from UTF-16 without JNI modified-UTF-8 surrogate mangling.
jstring NewJStringWide(JNIEnv* env, const std::wstring& w) {
    return env->NewString(reinterpret_cast<const jchar*>(w.data()), static_cast<jsize>(w.size()));
}

/// Reads a Java String as UTF-16 (returns false when null).
bool ReadJStringWide(JNIEnv* env, jstring s, std::wstring& out) {
    if (!s) return false;
    const jsize n = env->GetStringLength(s);
    const jchar* ch = env->GetStringChars(s, nullptr);
    if (!ch && n > 0) {
        env->ExceptionClear();
        return false;
    }
    out.assign(reinterpret_cast<const wchar_t*>(ch), static_cast<size_t>(n));
    if (ch) env->ReleaseStringChars(s, ch);
    return true;
}

/// Maps Windows UCRT errno values to Bionic/Linux errno numbers expected by android.system.Os.
jint ToBionicErrno(int e) {
    switch (e) {
        case EPERM: return 1;
        case ENOENT: return 2;
        case EIO: return 5;
        case EBADF: return 9;
        case EAGAIN:
        case EWOULDBLOCK: return 11;
        case ENOMEM: return 12;
        case EACCES: return 13;
        case EFAULT: return 14;
        case EEXIST: return 17;
        case ENODEV: return 19;
        case ENOTDIR: return 20;
        case EISDIR: return 21;
        case EINVAL: return 22;
        case EMFILE: return 24;
        case ENOSPC: return 28;
        case EROFS: return 30;
        default: return e > 0 ? e : 5;
    }
}

// ---- Shared guest/Java environment & properties ---------------------------

std::mutex g_envMu;
bool g_envSeeded = false;
// Stored strings are owned in unique_ptr<std::string> and retired into g_envRetired on overwrite so
// concurrent guest native readers holding a const char* from getenv(3) never read freed memory.
std::unordered_map<std::string, std::unique_ptr<std::string>> g_envMap;
std::vector<std::unique_ptr<std::string>> g_envRetired;
std::unordered_map<std::string, std::string> g_sysProps;
std::string g_systemFontsDir;
std::string g_sharedStorageRoot;

void EnsureEnvSeededLocked() {
    if (g_envSeeded) return;
    g_envSeeded = true;
    LPWCH block = GetEnvironmentStringsW();
    if (!block) return;
    for (const wchar_t* p = block; *p; p += std::wcslen(p) + 1) {
        if (*p == L'=') continue;  // Windows per-drive working dir pseudo-vars
        const wchar_t* eq = std::wcschr(p, L'=');
        if (!eq) continue;
        std::string k = FromWide(p, static_cast<size_t>(eq - p));
        std::string v = FromWide(eq + 1, std::wcslen(eq + 1));
        g_envMap[k] = std::make_unique<std::string>(std::move(v));
    }
    FreeEnvironmentStringsW(block);
}

// ---- NativeOs implementations ---------------------------------------------

jint JNICALL OsOpen(JNIEnv* env, jclass, jstring jpath, jint bionicFlags, jint mode) {
    if (!jpath) return -22;  // -EINVAL
    const std::wstring wpath = ToWide(Utf8(env, jpath));
    if (wpath.empty()) return -2;  // -ENOENT

    enum : int {
        B_ACCMODE = 03, B_WRONLY = 01, B_RDWR = 02,
        B_CREAT = 0100, B_EXCL = 0200, B_TRUNC = 01000,
        B_APPEND = 02000, B_DIRECTORY = 0200000, B_CLOEXEC = 02000000
    };
    const DWORD attrs = GetFileAttributesW(wpath.c_str());
    if ((bionicFlags & B_DIRECTORY) && (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY))) {
        return attrs == INVALID_FILE_ATTRIBUTES ? -2 : -20;  // -ENOENT / -ENOTDIR
    }
    if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        if ((bionicFlags & B_ACCMODE) != 0) return -21;  // -EISDIR on write
    }

    int oflag = _O_BINARY;
    switch (bionicFlags & B_ACCMODE) {
        case B_WRONLY: oflag |= _O_WRONLY; break;
        case B_RDWR:   oflag |= _O_RDWR; break;
        default:       oflag |= _O_RDONLY; break;
    }
    if (bionicFlags & B_CREAT)   oflag |= _O_CREAT;
    if (bionicFlags & B_EXCL)    oflag |= _O_EXCL;
    if (bionicFlags & B_TRUNC)   oflag |= _O_TRUNC;
    if (bionicFlags & B_APPEND)  oflag |= _O_APPEND;
    if (bionicFlags & B_CLOEXEC) oflag |= _O_NOINHERIT;

    int pmode = _S_IREAD;
    if ((mode & 0200) || (bionicFlags & B_ACCMODE) != 0 || !(bionicFlags & B_CREAT)) pmode |= _S_IWRITE;

    SilentInvalidParam guard;
    int fd = -1;
    const errno_t err = _wsopen_s(&fd, wpath.c_str(), oflag, _SH_DENYNO, pmode);
    if (err != 0 || fd < 0) return -ToBionicErrno(err != 0 ? err : errno);
    return fd;
}

jlong JNICALL OsLseek(JNIEnv*, jclass, jint fd, jlong offset, jint whence) {
    if (!ValidFdRange(fd) || (whence != SEEK_SET && whence != SEEK_CUR && whence != SEEK_END))
        return !ValidFdRange(fd) ? -9 : -22;
    SilentInvalidParam guard;
    errno = 0;
    const __int64 r = _lseeki64(fd, offset, whence);
    return r < 0 ? -ToBionicErrno(errno ? errno : EBADF) : static_cast<jlong>(r);
}

jint JNICALL OsClose(JNIEnv*, jclass, jint fd) {
    if (!ValidFdRange(fd)) return -9;
    SilentInvalidParam guard;
    errno = 0;
    return _close(fd) < 0 ? -ToBionicErrno(errno ? errno : EBADF) : 0;
}

jint JNICALL OsDup(JNIEnv*, jclass, jint fd) {
    if (!ValidFdRange(fd)) return -9;
    SilentInvalidParam guard;
    errno = 0;
    const int r = _dup(fd);
    return r < 0 ? -ToBionicErrno(errno ? errno : EBADF) : r;
}

jint JNICALL OsDup2(JNIEnv*, jclass, jint oldfd, jint newfd) {
    if (!ValidFdRange(oldfd) || !ValidFdRange(newfd)) return -9;
    SilentInvalidParam guard;
    errno = 0;
    const int r = _dup2(oldfd, newfd);
    return r < 0 ? -ToBionicErrno(errno ? errno : EBADF) : newfd;
}

jlong JNICALL OsSysconf(JNIEnv*, jclass, jint bionicName) {
    SYSTEM_INFO si{};
    GetSystemInfo(&si);
    switch (bionicName) {
        case 0x27:  // _SC_PAGESIZE
        case 0x28:  // _SC_PAGE_SIZE
            return static_cast<jlong>(si.dwPageSize ? si.dwPageSize : 4096);
        case 0x06:  // _SC_CLK_TCK
            return 100;
        case 0x60:  // _SC_NPROCESSORS_CONF
        case 0x61:  // _SC_NPROCESSORS_ONLN
            return static_cast<jlong>(std::max<DWORD>(1, GetActiveProcessorCount(ALL_PROCESSOR_GROUPS)));
        default:
            return -1;
    }
}

jint JNICALL OsMemfdCreate(JNIEnv*, jclass, jstring, jint) {
    wchar_t tmpDir[MAX_PATH + 1]{};
    const DWORD n = GetTempPathW(MAX_PATH, tmpDir);
    if (!n || n > MAX_PATH) return -5;
    static std::atomic<uint64_t> seq{1};
    const std::wstring dir = std::wstring(tmpDir) + L"winapk-memfd";
    CreateDirectoryW(dir.c_str(), nullptr);
    const std::wstring path = dir + L"\\mfd-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
                              std::to_wstring(seq.fetch_add(1, std::memory_order_relaxed)) + L".tmp";
    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, FALSE};
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE,
                           FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, &sa, CREATE_NEW,
                           FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
    if (h == INVALID_HANDLE_VALUE) return -5;
    SilentInvalidParam guard;
    const int fd = _open_osfhandle(reinterpret_cast<intptr_t>(h), _O_RDWR | _O_BINARY | _O_NOINHERIT);
    if (fd < 0) {
        CloseHandle(h);
        return -24;
    }
    return fd;
}

jlongArray JNICALL OsFstat(JNIEnv* env, jclass, jint fd) {
    if (fd < 0) return nullptr;
    SilentInvalidParam guard;
    struct _stat64 st{};
    if (_fstat64(fd, &st) < 0) return nullptr;
    const jlong vals[11] = {
        static_cast<jlong>(st.st_dev), static_cast<jlong>(st.st_ino), static_cast<jlong>(st.st_mode),
        static_cast<jlong>(st.st_nlink), static_cast<jlong>(st.st_size), static_cast<jlong>(st.st_atime),
        static_cast<jlong>(st.st_mtime), static_cast<jlong>(st.st_ctime), 4096,
        static_cast<jlong>((st.st_size + 511) / 512), static_cast<jlong>(st.st_rdev),
    };
    jlongArray arr = env->NewLongArray(11);
    if (arr) env->SetLongArrayRegion(arr, 0, 11, vals);
    return arr;
}

jint JNICALL OsFtruncate(JNIEnv*, jclass, jint fd, jlong length) {
    if (fd < 0) return -9;
    if (length < 0) return -22;
    SilentInvalidParam guard;
    const errno_t err = _chsize_s(fd, length);
    return err == 0 ? 0 : -ToBionicErrno(err);
}

jint JNICALL OsTakeRunLock(JNIEnv* env, jclass, jstring jpath) {
    if (!jpath) return -22;
    const std::wstring wpath = ToWide(Utf8(env, jpath));
    if (wpath.empty()) return -22;
    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, FALSE};
    HANDLE h = CreateFileW(wpath.c_str(), GENERIC_READ | GENERIC_WRITE,
                           FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return -13;  // -EACCES
    OVERLAPPED ov{};
    if (!LockFileEx(h, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0, MAXDWORD, MAXDWORD, &ov)) {
        const DWORD gle = GetLastError();
        CloseHandle(h);
        return (gle == ERROR_LOCK_VIOLATION || gle == ERROR_IO_PENDING) ? -11 : -5;  // -EWOULDBLOCK
    }
    SetFilePointer(h, 0, nullptr, FILE_BEGIN);
    SetEndOfFile(h);
    char buf[32]{};
    const int n = std::snprintf(buf, sizeof(buf), "%lu\n", GetCurrentProcessId());
    if (n > 0) {
        DWORD written = 0;
        WriteFile(h, buf, static_cast<DWORD>(n), &written, nullptr);
    }
    SilentInvalidParam guard;
    const int fd = _open_osfhandle(reinterpret_cast<intptr_t>(h), _O_RDWR | _O_BINARY | _O_NOINHERIT);
    if (fd < 0) {
        CloseHandle(h);
        return -24;
    }
    return fd;
}

jint JNICALL OsRead(JNIEnv* env, jclass, jint fd, jbyteArray buf, jint off, jint count) {
    if (!buf) return -14;  // -EFAULT
    const jsize len = env->GetArrayLength(buf);
    if (off < 0 || count < 0 || off > len || count > len - off) return -22;
    if (fd < 0) return -9;
    if (count == 0) return 0;
    std::vector<uint8_t> tmp(static_cast<size_t>(count));
    SilentInvalidParam guard;
    errno = 0;
    const int r = _read(fd, tmp.data(), static_cast<unsigned int>(count));
    if (r < 0) return -ToBionicErrno(errno ? errno : EBADF);
    if (r > 0) env->SetByteArrayRegion(buf, off, r, reinterpret_cast<const jbyte*>(tmp.data()));
    return r;
}

jint JNICALL OsWrite(JNIEnv* env, jclass, jint fd, jbyteArray buf, jint off, jint count) {
    if (!buf) return -14;
    const jsize len = env->GetArrayLength(buf);
    if (off < 0 || count < 0 || off > len || count > len - off) return -22;
    if (fd < 0) return -9;
    if (count == 0) return 0;
    std::vector<uint8_t> tmp(static_cast<size_t>(count));
    env->GetByteArrayRegion(buf, off, count, reinterpret_cast<jbyte*>(tmp.data()));
    SilentInvalidParam guard;
    errno = 0;
    const int r = _write(fd, tmp.data(), static_cast<unsigned int>(count));
    if (r < 0) return -ToBionicErrno(errno ? errno : EBADF);
    return r;
}

jlongArray StatVfsFromWidePath(JNIEnv* env, const std::wstring& wpath) {
    if (wpath.empty()) return nullptr;
    DWORD attrs = GetFileAttributesW(wpath.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) return nullptr;
    std::wstring dir = (attrs & FILE_ATTRIBUTE_DIRECTORY)
        ? wpath
        : std::filesystem::path(wpath).parent_path().wstring();
    if (dir.empty()) dir = L".";
    ULARGE_INTEGER freeAvail{}, totalBytes{}, totalFree{};
    if (!GetDiskFreeSpaceExW(dir.c_str(), &freeAvail, &totalBytes, &totalFree)) return nullptr;

    wchar_t volRoot[MAX_PATH + 1]{};
    DWORD serial = 0, maxComp = 255, fsFlags = 0;
    if (GetVolumePathNameW(dir.c_str(), volRoot, MAX_PATH)) {
        GetVolumeInformationW(volRoot, nullptr, 0, &serial, &maxComp, &fsFlags, nullptr, 0);
    }
    constexpr uint64_t kBlock = 4096;
    const jlong vals[11] = {
        static_cast<jlong>(kBlock),
        static_cast<jlong>(kBlock),
        static_cast<jlong>(totalBytes.QuadPart / kBlock),
        static_cast<jlong>(totalFree.QuadPart / kBlock),
        static_cast<jlong>(freeAvail.QuadPart / kBlock),
        1000000,
        1000000,
        1000000,
        static_cast<jlong>(serial),
        static_cast<jlong>((fsFlags & FILE_READ_ONLY_VOLUME) ? 1 : 0),
        static_cast<jlong>(maxComp ? maxComp : 255),
    };
    jlongArray arr = env->NewLongArray(11);
    if (arr) env->SetLongArrayRegion(arr, 0, 11, vals);
    return arr;
}

jlongArray JNICALL OsStatvfs(JNIEnv* env, jclass, jstring path) {
    if (!path) return nullptr;
    return StatVfsFromWidePath(env, ToWide(Utf8(env, path)));
}

jlongArray JNICALL OsFstatvfs(JNIEnv* env, jclass, jint fd) {
    if (fd < 0) return nullptr;
    SilentInvalidParam guard;
    const intptr_t raw = _get_osfhandle(fd);
    if (raw == -1 || reinterpret_cast<HANDLE>(raw) == INVALID_HANDLE_VALUE) return nullptr;
    std::wstring buf(32768, L'\0');
    const DWORD n = GetFinalPathNameByHandleW(reinterpret_cast<HANDLE>(raw), buf.data(),
                                              static_cast<DWORD>(buf.size()), FILE_NAME_NORMALIZED);
    if (!n || n >= buf.size()) return nullptr;
    buf.resize(n);
    return StatVfsFromWidePath(env, buf);
}

jint JNICALL OsSetenv(JNIEnv* env, jclass, jstring name, jstring value, jboolean overwrite) {
    if (!name || !value) return -22;
    const std::string n = Utf8(env, name);
    if (n.empty() || n.find('=') != std::string::npos) return -22;
    const std::string v = Utf8(env, value);
    std::lock_guard<std::mutex> lock(g_envMu);
    EnsureEnvSeededLocked();
    auto it = g_envMap.find(n);
    if (it != g_envMap.end()) {
        if (!overwrite) return 0;
        g_envRetired.push_back(std::move(it->second));
        it->second = std::make_unique<std::string>(v);
    } else {
        g_envMap.emplace(n, std::make_unique<std::string>(v));
    }
    return 0;
}

jstring JNICALL OsGetenv(JNIEnv* env, jclass, jstring name) {
    if (!name) return nullptr;
    const std::string n = Utf8(env, name);
    if (n.empty() || n.find('=') != std::string::npos) return nullptr;
    const char* v = GuestEnvGet(n.c_str());
    return v ? NewJStringWide(env, ToWide(v)) : nullptr;
}

jint JNICALL OsUnsetenv(JNIEnv* env, jclass, jstring name) {
    if (!name) return -22;
    const std::string n = Utf8(env, name);
    if (n.empty() || n.find('=') != std::string::npos) return -22;
    std::lock_guard<std::mutex> lock(g_envMu);
    EnsureEnvSeededLocked();
    auto it = g_envMap.find(n);
    if (it != g_envMap.end()) {
        g_envRetired.push_back(std::move(it->second));
        g_envMap.erase(it);
    }
    return 0;
}

jbyteArray JNICALL OsXzDecompress(JNIEnv* env, jclass, jbyteArray in) {
    if (!in) return nullptr;
    const jsize n = env->GetArrayLength(in);
    if (n <= 0) return nullptr;
    std::vector<uint8_t> src(static_cast<size_t>(n));
    env->GetByteArrayRegion(in, 0, n, reinterpret_cast<jbyte*>(src.data()));

    lzma_stream strm = LZMA_STREAM_INIT;
    if (lzma_stream_decoder(&strm, UINT64_MAX, LZMA_CONCATENATED) != LZMA_OK) {
        return nullptr;
    }
    std::vector<uint8_t> out(std::min<size_t>(static_cast<size_t>(n) * 8 + 65536, 16u << 20));
    strm.next_in = src.data();
    strm.avail_in = src.size();
    bool ok = false;
    size_t produced = 0;
    for (;;) {
        strm.next_out = out.data() + produced;
        strm.avail_out = out.size() - produced;
        const lzma_ret ret = lzma_code(&strm, LZMA_FINISH);
        produced = out.size() - strm.avail_out;
        if (ret == LZMA_STREAM_END) {
            ok = true;
            break;
        }
        if (ret != LZMA_OK && ret != LZMA_BUF_ERROR) break;
        if (strm.avail_out == 0) {
            if (out.size() >= (256u << 20)) break;
            out.resize(std::min<size_t>(out.size() * 2, 256u << 20));
            continue;
        }
        break;
    }
    lzma_end(&strm);
    if (!ok || produced == 0) return nullptr;
    jbyteArray res = env->NewByteArray(static_cast<jsize>(produced));
    if (!res) {
        env->ExceptionClear();
        return nullptr;
    }
    env->SetByteArrayRegion(res, 0, static_cast<jsize>(produced), reinterpret_cast<const jbyte*>(out.data()));
    return res;
}

// ---- ClipboardManager$MacApkHostPasteboard --------------------------------

enum : jint {
    kClipText      = 1,
    kClipHtml      = 2,
    kClipUrl       = 4,
    kClipConcealed = 8,
    kClipMacApk    = 16,
};

struct TestPasteboardState {
    std::wstring name;
    int64_t changeCount = 1;
    jint mask = 0;
    bool hasText = false, hasHtml = false, hasUrl = false, hasSource = false, hasLabel = false;
    std::wstring text, html, url, source, label;
};

std::mutex g_clipMu;
bool g_clipInit = false;
bool g_clipTestMode = false;
TestPasteboardState g_testClip;

void EnsureClipboardInitLocked() {
    if (g_clipInit) return;
    g_clipInit = true;
    wchar_t buf[256]{};
    const DWORD n = GetEnvironmentVariableW(L"MACAPK_CLIPBOARD_TEST_PASTEBOARD", buf, ARRAYSIZE(buf));
    if (n > 0 && n < ARRAYSIZE(buf)) {
        g_clipTestMode = true;
        std::wstring val(buf, n);
        if (val == L"unique") {
            g_testClip.name = L"winapk-test-pasteboard-" + std::to_wstring(GetCurrentProcessId());
        } else {
            g_testClip.name = val;
        }
        LogLine("[winapk] clipboard: TEST pasteboard '" + FromWide(g_testClip.name.c_str(), g_testClip.name.size()) +
                "' in use; Windows system clipboard is untouched");
    }
}

struct WinClipFormats {
    UINT html = RegisterClipboardFormatW(L"HTML Format");
    UINT url = RegisterClipboardFormatW(L"UniformResourceLocatorW");
    UINT concealed = RegisterClipboardFormatW(L"org.nspasteboard.ConcealedType");
    UINT excludeMonitor = RegisterClipboardFormatW(L"ExcludeClipboardContentFromMonitorProcessing");
    UINT history = RegisterClipboardFormatW(L"CanIncludeInClipboardHistory");
    UINT cloud = RegisterClipboardFormatW(L"CanUploadToCloudClipboard");
    UINT source = RegisterClipboardFormatW(L"org.macapk.android.clip.source");
    UINT label = RegisterClipboardFormatW(L"org.macapk.android.clip.label");
};

const WinClipFormats& Formats() {
    static const WinClipFormats f{};
    return f;
}

bool PutWideGlobal(UINT fmt, const std::wstring& s) {
    const size_t bytes = (s.size() + 1) * sizeof(wchar_t);
    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (!mem) return false;
    void* dst = GlobalLock(mem);
    if (!dst) {
        GlobalFree(mem);
        return false;
    }
    std::memcpy(dst, s.c_str(), bytes);
    GlobalUnlock(mem);
    if (!SetClipboardData(fmt, mem)) {
        GlobalFree(mem);
        return false;
    }
    return true;
}

bool PutDwordGlobal(UINT fmt, DWORD val) {
    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, sizeof(DWORD));
    if (!mem) return false;
    void* dst = GlobalLock(mem);
    if (!dst) {
        GlobalFree(mem);
        return false;
    }
    std::memcpy(dst, &val, sizeof(DWORD));
    GlobalUnlock(mem);
    if (!SetClipboardData(fmt, mem)) {
        GlobalFree(mem);
        return false;
    }
    return true;
}

bool PutCfHtml(UINT fmt, const std::string& fragment) {
    static constexpr const char* kHdrTmpl =
        "Version:0.9\r\nStartHTML:%010zu\r\nEndHTML:%010zu\r\nStartFragment:%010zu\r\nEndFragment:%010zu\r\n";
    static constexpr const char* kPre = "<html><body><!--StartFragment-->";
    static constexpr const char* kPost = "<!--EndFragment--></body></html>";
    char dummy[128]{};
    const size_t hdrLen = static_cast<size_t>(std::snprintf(dummy, sizeof(dummy), kHdrTmpl, size_t{0}, size_t{0}, size_t{0}, size_t{0}));
    const size_t startHtml = hdrLen;
    const size_t startFrag = startHtml + std::strlen(kPre);
    const size_t endFrag = startFrag + fragment.size();
    const size_t endHtml = endFrag + std::strlen(kPost);
    char hdr[160]{};
    std::snprintf(hdr, sizeof(hdr), kHdrTmpl, startHtml, endHtml, startFrag, endFrag);
    std::string payload = std::string(hdr) + kPre + fragment + kPost;
    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, payload.size() + 1);
    if (!mem) return false;
    void* dst = GlobalLock(mem);
    if (!dst) {
        GlobalFree(mem);
        return false;
    }
    std::memcpy(dst, payload.c_str(), payload.size() + 1);
    GlobalUnlock(mem);
    if (!SetClipboardData(fmt, mem)) {
        GlobalFree(mem);
        return false;
    }
    return true;
}

bool GetWideGlobal(UINT fmt, std::wstring& out) {
    if (!IsClipboardFormatAvailable(fmt)) return false;
    HANDLE h = GetClipboardData(fmt);
    if (!h) return false;
    const size_t cap = GlobalSize(h) / sizeof(wchar_t);
    const wchar_t* p = static_cast<const wchar_t*>(GlobalLock(h));
    if (!p || cap == 0) {
        if (p) GlobalUnlock(h);
        return false;
    }
    size_t len = 0;
    while (len < cap && p[len] != L'\0') ++len;
    out.assign(p, len);
    GlobalUnlock(h);
    return true;
}

bool GetCfHtml(UINT fmt, std::wstring& out) {
    if (!IsClipboardFormatAvailable(fmt)) return false;
    HANDLE h = GetClipboardData(fmt);
    if (!h) return false;
    const size_t cap = GlobalSize(h);
    const char* p = static_cast<const char*>(GlobalLock(h));
    if (!p || cap == 0) {
        if (p) GlobalUnlock(h);
        return false;
    }
    const size_t len = strnlen(p, cap);
    std::string raw(p, len);
    GlobalUnlock(h);
    static constexpr const char* kStartTag = "<!--StartFragment-->";
    static constexpr const char* kEndTag = "<!--EndFragment-->";
    const size_t s = raw.find(kStartTag);
    const size_t e = raw.find(kEndTag);
    if (s != std::string::npos && e != std::string::npos && e >= s + std::strlen(kStartTag)) {
        const size_t bodyOff = s + std::strlen(kStartTag);
        out = ToWide(raw.substr(bodyOff, e - bodyOff));
    } else {
        out = ToWide(raw);
    }
    return true;
}

jint WinClipboardTypesMask() {
    const auto& f = Formats();
    jint m = 0;
    if (IsClipboardFormatAvailable(CF_UNICODETEXT)) m |= kClipText;
    if (f.html && IsClipboardFormatAvailable(f.html)) m |= kClipHtml;
    if (f.url && IsClipboardFormatAvailable(f.url)) m |= kClipUrl;
    if ((f.concealed && IsClipboardFormatAvailable(f.concealed)) ||
        (f.excludeMonitor && IsClipboardFormatAvailable(f.excludeMonitor))) m |= kClipConcealed;
    if (f.source && IsClipboardFormatAvailable(f.source)) m |= kClipMacApk;
    return m;
}

bool OpenWinClipboardRetry() {
    for (int i = 0; i < 5; ++i) {
        if (OpenClipboard(nullptr)) return true;
        Sleep(5);
    }
    return false;
}

jlong JNICALL ClipChangeCount(JNIEnv*, jclass) {
    std::lock_guard<std::mutex> lock(g_clipMu);
    EnsureClipboardInitLocked();
    if (g_clipTestMode) return g_testClip.changeCount;
    return static_cast<jlong>(GetClipboardSequenceNumber());
}

jint JNICALL ClipTypes(JNIEnv*, jclass) {
    std::lock_guard<std::mutex> lock(g_clipMu);
    EnsureClipboardInitLocked();
    if (g_clipTestMode) return g_testClip.mask;
    return WinClipboardTypesMask();
}

jlong JNICALL ClipWrite(JNIEnv* env, jclass, jstring jtext, jstring jhtml, jstring jurl,
                        jstring jsource, jstring jlabel, jboolean sensitive) {
    std::wstring text, html, url, source, label;
    const bool hasText = ReadJStringWide(env, jtext, text);
    const bool hasHtml = ReadJStringWide(env, jhtml, html);
    const bool hasUrl = ReadJStringWide(env, jurl, url);
    const bool hasSource = ReadJStringWide(env, jsource, source);
    const bool hasLabel = ReadJStringWide(env, jlabel, label);

    std::lock_guard<std::mutex> lock(g_clipMu);
    EnsureClipboardInitLocked();
    if (g_clipTestMode) {
        g_testClip.hasText = hasText;     g_testClip.text = hasText ? text : L"";
        g_testClip.hasHtml = hasHtml;     g_testClip.html = hasHtml ? html : L"";
        g_testClip.hasUrl = hasUrl;       g_testClip.url = hasUrl ? url : L"";
        g_testClip.hasSource = hasSource; g_testClip.source = hasSource ? source : L"";
        g_testClip.hasLabel = hasLabel;   g_testClip.label = hasLabel ? label : L"";
        jint m = 0;
        if (hasText) m |= kClipText;
        if (hasHtml) m |= kClipHtml;
        if (hasUrl) m |= kClipUrl;
        if (sensitive) m |= kClipConcealed;
        if (hasSource) m |= kClipMacApk;
        g_testClip.mask = m;
        return ++g_testClip.changeCount;
    }

    if (!OpenWinClipboardRetry()) return -1;
    EmptyClipboard();
    const auto& f = Formats();
    if (hasText) PutWideGlobal(CF_UNICODETEXT, text);
    if (hasHtml && f.html) PutCfHtml(f.html, FromWide(html.c_str(), html.size()));
    if (hasUrl && f.url) PutWideGlobal(f.url, url);
    if (sensitive) {
        if (f.concealed) PutWideGlobal(f.concealed, L"");
        if (f.excludeMonitor) PutDwordGlobal(f.excludeMonitor, 1);
        if (f.history) PutDwordGlobal(f.history, 0);
        if (f.cloud) PutDwordGlobal(f.cloud, 0);
    }
    if (hasSource && f.source) PutWideGlobal(f.source, source);
    if (hasLabel && f.label) PutWideGlobal(f.label, label);
    CloseClipboard();
    return static_cast<jlong>(GetClipboardSequenceNumber());
}

jlong JNICALL ClipClear(JNIEnv*, jclass) {
    std::lock_guard<std::mutex> lock(g_clipMu);
    EnsureClipboardInitLocked();
    if (g_clipTestMode) {
        g_testClip.hasText = g_testClip.hasHtml = g_testClip.hasUrl = g_testClip.hasSource = g_testClip.hasLabel = false;
        g_testClip.text.clear(); g_testClip.html.clear(); g_testClip.url.clear();
        g_testClip.source.clear(); g_testClip.label.clear();
        g_testClip.mask = 0;
        return ++g_testClip.changeCount;
    }
    if (!OpenWinClipboardRetry()) return -1;
    EmptyClipboard();
    CloseClipboard();
    return static_cast<jlong>(GetClipboardSequenceNumber());
}

jobjectArray JNICALL ClipRead(JNIEnv* env, jclass, jlongArray out) {
    if (!out || env->GetArrayLength(out) < 2) return nullptr;
    std::wstring parts[5];
    bool present[5]{};
    jlong count = 0;
    jint mask = 0;

    {
        std::lock_guard<std::mutex> lock(g_clipMu);
        EnsureClipboardInitLocked();
        if (g_clipTestMode) {
            present[0] = g_testClip.hasText;   parts[0] = g_testClip.text;
            present[1] = g_testClip.hasHtml;   parts[1] = g_testClip.html;
            present[2] = g_testClip.hasUrl;    parts[2] = g_testClip.url;
            present[3] = g_testClip.hasSource; parts[3] = g_testClip.source;
            present[4] = g_testClip.hasLabel;  parts[4] = g_testClip.label;
            count = g_testClip.changeCount;
            mask = g_testClip.mask;
        } else {
            if (!OpenWinClipboardRetry()) return nullptr;
            const auto& f = Formats();
            present[0] = GetWideGlobal(CF_UNICODETEXT, parts[0]);
            present[1] = f.html && GetCfHtml(f.html, parts[1]);
            present[2] = f.url && GetWideGlobal(f.url, parts[2]);
            present[3] = f.source && GetWideGlobal(f.source, parts[3]);
            present[4] = f.label && GetWideGlobal(f.label, parts[4]);
            mask = WinClipboardTypesMask();
            count = static_cast<jlong>(GetClipboardSequenceNumber());
            CloseClipboard();
        }
    }

    jclass strCls = env->FindClass("java/lang/String");
    if (!strCls) {
        env->ExceptionClear();
        return nullptr;
    }
    jobjectArray arr = env->NewObjectArray(5, strCls, nullptr);
    env->DeleteLocalRef(strCls);
    if (!arr) {
        env->ExceptionClear();
        return nullptr;
    }
    for (int i = 0; i < 5; ++i) {
        if (!present[i]) continue;
        jstring js = NewJStringWide(env, parts[i]);
        if (js) {
            env->SetObjectArrayElement(arr, i, js);
            env->DeleteLocalRef(js);
        }
    }
    const jlong vals[2] = {count, static_cast<jlong>(mask)};
    env->SetLongArrayRegion(out, 0, 2, vals);
    return arr;
}

jboolean JNICALL ClipHostFocused(JNIEnv*, jclass) {
    if (IsEmbedded()) return JNI_TRUE;
    HWND fg = GetForegroundWindow();
    if (!fg) return JNI_FALSE;
    DWORD pid = 0;
    GetWindowThreadProcessId(fg, &pid);
    return pid == GetCurrentProcessId() ? JNI_TRUE : JNI_FALSE;
}

jstring JNICALL ClipPasteboardName(JNIEnv* env, jclass) {
    std::lock_guard<std::mutex> lock(g_clipMu);
    EnsureClipboardInitLocked();
    const std::wstring name = g_clipTestMode ? g_testClip.name : L"WindowsSystemClipboard";
    return NewJStringWide(env, name);
}

// ---- DirectWrite PostScript host font lookup -------------------------------

/// Maps PostScript names to their IDWriteFont handle from the shared DirectWrite cache without opening files up front.
const std::unordered_map<std::string, ComPtr<IDWriteFont>>& HostFontFaces() {
    static const std::unordered_map<std::string, ComPtr<IDWriteFont>> index = [] {
        std::unordered_map<std::string, ComPtr<IDWriteFont>> map;
        ComPtr<IDWriteFactory> factory;
        if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                       reinterpret_cast<IUnknown**>(factory.GetAddressOf())))) {
            return map;
        }
        ComPtr<IDWriteFontCollection> coll;
        if (FAILED(factory->GetSystemFontCollection(&coll, FALSE)) || !coll) return map;
        const UINT32 famCount = coll->GetFontFamilyCount();
        for (UINT32 fi = 0; fi < famCount; ++fi) {
            ComPtr<IDWriteFontFamily> fam;
            if (FAILED(coll->GetFontFamily(fi, &fam)) || !fam) continue;
            const UINT32 fcount = fam->GetFontCount();
            for (UINT32 j = 0; j < fcount; ++j) {
                ComPtr<IDWriteFont> font;
                if (FAILED(fam->GetFont(j, &font)) || !font || font->GetSimulations() != DWRITE_FONT_SIMULATIONS_NONE)
                    continue;
                ComPtr<IDWriteLocalizedStrings> psStrings;
                BOOL exists = FALSE;
                if (FAILED(font->GetInformationalStrings(DWRITE_INFORMATIONAL_STRING_POSTSCRIPT_NAME, &psStrings, &exists)) ||
                    !exists || !psStrings || psStrings->GetCount() == 0)
                    continue;
                const UINT32 scount = psStrings->GetCount();
                for (UINT32 si = 0; si < scount; ++si) {
                    UINT32 slen = 0;
                    if (FAILED(psStrings->GetStringLength(si, &slen)) || slen == 0) continue;
                    std::wstring wps(slen + 1, L'\0');
                    if (FAILED(psStrings->GetString(si, wps.data(), slen + 1))) continue;
                    wps.resize(slen);
                    map.emplace(FromWide(wps.c_str(), wps.size()), font);
                }
            }
        }
        return map;
    }();
    return index;
}

std::wstring ResolveDWriteFontPath(IDWriteFont* font) {
    if (!font) return {};
    ComPtr<IDWriteFontFace> face;
    if (FAILED(font->CreateFontFace(&face)) || !face) return {};
    UINT32 numFiles = 0;
    if (FAILED(face->GetFiles(&numFiles, nullptr)) || numFiles != 1) return {};
    ComPtr<IDWriteFontFile> file;
    if (FAILED(face->GetFiles(&numFiles, &file)) || !file) return {};
    const void* refKey = nullptr;
    UINT32 refKeySize = 0;
    if (FAILED(file->GetReferenceKey(&refKey, &refKeySize))) return {};
    ComPtr<IDWriteFontFileLoader> loader;
    if (FAILED(file->GetLoader(&loader)) || !loader) return {};
    ComPtr<IDWriteLocalFontFileLoader> localLoader;
    if (FAILED(loader.As(&localLoader)) || !localLoader) return {};
    UINT32 pathLen = 0;
    if (FAILED(localLoader->GetFilePathLengthFromKey(refKey, refKeySize, &pathLen)) || pathLen == 0) return {};
    std::wstring wpath(pathLen + 1, L'\0');
    if (FAILED(localLoader->GetFilePathFromKey(refKey, refKeySize, wpath.data(), pathLen + 1))) return {};
    wpath.resize(pathLen);
    return wpath;
}

#define M(name, sig, fn) {const_cast<char*>(name), const_cast<char*>(sig), reinterpret_cast<void*>(fn)}
const JNINativeMethod kOsMethods[] = {
    M("xzDecompress", "([B)[B", OsXzDecompress),
    M("open", "(Ljava/lang/String;II)I", OsOpen),
    M("lseek", "(IJI)J", OsLseek),
    M("close", "(I)I", OsClose),
    M("dup", "(I)I", OsDup),
    M("dup2", "(II)I", OsDup2),
    M("sysconf", "(I)J", OsSysconf),
    M("memfdCreate", "(Ljava/lang/String;I)I", OsMemfdCreate),
    M("fstat", "(I)[J", OsFstat),
    M("ftruncate", "(IJ)I", OsFtruncate),
    M("takeRunLock", "(Ljava/lang/String;)I", OsTakeRunLock),
    M("read", "(I[BII)I", OsRead),
    M("write", "(I[BII)I", OsWrite),
    M("statvfs", "(Ljava/lang/String;)[J", OsStatvfs),
    M("fstatvfs", "(I)[J", OsFstatvfs),
    M("setenv", "(Ljava/lang/String;Ljava/lang/String;Z)I", OsSetenv),
    M("getenv", "(Ljava/lang/String;)Ljava/lang/String;", OsGetenv),
    M("unsetenv", "(Ljava/lang/String;)I", OsUnsetenv),
};

const JNINativeMethod kClipMethods[] = {
    M("nativeChangeCount", "()J", ClipChangeCount),
    M("nativeTypes", "()I", ClipTypes),
    M("nativeWrite", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Z)J", ClipWrite),
    M("nativeClear", "()J", ClipClear),
    M("nativeRead", "([J)[Ljava/lang/String;", ClipRead),
    M("nativeHostFocused", "()Z", ClipHostFocused),
    M("nativePasteboardName", "()Ljava/lang/String;", ClipPasteboardName),
};
#undef M

}  // namespace

const char* GuestEnvGet(const char* name) {
    if (!name || !*name) return nullptr;
    std::lock_guard<std::mutex> lock(g_envMu);
    EnsureEnvSeededLocked();
    auto it = g_envMap.find(name);
    return it == g_envMap.end() ? nullptr : it->second->c_str();
}

jstring JNICALL HostFontFile(JNIEnv* env, jclass, jstring postScriptName) {
    if (!postScriptName) return nullptr;
    const std::string want = Utf8(env, postScriptName);
    if (want.empty()) return nullptr;
    const auto& idx = HostFontFaces();
    const auto it = idx.find(want);
    if (it == idx.end()) return nullptr;
    const std::wstring wpath = ResolveDWriteFontPath(it->second.Get());
    return wpath.empty() ? nullptr : NewJStringWide(env, wpath);
}

void JNICALL SetSystemFontsDir(JNIEnv* env, jclass, jstring dir) {
    std::lock_guard<std::mutex> lock(g_envMu);
    g_systemFontsDir = Utf8(env, dir);
}

void JNICALL SetSystemProperty(JNIEnv* env, jclass, jstring key, jstring value) {
    if (!key) return;
    std::lock_guard<std::mutex> lock(g_envMu);
    g_sysProps[Utf8(env, key)] = Utf8(env, value);
}

void JNICALL SetSharedStorageRoot(JNIEnv* env, jclass, jstring dir) {
    std::lock_guard<std::mutex> lock(g_envMu);
    g_sharedStorageRoot = Utf8(env, dir);
}

jstring JNICALL RemapProcPath(JNIEnv* env, jclass, jstring procPath) {
    if (!procPath) return nullptr;
    std::string p = Utf8(env, procPath);
    while (p.size() > 1 && p.find("//") != std::string::npos) {
        p.replace(p.find("//"), 2, "/");
    }
    const DWORD pid = GetCurrentProcessId();
    const std::string selfPrefix = "/proc/" + std::to_string(pid) + "/";
    if (p.rfind(selfPrefix, 0) == 0) {
        p = "/proc/self/" + p.substr(selfPrefix.size());
    }
    if (p != "/proc/meminfo" && p != "/proc/self/status") return nullptr;

    wchar_t tmpDir[MAX_PATH + 1]{};
    const DWORD n = GetTempPathW(MAX_PATH, tmpDir);
    if (!n || n > MAX_PATH) return nullptr;
    const std::wstring dir = std::wstring(tmpDir) + L"winapk-proc-" + std::to_wstring(pid);
    CreateDirectoryW(dir.c_str(), nullptr);

    std::wstring outPath;
    std::string content;
    if (p == "/proc/meminfo") {
        MEMORYSTATUSEX ms{sizeof(ms)};
        if (!GlobalMemoryStatusEx(&ms)) return nullptr;
        const uint64_t totalKB = ms.ullTotalPhys / 1024;
        const uint64_t availKB = ms.ullAvailPhys / 1024;
        char buf[512]{};
        std::snprintf(buf, sizeof(buf),
            "MemTotal:       %llu kB\n"
            "MemFree:        %llu kB\n"
            "MemAvailable:   %llu kB\n"
            "Buffers:               0 kB\n"
            "Cached:                0 kB\n"
            "SwapTotal:      %llu kB\n"
            "SwapFree:       %llu kB\n",
            static_cast<unsigned long long>(totalKB),
            static_cast<unsigned long long>(availKB),
            static_cast<unsigned long long>(availKB),
            static_cast<unsigned long long>(ms.ullTotalPageFile / 1024),
            static_cast<unsigned long long>(ms.ullAvailPageFile / 1024));
        outPath = dir + L"\\meminfo";
        content = buf;
    } else {
        PROCESS_MEMORY_COUNTERS_EX pmc{sizeof(pmc)};
        GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc));
        char buf[512]{};
        std::snprintf(buf, sizeof(buf),
            "Name:\twinapk\n"
            "State:\tR (running)\n"
            "Pid:\t%lu\n"
            "VmRSS:\t%llu kB\n"
            "VmSize:\t%llu kB\n",
            pid,
            static_cast<unsigned long long>(pmc.WorkingSetSize / 1024),
            static_cast<unsigned long long>(pmc.PrivateUsage / 1024));
        outPath = dir + L"\\status";
        content = buf;
    }

    std::ofstream f(outPath, std::ios::binary | std::ios::trunc);
    if (!f) return nullptr;
    f.write(content.data(), static_cast<std::streamsize>(content.size()));
    f.close();
    return NewJStringWide(env, outPath);
}

jboolean JNICALL OpenHostUrl(JNIEnv* env, jclass, jstring jurl) {
    if (!jurl) return JNI_FALSE;
    const std::string u = Utf8(env, jurl);
    if (u.empty() || u.size() > 4096) return JNI_FALSE;
    for (unsigned char c : u) {
        if (c <= 0x20 || c == 0x7f) return JNI_FALSE;
    }
    std::string lower;
    lower.resize(std::min<size_t>(u.size(), 10));
    std::transform(u.begin(), u.begin() + static_cast<ptrdiff_t>(lower.size()), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    const bool allowed = lower.rfind("https://", 0) == 0 ||
                         lower.rfind("http://", 0) == 0 ||
                         lower.rfind("mailto:", 0) == 0;
    if (!allowed) {
        LogLine("[winapk] host: refused non-web/mailto URI in openHostUrl");
        return JNI_FALSE;
    }
    wchar_t capturePath[MAX_PATH * 2]{};
    const DWORD capLen = GetEnvironmentVariableW(L"WINAPK_TEST_CAPTURE_URL", capturePath, ARRAYSIZE(capturePath));
    if (capLen > 0 && capLen < ARRAYSIZE(capturePath)) {
        std::ofstream out(std::wstring(capturePath, capLen), std::ios::binary | std::ios::trunc);
        if (!out) return JNI_FALSE;
        out << u << "\n";
        return JNI_TRUE;
    }
    const std::wstring wu = ToWide(u);
    const INT_PTR rc = reinterpret_cast<INT_PTR>(
        ShellExecuteW(nullptr, L"open", wu.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
    return rc > 32 ? JNI_TRUE : JNI_FALSE;
}

bool RegisterHostServices(JNIEnv* env) {
    const bool osOk = Bind(env, "com/macapk/loader/NativeOs", kOsMethods, static_cast<int>(std::size(kOsMethods)));
    // ClipboardManager$MacApkHostPasteboard is optional when running minimal probe classpaths.
    jclass clipCls = env->FindClass("android/content/ClipboardManager$MacApkHostPasteboard");
    if (clipCls) {
        env->DeleteLocalRef(clipCls);
        Bind(env, "android/content/ClipboardManager$MacApkHostPasteboard",
             kClipMethods, static_cast<int>(std::size(kClipMethods)));
    } else {
        env->ExceptionClear();
    }
    return osOk;
}

}  // namespace winapk::bridge
