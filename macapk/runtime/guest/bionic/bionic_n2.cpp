// bionic_n2.cpp -- Profile N2 Bionic (`libc.so`, `libdl.so`, `libm.so`) engine runtime.
#define _CRT_RAND_S
#include "bionic_n2.h"
#include "threads.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#pragma comment(lib, "synchronization.lib")

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cwchar>
#include <cwctype>
#include <deque>
#include <filesystem>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <regex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace winapk::guest {
namespace {

// ---- Linux/Android ARM64 errno constants ----
constexpr int kEperm = 1, kEnoent = 2, kEintr = 4, kEio = 5;
constexpr int kEbadf = 9, kEagain = 11, kEnomem = 12, kEacces = 13, kEfault = 14;
constexpr int kEexist = 17, kEnotdir = 20, kEisdir = 21;
constexpr int kEinval = 22, kEmfile = 24, kEnotty = 25, kEnospc = 28, kEspipe = 29;
constexpr int kEpipe = 32, kErange = 34, kEnosys = 38, kEnotempty = 39;
constexpr int kEnotsock = 88, kEopnotsupp = 95, kEafnosupp = 97, kEaddrinuse = 98, kEaddrnotavail = 99;
constexpr int kEnetunreach = 101, kEconnreset = 104, kEnobufs = 105, kEisconn = 106, kEnotconn = 107;
constexpr int kEtimedout = 110, kEconnrefused = 111, kEinprogress = 115;

int SetErr(int e) {
    *GuestErrnoPtr() = e;
    return -1;
}
int64_t SetErr64(int e) {
    *GuestErrnoPtr() = e;
    return -1;
}

int MapWinError(DWORD err) {
    switch (err) {
        case ERROR_FILE_NOT_FOUND: case ERROR_PATH_NOT_FOUND: return kEnoent;
        case ERROR_ACCESS_DENIED: case ERROR_SHARING_VIOLATION: return kEacces;
        case ERROR_ALREADY_EXISTS: case ERROR_FILE_EXISTS: return kEexist;
        case ERROR_INVALID_HANDLE: return kEbadf;
        case ERROR_NOT_ENOUGH_MEMORY: case ERROR_OUTOFMEMORY: return kEnomem;
        case ERROR_DIR_NOT_EMPTY: return kEnotempty;
        case ERROR_DIRECTORY: return kEnotdir;
        case ERROR_DISK_FULL: return kEnospc;
        case ERROR_BROKEN_PIPE: case ERROR_NO_DATA: return kEpipe;
        default: return kEio;
    }
}

int MapWsaError(int wsa) {
    switch (wsa) {
        case WSAEWOULDBLOCK: return kEagain;
        case WSAEINPROGRESS: return kEinprogress;
        case WSAEALREADY: return kEinprogress;
        case WSAENOTSOCK: return kEnotsock;
        case WSAEAFNOSUPPORT: return kEafnosupp;
        case WSAEADDRINUSE: return kEaddrinuse;
        case WSAEADDRNOTAVAIL: return kEaddrnotavail;
        case WSAENETUNREACH: case WSAEHOSTUNREACH: return kEnetunreach;
        case WSAECONNRESET: case WSAECONNABORTED: return kEconnreset;
        case WSAENOBUFS: return kEnobufs;
        case WSAEISCONN: return kEisconn;
        case WSAENOTCONN: return kEnotconn;
        case WSAETIMEDOUT: return kEtimedout;
        case WSAECONNREFUSED: return kEconnrefused;
        case WSAEACCES: return kEacces;
        case WSAEINVAL: return kEinval;
        case WSAEFAULT: return kEfault;
        case WSAEMFILE: return kEmfile;
        case WSAEOPNOTSUPP: return kEopnotsupp;
        default: return kEio;
    }
}

uint64_t MonoNowNs() {
    ULONGLONG ticks = 0;
    QueryUnbiasedInterruptTimePrecise(&ticks);
    return static_cast<uint64_t>(ticks) * 100ull;
}

struct GuestTimespec { int64_t sec, nsec; };
struct GuestTimeval { int64_t sec, usec; };
struct GuestItimerspec { GuestTimespec interval, value; };

GuestTimespec FiletimeToUnix(FILETIME ft) {
    const int64_t ticks = static_cast<int64_t>((static_cast<uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime) - 116444736000000000LL;
    int64_t sec = ticks / 10000000LL;
    int64_t rem = ticks % 10000000LL;
    if (rem < 0) { sec--; rem += 10000000LL; }
    return {sec, rem * 100LL};
}

struct GuestStat64 {
    uint64_t st_dev, st_ino;
    uint32_t st_mode, st_nlink, st_uid, st_gid;
    uint64_t st_rdev, __pad1;
    int64_t st_size;
    int32_t st_blksize, __pad2;
    int64_t st_blocks;
    GuestTimespec st_atim, st_mtim, st_ctim;
    uint32_t __unused4, __unused5;
};
static_assert(sizeof(GuestStat64) == 128);

// ---- Wakeup Registry (shared with `ALooper` & `epoll_wait`) ----
std::mutex g_wakeMu;
std::condition_variable g_wakeCv;
uint64_t g_wakeGen = 1;
uint64_t g_nextListenerId = 1;
std::map<uint64_t, std::function<void()>> g_wakeListeners;

void BroadcastFdWake() {
    std::vector<std::function<void()>> callbacks;
    {
        std::lock_guard lock(g_wakeMu);
        ++g_wakeGen;
        for (const auto& [id, cb] : g_wakeListeners) callbacks.push_back(cb);
    }
    g_wakeCv.notify_all();
    for (const auto& cb : callbacks) cb();
}

// ---- Path Sandbox & Virtual `/proc` / `/dev` Resolution ----
std::wstring g_n2Root;
std::mutex g_cwdMu;
std::wstring g_cwd;

std::string WideToUtf8(const std::wstring& w) {
    if (w.empty()) return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), s.data(), n, nullptr, nullptr);
    return s;
}

std::wstring Utf8ToWide(std::string_view s) {
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    if (n <= 0) return {};
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
    return w;
}

enum class SpecialPath { None, DevNull, DevUrandom, ProcCpuinfo, ProcMeminfo, ProcMaps, EtcHosts, EtcFontsXml };

const std::wstring& HostWindowsFontsDir() {
    static const std::wstring kDir = [] {
        wchar_t buf[MAX_PATH]{};
        const UINT n = GetSystemWindowsDirectoryW(buf, MAX_PATH);
        std::wstring base = (n > 0 && n < MAX_PATH) ? std::wstring(buf, n) : L"C:\\Windows";
        if (!base.empty() && base.back() == L'\\') base.pop_back();
        return base + L"\\Fonts";
    }();
    return kDir;
}

bool IsReadOnlyHostFontsPath(const std::wstring& hostPath) {
    const std::wstring& fontsDir = HostWindowsFontsDir();
    if (hostPath.size() < fontsDir.size()) return false;
    if (_wcsnicmp(hostPath.c_str(), fontsDir.c_str(), fontsDir.size()) != 0) return false;
    return hostPath.size() == fontsDir.size() || hostPath[fontsDir.size()] == L'\\';
}

std::wstring MapAndroidFontLeaf(std::wstring_view leaf) {
    static constexpr std::pair<std::wstring_view, std::wstring_view> kAliases[] = {
        {L"Roboto-Regular.ttf", L"segoeui.ttf"},
        {L"Roboto-Bold.ttf", L"segoeuib.ttf"},
        {L"Roboto-Italic.ttf", L"segoeuii.ttf"},
        {L"Roboto-BoldItalic.ttf", L"segoeuiz.ttf"},
        {L"Roboto-Light.ttf", L"segoeuil.ttf"},
        {L"Roboto-Medium.ttf", L"segoeuisl.ttf"},
        {L"DroidSans.ttf", L"segoeui.ttf"},
        {L"DroidSans-Bold.ttf", L"segoeuib.ttf"},
        {L"DroidSansMono.ttf", L"consola.ttf"},
        {L"NotoSerif-Regular.ttf", L"times.ttf"},
        {L"NotoColorEmoji.ttf", L"seguiemj.ttf"},
        {L"NotoSansCJK-Regular.ttc", L"msyh.ttc"},
    };
    for (const auto& [src, dst] : kAliases) {
        if (_wcsicmp(std::wstring(leaf).c_str(), std::wstring(src).c_str()) == 0) {
            return std::wstring(dst);
        }
    }
    return std::wstring(leaf);
}

bool ResolveGuestPath(const char* raw, std::wstring& hostOut, SpecialPath& specialOut) {
    specialOut = SpecialPath::None;
    if (!raw) { SetErr(kEfault); return false; }
    std::string_view sv(raw);
    if (sv.empty()) { SetErr(kEnoent); return false; }
    if (sv == "/dev/null") { specialOut = SpecialPath::DevNull; return true; }
    if (sv == "/dev/urandom" || sv == "/dev/random") { specialOut = SpecialPath::DevUrandom; return true; }
    if (sv == "/proc/cpuinfo" || sv == "/sys/devices/system/cpu/online" || sv == "/sys/devices/system/cpu/present") {
        specialOut = SpecialPath::ProcCpuinfo;
        return true;
    }
    if (sv == "/proc/meminfo") { specialOut = SpecialPath::ProcMeminfo; return true; }
    if (sv == "/proc/self/maps" || sv == "/proc/self/smaps") { specialOut = SpecialPath::ProcMaps; return true; }
    if (sv == "/system/etc/hosts" || sv == "/etc/hosts") { specialOut = SpecialPath::EtcHosts; return true; }
    if (sv == "/system/etc/fonts.xml" || sv == "/system/etc/system_fonts.xml") {
        specialOut = SpecialPath::EtcFontsXml;
        return true;
    }
    if (sv == "/system/fonts" || sv == "/system/fonts/" || sv == "/fonts" || sv == "/fonts/") {
        hostOut = HostWindowsFontsDir();
        return true;
    }
    if (sv.starts_with("/system/fonts/") || sv.starts_with("/fonts/")) {
        std::string_view rest = sv.starts_with("/system/fonts/") ? sv.substr(14) : sv.substr(7);
        while (!rest.empty() && rest.front() == '/') rest.remove_prefix(1);
        while (!rest.empty() && (rest.back() == ' ' || rest.back() == '\n' || rest.back() == '\r')) {
            rest.remove_suffix(1);
        }
        if (rest.empty() || rest.find('/') != std::string_view::npos || rest.find('\\') != std::string_view::npos ||
            rest == "." || rest == "..") {
            SetErr(kEnoent);
            return false;
        }
        hostOut = HostWindowsFontsDir() + L"\\" + MapAndroidFontLeaf(Utf8ToWide(rest));
        return true;
    }

    std::wstring w = Utf8ToWide(sv);
    if (w.empty()) { SetErr(kEinval); return false; }
    for (wchar_t& ch : w) if (ch == L'/') ch = L'\\';

    // Host absolute paths inside `g_n2Root`, staged guest library dir, or temp/app dir:
    if ((w.size() >= 2 && w[1] == L':') || w.starts_with(L"\\\\?\\")) {
        std::error_code ec;
        auto norm = std::filesystem::weakly_canonical(std::filesystem::path(w), ec);
        hostOut = ec ? w : norm.wstring();
        return true;
    }

    // Android virtual paths (`/data/user/0/<pkg>/...` -> `<root>\data\data\<pkg>\...`, `/sdcard/...`, `/data/...`)
    std::wstring sub = w;
    if (sub.starts_with(L"\\data\\user\\0\\")) {
        sub = L"\\data\\data\\" + sub.substr(13);
    } else if (sub.starts_with(L"\\storage\\emulated\\0\\")) {
        sub = L"\\sdcard\\" + sub.substr(20);
    } else if (sub == L"\\storage\\emulated\\0") {
        sub = L"\\sdcard";
    }
    if (!sub.empty() && sub[0] == L'\\') {
        if (g_n2Root.empty()) { SetErr(kEacces); return false; }
        std::wstring joined = g_n2Root;
        if (joined.back() == L'\\') joined.pop_back();
        joined += sub;
        hostOut = joined;
        return true;
    }

    // Relative path against guest CWD:
    std::wstring base;
    {
        std::lock_guard lock(g_cwdMu);
        base = g_cwd.empty() ? g_n2Root : g_cwd;
    }
    if (base.empty()) { SetErr(kEacces); return false; }
    if (base.back() != L'\\') base += L'\\';
    hostOut = base + sub;
    return true;
}

std::string GenerateVirtualContent(SpecialPath sp) {
    if (sp == SpecialPath::ProcCpuinfo) {
        SYSTEM_INFO si{};
        GetSystemInfo(&si);
        const uint32_t cores = std::max<DWORD>(1, si.dwNumberOfProcessors);
        // Note: `/sys/devices/system/cpu/online` expects `0-(N-1)\n` on first line if parsed,
        // while `/proc/cpuinfo` expects `processor : i`. Provide standard ARMv8 `/proc/cpuinfo`.
        std::string s;
        for (uint32_t i = 0; i < cores; ++i) {
            s += "processor\t: " + std::to_string(i) + "\n";
            s += "BogoMIPS\t: 38.40\n";
            s += "Features\t: fp asimd evtstrm aes pmull sha1 sha2 crc32 atomics fphp asimdhp\n";
            s += "CPU implementer\t: 0x51\nCPU architecture: 8\n\n";
        }
        return s;
    }
    if (sp == SpecialPath::ProcMeminfo) {
        MEMORYSTATUSEX ms{sizeof(ms)};
        GlobalMemoryStatusEx(&ms);
        const uint64_t totalKb = ms.ullTotalPhys / 1024ull;
        const uint64_t availKb = ms.ullAvailPhys / 1024ull;
        char buf[256];
        std::snprintf(buf, sizeof(buf),
                      "MemTotal:       %llu kB\nMemFree:        %llu kB\nMemAvailable:   %llu kB\n",
                      static_cast<unsigned long long>(totalKb),
                      static_cast<unsigned long long>(availKb),
                      static_cast<unsigned long long>(availKb));
        return buf;
    }
    if (sp == SpecialPath::EtcHosts) {
        return "127.0.0.1 localhost\n::1 ip6-localhost\n";
    }
    if (sp == SpecialPath::EtcFontsXml) {
        return "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
               "<familyset version=\"22\">\n"
               "  <family name=\"sans-serif\">\n"
               "    <font weight=\"300\" style=\"normal\">segoeuil.ttf</font>\n"
               "    <font weight=\"400\" style=\"normal\">segoeui.ttf</font>\n"
               "    <font weight=\"400\" style=\"italic\">segoeuii.ttf</font>\n"
               "    <font weight=\"600\" style=\"normal\">segoeuisl.ttf</font>\n"
               "    <font weight=\"700\" style=\"normal\">segoeuib.ttf</font>\n"
               "    <font weight=\"700\" style=\"italic\">segoeuiz.ttf</font>\n"
               "  </family>\n"
               "  <alias name=\"roboto\" to=\"sans-serif\" />\n"
               "  <alias name=\"arial\" to=\"sans-serif\" />\n"
               "  <alias name=\"helvetica\" to=\"sans-serif\" />\n"
               "  <family name=\"serif\">\n"
               "    <font weight=\"400\" style=\"normal\">times.ttf</font>\n"
               "  </family>\n"
               "  <family name=\"monospace\">\n"
               "    <font weight=\"400\" style=\"normal\">consola.ttf</font>\n"
               "    <font weight=\"400\" style=\"italic\">consolai.ttf</font>\n"
               "    <font weight=\"700\" style=\"normal\">consolab.ttf</font>\n"
               "    <font weight=\"700\" style=\"italic\">consolaz.ttf</font>\n"
               "  </family>\n"
               "  <family lang=\"zh-Hans\">\n"
               "    <font weight=\"400\" style=\"normal\" index=\"0\">msyh.ttc</font>\n"
               "    <font weight=\"700\" style=\"normal\" index=\"0\">msyhbd.ttc</font>\n"
               "  </family>\n"
               "  <family lang=\"zh-Hant\">\n"
               "    <font weight=\"400\" style=\"normal\" index=\"0\">msyh.ttc</font>\n"
               "  </family>\n"
               "  <family lang=\"ja\">\n"
               "    <font weight=\"400\" style=\"normal\" index=\"0\">YuGothR.ttc</font>\n"
               "    <font weight=\"700\" style=\"normal\" index=\"0\">YuGothB.ttc</font>\n"
               "  </family>\n"
               "  <family lang=\"ko\">\n"
               "    <font weight=\"400\" style=\"normal\">malgun.ttf</font>\n"
               "    <font weight=\"700\" style=\"normal\">malgunbd.ttf</font>\n"
               "  </family>\n"
               "  <family lang=\"und-Zsye\">\n"
               "    <font weight=\"400\" style=\"normal\">seguiemj.ttf</font>\n"
               "  </family>\n"
               "</familyset>\n";
    }
    return "";
}

// ---- Unified Virtual FD Table ----
enum class FdKind { DiskFile, VirtualBytes, DevNull, DevUrandom, PipeRead, PipeWrite, EventFd, TimerFd, Inotify, Epoll, Socket };

struct PipeShared {
    std::mutex mu;
    std::deque<uint8_t> buf;
    int readers = 1;
    int writers = 1;
};

struct EventFdState {
    std::mutex mu;
    uint64_t counter = 0;
    bool semaphore = false;
};

struct TimerFdState {
    std::mutex mu;
    uint64_t nextFireNs = 0; // 0 = disarmed
    uint64_t intervalNs = 0;
    uint64_t consumedExpirations = 0;
};

struct EpollEntry {
    int fd = -1;
    uint32_t events = 0;
    uint64_t data = 0;
};

struct FdEntry {
    FdKind kind = FdKind::DiskFile;
    bool nonblock = false;
    bool append = false;
    HANDLE winHandle = INVALID_HANDLE_VALUE;
    SOCKET sock = INVALID_SOCKET;
    std::string virtBytes;
    size_t virtPos = 0;
    std::shared_ptr<PipeShared> pipe;
    std::shared_ptr<EventFdState> evfd;
    std::shared_ptr<TimerFdState> tfd;
    std::map<int, EpollEntry> epollSet;

    ~FdEntry() {
        if (winHandle != INVALID_HANDLE_VALUE) CloseHandle(winHandle);
        if (sock != INVALID_SOCKET) closesocket(sock);
        if (kind == FdKind::PipeRead && pipe) {
            { std::lock_guard lk(pipe->mu); --pipe->readers; }
            BroadcastFdWake();
        } else if (kind == FdKind::PipeWrite && pipe) {
            { std::lock_guard lk(pipe->mu); --pipe->writers; }
            BroadcastFdWake();
        }
    }
};

std::mutex g_fdMu;
int g_nextN2Fd = 100;
std::map<int, std::shared_ptr<FdEntry>> g_fds;

int AllocFd(std::shared_ptr<FdEntry> entry) {
    std::lock_guard lock(g_fdMu);
    while (g_fds.contains(g_nextN2Fd)) ++g_nextN2Fd;
    const int fd = g_nextN2Fd++;
    g_fds[fd] = std::move(entry);
    return fd;
}

std::shared_ptr<FdEntry> GetFd(int fd) {
    std::lock_guard lock(g_fdMu);
    auto it = g_fds.find(fd);
    return it != g_fds.end() ? it->second : nullptr;
}

void EnsureWinsock() {
    static std::once_flag once;
    std::call_once(once, [] {
        WSADATA wsa{};
        WSAStartup(MAKEWORD(2, 2), &wsa);
    });
}

// Evaluates current EPOLL* readiness mask for `e` (1=IN, 4=OUT, 8=ERR, 16=HUP).
uint32_t EvalEntryEpollMask(FdEntry& e, uint64_t nowNs, uint64_t* earliestDeadlineOut = nullptr) {
    switch (e.kind) {
        case FdKind::DiskFile:
        case FdKind::VirtualBytes:
        case FdKind::DevNull:
        case FdKind::DevUrandom:
            return 1u | 4u; // EPOLLIN | EPOLLOUT
        case FdKind::PipeRead: {
            if (!e.pipe) return 16u;
            std::lock_guard lk(e.pipe->mu);
            uint32_t m = 0;
            if (!e.pipe->buf.empty()) m |= 1u;
            if (e.pipe->writers <= 0) m |= 16u;
            return m;
        }
        case FdKind::PipeWrite: {
            if (!e.pipe) return 8u | 16u;
            std::lock_guard lk(e.pipe->mu);
            if (e.pipe->readers <= 0) return 8u | 16u;
            return e.pipe->buf.size() < 262144 ? 4u : 0u;
        }
        case FdKind::EventFd: {
            if (!e.evfd) return 8u;
            std::lock_guard lk(e.evfd->mu);
            uint32_t m = 4u;
            if (e.evfd->counter > 0) m |= 1u;
            return m;
        }
        case FdKind::TimerFd: {
            if (!e.tfd) return 8u;
            std::lock_guard lk(e.tfd->mu);
            if (e.tfd->nextFireNs == 0) return 0u;
            if (nowNs >= e.tfd->nextFireNs) return 1u;
            if (earliestDeadlineOut && e.tfd->nextFireNs < *earliestDeadlineOut) {
                *earliestDeadlineOut = e.tfd->nextFireNs;
            }
            return 0u;
        }
        case FdKind::Inotify:
            return 0u;
        case FdKind::Socket: {
            if (e.sock == INVALID_SOCKET) return 8u;
            WSAPOLLFD pfd{};
            pfd.fd = e.sock;
            pfd.events = POLLRDNORM | POLLWRNORM;
            const int rc = WSAPoll(&pfd, 1, 0);
            if (rc <= 0) return 0u;
            uint32_t m = 0;
            if (pfd.revents & (POLLRDNORM | POLLRDBAND)) m |= 1u;
            if (pfd.revents & POLLWRNORM) m |= 4u;
            if (pfd.revents & POLLERR) m |= 8u;
            if (pfd.revents & POLLHUP) m |= 16u;
            return m;
        }
        case FdKind::Epoll:
            return 0u;
    }
    return 0u;
}

// ---- File & Descriptor Operations ----

extern "C" int N2OpenImpl(const char* path, int flags, uint32_t mode) {
    (void)mode;
    SpecialPath sp = SpecialPath::None;
    std::wstring hostPath;
    if (!ResolveGuestPath(path, hostPath, sp)) return -1;

    if (sp == SpecialPath::DevNull || sp == SpecialPath::DevUrandom) {
        auto ent = std::make_shared<FdEntry>();
        ent->kind = sp == SpecialPath::DevNull ? FdKind::DevNull : FdKind::DevUrandom;
        ent->nonblock = (flags & 04000 /* O_NONBLOCK */) != 0;
        return AllocFd(std::move(ent));
    }
    if (sp != SpecialPath::None) {
        auto ent = std::make_shared<FdEntry>();
        ent->kind = FdKind::VirtualBytes;
        if (sp == SpecialPath::ProcCpuinfo && std::string_view(path).starts_with("/sys/")) {
            SYSTEM_INFO si{};
            GetSystemInfo(&si);
            ent->virtBytes = "0-" + std::to_string(std::max<DWORD>(1, si.dwNumberOfProcessors) - 1) + "\n";
        } else {
            ent->virtBytes = GenerateVirtualContent(sp);
        }
        return AllocFd(std::move(ent));
    }

    const int acc = flags & 3;
    if (IsReadOnlyHostFontsPath(hostPath) && (acc != 0 || (flags & (0100 /* O_CREAT */ | 01000 /* O_TRUNC */)) != 0)) {
        return SetErr(kEacces);
    }
    const bool wantDir = (flags & 040000 /* O_DIRECTORY ARM64 */) != 0 || (flags & 0200000) != 0;
    DWORD desired = 0;
    if (acc == 0) desired = GENERIC_READ;
    else if (acc == 1) desired = GENERIC_WRITE;
    else desired = GENERIC_READ | GENERIC_WRITE;

    DWORD disp = OPEN_EXISTING;
    if (flags & 0100 /* O_CREAT */) {
        if (flags & 0200 /* O_EXCL */) disp = CREATE_NEW;
        else if (flags & 01000 /* O_TRUNC */) disp = CREATE_ALWAYS;
        else disp = OPEN_ALWAYS;
    } else if (flags & 01000 /* O_TRUNC */) {
        disp = TRUNCATE_EXISTING;
    }

    DWORD attrFlags = FILE_ATTRIBUTE_NORMAL;
    const DWORD existingAttrs = GetFileAttributesW(hostPath.c_str());
    if (wantDir || (existingAttrs != INVALID_FILE_ATTRIBUTES && (existingAttrs & FILE_ATTRIBUTE_DIRECTORY))) {
        if (acc != 0) return SetErr(kEisdir);
        desired = FILE_LIST_DIRECTORY | FILE_READ_ATTRIBUTES;
        disp = OPEN_EXISTING;
        attrFlags = FILE_FLAG_BACKUP_SEMANTICS;
    }

    HANDLE h = CreateFileW(hostPath.c_str(), desired,
                           FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           nullptr, disp, attrFlags, nullptr);
    if (h == INVALID_HANDLE_VALUE) return SetErr(MapWinError(GetLastError()));

    auto ent = std::make_shared<FdEntry>();
    ent->kind = FdKind::DiskFile;
    ent->winHandle = h;
    ent->append = (flags & 02000 /* O_APPEND */) != 0;
    ent->nonblock = (flags & 04000 /* O_NONBLOCK */) != 0;
    return AllocFd(std::move(ent));
}

int N2OpenChecked(const char* path, int flags) {
    return N2OpenImpl(path, flags, 0600);
}

extern "C" int N2OpenAtImpl(int dirfd, const char* path, int flags, uint32_t mode) {
    if (!path) return SetErr(kEfault);
    if (path[0] == '/' || dirfd == -100 /* AT_FDCWD */) {
        return N2OpenImpl(path, flags, mode);
    }
    auto dirEnt = GetFd(dirfd);
    if (!dirEnt || dirEnt->kind != FdKind::DiskFile) return SetErr(kEbadf);
    wchar_t dirBuf[4096]{};
    const DWORD len = GetFinalPathNameByHandleW(dirEnt->winHandle, dirBuf, 4096, FILE_NAME_NORMALIZED);
    if (!len || len >= 4096) return SetErr(kEbadf);
    std::wstring joined = std::wstring(dirBuf, len) + L"\\" + Utf8ToWide(path);
    return N2OpenImpl(WideToUtf8(joined).c_str(), flags, mode);
}

int N2OpenAtChecked(int dirfd, const char* path, int flags) {
    return N2OpenAtImpl(dirfd, path, flags, 0600);
}

int64_t N2Read(int fd, void* buf, uint64_t count) {
    if (count == 0) return 0;
    if (!buf) return SetErr64(kEfault);
    if (fd == 0) return 0; // stdin EOF
    auto ent = GetFd(fd);
    if (!ent) return SetErr64(kEbadf);

    switch (ent->kind) {
        case FdKind::DevNull:
            return 0;
        case FdKind::DevUrandom: {
            auto* dst = static_cast<uint8_t*>(buf);
            size_t i = 0;
            while (i < count) {
                unsigned int rnd = 0;
                rand_s(&rnd);
                const size_t chunk = std::min<size_t>(4, static_cast<size_t>(count) - i);
                std::memcpy(dst + i, &rnd, chunk);
                i += chunk;
            }
            return static_cast<int64_t>(count);
        }
        case FdKind::VirtualBytes: {
            const size_t rem = ent->virtPos < ent->virtBytes.size() ? ent->virtBytes.size() - ent->virtPos : 0;
            const size_t n = std::min<size_t>(rem, static_cast<size_t>(count));
            if (n > 0) {
                std::memcpy(buf, ent->virtBytes.data() + ent->virtPos, n);
                ent->virtPos += n;
            }
            return static_cast<int64_t>(n);
        }
        case FdKind::DiskFile: {
            DWORD got = 0;
            const DWORD ask = static_cast<DWORD>(std::min<uint64_t>(count, 0x7fffffffull));
            if (!ReadFile(ent->winHandle, buf, ask, &got, nullptr)) {
                const DWORD err = GetLastError();
                if (err == ERROR_HANDLE_EOF) return 0;
                return SetErr64(MapWinError(err));
            }
            return static_cast<int64_t>(got);
        }
        case FdKind::PipeRead: {
            if (!ent->pipe) return 0;
            for (;;) {
                {
                    std::lock_guard lk(ent->pipe->mu);
                    if (!ent->pipe->buf.empty()) {
                        const size_t n = std::min<size_t>(ent->pipe->buf.size(), static_cast<size_t>(count));
                        auto* dst = static_cast<uint8_t*>(buf);
                        for (size_t i = 0; i < n; ++i) {
                            dst[i] = ent->pipe->buf.front();
                            ent->pipe->buf.pop_front();
                        }
                        return static_cast<int64_t>(n);
                    }
                    if (ent->pipe->writers <= 0) return 0;
                    if (ent->nonblock) return SetErr64(kEagain);
                }
                std::unique_lock wlk(g_wakeMu);
                g_wakeCv.wait_for(wlk, std::chrono::milliseconds(10));
            }
        }
        case FdKind::EventFd: {
            if (count < 8) return SetErr64(kEinval);
            for (;;) {
                {
                    std::lock_guard lk(ent->evfd->mu);
                    if (ent->evfd->counter > 0) {
                        uint64_t val = ent->evfd->semaphore ? 1 : ent->evfd->counter;
                        ent->evfd->counter -= val;
                        std::memcpy(buf, &val, 8);
                        return 8;
                    }
                    if (ent->nonblock) return SetErr64(kEagain);
                }
                std::unique_lock wlk(g_wakeMu);
                g_wakeCv.wait_for(wlk, std::chrono::milliseconds(10));
            }
        }
        case FdKind::TimerFd: {
            if (count < 8) return SetErr64(kEinval);
            for (;;) {
                const uint64_t now = MonoNowNs();
                {
                    std::lock_guard lk(ent->tfd->mu);
                    if (ent->tfd->nextFireNs != 0 && now >= ent->tfd->nextFireNs) {
                        uint64_t exp = 1;
                        if (ent->tfd->intervalNs > 0) {
                            const uint64_t elapsed = now - ent->tfd->nextFireNs;
                            const uint64_t steps = elapsed / ent->tfd->intervalNs + 1;
                            exp = steps;
                            ent->tfd->nextFireNs += steps * ent->tfd->intervalNs;
                        } else {
                            ent->tfd->nextFireNs = 0;
                        }
                        std::memcpy(buf, &exp, 8);
                        return 8;
                    }
                    if (ent->nonblock || ent->tfd->nextFireNs == 0) return SetErr64(kEagain);
                }
                std::unique_lock wlk(g_wakeMu);
                g_wakeCv.wait_for(wlk, std::chrono::milliseconds(2));
            }
        }
        case FdKind::Socket: {
            const int rc = ::recv(ent->sock, static_cast<char*>(buf),
                                  static_cast<int>(std::min<uint64_t>(count, 0x7fffffffull)), 0);
            if (rc == SOCKET_ERROR) return SetErr64(MapWsaError(WSAGetLastError()));
            return rc;
        }
        default:
            return SetErr64(kEinval);
    }
}

int64_t N2ReadChk(int fd, void* buf, uint64_t count, uint64_t buflen) {
    return N2Read(fd, buf, std::min(count, buflen));
}

int64_t N2Pread(int fd, void* buf, uint64_t count, int64_t offset) {
    if (offset < 0) return SetErr64(kEinval);
    if (count == 0) return 0;
    if (!buf) return SetErr64(kEfault);
    auto ent = GetFd(fd);
    if (!ent) return SetErr64(kEbadf);
    if (ent->kind == FdKind::VirtualBytes) {
        const auto off = static_cast<size_t>(offset);
        if (off >= ent->virtBytes.size()) return 0;
        const size_t n = std::min<size_t>(ent->virtBytes.size() - off, static_cast<size_t>(count));
        std::memcpy(buf, ent->virtBytes.data() + off, n);
        return static_cast<int64_t>(n);
    }
    if (ent->kind != FdKind::DiskFile) return SetErr64(kEspipe);
    OVERLAPPED ov{};
    ov.Offset = static_cast<DWORD>(static_cast<uint64_t>(offset) & 0xffffffffull);
    ov.OffsetHigh = static_cast<DWORD>(static_cast<uint64_t>(offset) >> 32);
    DWORD got = 0;
    if (!ReadFile(ent->winHandle, buf, static_cast<DWORD>(std::min<uint64_t>(count, 0x7fffffffull)), &got, &ov)) {
        const DWORD err = GetLastError();
        if (err == ERROR_HANDLE_EOF) return 0;
        return SetErr64(MapWinError(err));
    }
    return static_cast<int64_t>(got);
}

int64_t N2PreadChk(int fd, void* buf, uint64_t count, int64_t offset, uint64_t buflen) {
    return N2Pread(fd, buf, std::min(count, buflen), offset);
}

int64_t N2Write(int fd, const void* buf, uint64_t count) {
    if (count == 0) return 0;
    if (!buf) return SetErr64(kEfault);
    if (fd == 1 || fd == 2) {
        std::fwrite(buf, 1, static_cast<size_t>(std::min<uint64_t>(count, 65536)), stderr);
        std::fflush(stderr);
        return static_cast<int64_t>(count);
    }
    auto ent = GetFd(fd);
    if (!ent) return SetErr64(kEbadf);

    switch (ent->kind) {
        case FdKind::DevNull:
        case FdKind::DevUrandom:
            return static_cast<int64_t>(count);
        case FdKind::DiskFile: {
            OVERLAPPED ov{};
            ov.Offset = ov.OffsetHigh = MAXDWORD;
            DWORD put = 0;
            if (!WriteFile(ent->winHandle, buf, static_cast<DWORD>(std::min<uint64_t>(count, 0x7fffffffull)),
                           &put, ent->append ? &ov : nullptr)) {
                return SetErr64(MapWinError(GetLastError()));
            }
            return static_cast<int64_t>(put);
        }
        case FdKind::PipeWrite: {
            if (!ent->pipe) return SetErr64(kEpipe);
            {
                std::lock_guard lk(ent->pipe->mu);
                if (ent->pipe->readers <= 0) return SetErr64(kEpipe);
                const auto* src = static_cast<const uint8_t*>(buf);
                for (uint64_t i = 0; i < count; ++i) ent->pipe->buf.push_back(src[i]);
            }
            BroadcastFdWake();
            return static_cast<int64_t>(count);
        }
        case FdKind::EventFd: {
            if (count < 8) return SetErr64(kEinval);
            uint64_t add = 0;
            std::memcpy(&add, buf, 8);
            if (add == UINT64_MAX) return SetErr64(kEinval);
            {
                std::lock_guard lk(ent->evfd->mu);
                ent->evfd->counter += add;
            }
            BroadcastFdWake();
            return 8;
        }
        case FdKind::Socket: {
            const int rc = ::send(ent->sock, static_cast<const char*>(buf),
                                  static_cast<int>(std::min<uint64_t>(count, 0x7fffffffull)), 0);
            if (rc == SOCKET_ERROR) return SetErr64(MapWsaError(WSAGetLastError()));
            return rc;
        }
        default:
            return SetErr64(kEinval);
    }
}

int64_t N2WriteChk(int fd, const void* buf, uint64_t count, uint64_t buflen) {
    return N2Write(fd, buf, std::min(count, buflen));
}

int N2Close(int fd) {
    if (fd >= 0 && fd <= 2) return 0;
    std::shared_ptr<FdEntry> removed;
    {
        std::lock_guard lock(g_fdMu);
        auto it = g_fds.find(fd);
        if (it == g_fds.end()) return SetErr(kEbadf);
        removed = std::move(it->second);
        g_fds.erase(it);
    }
    return 0;
}

int N2Dup(int oldfd) {
    auto ent = GetFd(oldfd);
    if (!ent) return SetErr(kEbadf);
    if (ent->kind == FdKind::PipeRead && ent->pipe) { std::lock_guard lk(ent->pipe->mu); ++ent->pipe->readers; }
    if (ent->kind == FdKind::PipeWrite && ent->pipe) { std::lock_guard lk(ent->pipe->mu); ++ent->pipe->writers; }
    return AllocFd(ent);
}

int N2Dup2(int oldfd, int newfd) {
    if (newfd < 0) return SetErr(kEbadf);
    if (oldfd == newfd) return GetFd(oldfd) ? newfd : SetErr(kEbadf);
    auto ent = GetFd(oldfd);
    if (!ent) return SetErr(kEbadf);
    if (ent->kind == FdKind::PipeRead && ent->pipe) { std::lock_guard lk(ent->pipe->mu); ++ent->pipe->readers; }
    if (ent->kind == FdKind::PipeWrite && ent->pipe) { std::lock_guard lk(ent->pipe->mu); ++ent->pipe->writers; }
    std::lock_guard lock(g_fdMu);
    g_fds[newfd] = ent;
    return newfd;
}

int64_t N2Lseek(int fd, int64_t offset, int whence) {
    auto ent = GetFd(fd);
    if (!ent) return SetErr64(kEbadf);
    if (ent->kind == FdKind::VirtualBytes) {
        int64_t base = whence == 0 ? 0 : (whence == 1 ? static_cast<int64_t>(ent->virtPos) : static_cast<int64_t>(ent->virtBytes.size()));
        int64_t next = base + offset;
        if (next < 0) return SetErr64(kEinval);
        ent->virtPos = static_cast<size_t>(next);
        return next;
    }
    if (ent->kind != FdKind::DiskFile) return SetErr64(kEspipe);
    LARGE_INTEGER dist{}, pos{};
    dist.QuadPart = offset;
    if (!SetFilePointerEx(ent->winHandle, dist, &pos, static_cast<DWORD>(whence))) {
        return SetErr64(MapWinError(GetLastError()));
    }
    return pos.QuadPart;
}

bool FillStatFromHandle(HANDLE h, GuestStat64* st) {
    BY_HANDLE_FILE_INFORMATION info{};
    if (!GetFileInformationByHandle(h, &info)) return false;
    std::memset(st, 0, sizeof(*st));
    st->st_dev = info.dwVolumeSerialNumber;
    st->st_ino = (static_cast<uint64_t>(info.nFileIndexHigh) << 32) | info.nFileIndexLow;
    const bool isDir = (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    st->st_mode = (isDir ? 0040755u : 0100644u);
    st->st_nlink = std::max<DWORD>(1, info.nNumberOfLinks);
    st->st_size = static_cast<int64_t>((static_cast<uint64_t>(info.nFileSizeHigh) << 32) | info.nFileSizeLow);
    st->st_blksize = 4096;
    st->st_blocks = (st->st_size + 511) / 512;
    st->st_atim = FiletimeToUnix(info.ftLastAccessTime);
    st->st_mtim = FiletimeToUnix(info.ftLastWriteTime);
    st->st_ctim = FiletimeToUnix(info.ftCreationTime);
    return true;
}

int N2Fstat(int fd, GuestStat64* st) {
    if (!st) return SetErr(kEfault);
    if (fd >= 0 && fd <= 2) {
        std::memset(st, 0, sizeof(*st));
        st->st_mode = 0020666u; // character device
        return 0;
    }
    auto ent = GetFd(fd);
    if (!ent) return SetErr(kEbadf);
    if (ent->kind == FdKind::VirtualBytes) {
        std::memset(st, 0, sizeof(*st));
        st->st_mode = 0100444u;
        st->st_size = static_cast<int64_t>(ent->virtBytes.size());
        st->st_blksize = 4096;
        return 0;
    }
    if (ent->kind != FdKind::DiskFile) {
        std::memset(st, 0, sizeof(*st));
        st->st_mode = 0010600u; // FIFO / socket
        return 0;
    }
    return FillStatFromHandle(ent->winHandle, st) ? 0 : SetErr(MapWinError(GetLastError()));
}

int N2StatPath(const char* path, GuestStat64* st) {
    if (!st) return SetErr(kEfault);
    SpecialPath sp = SpecialPath::None;
    std::wstring hostPath;
    if (!ResolveGuestPath(path, hostPath, sp)) return -1;
    if (sp != SpecialPath::None) {
        std::memset(st, 0, sizeof(*st));
        st->st_mode = (sp == SpecialPath::DevNull || sp == SpecialPath::DevUrandom) ? 0020666u : 0100444u;
        if (sp != SpecialPath::DevNull && sp != SpecialPath::DevUrandom) {
            const std::string content = (sp == SpecialPath::ProcCpuinfo && std::string_view(path).starts_with("/sys/"))
                ? ("0-" + std::to_string(std::max<DWORD>(1, [] { SYSTEM_INFO si{}; GetSystemInfo(&si); return si.dwNumberOfProcessors; }()) - 1) + "\n")
                : GenerateVirtualContent(sp);
            st->st_size = static_cast<int64_t>(content.size());
            st->st_blksize = 4096;
        }
        return 0;
    }
    HANDLE h = CreateFileW(hostPath.c_str(), FILE_READ_ATTRIBUTES,
                           FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (h == INVALID_HANDLE_VALUE) return SetErr(MapWinError(GetLastError()));
    const bool ok = FillStatFromHandle(h, st);
    const DWORD err = GetLastError();
    CloseHandle(h);
    return ok ? 0 : SetErr(MapWinError(err));
}

int N2FstatAt(int dirfd, const char* path, GuestStat64* st, int flags) {
    (void)flags;
    if (!path) return SetErr(kEfault);
    if (path[0] == '\0' && (flags & 0x1000 /* AT_EMPTY_PATH */)) return N2Fstat(dirfd, st);
    if (path[0] == '/' || dirfd == -100) return N2StatPath(path, st);
    int fd = N2OpenAtImpl(dirfd, path, 0, 0);
    if (fd < 0) return -1;
    const int rc = N2Fstat(fd, st);
    N2Close(fd);
    return rc;
}

extern "C" int N2FcntlImpl(int fd, int cmd, uint64_t arg) {
    auto ent = GetFd(fd);
    if (!ent) return SetErr(kEbadf);
    switch (cmd) {
        case 0: // F_DUPFD
        case 1030: // F_DUPFD_CLOEXEC
            return N2Dup(fd);
        case 1: // F_GETFD
            return 1; // FD_CLOEXEC
        case 2: // F_SETFD
            return 0;
        case 3: // F_GETFL
            return (ent->nonblock ? 04000 : 0) | (ent->append ? 02000 : 0);
        case 4: { // F_SETFL
            const bool nb = (arg & 04000u) != 0;
            ent->nonblock = nb;
            if (ent->kind == FdKind::Socket && ent->sock != INVALID_SOCKET) {
                u_long mode = nb ? 1 : 0;
                ioctlsocket(ent->sock, FIONBIO, &mode);
            }
            return 0;
        }
        default:
            return 0;
    }
}

extern "C" int N2IoctlImpl(int fd, uint64_t req, uint64_t arg) {
    auto ent = GetFd(fd);
    if (!ent) return SetErr(kEbadf);
    if (req == 0x5421u /* FIONBIO */) {
        if (!arg) return SetErr(kEfault);
        const int v = *reinterpret_cast<const int*>(arg);
        ent->nonblock = v != 0;
        if (ent->kind == FdKind::Socket && ent->sock != INVALID_SOCKET) {
            u_long mode = v ? 1 : 0;
            ioctlsocket(ent->sock, FIONBIO, &mode);
        }
        return 0;
    }
    if (req == 0x541Bu /* FIONREAD */ && ent->kind == FdKind::Socket && arg) {
        u_long avail = 0;
        if (ioctlsocket(ent->sock, FIONREAD, &avail) != 0) return SetErr(MapWsaError(WSAGetLastError()));
        *reinterpret_cast<int*>(arg) = static_cast<int>(avail);
        return 0;
    }
    return SetErr(kEnotty);
}

// ---- `pipe` / `eventfd` / `timerfd` / `inotify` / `epoll` / `poll` ----

int N2Pipe2(int pipefd[2], int flags) {
    if (!pipefd) return SetErr(kEfault);
    const bool nb = (flags & 04000 /* O_NONBLOCK */) != 0;
    auto shared = std::make_shared<PipeShared>();
    auto r = std::make_shared<FdEntry>();
    r->kind = FdKind::PipeRead;
    r->nonblock = nb;
    r->pipe = shared;
    auto w = std::make_shared<FdEntry>();
    w->kind = FdKind::PipeWrite;
    w->nonblock = nb;
    w->pipe = shared;
    pipefd[0] = AllocFd(std::move(r));
    pipefd[1] = AllocFd(std::move(w));
    return 0;
}

int N2Pipe(int pipefd[2]) { return N2Pipe2(pipefd, 0); }

int N2EventFd(unsigned int initval, int flags) {
    auto ent = std::make_shared<FdEntry>();
    ent->kind = FdKind::EventFd;
    ent->nonblock = (flags & 04000) != 0;
    ent->evfd = std::make_shared<EventFdState>();
    ent->evfd->counter = initval;
    ent->evfd->semaphore = (flags & 1 /* EFD_SEMAPHORE */) != 0;
    return AllocFd(std::move(ent));
}

int N2TimerFdCreate(int clockid, int flags) {
    if (clockid != 0 && clockid != 1 && clockid != 7) return SetErr(kEinval);
    auto ent = std::make_shared<FdEntry>();
    ent->kind = FdKind::TimerFd;
    ent->nonblock = (flags & 04000) != 0;
    ent->tfd = std::make_shared<TimerFdState>();
    return AllocFd(std::move(ent));
}

int N2TimerFdSetTime(int fd, int flags, const GuestItimerspec* newVal, GuestItimerspec* oldVal) {
    if (!newVal) return SetErr(kEfault);
    auto ent = GetFd(fd);
    if (!ent || ent->kind != FdKind::TimerFd) return SetErr(kEbadf);
    if (oldVal) std::memset(oldVal, 0, sizeof(*oldVal));

    const uint64_t now = MonoNowNs();
    const bool absTime = (flags & 1 /* TFD_TIMER_ABSTIME */) != 0;
    {
        std::lock_guard lk(ent->tfd->mu);
        ent->tfd->intervalNs = (newVal->interval.sec > 0 || newVal->interval.nsec > 0)
            ? static_cast<uint64_t>(newVal->interval.sec) * 1000000000ull + static_cast<uint64_t>(newVal->interval.nsec)
            : 0;
        if (newVal->value.sec == 0 && newVal->value.nsec == 0) {
            ent->tfd->nextFireNs = 0;
        } else {
            const uint64_t valNs = static_cast<uint64_t>(std::max<int64_t>(0, newVal->value.sec)) * 1000000000ull +
                                   static_cast<uint64_t>(std::max<int64_t>(0, newVal->value.nsec));
            ent->tfd->nextFireNs = absTime ? valNs : (now + valNs);
            if (ent->tfd->nextFireNs == 0) ent->tfd->nextFireNs = 1;
        }
    }
    BroadcastFdWake();
    return 0;
}

int N2TimerFdGetTime(int fd, GuestItimerspec* currVal) {
    if (!currVal) return SetErr(kEfault);
    auto ent = GetFd(fd);
    if (!ent || ent->kind != FdKind::TimerFd) return SetErr(kEbadf);
    std::memset(currVal, 0, sizeof(*currVal));
    return 0;
}

int N2InotifyInit1(int flags) {
    auto ent = std::make_shared<FdEntry>();
    ent->kind = FdKind::Inotify;
    ent->nonblock = (flags & 04000) != 0;
    return AllocFd(std::move(ent));
}
int N2InotifyAddWatch(int fd, const char*, uint32_t) {
    auto ent = GetFd(fd);
    return (ent && ent->kind == FdKind::Inotify) ? 1 : SetErr(kEbadf);
}
int N2InotifyRmWatch(int fd, int) {
    auto ent = GetFd(fd);
    return (ent && ent->kind == FdKind::Inotify) ? 0 : SetErr(kEbadf);
}

struct GuestEpollEvent {
    uint32_t events;
    uint32_t pad;
    uint64_t data;
};
static_assert(sizeof(GuestEpollEvent) == 16);

int N2EpollCreate1(int) {
    auto ent = std::make_shared<FdEntry>();
    ent->kind = FdKind::Epoll;
    return AllocFd(std::move(ent));
}

int N2EpollCtl(int epfd, int op, int fd, const GuestEpollEvent* event) {
    auto ep = GetFd(epfd);
    if (!ep || ep->kind != FdKind::Epoll) return SetErr(kEbadf);
    {
        std::lock_guard lock(g_fdMu);
        if (op == 1 /* EPOLL_CTL_ADD */) {
            if (!event) return SetErr(kEfault);
            if (ep->epollSet.contains(fd)) return SetErr(kEexist);
            ep->epollSet[fd] = {fd, event->events, event->data};
        } else if (op == 2 /* EPOLL_CTL_DEL */) {
            if (ep->epollSet.erase(fd) == 0) return SetErr(kEnoent);
        } else if (op == 3 /* EPOLL_CTL_MOD */) {
            if (!event) return SetErr(kEfault);
            auto it = ep->epollSet.find(fd);
            if (it == ep->epollSet.end()) return SetErr(kEnoent);
            it->second = {fd, event->events, event->data};
        } else {
            return SetErr(kEinval);
        }
    }
    BroadcastFdWake();
    return 0;
}

int N2EpollWait(int epfd, GuestEpollEvent* events, int maxevents, int timeoutMs) {
    if (!events || maxevents <= 0) return SetErr(kEinval);
    auto ep = GetFd(epfd);
    if (!ep || ep->kind != FdKind::Epoll) return SetErr(kEbadf);

    const uint64_t startNs = MonoNowNs();
    const uint64_t deadlineNs = timeoutMs < 0 ? UINT64_MAX
        : startNs + static_cast<uint64_t>(timeoutMs) * 1000000ull;

    for (;;) {
        uint64_t wakeGenSnap = 0;
        {
            std::lock_guard lk(g_wakeMu);
            wakeGenSnap = g_wakeGen;
        }
        std::vector<std::pair<EpollEntry, std::shared_ptr<FdEntry>>> items;
        {
            std::lock_guard lock(g_fdMu);
            for (auto it = ep->epollSet.begin(); it != ep->epollSet.end();) {
                auto fit = g_fds.find(it->first);
                if (fit == g_fds.end()) {
                    it = ep->epollSet.erase(it);
                } else {
                    items.emplace_back(it->second, fit->second);
                    ++it;
                }
            }
        }

        const uint64_t nowNs = MonoNowNs();
        uint64_t nextTimerNs = deadlineNs;
        int hasSockets = 0;
        int readyCount = 0;
        for (const auto& [reg, target] : items) {
            if (target->kind == FdKind::Socket) ++hasSockets;
            const uint32_t mask = EvalEntryEpollMask(*target, nowNs, &nextTimerNs);
            const uint32_t matched = mask & (reg.events | 8u | 16u);
            if (matched != 0) {
                events[readyCount].events = matched;
                events[readyCount].pad = 0;
                events[readyCount].data = reg.data;
                if (++readyCount >= maxevents) break;
            }
        }
        if (readyCount > 0 || timeoutMs == 0 || nowNs >= deadlineNs) return readyCount;

        uint64_t sleepNs = nextTimerNs == UINT64_MAX ? 1000000000ull : (nextTimerNs > nowNs ? nextTimerNs - nowNs : 0);
        if (hasSockets > 0) sleepNs = std::min<uint64_t>(sleepNs, 4000000ull); // 4ms poll cap when sockets present
        if (sleepNs == 0) continue;

        std::unique_lock wlk(g_wakeMu);
        if (g_wakeGen == wakeGenSnap) {
            g_wakeCv.wait_for(wlk, std::chrono::nanoseconds(sleepNs));
        }
    }
}

struct GuestPollFd { int fd; int16_t events, revents; };
static_assert(sizeof(GuestPollFd) == 8);

int N2Poll(GuestPollFd* fds, uint64_t nfds, int timeoutMs) {
    if (nfds > 0 && !fds) return SetErr(kEfault);
    const uint64_t startNs = MonoNowNs();
    const uint64_t deadlineNs = timeoutMs < 0 ? UINT64_MAX
        : startNs + static_cast<uint64_t>(timeoutMs) * 1000000ull;

    for (;;) {
        uint64_t wakeGenSnap = 0;
        { std::lock_guard lk(g_wakeMu); wakeGenSnap = g_wakeGen; }
        const uint64_t nowNs = MonoNowNs();
        uint64_t nextTimerNs = deadlineNs;
        bool hasSockets = false;
        int ready = 0;

        for (uint64_t i = 0; i < nfds; ++i) {
            fds[i].revents = 0;
            if (fds[i].fd < 0) continue;
            auto ent = GetFd(fds[i].fd);
            if (!ent) {
                fds[i].revents = 0x0020; // POLLNVAL
                ++ready;
                continue;
            }
            if (ent->kind == FdKind::Socket) hasSockets = true;
            const uint32_t m = EvalEntryEpollMask(*ent, nowNs, &nextTimerNs);
            int16_t rev = 0;
            if ((m & 1u) && (fds[i].events & 0x0001)) rev |= 0x0001; // POLLIN
            if ((m & 4u) && (fds[i].events & 0x0004)) rev |= 0x0004; // POLLOUT
            if (m & 8u) rev |= 0x0008;  // POLLERR
            if (m & 16u) rev |= 0x0010; // POLLHUP
            fds[i].revents = rev;
            if (rev != 0) ++ready;
        }
        if (ready > 0 || timeoutMs == 0 || nowNs >= deadlineNs) return ready;

        uint64_t sleepNs = nextTimerNs == UINT64_MAX ? 1000000000ull : (nextTimerNs > nowNs ? nextTimerNs - nowNs : 0);
        if (hasSockets) sleepNs = std::min<uint64_t>(sleepNs, 4000000ull);
        if (sleepNs == 0) continue;
        std::unique_lock wlk(g_wakeMu);
        if (g_wakeGen == wakeGenSnap) g_wakeCv.wait_for(wlk, std::chrono::nanoseconds(sleepNs));
    }
}

int N2PollChk(GuestPollFd* fds, uint64_t nfds, int timeoutMs, uint64_t) {
    return N2Poll(fds, nfds, timeoutMs);
}

int N2Ppoll(GuestPollFd* fds, uint64_t nfds, const GuestTimespec* tmo, const void*) {
    int ms = -1;
    if (tmo) ms = static_cast<int>(std::min<int64_t>(tmo->sec * 1000 + (tmo->nsec + 999999) / 1000000, 0x7fffffffLL));
    return N2Poll(fds, nfds, ms);
}

// ---- Winsock2 Socket Bridge (translating Linux `AF_INET6=10` <-> Win32 `AF_INET6=23`) ----

int GuestAfToHost(int af) {
    if (af == 2) return AF_INET;
    if (af == 10) return AF_INET6;
    if (af == 0) return AF_UNSPEC;
    return -1;
}
int HostAfToGuest(int af) {
    if (af == AF_INET) return 2;
    if (af == AF_INET6) return 10;
    return af;
}

bool GuestSockaddrToHost(const void* gaddr, uint32_t glen, sockaddr_storage& hstore, int& hlen) {
    if (!gaddr || glen < 2) return false;
    std::memset(&hstore, 0, sizeof(hstore));
    uint16_t gaf = 0;
    std::memcpy(&gaf, gaddr, 2);
    if (gaf == 2 && glen >= 16) {
        std::memcpy(&hstore, gaddr, 16);
        hstore.ss_family = AF_INET;
        hlen = sizeof(sockaddr_in);
        return true;
    }
    if (gaf == 10 && glen >= 28) {
        std::memcpy(&hstore, gaddr, 28);
        hstore.ss_family = AF_INET6;
        hlen = sizeof(sockaddr_in6);
        return true;
    }
    return false;
}

void HostSockaddrToGuest(const sockaddr* haddr, int hlen, void* gaddr, uint32_t* glenInOut) {
    if (!haddr || !glenInOut) return;
    uint32_t need = 0;
    sockaddr_storage tmp{};
    if (haddr->sa_family == AF_INET && hlen >= static_cast<int>(sizeof(sockaddr_in))) {
        need = 16;
        std::memcpy(&tmp, haddr, 16);
        *reinterpret_cast<uint16_t*>(&tmp) = 2;
    } else if (haddr->sa_family == AF_INET6 && hlen >= static_cast<int>(sizeof(sockaddr_in6))) {
        need = 28;
        std::memcpy(&tmp, haddr, 28);
        *reinterpret_cast<uint16_t*>(&tmp) = 10;
    }
    if (gaddr && need > 0) std::memcpy(gaddr, &tmp, std::min<uint32_t>(*glenInOut, need));
    *glenInOut = need;
}

int N2Socket(int domain, int type, int protocol) {
    EnsureWinsock();
    const bool nonblock = (type & 04000 /* SOCK_NONBLOCK */) != 0;
    const int baseType = type & 0xf; // 1=SOCK_STREAM, 2=SOCK_DGRAM
    const int hostAf = GuestAfToHost(domain);
    if (hostAf < 0 || (baseType != 1 && baseType != 2)) return SetErr(kEafnosupp);
    SOCKET s = ::socket(hostAf, baseType == 1 ? SOCK_STREAM : SOCK_DGRAM, protocol);
    if (s == INVALID_SOCKET) return SetErr(MapWsaError(WSAGetLastError()));
    if (nonblock) {
        u_long nb = 1;
        ioctlsocket(s, FIONBIO, &nb);
    }
    auto ent = std::make_shared<FdEntry>();
    ent->kind = FdKind::Socket;
    ent->sock = s;
    ent->nonblock = nonblock;
    return AllocFd(std::move(ent));
}

int N2SocketPair(int domain, int type, int, int sv[2]) {
    if (!sv) return SetErr(kEfault);
    if (domain != 1 /* AF_UNIX */ && domain != 2) return SetErr(kEafnosupp);
    EnsureWinsock();
    const bool nb = (type & 04000) != 0;
    SOCKET listener = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == INVALID_SOCKET) return SetErr(kEio);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    int addrLen = sizeof(addr);
    if (::bind(listener, reinterpret_cast<sockaddr*>(&addr), addrLen) != 0 ||
        ::getsockname(listener, reinterpret_cast<sockaddr*>(&addr), &addrLen) != 0 ||
        ::listen(listener, 1) != 0) {
        closesocket(listener);
        return SetErr(kEio);
    }
    SOCKET c1 = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (c1 == INVALID_SOCKET || ::connect(c1, reinterpret_cast<sockaddr*>(&addr), addrLen) != 0) {
        if (c1 != INVALID_SOCKET) closesocket(c1);
        closesocket(listener);
        return SetErr(kEio);
    }
    SOCKET c2 = ::accept(listener, nullptr, nullptr);
    closesocket(listener);
    if (c2 == INVALID_SOCKET) { closesocket(c1); return SetErr(kEio); }
    if (nb) {
        u_long m = 1;
        ioctlsocket(c1, FIONBIO, &m);
        ioctlsocket(c2, FIONBIO, &m);
    }
    auto e1 = std::make_shared<FdEntry>(); e1->kind = FdKind::Socket; e1->sock = c1; e1->nonblock = nb;
    auto e2 = std::make_shared<FdEntry>(); e2->kind = FdKind::Socket; e2->sock = c2; e2->nonblock = nb;
    sv[0] = AllocFd(std::move(e1));
    sv[1] = AllocFd(std::move(e2));
    return 0;
}

int N2Connect(int fd, const void* addr, uint32_t addrlen) {
    auto ent = GetFd(fd);
    if (!ent || ent->kind != FdKind::Socket) return SetErr(kEnotsock);
    sockaddr_storage hs{}; int hlen = 0;
    if (!GuestSockaddrToHost(addr, addrlen, hs, hlen)) return SetErr(kEafnosupp);
    if (::connect(ent->sock, reinterpret_cast<sockaddr*>(&hs), hlen) == SOCKET_ERROR) {
        const int wsa = WSAGetLastError();
        if (wsa == WSAEWOULDBLOCK) return SetErr(kEinprogress);
        return SetErr(MapWsaError(wsa));
    }
    return 0;
}

int N2Bind(int fd, const void* addr, uint32_t addrlen) {
    auto ent = GetFd(fd);
    if (!ent || ent->kind != FdKind::Socket) return SetErr(kEnotsock);
    sockaddr_storage hs{}; int hlen = 0;
    if (!GuestSockaddrToHost(addr, addrlen, hs, hlen)) return SetErr(kEafnosupp);
    if (::bind(ent->sock, reinterpret_cast<sockaddr*>(&hs), hlen) == SOCKET_ERROR)
        return SetErr(MapWsaError(WSAGetLastError()));
    return 0;
}

int N2Listen(int fd, int backlog) {
    auto ent = GetFd(fd);
    if (!ent || ent->kind != FdKind::Socket) return SetErr(kEnotsock);
    return ::listen(ent->sock, backlog) == SOCKET_ERROR ? SetErr(MapWsaError(WSAGetLastError())) : 0;
}

int N2Accept4(int fd, void* addr, uint32_t* addrlen, int flags) {
    auto ent = GetFd(fd);
    if (!ent || ent->kind != FdKind::Socket) return SetErr(kEnotsock);
    sockaddr_storage hs{}; int hlen = sizeof(hs);
    SOCKET cs = ::accept(ent->sock, reinterpret_cast<sockaddr*>(&hs), &hlen);
    if (cs == INVALID_SOCKET) return SetErr(MapWsaError(WSAGetLastError()));
    if (addrlen) HostSockaddrToGuest(reinterpret_cast<sockaddr*>(&hs), hlen, addr, addrlen);
    const bool nb = (flags & 04000) != 0;
    if (nb) { u_long m = 1; ioctlsocket(cs, FIONBIO, &m); }
    auto ce = std::make_shared<FdEntry>();
    ce->kind = FdKind::Socket; ce->sock = cs; ce->nonblock = nb;
    return AllocFd(std::move(ce));
}
int N2Accept(int fd, void* addr, uint32_t* addrlen) { return N2Accept4(fd, addr, addrlen, 0); }

int64_t N2SendTo(int fd, const void* buf, uint64_t len, int flags, const void* dest, uint32_t addrlen) {
    auto ent = GetFd(fd);
    if (!ent || ent->kind != FdKind::Socket) return SetErr64(kEnotsock);
    const int wflags = (flags & 0x4 /* MSG_DONTROUTE */) ? MSG_DONTROUTE : 0;
    int rc = 0;
    if (dest && addrlen > 0) {
        sockaddr_storage hs{}; int hlen = 0;
        if (!GuestSockaddrToHost(dest, addrlen, hs, hlen)) return SetErr64(kEafnosupp);
        rc = ::sendto(ent->sock, static_cast<const char*>(buf), static_cast<int>(std::min<uint64_t>(len, 0x7fffffffull)),
                      wflags, reinterpret_cast<sockaddr*>(&hs), hlen);
    } else {
        rc = ::send(ent->sock, static_cast<const char*>(buf), static_cast<int>(std::min<uint64_t>(len, 0x7fffffffull)), wflags);
    }
    return rc == SOCKET_ERROR ? SetErr64(MapWsaError(WSAGetLastError())) : rc;
}
int64_t N2Send(int fd, const void* buf, uint64_t len, int flags) { return N2SendTo(fd, buf, len, flags, nullptr, 0); }

int64_t N2RecvFrom(int fd, void* buf, uint64_t len, int flags, void* src, uint32_t* addrlen) {
    auto ent = GetFd(fd);
    if (!ent || ent->kind != FdKind::Socket) return SetErr64(kEnotsock);
    const int wflags = (flags & 0x2 /* MSG_PEEK */) ? MSG_PEEK : 0;
    sockaddr_storage hs{}; int hlen = sizeof(hs);
    const int rc = ::recvfrom(ent->sock, static_cast<char*>(buf), static_cast<int>(std::min<uint64_t>(len, 0x7fffffffull)),
                              wflags, src ? reinterpret_cast<sockaddr*>(&hs) : nullptr, src ? &hlen : nullptr);
    if (rc == SOCKET_ERROR) return SetErr64(MapWsaError(WSAGetLastError()));
    if (src && addrlen) HostSockaddrToGuest(reinterpret_cast<sockaddr*>(&hs), hlen, src, addrlen);
    return rc;
}
int64_t N2Recv(int fd, void* buf, uint64_t len, int flags) { return N2RecvFrom(fd, buf, len, flags, nullptr, nullptr); }

struct GuestIovec { void* iov_base; uint64_t iov_len; };
struct GuestMsghdr {
    void* msg_name;
    uint32_t msg_namelen, pad0;
    GuestIovec* msg_iov;
    uint64_t msg_iovlen;
    void* msg_control;
    uint64_t msg_controllen;
    int msg_flags, pad1;
};
static_assert(sizeof(GuestMsghdr) == 56);

int64_t N2SendMsg(int fd, const GuestMsghdr* msg, int flags) {
    if (!msg) return SetErr64(kEfault);
    std::vector<uint8_t> flat;
    for (uint64_t i = 0; i < msg->msg_iovlen; ++i) {
        const auto* p = static_cast<const uint8_t*>(msg->msg_iov[i].iov_base);
        flat.insert(flat.end(), p, p + msg->msg_iov[i].iov_len);
    }
    return N2SendTo(fd, flat.data(), flat.size(), flags, msg->msg_name, msg->msg_namelen);
}

int64_t N2RecvMsg(int fd, GuestMsghdr* msg, int flags) {
    if (!msg) return SetErr64(kEfault);
    uint64_t cap = 0;
    for (uint64_t i = 0; i < msg->msg_iovlen; ++i) cap += msg->msg_iov[i].iov_len;
    std::vector<uint8_t> flat(static_cast<size_t>(std::min<uint64_t>(cap, 16ull << 20)));
    const int64_t got = N2RecvFrom(fd, flat.data(), flat.size(), flags, msg->msg_name, &msg->msg_namelen);
    if (got <= 0) return got;
    size_t off = 0;
    for (uint64_t i = 0; i < msg->msg_iovlen && off < static_cast<size_t>(got); ++i) {
        const size_t n = std::min<size_t>(msg->msg_iov[i].iov_len, static_cast<size_t>(got) - off);
        std::memcpy(msg->msg_iov[i].iov_base, flat.data() + off, n);
        off += n;
    }
    msg->msg_controllen = 0;
    return got;
}

int N2SetSockOpt(int fd, int level, int optname, const void* optval, uint32_t optlen) {
    auto ent = GetFd(fd);
    if (!ent || ent->kind != FdKind::Socket) return SetErr(kEnotsock);
    if (!optval || optlen < 4) return 0;
    int hlevel = level, hopt = optname;
    if (level == 1 /* SOL_SOCKET */) {
        hlevel = SOL_SOCKET;
        switch (optname) {
            case 2: hopt = SO_REUSEADDR; break;
            case 7: hopt = SO_SNDBUF; break;
            case 8: hopt = SO_RCVBUF; break;
            case 9: hopt = SO_KEEPALIVE; break;
            case 6: hopt = SO_BROADCAST; break;
            default: return 0; // harmlessly ignore SO_REUSEPORT / SO_NOSIGPIPE
        }
    } else if (level == 6 /* IPPROTO_TCP */) {
        hlevel = IPPROTO_TCP;
        if (optname == 1) hopt = TCP_NODELAY;
        else return 0;
    } else if (level == 41 /* IPPROTO_IPV6 */) {
        hlevel = IPPROTO_IPV6;
        if (optname == 26) hopt = IPV6_V6ONLY;
        else return 0;
    } else {
        return 0;
    }
    ::setsockopt(ent->sock, hlevel, hopt, static_cast<const char*>(optval), static_cast<int>(optlen));
    return 0;
}

int N2GetSockOpt(int fd, int level, int optname, void* optval, uint32_t* optlen) {
    auto ent = GetFd(fd);
    if (!ent || ent->kind != FdKind::Socket) return SetErr(kEnotsock);
    if (!optval || !optlen || *optlen < 4) return SetErr(kEfault);
    if (level == 1 && optname == 4 /* SO_ERROR */) {
        int hostErr = 0, len = sizeof(hostErr);
        if (::getsockopt(ent->sock, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&hostErr), &len) == SOCKET_ERROR)
            return SetErr(MapWsaError(WSAGetLastError()));
        *static_cast<int*>(optval) = hostErr ? MapWsaError(hostErr) : 0;
        *optlen = 4;
        return 0;
    }
    std::memset(optval, 0, *optlen);
    return 0;
}

int N2GetSockName(int fd, void* addr, uint32_t* addrlen) {
    auto ent = GetFd(fd);
    if (!ent || ent->kind != FdKind::Socket) return SetErr(kEnotsock);
    sockaddr_storage hs{}; int hlen = sizeof(hs);
    if (::getsockname(ent->sock, reinterpret_cast<sockaddr*>(&hs), &hlen) == SOCKET_ERROR)
        return SetErr(MapWsaError(WSAGetLastError()));
    HostSockaddrToGuest(reinterpret_cast<sockaddr*>(&hs), hlen, addr, addrlen);
    return 0;
}

int N2GetPeerName(int fd, void* addr, uint32_t* addrlen) {
    auto ent = GetFd(fd);
    if (!ent || ent->kind != FdKind::Socket) return SetErr(kEnotsock);
    sockaddr_storage hs{}; int hlen = sizeof(hs);
    if (::getpeername(ent->sock, reinterpret_cast<sockaddr*>(&hs), &hlen) == SOCKET_ERROR)
        return SetErr(MapWsaError(WSAGetLastError()));
    HostSockaddrToGuest(reinterpret_cast<sockaddr*>(&hs), hlen, addr, addrlen);
    return 0;
}

int N2Shutdown(int fd, int how) {
    auto ent = GetFd(fd);
    if (!ent || ent->kind != FdKind::Socket) return SetErr(kEnotsock);
    return ::shutdown(ent->sock, how) == SOCKET_ERROR ? SetErr(MapWsaError(WSAGetLastError())) : 0;
}

// Android Bionic `<netdb.h>` `struct addrinfo` layout (verified against NDK sysroot `netdb.h`).
struct GuestAddrInfo {
    int32_t ai_flags;
    int32_t ai_family;
    int32_t ai_socktype;
    int32_t ai_protocol;
    uint32_t ai_addrlen;
    uint32_t pad;
    char* ai_canonname;
    void* ai_addr;
    GuestAddrInfo* ai_next;
};
static_assert(sizeof(GuestAddrInfo) == 48);

int N2GetAddrInfo(const char* node, const char* service, const GuestAddrInfo* hints, GuestAddrInfo** res) {
    if (!res) return -11; // EAI_SYSTEM
    *res = nullptr;
    EnsureWinsock();
    ADDRINFOA whints{};
    if (hints) {
        whints.ai_family = GuestAfToHost(hints->ai_family);
        if (whints.ai_family < 0) whints.ai_family = AF_UNSPEC;
        whints.ai_socktype = (hints->ai_socktype & 0xf) == 2 ? SOCK_DGRAM : ((hints->ai_socktype & 0xf) == 1 ? SOCK_STREAM : 0);
        whints.ai_protocol = hints->ai_protocol;
        if (hints->ai_flags & 1) whints.ai_flags |= AI_PASSIVE;
        if (hints->ai_flags & 4) whints.ai_flags |= AI_NUMERICHOST;
    }
    ADDRINFOA* wres = nullptr;
    if (::getaddrinfo(node, service, hints ? &whints : nullptr, &wres) != 0 || !wres) {
        return 8; // Bionic EAI_NONAME
    }
    GuestAddrInfo* head = nullptr;
    GuestAddrInfo** tail = &head;
    for (ADDRINFOA* cur = wres; cur; cur = cur->ai_next) {
        if (cur->ai_family != AF_INET && cur->ai_family != AF_INET6) continue;
        const uint32_t saLen = cur->ai_family == AF_INET ? 16 : 28;
        const size_t canonLen = cur->ai_canonname ? std::strlen(cur->ai_canonname) + 1 : 0;
        auto* block = static_cast<uint8_t*>(GuestHeapCalloc(1, sizeof(GuestAddrInfo) + 32 + canonLen));
        if (!block) break;
        auto* gai = reinterpret_cast<GuestAddrInfo*>(block);
        void* gsa = block + sizeof(GuestAddrInfo);
        uint32_t outLen = saLen;
        HostSockaddrToGuest(cur->ai_addr, static_cast<int>(cur->ai_addrlen), gsa, &outLen);
        gai->ai_family = HostAfToGuest(cur->ai_family);
        gai->ai_socktype = cur->ai_socktype == SOCK_DGRAM ? 2 : 1;
        gai->ai_protocol = cur->ai_protocol;
        gai->ai_addrlen = outLen;
        gai->ai_addr = gsa;
        if (canonLen > 0) {
            gai->ai_canonname = reinterpret_cast<char*>(block + sizeof(GuestAddrInfo) + 32);
            std::memcpy(gai->ai_canonname, cur->ai_canonname, canonLen);
        }
        *tail = gai;
        tail = &gai->ai_next;
    }
    ::freeaddrinfo(wres);
    if (!head) return 8;
    *res = head;
    return 0;
}

void N2FreeAddrInfo(GuestAddrInfo* ai) {
    while (ai) {
        GuestAddrInfo* next = ai->ai_next;
        GuestHeapFree(ai);
        ai = next;
    }
}

const char* N2GaiStrerror(int) { return "address resolution error"; }

int N2GetNameInfo(const void* sa, uint32_t salen, char* host, uint32_t hostlen,
                  char* serv, uint32_t servlen, int flags) {
    EnsureWinsock();
    sockaddr_storage hs{}; int hlen = 0;
    if (!GuestSockaddrToHost(sa, salen, hs, hlen)) return 8;
    int wflags = 0;
    if (flags & 2) wflags |= NI_NUMERICHOST;
    if (flags & 8) wflags |= NI_NUMERICSERV;
    return ::getnameinfo(reinterpret_cast<sockaddr*>(&hs), hlen, host, hostlen, serv, servlen, wflags) == 0 ? 0 : 8;
}

int N2InetPton(int af, const char* src, void* dst) {
    EnsureWinsock();
    const int haf = GuestAfToHost(af);
    if (haf < 0) return SetErr(kEafnosupp);
    return ::inet_pton(haf, src, dst);
}

const char* N2InetNtop(int af, const void* src, char* dst, uint32_t size) {
    EnsureWinsock();
    const int haf = GuestAfToHost(af);
    if (haf < 0) { SetErr(kEafnosupp); return nullptr; }
    return ::inet_ntop(haf, src, dst, size);
}

int N2GetIfAddrs(void** ifap) { if (ifap) *ifap = nullptr; return 0; }
void N2FreeIfAddrs(void*) {}
unsigned int N2IfNameToIndex(const char*) { return 0; }
int N2GetHostName(char* name, uint64_t len) {
    if (!name || len == 0) return SetErr(kEfault);
    EnsureWinsock();
    return ::gethostname(name, static_cast<int>(std::min<uint64_t>(len, 256))) == 0 ? 0 : SetErr(kEio);
}
void* N2CmsgNxthdr(void*, void*) { return nullptr; }

// ---- Guest `FILE*` Streams & Directories ----

alignas(16) uint8_t g_bionicSF[3 * 152]{};
void* g_stdinPtr = &g_bionicSF[0];
void* g_stdoutPtr = &g_bionicSF[152];
void* g_stderrPtr = &g_bionicSF[304];

struct GuestStream {
    int fd = -1;
    bool ownsFd = true;
    bool eof = false;
    bool err = false;
    int ungetByte = -1;
};

std::mutex g_streamMu;
std::map<void*, std::unique_ptr<GuestStream>> g_streams;

GuestStream* LookupStream(void* s, int* stdFdOut = nullptr) {
    if (stdFdOut) *stdFdOut = -1;
    if (!s) return nullptr;
    if (s == &g_bionicSF[0] || s == &g_stdinPtr) { if (stdFdOut) *stdFdOut = 0; return nullptr; }
    if (s == &g_bionicSF[152] || s == &g_stdoutPtr) { if (stdFdOut) *stdFdOut = 1; return nullptr; }
    if (s == &g_bionicSF[304] || s == &g_stderrPtr) { if (stdFdOut) *stdFdOut = 2; return nullptr; }
    std::lock_guard lk(g_streamMu);
    auto it = g_streams.find(s);
    return it != g_streams.end() ? it->second.get() : nullptr;
}

void* N2FdOpen(int fd, const char*) {
    if (fd < 0) { SetErr(kEbadf); return nullptr; }
    auto gs = std::make_unique<GuestStream>();
    gs->fd = fd;
    gs->ownsFd = true;
    void* key = gs.get();
    std::lock_guard lk(g_streamMu);
    g_streams[key] = std::move(gs);
    return key;
}

void* N2Fopen(const char* path, const char* mode) {
    if (!path || !mode) { SetErr(kEinval); return nullptr; }
    int flags = 0;
    const bool plus = std::strchr(mode, '+') != nullptr;
    if (mode[0] == 'r') flags = plus ? 2 : 0;
    else if (mode[0] == 'w') flags = (plus ? 2 : 1) | 0100 | 01000;
    else if (mode[0] == 'a') flags = (plus ? 2 : 1) | 0100 | 02000;
    else { SetErr(kEinval); return nullptr; }
    const int fd = N2OpenImpl(path, flags, 0644);
    return fd >= 0 ? N2FdOpen(fd, mode) : nullptr;
}

int N2Fclose(void* s) {
    int stdFd = -1;
    LookupStream(s, &stdFd);
    if (stdFd >= 0) return 0;
    std::unique_ptr<GuestStream> owned;
    {
        std::lock_guard lk(g_streamMu);
        auto it = g_streams.find(s);
        if (it == g_streams.end()) return -1;
        owned = std::move(it->second);
        g_streams.erase(it);
    }
    return owned->ownsFd ? N2Close(owned->fd) : 0;
}

uint64_t N2Fread(void* ptr, uint64_t size, uint64_t nmemb, void* s) {
    if (size == 0 || nmemb == 0) return 0;
    int stdFd = -1;
    GuestStream* gs = LookupStream(s, &stdFd);
    if (stdFd == 0) return 0;
    if (!gs) return 0;
    const uint64_t total = size * nmemb;
    auto* dst = static_cast<uint8_t*>(ptr);
    uint64_t done = 0;
    if (gs->ungetByte >= 0 && total > 0) {
        dst[0] = static_cast<uint8_t>(gs->ungetByte);
        gs->ungetByte = -1;
        done = 1;
    }
    while (done < total) {
        const int64_t got = N2Read(gs->fd, dst + done, total - done);
        if (got < 0) { gs->err = true; break; }
        if (got == 0) { gs->eof = true; break; }
        done += static_cast<uint64_t>(got);
    }
    return done / size;
}

uint64_t N2Fwrite(const void* ptr, uint64_t size, uint64_t nmemb, void* s) {
    if (size == 0 || nmemb == 0) return 0;
    const uint64_t total = size * nmemb;
    int stdFd = -1;
    GuestStream* gs = LookupStream(s, &stdFd);
    if (stdFd == 1 || stdFd == 2) {
        N2Write(stdFd, ptr, total);
        return nmemb;
    }
    if (!gs) return 0;
    const int64_t put = N2Write(gs->fd, ptr, total);
    if (put < 0) { gs->err = true; return 0; }
    return static_cast<uint64_t>(put) / size;
}

uint64_t N2FwriteChk(const void* ptr, uint64_t size, uint64_t nmemb, void* s, uint64_t) {
    return N2Fwrite(ptr, size, nmemb, s);
}

int N2Fflush(void*) { std::fflush(stderr); return 0; }
int N2Fseeko(void* s, int64_t off, int whence) {
    GuestStream* gs = LookupStream(s);
    if (!gs) return -1;
    gs->eof = false;
    gs->ungetByte = -1;
    return N2Lseek(gs->fd, off, whence) >= 0 ? 0 : -1;
}
int64_t N2Ftello(void* s) {
    GuestStream* gs = LookupStream(s);
    return gs ? N2Lseek(gs->fd, 0, 1) : -1;
}
void N2Rewind(void* s) { N2Fseeko(s, 0, 0); }
int N2Feof(void* s) { GuestStream* gs = LookupStream(s); return (gs && gs->eof) ? 1 : 0; }
int N2Ferror(void* s) { GuestStream* gs = LookupStream(s); return (gs && gs->err) ? 1 : 0; }
int N2Fileno(void* s) {
    int stdFd = -1;
    GuestStream* gs = LookupStream(s, &stdFd);
    if (stdFd >= 0) return stdFd;
    return gs ? gs->fd : SetErr(kEbadf);
}

int N2Getc(void* s) {
    uint8_t b = 0;
    return N2Fread(&b, 1, 1, s) == 1 ? static_cast<int>(b) : -1;
}
int N2Ungetc(int c, void* s) {
    GuestStream* gs = LookupStream(s);
    if (!gs || c < 0) return -1;
    gs->ungetByte = c & 0xff;
    gs->eof = false;
    return gs->ungetByte;
}
char* N2Fgets(char* buf, int size, void* s) {
    if (!buf || size <= 0) return nullptr;
    int pos = 0;
    while (pos + 1 < size) {
        const int ch = N2Getc(s);
        if (ch < 0) break;
        buf[pos++] = static_cast<char>(ch);
        if (ch == '\n') break;
    }
    if (pos == 0) return nullptr;
    buf[pos] = '\0';
    return buf;
}
int N2Fputc(int c, void* s) {
    const uint8_t b = static_cast<uint8_t>(c);
    return N2Fwrite(&b, 1, 1, s) == 1 ? (c & 0xff) : -1;
}
int N2Putchar(int c) { return N2Fputc(c, &g_bionicSF[152]); }
int N2Fputs(const char* str, void* s) {
    if (!str) return -1;
    const size_t n = std::strlen(str);
    return N2Fwrite(str, 1, n, s) == n ? 0 : -1;
}
int N2Puts(const char* str) {
    if (N2Fputs(str ? str : "(null)", &g_bionicSF[152]) < 0) return -1;
    return N2Fputc('\n', &g_bionicSF[152]);
}
void N2Perror(const char* msg) {
    if (msg && *msg) std::fprintf(stderr, "%s: errno=%d\n", msg, *GuestErrnoPtr());
}
void N2Setbuf(void*, char*) {}
int N2Setvbuf(void*, char*, int, uint64_t) { return 0; }
int32_t N2Getwc(void* s) { return N2Getc(s); }
int32_t N2Ungetwc(int32_t wc, void* s) { return N2Ungetc(wc, s); }
int32_t N2Fputwc(int32_t wc, void* s) { return N2Fputc(wc & 0xff, s); }

int64_t N2Getline(char** lineptr, uint64_t* n, void* stream) {
    if (!lineptr || !n || !stream) return SetErr64(kEinval);
    if (!*lineptr || *n < 128) {
        *n = 128;
        *lineptr = static_cast<char*>(GuestHeapRealloc(*lineptr, *n));
        if (!*lineptr) return -1;
    }
    uint64_t len = 0;
    for (;;) {
        const int ch = N2Getc(stream);
        if (ch < 0) {
            if (len == 0) return -1;
            break;
        }
        if (len + 2 >= *n) {
            *n *= 2;
            char* grown = static_cast<char*>(GuestHeapRealloc(*lineptr, *n));
            if (!grown) return -1;
            *lineptr = grown;
        }
        (*lineptr)[len++] = static_cast<char>(ch);
        if (ch == '\n') break;
    }
    (*lineptr)[len] = '\0';
    return static_cast<int64_t>(len);
}

// Android ARM64 `struct dirent` (280 bytes).
struct GuestDirent {
    uint64_t d_ino;
    int64_t d_off;
    uint16_t d_reclen;
    uint8_t d_type;
    char d_name[256];
};
static_assert(sizeof(GuestDirent) == 280);

struct GuestDir {
    std::wstring path;
    std::vector<WIN32_FIND_DATAW> entries;
    size_t index = 0;
    GuestDirent current{};
};

void* N2OpenDir(const char* path) {
    SpecialPath sp = SpecialPath::None;
    std::wstring hostPath;
    if (!ResolveGuestPath(path, hostPath, sp) || sp != SpecialPath::None) return nullptr;
    const DWORD attrs = GetFileAttributesW(hostPath.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        SetErr(attrs == INVALID_FILE_ATTRIBUTES ? kEnoent : kEnotdir);
        return nullptr;
    }
    auto dir = std::make_unique<GuestDir>();
    dir->path = hostPath;
    WIN32_FIND_DATAW fd{};
    HANDLE h = FindFirstFileW((hostPath + L"\\*").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do { dir->entries.push_back(fd); } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    return dir.release();
}

void* N2FdOpenDir(int fd) {
    auto ent = GetFd(fd);
    if (!ent || ent->kind != FdKind::DiskFile) { SetErr(kEbadf); return nullptr; }
    wchar_t buf[4096]{};
    const DWORD len = GetFinalPathNameByHandleW(ent->winHandle, buf, 4096, FILE_NAME_NORMALIZED);
    if (!len || len >= 4096) { SetErr(kEbadf); return nullptr; }
    return N2OpenDir(WideToUtf8(std::wstring(buf, len)).c_str());
}

GuestDirent* N2ReadDir(void* dirPtr) {
    auto* dir = static_cast<GuestDir*>(dirPtr);
    if (!dir || dir->index >= dir->entries.size()) return nullptr;
    const auto& fd = dir->entries[dir->index++];
    std::memset(&dir->current, 0, sizeof(dir->current));
    dir->current.d_ino = dir->index;
    dir->current.d_off = static_cast<int64_t>(dir->index);
    dir->current.d_reclen = sizeof(GuestDirent);
    dir->current.d_type = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? 4 /* DT_DIR */ : 8 /* DT_REG */;
    const std::string name8 = WideToUtf8(fd.cFileName);
    std::snprintf(dir->current.d_name, sizeof(dir->current.d_name), "%s", name8.c_str());
    return &dir->current;
}

void N2RewindDir(void* dirPtr) { if (dirPtr) static_cast<GuestDir*>(dirPtr)->index = 0; }
int N2CloseDir(void* dirPtr) { delete static_cast<GuestDir*>(dirPtr); return 0; }

// ---- Filesystem Operations ----

int N2Access(const char* path, int) {
    SpecialPath sp = SpecialPath::None; std::wstring hp;
    if (!ResolveGuestPath(path, hp, sp)) return -1;
    if (sp != SpecialPath::None) return 0;
    return GetFileAttributesW(hp.c_str()) != INVALID_FILE_ATTRIBUTES ? 0 : SetErr(kEnoent);
}
int N2FaccessAt(int dirfd, const char* path, int mode, int) {
    if (!path) return SetErr(kEfault);
    if (path[0] == '/' || dirfd == -100) return N2Access(path, mode);
    GuestStat64 st{};
    return N2FstatAt(dirfd, path, &st, 0);
}
int N2Mkdir(const char* path, uint32_t) {
    SpecialPath sp = SpecialPath::None; std::wstring hp;
    if (!ResolveGuestPath(path, hp, sp) || sp != SpecialPath::None) return -1;
    if (IsReadOnlyHostFontsPath(hp)) return SetErr(kEacces);
    return CreateDirectoryW(hp.c_str(), nullptr) ? 0 : SetErr(MapWinError(GetLastError()));
}
int N2MkdirAt(int dirfd, const char* path, uint32_t mode) {
    if (!path) return SetErr(kEfault);
    if (path[0] == '/' || dirfd == -100) return N2Mkdir(path, mode);
    return SetErr(kEacces);
}
int N2Rmdir(const char* path) {
    SpecialPath sp = SpecialPath::None; std::wstring hp;
    if (!ResolveGuestPath(path, hp, sp) || sp != SpecialPath::None) return -1;
    if (IsReadOnlyHostFontsPath(hp)) return SetErr(kEacces);
    return RemoveDirectoryW(hp.c_str()) ? 0 : SetErr(MapWinError(GetLastError()));
}
int N2Unlink(const char* path) {
    SpecialPath sp = SpecialPath::None; std::wstring hp;
    if (!ResolveGuestPath(path, hp, sp) || sp != SpecialPath::None) return -1;
    if (IsReadOnlyHostFontsPath(hp)) return SetErr(kEacces);
    return DeleteFileW(hp.c_str()) ? 0 : SetErr(MapWinError(GetLastError()));
}
int N2UnlinkAt(int dirfd, const char* path, int flags) {
    (void)dirfd;
    return (flags & 0x200 /* AT_REMOVEDIR */) ? N2Rmdir(path) : N2Unlink(path);
}
int N2Remove(const char* path) {
    SpecialPath sp = SpecialPath::None; std::wstring hp;
    if (!ResolveGuestPath(path, hp, sp) || sp != SpecialPath::None) return -1;
    if (IsReadOnlyHostFontsPath(hp)) return SetErr(kEacces);
    const DWORD a = GetFileAttributesW(hp.c_str());
    if (a == INVALID_FILE_ATTRIBUTES) return SetErr(kEnoent);
    return (a & FILE_ATTRIBUTE_DIRECTORY) ? N2Rmdir(path) : N2Unlink(path);
}
int N2Rename(const char* oldp, const char* newp) {
    SpecialPath s1 = SpecialPath::None, s2 = SpecialPath::None;
    std::wstring h1, h2;
    if (!ResolveGuestPath(oldp, h1, s1) || !ResolveGuestPath(newp, h2, s2)) return -1;
    if (IsReadOnlyHostFontsPath(h1) || IsReadOnlyHostFontsPath(h2)) return SetErr(kEacces);
    return MoveFileExW(h1.c_str(), h2.c_str(), MOVEFILE_REPLACE_EXISTING) ? 0 : SetErr(MapWinError(GetLastError()));
}
int N2RenameAt(int, const char* oldp, int, const char* newp) { return N2Rename(oldp, newp); }

char* N2Realpath(const char* path, char* resolved) {
    SpecialPath sp = SpecialPath::None; std::wstring hp;
    if (!ResolveGuestPath(path, hp, sp)) return nullptr;
    std::string out = sp != SpecialPath::None ? std::string(path) : WideToUtf8(hp);
    for (char& c : out) if (c == '\\') c = '/';
    if (!resolved) {
        resolved = static_cast<char*>(GuestHeapMalloc(out.size() + 1));
        if (!resolved) return nullptr;
    }
    std::snprintf(resolved, 4096, "%s", out.c_str());
    return resolved;
}

char* N2Getcwd(char* buf, uint64_t size) {
    if (!buf || size == 0) { SetErr(kEinval); return nullptr; }
    const char* v = "/data/local/tmp";
    if (size <= std::strlen(v)) { SetErr(kErange); return nullptr; }
    std::strcpy(buf, v);
    return buf;
}
int N2Chdir(const char* path) {
    SpecialPath sp = SpecialPath::None; std::wstring hp;
    if (!ResolveGuestPath(path, hp, sp) || sp != SpecialPath::None) return -1;
    std::lock_guard lk(g_cwdMu);
    g_cwd = hp;
    return 0;
}
int N2Ftruncate(int fd, int64_t length) {
    auto ent = GetFd(fd);
    if (!ent || ent->kind != FdKind::DiskFile || length < 0) return SetErr(kEinval);
    LARGE_INTEGER pos{}, saved{};
    SetFilePointerEx(ent->winHandle, pos, &saved, FILE_CURRENT);
    pos.QuadPart = length;
    const bool ok = SetFilePointerEx(ent->winHandle, pos, nullptr, FILE_BEGIN) && SetEndOfFile(ent->winHandle);
    SetFilePointerEx(ent->winHandle, saved, nullptr, FILE_BEGIN);
    return ok ? 0 : SetErr(MapWinError(GetLastError()));
}
int N2Truncate(const char* path, int64_t length) {
    const int fd = N2OpenImpl(path, 1, 0);
    if (fd < 0) return -1;
    const int rc = N2Ftruncate(fd, length);
    N2Close(fd);
    return rc;
}
int N2Fsync(int fd) {
    auto ent = GetFd(fd);
    if (!ent) return SetErr(kEbadf);
    if (ent->kind == FdKind::DiskFile) FlushFileBuffers(ent->winHandle);
    return 0;
}
int N2Mkstemp(char* tmpl) {
    if (!tmpl) return SetErr(kEinval);
    const size_t n = std::strlen(tmpl);
    if (n < 6 || std::strcmp(tmpl + n - 6, "XXXXXX") != 0) return SetErr(kEinval);
    static constexpr char kChars[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    for (int tries = 0; tries < 32; ++tries) {
        unsigned int r = 0;
        rand_s(&r);
        for (int i = 0; i < 6; ++i) { tmpl[n - 6 + i] = kChars[r % 36]; r /= 36; }
        const int fd = N2OpenImpl(tmpl, 2 | 0100 | 0200, 0600);
        if (fd >= 0) return fd;
    }
    return SetErr(kEexist);
}

int64_t N2SendFile(int outFd, int inFd, int64_t* offset, uint64_t count) {
    std::vector<uint8_t> buf(static_cast<size_t>(std::min<uint64_t>(count, 65536)));
    uint64_t total = 0;
    while (total < count) {
        const uint64_t ask = std::min<uint64_t>(buf.size(), count - total);
        const int64_t got = offset ? N2Pread(inFd, buf.data(), ask, *offset) : N2Read(inFd, buf.data(), ask);
        if (got <= 0) return total > 0 ? static_cast<int64_t>(total) : got;
        if (offset) *offset += got;
        const int64_t put = N2Write(outFd, buf.data(), static_cast<uint64_t>(got));
        if (put <= 0) return total > 0 ? static_cast<int64_t>(total) : put;
        total += static_cast<uint64_t>(put);
    }
    return static_cast<int64_t>(total);
}

int N2StatFs(const char*, void* buf) {
    if (!buf) return SetErr(kEfault);
    std::memset(buf, 0, 120); // Android ARM64 `struct statfs` / `statvfs`
    *reinterpret_cast<uint64_t*>(static_cast<uint8_t*>(buf) + 8) = 4096;
    *reinterpret_cast<uint64_t*>(static_cast<uint8_t*>(buf) + 16) = 1048576;
    *reinterpret_cast<uint64_t*>(static_cast<uint8_t*>(buf) + 24) = 524288;
    *reinterpret_cast<uint64_t*>(static_cast<uint8_t*>(buf) + 32) = 524288;
    return 0;
}
int N2FstatFs(int, void* buf) { return N2StatFs("/", buf); }

int64_t N2ReadLink(const char*, char*, uint64_t) { return SetErr64(kEinval); }
int64_t N2ReadLinkAt(int, const char*, char*, uint64_t) { return SetErr64(kEinval); }
int64_t N2ReadLinkChk(const char*, char*, uint64_t, uint64_t) { return SetErr64(kEinval); }
int N2Symlink(const char*, const char*) { return SetErr(kEperm); }
int N2SymlinkAt(const char*, int, const char*) { return SetErr(kEperm); }
int N2Link(const char*, const char*) { return SetErr(kEperm); }
int N2Fchmod(int, uint32_t) { return 0; }
int N2FchmodAt(int, const char*, uint32_t, int) { return 0; }
int N2Utime(const char*, const void*) { return 0; }
int N2UtimensAt(int, const char*, const void*, int) { return 0; }
int N2PosixFadvise(int, int64_t, int64_t, int) { return 0; }
int N2Isatty(int) { SetErr(kEnotty); return 0; }
int64_t N2Pathconf(const char*, int) { return 4096; }

// ---- W^X Virtual Memory (`mmap` / `munmap` / `mprotect` / `madvise`) ----

constexpr uintptr_t kMmapPageSize = 4096u;
uintptr_t DownMmapPage(uintptr_t v) { return v & ~(kMmapPageSize - 1u); }
uintptr_t UpMmapPage(uintptr_t v) { return (v + kMmapPageSize - 1u) & ~(kMmapPageSize - 1u); }

struct MmapRegion {
    void* base = nullptr;
    uint64_t size = 0;
    std::vector<std::pair<uintptr_t, uintptr_t>> liveSpans;
};
std::mutex g_mmapMu;
std::map<uintptr_t, MmapRegion> g_mmaps;

DWORD GuestProtToWin(int prot, bool& validOut) {
    validOut = true;
    const bool r = (prot & 1) != 0, w = (prot & 2) != 0, x = (prot & 4) != 0;
    if (w && x) { validOut = false; return 0; } // W^X strictly enforced (`AGENTS.md`).
    if (x) return r ? PAGE_EXECUTE_READ : PAGE_EXECUTE;
    if (w) return PAGE_READWRITE;
    if (r) return PAGE_READONLY;
    return PAGE_NOACCESS;
}

void* N2Mmap(void* addr, uint64_t length, int prot, int flags, int fd, int64_t offset) {
    if (length == 0) { SetErr(kEinval); return reinterpret_cast<void*>(static_cast<intptr_t>(-1)); }
    bool protOk = false;
    const DWORD winProt = GuestProtToWin(prot, protOk);
    if (!protOk) { SetErr(kEinval); return reinterpret_cast<void*>(static_cast<intptr_t>(-1)); }

    const uint64_t alignedLen = static_cast<uint64_t>(UpMmapPage(static_cast<uintptr_t>(length)));
    const bool fixed = (flags & 0x10 /* MAP_FIXED */) != 0;
    const bool anon = (flags & 0x20 /* MAP_ANONYMOUS */) != 0;

    if (fixed && addr) {
        // Sub-range re-commit / zeroing inside an already-reserved guest mmap region (used by Dart VM heap).
        const auto target = reinterpret_cast<uintptr_t>(addr);
        bool inside = false;
        {
            std::lock_guard lk(g_mmapMu);
            for (auto& [baseAddr, reg] : g_mmaps) {
                if (target >= baseAddr && target + alignedLen <= baseAddr + reg.size) {
                    inside = true;
                    // Ensure [target, target + alignedLen) is tracked in liveSpans.
                    reg.liveSpans.emplace_back(target, target + static_cast<uintptr_t>(alignedLen));
                    std::sort(reg.liveSpans.begin(), reg.liveSpans.end());
                    std::vector<std::pair<uintptr_t, uintptr_t>> merged;
                    for (const auto& [s, e] : reg.liveSpans) {
                        if (!merged.empty() && s <= merged.back().second) {
                            merged.back().second = std::max(merged.back().second, e);
                        } else {
                            merged.emplace_back(s, e);
                        }
                    }
                    reg.liveSpans = std::move(merged);
                    break;
                }
            }
        }
        if (inside) {
            if (prot == 0) {
                VirtualFree(addr, static_cast<SIZE_T>(alignedLen), MEM_DECOMMIT);
                return addr;
            }
            if (VirtualAlloc(addr, static_cast<SIZE_T>(alignedLen), MEM_COMMIT, PAGE_READWRITE)) {
                if (anon) std::memset(addr, 0, static_cast<size_t>(alignedLen));
                else if (fd >= 0) N2Pread(fd, addr, length, offset);
                DWORD old = 0;
                if (winProt != PAGE_READWRITE) VirtualProtect(addr, static_cast<SIZE_T>(alignedLen), winProt, &old);
                if (prot & 4) FlushInstructionCache(GetCurrentProcess(), addr, static_cast<SIZE_T>(alignedLen));
                return addr;
            }
        }
    }

    const DWORD allocFlags = (prot == 0) ? MEM_RESERVE : (MEM_RESERVE | MEM_COMMIT);
    const DWORD initProt = (prot == 0) ? PAGE_NOACCESS : PAGE_READWRITE;
    void* mem = VirtualAlloc(fixed ? addr : nullptr, static_cast<SIZE_T>(alignedLen), allocFlags, initProt);
    if (!mem && !fixed) {
        mem = VirtualAlloc(nullptr, static_cast<SIZE_T>(alignedLen), allocFlags, initProt);
    }
    if (!mem) { SetErr(kEnomem); return reinterpret_cast<void*>(static_cast<intptr_t>(-1)); }

    if (!anon && fd >= 0) {
        if (prot == 0) VirtualAlloc(mem, static_cast<SIZE_T>(alignedLen), MEM_COMMIT, PAGE_READWRITE);
        N2Pread(fd, mem, length, offset);
    }
    if (prot != 0 && winProt != PAGE_READWRITE) {
        DWORD old = 0;
        VirtualProtect(mem, static_cast<SIZE_T>(alignedLen), winProt, &old);
    }
    if (prot & 4) FlushInstructionCache(GetCurrentProcess(), mem, static_cast<SIZE_T>(alignedLen));

    {
        std::lock_guard lk(g_mmapMu);
        const auto mStart = reinterpret_cast<uintptr_t>(mem);
        g_mmaps[mStart] = {mem, alignedLen, {{mStart, mStart + static_cast<uintptr_t>(alignedLen)}}};
    }
    return mem;
}

int N2Munmap(void* addr, uint64_t length) {
    if (!addr || length == 0) return SetErr(kEinval);
    const uintptr_t uStart = DownMmapPage(reinterpret_cast<uintptr_t>(addr));
    const uintptr_t uEnd = UpMmapPage(reinterpret_cast<uintptr_t>(addr) + static_cast<uintptr_t>(length));
    if (uEnd <= uStart) return SetErr(kEinval);

    bool matchedRegion = false;
    {
        std::lock_guard lk(g_mmapMu);
        for (auto it = g_mmaps.begin(); it != g_mmaps.end();) {
            const uintptr_t rStart = it->first;
            const uintptr_t rEnd = rStart + static_cast<uintptr_t>(it->second.size);
            if (uEnd <= rStart || uStart >= rEnd) {
                ++it;
                continue;
            }
            matchedRegion = true;
            const uintptr_t clipStart = std::max(uStart, rStart);
            const uintptr_t clipEnd = std::min(uEnd, rEnd);

            std::vector<std::pair<uintptr_t, uintptr_t>> nextSpans;
            for (const auto& [s, e] : it->second.liveSpans) {
                if (e <= clipStart || s >= clipEnd) {
                    nextSpans.emplace_back(s, e);
                } else {
                    if (s < clipStart) nextSpans.emplace_back(s, clipStart);
                    if (e > clipEnd) nextSpans.emplace_back(clipEnd, e);
                }
            }
            if (nextSpans.empty()) {
                VirtualFree(it->second.base, 0, MEM_RELEASE);
                it = g_mmaps.erase(it);
            } else {
                // Partial unmap (e.g. Dart `VirtualMemory::AllocateAligned` prefix/suffix trim):
                // decommit the unmapped pages while preserving the remaining `MEM_RESERVE` cage!
                VirtualFree(reinterpret_cast<void*>(clipStart), clipEnd - clipStart, MEM_DECOMMIT);
                it->second.liveSpans = std::move(nextSpans);
                ++it;
            }
        }
    }
    if (!matchedRegion) {
        VirtualFree(reinterpret_cast<void*>(uStart), uEnd - uStart, MEM_DECOMMIT);
    }
    return 0;
}

int N2Mprotect(void* addr, uint64_t len, int prot) {
    if (!addr || len == 0) return 0;
    bool protOk = false;
    const DWORD winProt = GuestProtToWin(prot, protOk);
    if (!protOk) return SetErr(kEinval);

    const uintptr_t pStart = DownMmapPage(reinterpret_cast<uintptr_t>(addr));
    const uintptr_t pEnd = UpMmapPage(reinterpret_cast<uintptr_t>(addr) + static_cast<uintptr_t>(len));
    uintptr_t cur = pStart;
    while (cur < pEnd) {
        MEMORY_BASIC_INFORMATION mbi{};
        if (!VirtualQuery(reinterpret_cast<const void*>(cur), &mbi, sizeof(mbi)) || mbi.RegionSize == 0) {
            return SetErr(kEnomem);
        }
        const uintptr_t mbiEnd = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
        const uintptr_t chunkEnd = std::min(pEnd, mbiEnd > cur ? mbiEnd : pEnd);
        const SIZE_T chunkLen = chunkEnd - cur;
        if (mbi.State == MEM_FREE) return SetErr(kEnomem);
        if (prot == 0) {
            if (mbi.State == MEM_COMMIT) {
                DWORD old = 0;
                if (!VirtualProtect(reinterpret_cast<void*>(cur), chunkLen, PAGE_NOACCESS, &old)) {
                    return SetErr(kEnomem);
                }
            }
        } else {
            if (mbi.State == MEM_RESERVE) {
                if (!VirtualAlloc(reinterpret_cast<void*>(cur), chunkLen, MEM_COMMIT, PAGE_READWRITE)) {
                    return SetErr(kEnomem);
                }
            }
            DWORD old = 0;
            if (!VirtualProtect(reinterpret_cast<void*>(cur), chunkLen, winProt, &old)) {
                return SetErr(kEnomem);
            }
        }
        cur = chunkEnd;
    }
    if (prot & 4) FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(pStart), pEnd - pStart);
    return 0;
}

int N2Madvise(void*, uint64_t, int) { return 0; }
int N2PosixMadvise(void*, uint64_t, int) { return 0; }
int N2Mlock(const void*, uint64_t) { return 0; }
int N2Munlock(const void*, uint64_t) { return 0; }

// ---- Pthread Attributes, `pthread_once`, `rwlock`, `sem_t`, Affinity ----

struct GuestPthreadAttr {
    uint32_t flags, pad;
    void* stack_base;
    uint64_t stack_size, guard_size;
    int32_t sched_policy, sched_priority;
    char reserved[16];
};

int N2AttrInit(GuestPthreadAttr* a) {
    if (!a) return kEinval;
    std::memset(a, 0, sizeof(*a));
    a->stack_size = 1024 * 1024;
    a->guard_size = 4096;
    return 0;
}
int N2AttrDestroy(GuestPthreadAttr*) { return 0; }
int N2AttrSetDetachState(GuestPthreadAttr* a, int state) {
    if (!a || (state != 0 && state != 1)) return kEinval;
    if (state == 1) a->flags |= 1u; else a->flags &= ~1u;
    return 0;
}
int N2AttrSetStackSize(GuestPthreadAttr* a, uint64_t sz) {
    if (!a || sz < 16384) return kEinval;
    a->stack_size = sz;
    return 0;
}
int N2AttrGetStack(const GuestPthreadAttr* a, void** base, uint64_t* sz) {
    if (!a || !base || !sz) return kEinval;
    *base = a->stack_base;
    *sz = a->stack_size;
    return 0;
}
int N2GetAttrNp(int64_t, GuestPthreadAttr* a) {
    if (!a) return kEinval;
    std::memset(a, 0, sizeof(*a));
    ULONG_PTR low = 0, high = 0;
    GetCurrentThreadStackLimits(&low, &high);
    a->stack_base = reinterpret_cast<void*>(low);
    a->stack_size = high > low ? static_cast<uint64_t>(high - low) : (1024 * 1024);
    a->guard_size = 4096;
    return 0;
}
int N2SetNameNp(int64_t, const char* name) {
    if (name) SetThreadDescription(GetCurrentThread(), Utf8ToWide(name).c_str());
    return 0;
}
int N2GetSchedParam(int64_t, int* policy, int* prio) {
    if (policy) *policy = 0;
    if (prio) *prio = 0;
    return 0;
}
int N2SetSchedParam(int64_t, int, const int*) { return 0; }
int N2GetCpuClockId(int64_t, int* clk) { if (clk) *clk = 3; return 0; }
int N2PthreadSigmask(int, const void*, void*) { return 0; }

__declspec(guard(nocf)) void InvokeOnceInit(void (*fn)()) { fn(); }

std::mutex g_onceMu;
std::condition_variable g_onceCv;

int N2PthreadOnce(int* onceControl, void (*initRoutine)()) {
    if (!onceControl || !initRoutine) return kEinval;
    auto* atom = reinterpret_cast<std::atomic<int>*>(onceControl);
    if (atom->load(std::memory_order_acquire) == 2) return 0;
    {
        std::unique_lock lk(g_onceMu);
        while (atom->load(std::memory_order_acquire) == 1) g_onceCv.wait(lk);
        if (atom->load(std::memory_order_acquire) == 2) return 0;
        atom->store(1, std::memory_order_release);
    }
    InvokeOnceInit(initRoutine);
    {
        std::lock_guard lk(g_onceMu);
        atom->store(2, std::memory_order_release);
    }
    g_onceCv.notify_all();
    return 0;
}

// `pthread_rwlock_t` (56 bytes on Android ARM64; reader/writer state stored off-heap).
std::mutex g_rwMu;
struct RwlockRecord {
    std::mutex mu;
    std::condition_variable cv;
    int readers = 0;
    bool writer = false;
};
std::map<void*, std::unique_ptr<RwlockRecord>> g_rwRecs;
RwlockRecord& GetRwRec(void* rw) {
    std::lock_guard lk(g_rwMu);
    auto& p = g_rwRecs[rw];
    if (!p) p = std::make_unique<RwlockRecord>();
    return *p;
}
int N2RwInit(void* rw, const void*) { if (!rw) return kEinval; (void)GetRwRec(rw); return 0; }
int N2RwDestroy(void* rw) { std::lock_guard lk(g_rwMu); g_rwRecs.erase(rw); return 0; }
int N2RwRdlock(void* rw) {
    if (!rw) return kEinval;
    auto& r = GetRwRec(rw);
    std::unique_lock lk(r.mu);
    while (r.writer) r.cv.wait(lk);
    ++r.readers;
    return 0;
}
int N2RwWrlock(void* rw) {
    if (!rw) return kEinval;
    auto& r = GetRwRec(rw);
    std::unique_lock lk(r.mu);
    while (r.writer || r.readers > 0) r.cv.wait(lk);
    r.writer = true;
    return 0;
}
int N2RwUnlock(void* rw) {
    if (!rw) return kEinval;
    auto& r = GetRwRec(rw);
    {
        std::lock_guard lk(r.mu);
        if (r.writer) r.writer = false;
        else if (r.readers > 0) --r.readers;
    }
    r.cv.notify_all();
    return 0;
}

// `sem_t` (16 bytes on Android ARM64).
struct SemRecord {
    std::mutex mu;
    std::condition_variable cv;
    uint32_t count = 0;
};
std::mutex g_semMu;
std::map<void*, std::unique_ptr<SemRecord>> g_sems;
SemRecord* FindSem(void* sem, bool create = false, uint32_t initVal = 0) {
    if (!sem) return nullptr;
    std::lock_guard lk(g_semMu);
    auto it = g_sems.find(sem);
    if (it != g_sems.end()) return it->second.get();
    if (!create) return nullptr;
    auto rec = std::make_unique<SemRecord>();
    rec->count = initVal;
    auto* raw = rec.get();
    g_sems[sem] = std::move(rec);
    return raw;
}
int N2SemInit(void* sem, int, unsigned int val) {
    if (!sem) return SetErr(kEinval);
    std::lock_guard lk(g_semMu);
    auto rec = std::make_unique<SemRecord>();
    rec->count = val;
    g_sems[sem] = std::move(rec);
    return 0;
}
int N2SemDestroy(void* sem) {
    std::lock_guard lk(g_semMu);
    g_sems.erase(sem);
    return 0;
}
int N2SemWait(void* sem) {
    auto* r = FindSem(sem, true, 0);
    if (!r) return SetErr(kEinval);
    std::unique_lock lk(r->mu);
    while (r->count == 0) r->cv.wait(lk);
    --r->count;
    return 0;
}
int N2SemTryWait(void* sem) {
    auto* r = FindSem(sem, true, 0);
    if (!r) return SetErr(kEinval);
    std::lock_guard lk(r->mu);
    if (r->count == 0) return SetErr(kEagain);
    --r->count;
    return 0;
}
int N2SemPost(void* sem) {
    auto* r = FindSem(sem, true, 0);
    if (!r) return SetErr(kEinval);
    { std::lock_guard lk(r->mu); ++r->count; }
    r->cv.notify_one();
    return 0;
}

int N2SchedYield() { SwitchToThread(); return 0; }
int N2SchedGetAffinity(int, uint64_t cpusetsize, uint8_t* mask) {
    if (!mask || cpusetsize == 0) return SetErr(kEinval);
    std::memset(mask, 0, static_cast<size_t>(cpusetsize));
    SYSTEM_INFO si{}; GetSystemInfo(&si);
    const uint32_t cores = std::min<uint32_t>(64, std::max<DWORD>(1, si.dwNumberOfProcessors));
    for (uint32_t i = 0; i < cores && (i / 8) < cpusetsize; ++i) mask[i / 8] |= static_cast<uint8_t>(1u << (i % 8));
    return 0;
}
int N2SchedSetAffinity(int, uint64_t, const void*) { return 0; }
int N2SchedCpuCount(uint64_t setsize, const uint8_t* set) {
    if (!set) return 0;
    int total = 0;
    for (uint64_t i = 0; i < setsize; ++i) {
        uint8_t v = set[i];
        while (v) { total += v & 1; v >>= 1; }
    }
    return total;
}

// ---- C++ Static Guard (`__cxa_guard_acquire` / `release` / `abort`) ----
std::mutex g_cxaGuardMu;
std::condition_variable g_cxaGuardCv;

int N2CxaGuardAcquire(uint64_t* guard) {
    auto* byte0 = reinterpret_cast<std::atomic<uint8_t>*>(guard);
    if (byte0->load(std::memory_order_acquire) != 0) return 0;
    std::unique_lock lk(g_cxaGuardMu);
    auto* state = reinterpret_cast<uint8_t*>(guard);
    while (state[0] == 0 && state[1] == 1) g_cxaGuardCv.wait(lk);
    if (state[0] != 0) return 0;
    state[1] = 1;
    return 1;
}
void N2CxaGuardRelease(uint64_t* guard) {
    {
        std::lock_guard lk(g_cxaGuardMu);
        auto* state = reinterpret_cast<uint8_t*>(guard);
        state[1] = 0;
        reinterpret_cast<std::atomic<uint8_t>*>(guard)->store(1, std::memory_order_release);
    }
    g_cxaGuardCv.notify_all();
}
void N2CxaGuardAbort(uint64_t* guard) {
    {
        std::lock_guard lk(g_cxaGuardMu);
        reinterpret_cast<uint8_t*>(guard)[1] = 0;
    }
    g_cxaGuardCv.notify_all();
}
void N2CxaPureVirtual() { std::fprintf(stderr, "GUEST_EXECUTION_FAULT: __cxa_pure_virtual\n"); TerminateProcess(GetCurrentProcess(), 3); }

// ---- System Properties, `getauxval`, `sysconf`, Time, Locale, Ctype, Wchar, Strings ----

std::mutex g_propMu;
std::map<std::string, std::string> g_sysProps = {
    {"ro.build.version.sdk", "34"},
    {"ro.build.version.release", "14"},
    {"ro.product.cpu.abi", "arm64-v8a"},
    {"ro.hardware", "windows-arm64"},
    {"ro.debuggable", "0"},
};

int N2SysPropGet(const char* name, char* value) {
    if (!value) return 0;
    value[0] = '\0';
    if (!name) return 0;
    std::lock_guard lk(g_propMu);
    auto it = g_sysProps.find(name);
    if (it == g_sysProps.end()) return 0;
    std::snprintf(value, 92, "%s", it->second.c_str());
    return static_cast<int>(std::min<size_t>(it->second.size(), 91));
}

const void* N2SysPropFind(const char* name) {
    if (!name) return nullptr;
    std::lock_guard lk(g_propMu);
    auto it = g_sysProps.find(name);
    return it != g_sysProps.end() ? reinterpret_cast<const void*>(&it->first) : nullptr;
}

uint64_t N2GetAuxVal(uint64_t type) {
    if (type == 6 /* AT_PAGESZ */) return 4096;
    if (type == 16 /* AT_HWCAP */) {
        // Basic ARMv8-A baseline: FP(0) | ASIMD(1) | AES(3) | PMULL(4) | SHA1(5) | SHA2(6) | CRC32(7) | ATOMICS(8)
        return (1ull << 0) | (1ull << 1) | (1ull << 3) | (1ull << 4) |
               (1ull << 5) | (1ull << 6) | (1ull << 7) | (1ull << 8);
    }
    return 0;
}

int N2GetPageSize() { return 4096; }

int64_t N2Sysconf(int name) {
    SYSTEM_INFO si{}; GetSystemInfo(&si);
    const int64_t cores = std::max<DWORD>(1, si.dwNumberOfProcessors);
    switch (name) {
        case 0x0027 /* _SC_PAGESIZE */: case 0x0028 /* _SC_PAGE_SIZE */: return 4096;
        case 0x0060 /* _SC_NPROCESSORS_CONF */: case 0x0061 /* _SC_NPROCESSORS_ONLN */: return cores;
        case 0x0006 /* _SC_CLK_TCK */: return 100;
        case 0x0004 /* _SC_OPEN_MAX */: return 4096;
        default: return 4096;
    }
}

int N2GetTimeOfDay(GuestTimeval* tv, void*) {
    if (tv) {
        FILETIME ft{};
        GetSystemTimePreciseAsFileTime(&ft);
        const GuestTimespec ts = FiletimeToUnix(ft);
        tv->sec = ts.sec;
        tv->usec = ts.nsec / 1000LL;
    }
    return 0;
}

int64_t N2Time(int64_t* tloc) {
    GuestTimeval tv{};
    N2GetTimeOfDay(&tv, nullptr);
    if (tloc) *tloc = tv.sec;
    return tv.sec;
}

int64_t N2Clock() { return static_cast<int64_t>(MonoNowNs() / 1000ull); }

// Android ARM64 `struct tm` (56 bytes, including `long tm_gmtoff; const char* tm_zone;`).
struct GuestTm {
    int tm_sec, tm_min, tm_hour, tm_mday, tm_mon, tm_year, tm_wday, tm_yday, tm_isdst;
    int pad;
    int64_t tm_gmtoff;
    const char* tm_zone;
};
static_assert(sizeof(GuestTm) == 56);

void HostTmToGuest(const std::tm& h, GuestTm* g, const char* zone) {
    std::memset(g, 0, sizeof(*g));
    g->tm_sec = h.tm_sec; g->tm_min = h.tm_min; g->tm_hour = h.tm_hour;
    g->tm_mday = h.tm_mday; g->tm_mon = h.tm_mon; g->tm_year = h.tm_year;
    g->tm_wday = h.tm_wday; g->tm_yday = h.tm_yday; g->tm_isdst = h.tm_isdst;
    g->tm_zone = zone;
}
void GuestTmToHost(const GuestTm& g, std::tm& h) {
    std::memset(&h, 0, sizeof(h));
    h.tm_sec = g.tm_sec; h.tm_min = g.tm_min; h.tm_hour = g.tm_hour;
    h.tm_mday = g.tm_mday; h.tm_mon = g.tm_mon; h.tm_year = g.tm_year;
    h.tm_wday = g.tm_wday; h.tm_yday = g.tm_yday; h.tm_isdst = g.tm_isdst;
}

GuestTm* N2LocaltimeR(const int64_t* timep, GuestTm* res) {
    if (!timep || !res) return nullptr;
    std::time_t t = static_cast<std::time_t>(*timep);
    std::tm h{};
    if (localtime_s(&h, &t) != 0) return nullptr;
    HostTmToGuest(h, res, "LOCAL");
    return res;
}
GuestTm* N2Localtime(const int64_t* timep) {
    thread_local GuestTm buf{};
    return N2LocaltimeR(timep, &buf);
}
GuestTm* N2GmtimeR(const int64_t* timep, GuestTm* res) {
    if (!timep || !res) return nullptr;
    std::time_t t = static_cast<std::time_t>(*timep);
    std::tm h{};
    if (gmtime_s(&h, &t) != 0) return nullptr;
    HostTmToGuest(h, res, "UTC");
    return res;
}
int64_t N2Mktime(GuestTm* g) {
    if (!g) return -1;
    std::tm h{};
    GuestTmToHost(*g, h);
    return static_cast<int64_t>(std::mktime(&h));
}
uint64_t N2Strftime(char* s, uint64_t max, const char* fmt, const GuestTm* g) {
    if (!s || max == 0 || !fmt || !g) return 0;
    std::tm h{};
    GuestTmToHost(*g, h);
    return std::strftime(s, static_cast<size_t>(max), fmt, &h);
}
uint64_t N2StrftimeL(char* s, uint64_t max, const char* fmt, const GuestTm* g, void*) {
    return N2Strftime(s, max, fmt, g);
}
void N2Tzset() {}

// Locale & Ctype (`C.UTF-8`):
const char* N2SetLocale(int, const char*) { return "C.UTF-8"; }
void* N2NewLocale(int, const char*, void*) { return reinterpret_cast<void*>(static_cast<uintptr_t>(1)); }
void* N2UseLocale(void* l) { return l ? l : reinterpret_cast<void*>(static_cast<uintptr_t>(1)); }
void N2FreeLocale(void*) {}

struct GuestLconv {
    const char *decimal_point, *thousands_sep, *grouping, *int_curr_symbol, *currency_symbol;
    const char *mon_decimal_point, *mon_thousands_sep, *mon_grouping, *positive_sign, *negative_sign;
    char int_frac_digits, frac_digits, p_cs_precedes, p_sep_by_space, n_cs_precedes, n_sep_by_space;
    char p_sign_posn, n_sign_posn, int_p_cs_precedes, int_p_sep_by_space, int_n_cs_precedes, int_n_sep_by_space;
    char int_p_sign_posn, int_n_sign_posn;
};
GuestLconv* N2LocaleConv() {
    static GuestLconv lc = {".", "", "", "", "", "", "", "", "", "-", 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127};
    return &lc;
}
uint64_t N2MbCurMax() { return 4; }

// Android `wint_t` / `wchar_t` is 32-bit UTF-32:
int N2IswalphaL(uint32_t c, void*) { return c < 0x10000 ? std::iswalpha(static_cast<wint_t>(c)) : 0; }
int N2IswblankL(uint32_t c, void*) { return c == ' ' || c == '\t'; }
int N2IswcntrlL(uint32_t c, void*) { return c < 0x10000 ? std::iswcntrl(static_cast<wint_t>(c)) : 0; }
int N2IswdigitL(uint32_t c, void*) { return c >= '0' && c <= '9'; }
int N2IswlowerL(uint32_t c, void*) { return c < 0x10000 ? std::iswlower(static_cast<wint_t>(c)) : 0; }
int N2IswprintL(uint32_t c, void*) { return c >= 0x20 && c != 0x7f; }
int N2IswpunctL(uint32_t c, void*) { return c < 0x10000 ? std::iswpunct(static_cast<wint_t>(c)) : 0; }
int N2IswspaceL(uint32_t c, void*) { return c == ' ' || (c >= '\t' && c <= '\r'); }
int N2IswupperL(uint32_t c, void*) { return c < 0x10000 ? std::iswupper(static_cast<wint_t>(c)) : 0; }
int N2IswxdigitL(uint32_t c, void*) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }
uint32_t N2TowlowerL(uint32_t c, void*) { return c < 0x10000 ? std::towlower(static_cast<wint_t>(c)) : c; }
uint32_t N2TowupperL(uint32_t c, void*) { return c < 0x10000 ? std::towupper(static_cast<wint_t>(c)) : c; }

// UTF-8 <-> UTF-32 (`wchar_t` = `uint32_t` on Android ARM64, NOT Windows 16-bit `wchar_t`!)
uint32_t N2Btowc(int c) { return (c >= 0 && c < 128) ? static_cast<uint32_t>(c) : 0xffffffffu; }
int N2Wctob(uint32_t wc) { return wc < 128 ? static_cast<int>(wc) : -1; }

uint64_t DecodeUtf8One(const char* s, uint64_t n, uint32_t* outWc) {
    if (!s) return 0;
    if (n == 0) return static_cast<uint64_t>(-2);
    const uint8_t b0 = static_cast<uint8_t>(s[0]);
    if (b0 == 0) { if (outWc) *outWc = 0; return 0; }
    if (b0 < 0x80) { if (outWc) *outWc = b0; return 1; }
    uint32_t len = (b0 & 0xE0) == 0xC0 ? 2 : ((b0 & 0xF0) == 0xE0 ? 3 : ((b0 & 0xF8) == 0xF0 ? 4 : 0));
    if (len == 0) { SetErr(84 /* EILSEQ */); return static_cast<uint64_t>(-1); }
    if (n < len) return static_cast<uint64_t>(-2);
    uint32_t cp = b0 & ((1u << (7 - len)) - 1u);
    for (uint32_t i = 1; i < len; ++i) {
        const uint8_t bi = static_cast<uint8_t>(s[i]);
        if ((bi & 0xC0) != 0x80) { SetErr(84); return static_cast<uint64_t>(-1); }
        cp = (cp << 6) | (bi & 0x3Fu);
    }
    if (outWc) *outWc = cp;
    return len;
}

uint64_t N2Mbrtowc(uint32_t* pwc, const char* s, uint64_t n, void*) { return s ? DecodeUtf8One(s, n, pwc) : 0; }
uint64_t N2Mbrlen(const char* s, uint64_t n, void*) { return s ? DecodeUtf8One(s, n, nullptr) : 0; }
int N2Mbtowc(uint32_t* pwc, const char* s, uint64_t n) {
    if (!s) return 0;
    const uint64_t r = DecodeUtf8One(s, n, pwc);
    return r >= static_cast<uint64_t>(-2) ? -1 : static_cast<int>(r);
}
uint64_t N2Wcrtomb(char* s, uint32_t wc, void*) {
    if (!s) return 1;
    if (wc < 0x80) { s[0] = static_cast<char>(wc); return 1; }
    if (wc < 0x800) { s[0] = static_cast<char>(0xC0 | (wc >> 6)); s[1] = static_cast<char>(0x80 | (wc & 0x3F)); return 2; }
    if (wc < 0x10000) {
        s[0] = static_cast<char>(0xE0 | (wc >> 12));
        s[1] = static_cast<char>(0x80 | ((wc >> 6) & 0x3F));
        s[2] = static_cast<char>(0x80 | (wc & 0x3F));
        return 3;
    }
    if (wc <= 0x10FFFF) {
        s[0] = static_cast<char>(0xF0 | (wc >> 18));
        s[1] = static_cast<char>(0x80 | ((wc >> 12) & 0x3F));
        s[2] = static_cast<char>(0x80 | ((wc >> 6) & 0x3F));
        s[3] = static_cast<char>(0x80 | (wc & 0x3F));
        return 4;
    }
    SetErr(84);
    return static_cast<uint64_t>(-1);
}

uint64_t N2Mbsnrtowcs(uint32_t* dst, const char** src, uint64_t nmc, uint64_t len, void*) {
    if (!src || !*src) return 0;
    const char* p = *src;
    uint64_t rem = nmc, count = 0;
    while (rem > 0 && (!dst || count < len)) {
        uint32_t wc = 0;
        const uint64_t step = DecodeUtf8One(p, rem, &wc);
        if (step == static_cast<uint64_t>(-1)) { *src = p; return static_cast<uint64_t>(-1); }
        if (step == static_cast<uint64_t>(-2)) { p += rem; break; }
        if (dst) dst[count] = wc;
        if (wc == 0) { p = nullptr; break; }
        p += step;
        rem -= step;
        ++count;
    }
    if (dst) *src = p;
    return count;
}
uint64_t N2Mbsrtowcs(uint32_t* dst, const char** src, uint64_t len, void* ps) {
    return N2Mbsnrtowcs(dst, src, src && *src ? std::strlen(*src) + 1 : 0, len, ps);
}
uint64_t N2Wcsnrtombs(char* dst, const uint32_t** src, uint64_t nwc, uint64_t len, void*) {
    if (!src || !*src) return 0;
    const uint32_t* p = *src;
    uint64_t written = 0;
    for (uint64_t i = 0; i < nwc; ++i) {
        char tmp[4];
        const uint64_t n = N2Wcrtomb(tmp, p[i], nullptr);
        if (n == static_cast<uint64_t>(-1)) { *src = p + i; return n; }
        if (p[i] == 0) {
            if (dst) { if (written < len) dst[written] = '\0'; *src = nullptr; }
            return written;
        }
        if (dst && written + n > len) { *src = p + i; return written; }
        if (dst) std::memcpy(dst + written, tmp, static_cast<size_t>(n));
        written += n;
    }
    if (dst) *src = p + nwc;
    return written;
}

uint64_t N2Wcslen(const uint32_t* s) { uint64_t n = 0; if (s) while (s[n]) ++n; return n; }
uint32_t* N2Wmemchr(const uint32_t* s, uint32_t c, uint64_t n) {
    for (uint64_t i = 0; i < n; ++i) if (s[i] == c) return const_cast<uint32_t*>(s + i);
    return nullptr;
}
int N2Wmemcmp(const uint32_t* a, const uint32_t* b, uint64_t n) {
    for (uint64_t i = 0; i < n; ++i) if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
    return 0;
}
int N2WcscollL(const uint32_t* a, const uint32_t* b, void*) {
    while (*a && *a == *b) { ++a; ++b; }
    return *a == *b ? 0 : (*a < *b ? -1 : 1);
}
uint64_t N2WcsxfrmL(uint32_t* dst, const uint32_t* src, uint64_t n, void*) {
    const uint64_t len = N2Wcslen(src);
    if (dst && n > 0) {
        const uint64_t cp = std::min(len, n - 1);
        std::memcpy(dst, src, static_cast<size_t>(cp) * 4);
        dst[cp] = 0;
    }
    return len;
}

std::string Wcs32ToAscii(const uint32_t* s, size_t* countOut = nullptr) {
    std::string out;
    size_t i = 0;
    while (s && s[i] && i < 256) {
        out.push_back(s[i] < 128 ? static_cast<char>(s[i]) : '?');
        ++i;
    }
    if (countOut) *countOut = i;
    return out;
}
int64_t N2Wcstoll(const uint32_t* s, uint32_t** endptr, int base) {
    const std::string a = Wcs32ToAscii(s);
    char* end8 = nullptr;
    const long long v = std::strtoll(a.c_str(), &end8, base);
    if (endptr) *endptr = const_cast<uint32_t*>(s + (end8 ? (end8 - a.c_str()) : 0));
    return v;
}
uint64_t N2Wcstoull(const uint32_t* s, uint32_t** endptr, int base) {
    const std::string a = Wcs32ToAscii(s);
    char* end8 = nullptr;
    const unsigned long long v = std::strtoull(a.c_str(), &end8, base);
    if (endptr) *endptr = const_cast<uint32_t*>(s + (end8 ? (end8 - a.c_str()) : 0));
    return v;
}
double N2Wcstod(const uint32_t* s, uint32_t** endptr) {
    const std::string a = Wcs32ToAscii(s);
    char* end8 = nullptr;
    const double v = std::strtod(a.c_str(), &end8);
    if (endptr) *endptr = const_cast<uint32_t*>(s + (end8 ? (end8 - a.c_str()) : 0));
    return v;
}
float N2Wcstof(const uint32_t* s, uint32_t** endptr) { return static_cast<float>(N2Wcstod(s, endptr)); }

// ---- Strings, Numbers, Sort, Regex, Signals, Process ----

double N2Strtod(const char* s, char** e) { return std::strtod(s, e); }
float N2Strtof(const char* s, char** e) { return std::strtof(s, e); }
double N2StrtoldL(const char* s, char** e, void*) { return std::strtod(s, e); }
int64_t N2StrtollL(const char* s, char** e, int b, void*) { return std::strtoll(s, e, b); }
uint64_t N2StrtoullL(const char* s, char** e, int b, void*) { return std::strtoull(s, e, b); }
double N2Atof(const char* s) { return s ? std::atof(s) : 0.0; }
int N2Abs(int x) { return std::abs(x); }
int64_t N2Llabs(int64_t x) { return x < 0 ? -x : x; }

__declspec(guard(nocf)) int InvokeGuestCmp(int (*cmp)(const void*, const void*), const void* a, const void* b) {
    return cmp(a, b);
}

void N2Qsort(void* base, uint64_t nmemb, uint64_t size, int (*compar)(const void*, const void*)) {
    if (!base || nmemb < 2 || size == 0 || !compar) return;
    auto* bytes = static_cast<uint8_t*>(base);
    std::vector<size_t> idx(static_cast<size_t>(nmemb));
    for (size_t i = 0; i < idx.size(); ++i) idx[i] = i;
    std::sort(idx.begin(), idx.end(), [&](size_t a, size_t b) {
        return InvokeGuestCmp(compar, bytes + a * size, bytes + b * size) < 0;
    });
    std::vector<uint8_t> sorted(static_cast<size_t>(nmemb * size));
    for (size_t i = 0; i < idx.size(); ++i) {
        std::memcpy(sorted.data() + i * size, bytes + idx[i] * size, static_cast<size_t>(size));
    }
    std::memcpy(bytes, sorted.data(), sorted.size());
}

void* N2Bsearch(const void* key, const void* base, uint64_t nmemb, uint64_t size,
                int (*compar)(const void*, const void*)) {
    if (!key || !base || nmemb == 0 || size == 0 || !compar) return nullptr;
    const auto* bytes = static_cast<const uint8_t*>(base);
    size_t lo = 0, hi = static_cast<size_t>(nmemb);
    while (lo < hi) {
        const size_t mid = lo + (hi - lo) / 2;
        const void* elem = bytes + mid * size;
        const int c = InvokeGuestCmp(compar, key, elem);
        if (c == 0) return const_cast<void*>(elem);
        if (c < 0) hi = mid; else lo = mid + 1;
    }
    return nullptr;
}

char* N2Strndup(const char* s, uint64_t n) {
    if (!s) return nullptr;
    const size_t len = strnlen(s, static_cast<size_t>(n));
    auto* dst = static_cast<char*>(GuestHeapMalloc(len + 1));
    if (dst) { std::memcpy(dst, s, len); dst[len] = '\0'; }
    return dst;
}
char* N2Strcat(char* d, const char* s) { return std::strcat(d, s); }
char* N2Strncat(char* d, const char* s, uint64_t n) { return std::strncat(d, s, static_cast<size_t>(n)); }
uint64_t N2Strcspn(const char* s, const char* r) { return std::strcspn(s, r); }
uint64_t N2Strspn(const char* s, const char* a) { return std::strspn(s, a); }
char* N2Strpbrk(const char* s, const char* a) { return const_cast<char*>(std::strpbrk(s, a)); }
char* N2StrtokR(char* s, const char* delim, char** save) { return strtok_s(s, delim, save); }
int N2Strcasecmp(const char* a, const char* b) { return _stricmp(a, b); }
int N2Strncasecmp(const char* a, const char* b, uint64_t n) { return _strnicmp(a, b, static_cast<size_t>(n)); }
int N2StrcollL(const char* a, const char* b, void*) { return std::strcmp(a, b); }
uint64_t N2StrxfrmL(char* d, const char* s, uint64_t n, void*) {
    const uint64_t len = std::strlen(s);
    if (d && n > 0) { const uint64_t cp = std::min(len, n - 1); std::memcpy(d, s, static_cast<size_t>(cp)); d[cp] = '\0'; }
    return len;
}

const char* ErrString(int err) {
    switch (err) {
        case 0: return "Success";
        case kEperm: return "Operation not permitted";
        case kEnoent: return "No such file or directory";
        case kEintr: return "Interrupted system call";
        case kEio: return "I/O error";
        case kEbadf: return "Bad file descriptor";
        case kEagain: return "Try again";
        case kEnomem: return "Out of memory";
        case kEacces: return "Permission denied";
        case kEexist: return "File exists";
        case kEinval: return "Invalid argument";
        case kEnosys: return "Function not implemented";
        case kEtimedout: return "Connection timed out";
        case kEconnrefused: return "Connection refused";
        default: return "System error";
    }
}
char* N2Strerror(int err) {
    thread_local char buf[64];
    std::snprintf(buf, sizeof(buf), "%s", ErrString(err));
    return buf;
}
int N2StrerrorR(int err, char* buf, uint64_t len) {
    if (!buf || len == 0) return kEinval;
    std::snprintf(buf, static_cast<size_t>(len), "%s", ErrString(err));
    return 0;
}
char* N2GnuStrerrorR(int err, char* buf, uint64_t len) {
    if (buf && len > 0) { std::snprintf(buf, static_cast<size_t>(len), "%s", ErrString(err)); return buf; }
    return N2Strerror(err);
}

void* N2MemcpyChk(void* d, const void* s, uint64_t n, uint64_t) { return std::memcpy(d, s, static_cast<size_t>(n)); }
void* N2MemmoveChk(void* d, const void* s, uint64_t n, uint64_t) { return std::memmove(d, s, static_cast<size_t>(n)); }
char* N2StrcpyChk(char* d, const char* s, uint64_t) { return std::strcpy(d, s); }
char* N2StrchrChk(const char* s, int c, uint64_t) { return const_cast<char*>(std::strchr(s, c)); }
uint64_t N2StrlenChk(const char* s, uint64_t) { return std::strlen(s); }

// POSIX `<regex.h>` over `std::regex` (`regex_t` is 32 bytes on Android ARM64; offset 16 holds `re_g`).
struct GuestRegex { int re_magic; int pad0; uint64_t re_nsub; const char* re_endp; void* re_g; };
struct GuestRegmatch { int64_t rm_so, rm_eo; };

int N2Regcomp(GuestRegex* preg, const char* pattern, int cflags) {
    if (!preg || !pattern) return 1;
    std::memset(preg, 0, sizeof(*preg));
    try {
        auto flags = (cflags & 1 /* REG_EXTENDED */) ? std::regex::extended : std::regex::basic;
        if (cflags & 2 /* REG_ICASE */) flags |= std::regex::icase;
        auto* rx = new std::regex(pattern, flags);
        preg->re_nsub = rx->mark_count();
        preg->re_g = rx;
        return 0;
    } catch (...) {
        return 1;
    }
}
int N2Regexec(const GuestRegex* preg, const char* str, uint64_t nmatch, GuestRegmatch* pmatch, int) {
    if (!preg || !preg->re_g || !str) return 1;
    try {
        std::cmatch cm;
        if (!std::regex_search(str, cm, *static_cast<const std::regex*>(preg->re_g))) return 1; // REG_NOMATCH
        for (uint64_t i = 0; i < nmatch && pmatch; ++i) {
            if (i < cm.size() && cm[i].matched) {
                pmatch[i].rm_so = cm.position(i);
                pmatch[i].rm_eo = cm.position(i) + cm.length(i);
            } else {
                pmatch[i].rm_so = pmatch[i].rm_eo = -1;
            }
        }
        return 0;
    } catch (...) {
        return 1;
    }
}
uint64_t N2Regerror(int, const GuestRegex*, char* errbuf, uint64_t sz) {
    if (errbuf && sz > 0) std::snprintf(errbuf, static_cast<size_t>(sz), "regex error");
    return 12;
}
void N2Regfree(GuestRegex* preg) {
    if (preg && preg->re_g) { delete static_cast<std::regex*>(preg->re_g); preg->re_g = nullptr; }
}

uint32_t N2Arc4random() { unsigned int r = 0; rand_s(&r); return r; }
int N2Rand() { return static_cast<int>(N2Arc4random() & 0x7fffffffu); }
int N2RandR(unsigned int* seed) {
    if (!seed) return 0;
    *seed = *seed * 1103515245u + 12345u;
    return static_cast<int>((*seed >> 16) & 0x7fffu);
}

// Signals (no-ops so HotSpot's Windows SEH handlers are NEVER pre-empted, per `AGENTS.md`):
int N2SigAction(int, const void*, void* oldact) { if (oldact) std::memset(oldact, 0, 32); return 0; }
int N2SigEmptySet(uint64_t* set) { if (set) *set = 0; return 0; }
int N2SigFillSet(uint64_t* set) { if (set) *set = UINT64_MAX; return 0; }
int N2SigAddSet(uint64_t* set, int sig) { if (set && sig > 0 && sig <= 64) *set |= (1ull << (sig - 1)); return 0; }
int N2SigProcmask(int, const void*, void* oldset) { if (oldset) *static_cast<uint64_t*>(oldset) = 0; return 0; }
int N2SigRtMax() { return 64; }
void N2OpenLog(const char*, int, int) {}
void N2CloseLog() {}

int N2GetRlimit(int, uint64_t rlim[2]) {
    if (!rlim) return SetErr(kEfault);
    rlim[0] = rlim[1] = 4096;
    return 0;
}
int N2GetRusage(int, void* usage) { if (usage) std::memset(usage, 0, 144); return 0; }
int N2SetPriority(int, uint32_t, int) { return 0; }

extern "C" int N2PrctlImpl(int option, uint64_t a2, uint64_t, uint64_t, uint64_t) {
    if (option == 15 /* PR_SET_NAME */ && a2) {
        SetThreadDescription(GetCurrentThread(), Utf8ToWide(reinterpret_cast<const char*>(a2)).c_str());
    }
    return 0;
}

extern "C" int64_t N2SyscallImpl(int64_t num, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t, uint64_t) {
    if (num == 178 /* SYS_gettid */) return static_cast<int64_t>(GetCurrentThreadId());
    if (num == 172 /* SYS_getpid */) return static_cast<int64_t>(GetCurrentProcessId());
    if (num == 278 /* SYS_getrandom */ && a1) {
        auto* dst = reinterpret_cast<uint8_t*>(a1);
        for (uint64_t i = 0; i < a2; ++i) dst[i] = static_cast<uint8_t>(N2Arc4random());
        return static_cast<int64_t>(a2);
    }
    if (num == 113 /* SYS_clock_gettime */ && a2) {
        auto* ts = reinterpret_cast<GuestTimespec*>(a2);
        if (a1 == 0 /* CLOCK_REALTIME */) {
            FILETIME ft{};
            GetSystemTimePreciseAsFileTime(&ft);
            *ts = FiletimeToUnix(ft);
        } else {
            const uint64_t ns = MonoNowNs();
            *ts = {static_cast<int64_t>(ns / 1000000000ull), static_cast<int64_t>(ns % 1000000000ull)};
        }
        return 0;
    }
    if (num == 98 /* SYS_futex */ && a1) {
        auto* uaddr = reinterpret_cast<volatile uint32_t*>(a1);
        const int rawOp = static_cast<int>(a2);
        const int op = rawOp & 0x7f;
        if (op == 0 /* FUTEX_WAIT */ || op == 9 /* FUTEX_WAIT_BITSET */) {
            uint32_t expected = static_cast<uint32_t>(a3);
            if (__atomic_load_n(uaddr, __ATOMIC_ACQUIRE) != expected) {
                return SetErr64(kEagain);
            }
            DWORD waitMs = INFINITE;
            if (a4) {
                const auto* ts = reinterpret_cast<const GuestTimespec*>(a4);
                if (op == 0) {
                    const int64_t ns = std::max<int64_t>(0, ts->sec) * 1000000000LL + std::max<int64_t>(0, ts->nsec);
                    waitMs = static_cast<DWORD>(std::min<int64_t>((ns + 999999LL) / 1000000LL, 3600000LL));
                } else {
                    const uint64_t targetNs = static_cast<uint64_t>(std::max<int64_t>(0, ts->sec)) * 1000000000ull +
                                              static_cast<uint64_t>(std::max<int64_t>(0, ts->nsec));
                    uint64_t nowNs = 0;
                    if (rawOp & 256 /* FUTEX_CLOCK_REALTIME */) {
                        FILETIME ft{};
                        GetSystemTimePreciseAsFileTime(&ft);
                        const GuestTimespec cur = FiletimeToUnix(ft);
                        nowNs = static_cast<uint64_t>(std::max<int64_t>(0, cur.sec)) * 1000000000ull +
                                static_cast<uint64_t>(std::max<int64_t>(0, cur.nsec));
                    } else {
                        nowNs = MonoNowNs();
                    }
                    waitMs = targetNs <= nowNs ? 0 : static_cast<DWORD>(std::min<uint64_t>((targetNs - nowNs + 999999ull) / 1000000ull, 3600000ull));
                }
            }
            if (!WaitOnAddress(const_cast<uint32_t*>(uaddr), &expected, sizeof(uint32_t), waitMs)) {
                const DWORD err = GetLastError();
                if (err == ERROR_TIMEOUT) return SetErr64(kEtimedout);
            }
            return 0;
        }
        if (op == 1 /* FUTEX_WAKE */ || op == 10 /* FUTEX_WAKE_BITSET */) {
            const int32_t count = static_cast<int32_t>(a3);
            if (count <= 0) return 0;
            if (count == 1) {
                WakeByAddressSingle(const_cast<uint32_t*>(uaddr));
                return 1;
            }
            WakeByAddressAll(const_cast<uint32_t*>(uaddr));
            return count;
        }
    }
    return SetErr64(kEnosys);
}

[[noreturn]] void N2Abort() {
    std::fprintf(stderr, "GUEST_EXECUTION_FAULT: guest abort()\n");
    std::fflush(stderr);
    TerminateProcess(GetCurrentProcess(), 3);
    std::abort();
}
[[noreturn]] void N2StackChkFail() {
    std::fprintf(stderr, "GUEST_EXECUTION_FAULT: __stack_chk_fail\n");
    std::fflush(stderr);
    TerminateProcess(GetCurrentProcess(), 3);
    std::abort();
}
[[noreturn]] void N2Assert2(const char* file, int line, const char* fn, const char* expr) {
    std::fprintf(stderr, "GUEST_EXECUTION_FAULT: assertion failed %s:%d (%s): %s\n",
                 file ? file : "?", line, fn ? fn : "?", expr ? expr : "?");
    std::fflush(stderr);
    TerminateProcess(GetCurrentProcess(), 3);
    std::abort();
}
void N2SetAbortMessage(const char* msg) {
    if (msg) std::fprintf(stderr, "guest-abort-msg: %s\n", msg);
}
[[noreturn]] void N2Exit(int status) {
    std::fflush(nullptr);
    TerminateProcess(GetCurrentProcess(), static_cast<UINT>(status));
    std::abort();
}
int N2Raise(int sig) {
    if (sig == 6 /* SIGABRT */) N2Abort();
    return 0;
}

int N2Enosys() { return SetErr(kEnosys); }

char* g_emptyEnviron[1] = {nullptr};
char** g_environVar = g_emptyEnviron;
uint64_t g_stackGuardVar = 0;

// ---- Fixed-signature Formatting/Scanning Backend for `bionic_n2_guest.c` ----

extern "C" int HostFmtInt(char* dst, uint64_t cap, const char* spec, uint64_t bits, int isSigned) {
    if (!dst || cap == 0 || !spec) return 0;
    if (isSigned) return std::snprintf(dst, static_cast<size_t>(cap), spec, static_cast<long long>(bits));
    return std::snprintf(dst, static_cast<size_t>(cap), spec, static_cast<unsigned long long>(bits));
}

extern "C" int HostFmtDouble(char* dst, uint64_t cap, const char* spec, double val) {
    if (!dst || cap == 0 || !spec) return 0;
    return std::snprintf(dst, static_cast<size_t>(cap), spec, val);
}

extern "C" int HostStreamWrite(void* stream, const void* buf, uint64_t len) {
    return static_cast<int>(N2Fwrite(buf, 1, len, stream));
}

extern "C" int HostStreamReadLine(void* stream, char* buf, uint64_t cap) {
    return N2Fgets(buf, static_cast<int>(cap), stream) ? 0 : -1;
}

extern "C" int HostScanOne(const char* src, const char* spec, void* outPtr, int kind, uint64_t* consumedOut) {
    if (!src || !spec || !consumedOut) return -1;
    *consumedOut = 0;
    std::string fullSpec = std::string(spec) + "%n";
    int nchars = -1;
    int rc = 0;
    switch (kind) {
        case 0: rc = std::sscanf(src, fullSpec.c_str(), &nchars); break;
        case 1: { int v = 0; rc = std::sscanf(src, fullSpec.c_str(), &v, &nchars); if (rc == 1 && outPtr) *static_cast<int*>(outPtr) = v; break; }
        case 2: { long long v = 0; rc = std::sscanf(src, fullSpec.c_str(), &v, &nchars); if (rc == 1 && outPtr) *static_cast<int64_t*>(outPtr) = v; break; }
        case 3: { unsigned int v = 0; rc = std::sscanf(src, fullSpec.c_str(), &v, &nchars); if (rc == 1 && outPtr) *static_cast<uint32_t*>(outPtr) = v; break; }
        case 4: { unsigned long long v = 0; rc = std::sscanf(src, fullSpec.c_str(), &v, &nchars); if (rc == 1 && outPtr) *static_cast<uint64_t*>(outPtr) = v; break; }
        case 5: { float v = 0; rc = std::sscanf(src, fullSpec.c_str(), &v, &nchars); if (rc == 1 && outPtr) *static_cast<float*>(outPtr) = v; break; }
        case 6: { double v = 0; rc = std::sscanf(src, fullSpec.c_str(), &v, &nchars); if (rc == 1 && outPtr) *static_cast<double*>(outPtr) = v; break; }
        case 7: { short v = 0; rc = std::sscanf(src, fullSpec.c_str(), &v, &nchars); if (rc == 1 && outPtr) *static_cast<int16_t*>(outPtr) = v; break; }
        case 8: { signed char v = 0; rc = std::sscanf(src, fullSpec.c_str(), &v, &nchars); if (rc == 1 && outPtr) *static_cast<int8_t*>(outPtr) = v; break; }
        default: rc = std::sscanf(src, fullSpec.c_str(), static_cast<char*>(outPtr), &nchars); break;
    }
    if (nchars >= 0) *consumedOut = static_cast<uint64_t>(nchars);
    return rc;
}

// ---- `libm.so` Complete C99/POSIX Math Wrappers (Direct ARM64 FP ABI) ----

double M_acos(double x) { return std::acos(x); }
float M_acosf(float x) { return std::acos(x); }
double M_acosh(double x) { return std::acosh(x); }
float M_acoshf(float x) { return std::acosh(x); }
double M_asin(double x) { return std::asin(x); }
float M_asinf(float x) { return std::asin(x); }
double M_asinh(double x) { return std::asinh(x); }
float M_asinhf(float x) { return std::asinh(x); }
double M_atan(double x) { return std::atan(x); }
float M_atanf(float x) { return std::atan(x); }
double M_atan2(double y, double x) { return std::atan2(y, x); }
float M_atan2f(float y, float x) { return std::atan2(y, x); }
double M_atanh(double x) { return std::atanh(x); }
float M_atanhf(float x) { return std::atanh(x); }
double M_cbrt(double x) { return std::cbrt(x); }
float M_cbrtf(float x) { return std::cbrt(x); }
double M_ceil(double x) { return std::ceil(x); }
float M_ceilf(float x) { return std::ceil(x); }
double M_copysign(double x, double y) { return std::copysign(x, y); }
float M_copysignf(float x, float y) { return std::copysign(x, y); }
double M_cos(double x) { return std::cos(x); }
float M_cosf(float x) { return std::cos(x); }
double M_cosh(double x) { return std::cosh(x); }
float M_coshf(float x) { return std::cosh(x); }
double M_erf(double x) { return std::erf(x); }
float M_erff(float x) { return std::erf(x); }
double M_erfc(double x) { return std::erfc(x); }
float M_erfcf(float x) { return std::erfc(x); }
double M_exp(double x) { return std::exp(x); }
float M_expf(float x) { return std::exp(x); }
double M_exp2(double x) { return std::exp2(x); }
float M_exp2f(float x) { return std::exp2(x); }
double M_expm1(double x) { return std::expm1(x); }
float M_expm1f(float x) { return std::expm1(x); }
double M_fabs(double x) { return std::fabs(x); }
float M_fabsf(float x) { return std::fabs(x); }
double M_fdim(double x, double y) { return std::fdim(x, y); }
float M_fdimf(float x, float y) { return std::fdim(x, y); }
double M_floor(double x) { return std::floor(x); }
float M_floorf(float x) { return std::floor(x); }
double M_fma(double x, double y, double z) { return std::fma(x, y, z); }
float M_fmaf(float x, float y, float z) { return std::fma(x, y, z); }
double M_fmax(double x, double y) { return std::fmax(x, y); }
float M_fmaxf(float x, float y) { return std::fmax(x, y); }
double M_fmin(double x, double y) { return std::fmin(x, y); }
float M_fminf(float x, float y) { return std::fmin(x, y); }
double M_fmod(double x, double y) { return std::fmod(x, y); }
float M_fmodf(float x, float y) { return std::fmod(x, y); }
double M_frexp(double x, int* e) { return std::frexp(x, e); }
float M_frexpf(float x, int* e) { return std::frexp(x, e); }
double M_hypot(double x, double y) { return std::hypot(x, y); }
float M_hypotf(float x, float y) { return std::hypot(x, y); }
int M_ilogb(double x) { return std::ilogb(x); }
int M_ilogbf(float x) { return std::ilogb(x); }
double M_j0(double x) { return _j0(x); }
double M_j1(double x) { return _j1(x); }
double M_jn(int n, double x) { return _jn(n, x); }
double M_y0(double x) { return _y0(x); }
double M_y1(double x) { return _y1(x); }
double M_yn(int n, double x) { return _yn(n, x); }
double M_ldexp(double x, int e) { return std::ldexp(x, e); }
float M_ldexpf(float x, int e) { return std::ldexp(x, e); }
double M_lgamma(double x) { return std::lgamma(x); }
float M_lgammaf(float x) { return std::lgamma(x); }
int64_t M_llrint(double x) { return std::llrint(x); }
int64_t M_llrintf(float x) { return std::llrint(x); }
int64_t M_llround(double x) { return std::llround(x); }
int64_t M_llroundf(float x) { return std::llround(x); }
double M_log(double x) { return std::log(x); }
float M_logf(float x) { return std::log(x); }
double M_log10(double x) { return std::log10(x); }
float M_log10f(float x) { return std::log10(x); }
double M_log1p(double x) { return std::log1p(x); }
float M_log1pf(float x) { return std::log1p(x); }
double M_log2(double x) { return std::log2(x); }
float M_log2f(float x) { return std::log2(x); }
double M_logb(double x) { return std::logb(x); }
float M_logbf(float x) { return std::logb(x); }
double M_modf(double x, double* ip) { return std::modf(x, ip); }
float M_modff(float x, float* ip) { return std::modf(x, ip); }
double M_nan(const char* t) { return std::nan(t ? t : ""); }
float M_nanf(const char* t) { return std::nanf(t ? t : ""); }
double M_nearbyint(double x) { return std::nearbyint(x); }
float M_nearbyintf(float x) { return std::nearbyint(x); }
double M_nextafter(double x, double y) { return std::nextafter(x, y); }
float M_nextafterf(float x, float y) { return std::nextafter(x, y); }
double M_pow(double x, double y) { return std::pow(x, y); }
float M_powf(float x, float y) { return std::pow(x, y); }
double M_remainder(double x, double y) { return std::remainder(x, y); }
float M_remainderf(float x, float y) { return std::remainder(x, y); }
double M_remquo(double x, double y, int* q) { return std::remquo(x, y, q); }
float M_remquof(float x, float y, int* q) { return std::remquo(x, y, q); }
double M_rint(double x) { return std::rint(x); }
float M_rintf(float x) { return std::rint(x); }
double M_round(double x) { return std::round(x); }
float M_roundf(float x) { return std::round(x); }
double M_scalbln(double x, int64_t n) { return std::scalbln(x, static_cast<long>(std::clamp<int64_t>(n, -100000, 100000))); }
float M_scalblnf(float x, int64_t n) { return std::scalbln(x, static_cast<long>(std::clamp<int64_t>(n, -100000, 100000))); }
double M_scalbn(double x, int n) { return std::scalbn(x, n); }
float M_scalbnf(float x, int n) { return std::scalbn(x, n); }
double M_sin(double x) { return std::sin(x); }
float M_sinf(float x) { return std::sin(x); }
void M_sincos(double x, double* s, double* c) { if (s) *s = std::sin(x); if (c) *c = std::cos(x); }
void M_sincosf(float x, float* s, float* c) { if (s) *s = std::sin(x); if (c) *c = std::cos(x); }
double M_sinh(double x) { return std::sinh(x); }
float M_sinhf(float x) { return std::sinh(x); }
double M_sqrt(double x) { return std::sqrt(x); }
float M_sqrtf(float x) { return std::sqrt(x); }
double M_tan(double x) { return std::tan(x); }
float M_tanf(float x) { return std::tan(x); }
double M_tanh(double x) { return std::tanh(x); }
float M_tanhf(float x) { return std::tanh(x); }
double M_tgamma(double x) { return std::tgamma(x); }
float M_tgammaf(float x) { return std::tgamma(x); }
double M_trunc(double x) { return std::trunc(x); }
float M_truncf(float x) { return std::trunc(x); }

const HostSymbol kLibmSymbols[] = {
    {"acos", reinterpret_cast<void*>(&M_acos)}, {"acosf", reinterpret_cast<void*>(&M_acosf)},
    {"acosh", reinterpret_cast<void*>(&M_acosh)}, {"acoshf", reinterpret_cast<void*>(&M_acoshf)},
    {"asin", reinterpret_cast<void*>(&M_asin)}, {"asinf", reinterpret_cast<void*>(&M_asinf)},
    {"asinh", reinterpret_cast<void*>(&M_asinh)}, {"asinhf", reinterpret_cast<void*>(&M_asinhf)},
    {"atan", reinterpret_cast<void*>(&M_atan)}, {"atanf", reinterpret_cast<void*>(&M_atanf)},
    {"atan2", reinterpret_cast<void*>(&M_atan2)}, {"atan2f", reinterpret_cast<void*>(&M_atan2f)},
    {"atanh", reinterpret_cast<void*>(&M_atanh)}, {"atanhf", reinterpret_cast<void*>(&M_atanhf)},
    {"cbrt", reinterpret_cast<void*>(&M_cbrt)}, {"cbrtf", reinterpret_cast<void*>(&M_cbrtf)},
    {"ceil", reinterpret_cast<void*>(&M_ceil)}, {"ceilf", reinterpret_cast<void*>(&M_ceilf)},
    {"copysign", reinterpret_cast<void*>(&M_copysign)}, {"copysignf", reinterpret_cast<void*>(&M_copysignf)},
    {"cos", reinterpret_cast<void*>(&M_cos)}, {"cosf", reinterpret_cast<void*>(&M_cosf)},
    {"cosh", reinterpret_cast<void*>(&M_cosh)}, {"coshf", reinterpret_cast<void*>(&M_coshf)},
    {"erf", reinterpret_cast<void*>(&M_erf)}, {"erff", reinterpret_cast<void*>(&M_erff)},
    {"erfc", reinterpret_cast<void*>(&M_erfc)}, {"erfcf", reinterpret_cast<void*>(&M_erfcf)},
    {"exp", reinterpret_cast<void*>(&M_exp)}, {"expf", reinterpret_cast<void*>(&M_expf)},
    {"exp2", reinterpret_cast<void*>(&M_exp2)}, {"exp2f", reinterpret_cast<void*>(&M_exp2f)},
    {"expm1", reinterpret_cast<void*>(&M_expm1)}, {"expm1f", reinterpret_cast<void*>(&M_expm1f)},
    {"fabs", reinterpret_cast<void*>(&M_fabs)}, {"fabsf", reinterpret_cast<void*>(&M_fabsf)},
    {"fdim", reinterpret_cast<void*>(&M_fdim)}, {"fdimf", reinterpret_cast<void*>(&M_fdimf)},
    {"floor", reinterpret_cast<void*>(&M_floor)}, {"floorf", reinterpret_cast<void*>(&M_floorf)},
    {"fma", reinterpret_cast<void*>(&M_fma)}, {"fmaf", reinterpret_cast<void*>(&M_fmaf)},
    {"fmax", reinterpret_cast<void*>(&M_fmax)}, {"fmaxf", reinterpret_cast<void*>(&M_fmaxf)},
    {"fmin", reinterpret_cast<void*>(&M_fmin)}, {"fminf", reinterpret_cast<void*>(&M_fminf)},
    {"fmod", reinterpret_cast<void*>(&M_fmod)}, {"fmodf", reinterpret_cast<void*>(&M_fmodf)},
    {"frexp", reinterpret_cast<void*>(&M_frexp)}, {"frexpf", reinterpret_cast<void*>(&M_frexpf)},
    {"hypot", reinterpret_cast<void*>(&M_hypot)}, {"hypotf", reinterpret_cast<void*>(&M_hypotf)},
    {"ilogb", reinterpret_cast<void*>(&M_ilogb)}, {"ilogbf", reinterpret_cast<void*>(&M_ilogbf)},
    {"j0", reinterpret_cast<void*>(&M_j0)}, {"j1", reinterpret_cast<void*>(&M_j1)},
    {"jn", reinterpret_cast<void*>(&M_jn)}, {"y0", reinterpret_cast<void*>(&M_y0)},
    {"y1", reinterpret_cast<void*>(&M_y1)}, {"yn", reinterpret_cast<void*>(&M_yn)},
    {"ldexp", reinterpret_cast<void*>(&M_ldexp)}, {"ldexpf", reinterpret_cast<void*>(&M_ldexpf)},
    {"lgamma", reinterpret_cast<void*>(&M_lgamma)}, {"lgammaf", reinterpret_cast<void*>(&M_lgammaf)},
    {"llrint", reinterpret_cast<void*>(&M_llrint)}, {"llrintf", reinterpret_cast<void*>(&M_llrintf)},
    {"llround", reinterpret_cast<void*>(&M_llround)}, {"llroundf", reinterpret_cast<void*>(&M_llroundf)},
    {"lrint", reinterpret_cast<void*>(&M_llrint)}, {"lrintf", reinterpret_cast<void*>(&M_llrintf)},
    {"lround", reinterpret_cast<void*>(&M_llround)}, {"lroundf", reinterpret_cast<void*>(&M_llroundf)},
    {"log", reinterpret_cast<void*>(&M_log)}, {"logf", reinterpret_cast<void*>(&M_logf)},
    {"log10", reinterpret_cast<void*>(&M_log10)}, {"log10f", reinterpret_cast<void*>(&M_log10f)},
    {"log1p", reinterpret_cast<void*>(&M_log1p)}, {"log1pf", reinterpret_cast<void*>(&M_log1pf)},
    {"log2", reinterpret_cast<void*>(&M_log2)}, {"log2f", reinterpret_cast<void*>(&M_log2f)},
    {"logb", reinterpret_cast<void*>(&M_logb)}, {"logbf", reinterpret_cast<void*>(&M_logbf)},
    {"modf", reinterpret_cast<void*>(&M_modf)}, {"modff", reinterpret_cast<void*>(&M_modff)},
    {"nan", reinterpret_cast<void*>(&M_nan)}, {"nanf", reinterpret_cast<void*>(&M_nanf)},
    {"nearbyint", reinterpret_cast<void*>(&M_nearbyint)}, {"nearbyintf", reinterpret_cast<void*>(&M_nearbyintf)},
    {"nextafter", reinterpret_cast<void*>(&M_nextafter)}, {"nextafterf", reinterpret_cast<void*>(&M_nextafterf)},
    {"pow", reinterpret_cast<void*>(&M_pow)}, {"powf", reinterpret_cast<void*>(&M_powf)},
    {"remainder", reinterpret_cast<void*>(&M_remainder)}, {"remainderf", reinterpret_cast<void*>(&M_remainderf)},
    {"remquo", reinterpret_cast<void*>(&M_remquo)}, {"remquof", reinterpret_cast<void*>(&M_remquof)},
    {"rint", reinterpret_cast<void*>(&M_rint)}, {"rintf", reinterpret_cast<void*>(&M_rintf)},
    {"round", reinterpret_cast<void*>(&M_round)}, {"roundf", reinterpret_cast<void*>(&M_roundf)},
    {"scalbln", reinterpret_cast<void*>(&M_scalbln)}, {"scalblnf", reinterpret_cast<void*>(&M_scalblnf)},
    {"scalbn", reinterpret_cast<void*>(&M_scalbn)}, {"scalbnf", reinterpret_cast<void*>(&M_scalbnf)},
    {"sin", reinterpret_cast<void*>(&M_sin)}, {"sinf", reinterpret_cast<void*>(&M_sinf)},
    {"sincos", reinterpret_cast<void*>(&M_sincos)}, {"sincosf", reinterpret_cast<void*>(&M_sincosf)},
    {"sinh", reinterpret_cast<void*>(&M_sinh)}, {"sinhf", reinterpret_cast<void*>(&M_sinhf)},
    {"sqrt", reinterpret_cast<void*>(&M_sqrt)}, {"sqrtf", reinterpret_cast<void*>(&M_sqrtf)},
    {"tan", reinterpret_cast<void*>(&M_tan)}, {"tanf", reinterpret_cast<void*>(&M_tanf)},
    {"tanh", reinterpret_cast<void*>(&M_tanh)}, {"tanhf", reinterpret_cast<void*>(&M_tanhf)},
    {"tgamma", reinterpret_cast<void*>(&M_tgamma)}, {"tgammaf", reinterpret_cast<void*>(&M_tgammaf)},
    {"trunc", reinterpret_cast<void*>(&M_trunc)}, {"truncf", reinterpret_cast<void*>(&M_truncf)},
};

void UpsertSymbol(std::vector<HostSymbol>& list, const char* name, void* addr) {
    for (auto& s : list) {
        if (std::strcmp(s.name, name) == 0) { s.address = addr; return; }
    }
    list.push_back({name, addr});
}

}  // namespace

N2BionicExtension::~N2BionicExtension() = default;

std::unique_ptr<N2BionicExtension> N2BionicExtension::Install(
        const std::wstring& n2HelperPath, const std::wstring& androidRoot,
        std::vector<BuiltinLibrary>& libraries, std::vector<HostSymbol>& combinedImports,
        LoadError* error) {
    g_n2Root = androidRoot;
    g_stackGuardVar = GuestStackGuardCookie();

    // Load pure Android AArch64 `libwinapk-bionic-n2.so` with its fixed-signature host backends.
    std::vector<HostSymbol> helperImports = {
        {"__errno", reinterpret_cast<void*>(&GuestErrnoPtr)},
        {"malloc", reinterpret_cast<void*>(&GuestHeapMalloc)},
        {"free", reinterpret_cast<void*>(&GuestHeapFree)},
        {"stdout", &g_stdoutPtr},
        {"__winapk_n2_fmt_int", reinterpret_cast<void*>(&HostFmtInt)},
        {"__winapk_n2_fmt_double", reinterpret_cast<void*>(&HostFmtDouble)},
        {"__winapk_n2_stream_write", reinterpret_cast<void*>(&HostStreamWrite)},
        {"__winapk_n2_stream_read_line", reinterpret_cast<void*>(&HostStreamReadLine)},
        {"__winapk_n2_scan_one", reinterpret_cast<void*>(&HostScanOne)},
        {"__winapk_n2_log_write", reinterpret_cast<void*>(&GuestLogWriteRaw)},
        {"__winapk_n2_open", reinterpret_cast<void*>(&N2OpenImpl)},
        {"__winapk_n2_openat", reinterpret_cast<void*>(&N2OpenAtImpl)},
        {"__winapk_n2_fcntl", reinterpret_cast<void*>(&N2FcntlImpl)},
        {"__winapk_n2_ioctl", reinterpret_cast<void*>(&N2IoctlImpl)},
        {"__winapk_n2_prctl", reinterpret_cast<void*>(&N2PrctlImpl)},
        {"__winapk_n2_syscall", reinterpret_cast<void*>(&N2SyscallImpl)},
    };
    auto ext = std::make_unique<N2BionicExtension>();
    ext->helper_ = guest::Load(n2HelperPath, helperImports, error);
    if (!ext->helper_) return nullptr;

    BuiltinLibrary* libc = nullptr;
    BuiltinLibrary* libdl = nullptr;
    BuiltinLibrary* liblog = nullptr;
    for (auto& lib : libraries) {
        if (lib.name == "libc.so") libc = &lib;
        else if (lib.name == "libdl.so") libdl = &lib;
        else if (lib.name == "liblog.so") liblog = &lib;
    }
    if (!libc || !libdl || !liblog) {
        *error = {Category::SymbolUnresolved, "N2 Bionic", "base libc/libdl/liblog missing"};
        return nullptr;
    }

    // 1. Export guest-ABI variadics & setjmp/longjmp from `libwinapk-bionic-n2.so`:
    for (const char* name : {
            "vsnprintf", "snprintf", "__vsnprintf_chk", "vsprintf", "sprintf",
            "vasprintf", "asprintf", "vfprintf", "fprintf", "vprintf", "printf",
            "vswprintf", "swprintf", "syslog", "vsscanf", "sscanf", "fscanf",
            "open", "open64", "openat", "openat64", "fcntl", "ioctl", "prctl", "syscall",
            "setjmp", "_setjmp", "longjmp", "_longjmp"}) {
        void* addr = ext->helper_->Find(name);
        if (!addr) {
            *error = {Category::SymbolUnresolved, "libwinapk-bionic-n2.so", std::string("missing ") + name};
            return nullptr;
        }
        UpsertSymbol(libc->symbols, name, addr);
        UpsertSymbol(combinedImports, name, addr);
    }
    for (const char* lname : {"__android_log_print", "__android_log_vprint"}) {
        void* addr = ext->helper_->Find(lname);
        if (!addr) {
            *error = {Category::SymbolUnresolved, "libwinapk-bionic-n2.so", std::string("missing ") + lname};
            return nullptr;
        }
        UpsertSymbol(liblog->symbols, lname, addr);
        UpsertSymbol(combinedImports, lname, addr);
    }
    UpsertSymbol(liblog->symbols, "__android_log_write", reinterpret_cast<void*>(&GuestLogWriteRaw));
    UpsertSymbol(combinedImports, "__android_log_write", reinterpret_cast<void*>(&GuestLogWriteRaw));

    // 2. Apply N2 overrides from `threads.cpp`:
    for (const auto& ov : ThreadRuntime::N2Overrides()) {
        const std::string_view sv(ov.name);
        if (sv.starts_with("dl")) UpsertSymbol(libdl->symbols, ov.name, ov.address);
        UpsertSymbol(libc->symbols, ov.name, ov.address);
        UpsertSymbol(combinedImports, ov.name, ov.address);
    }

    // 3. Apply N2 `libc.so` POSIX, epoll, socket, stream, locale, wchar & C++ guard symbols:
    const HostSymbol n2Libc[] = {
        // Global data symbols (`STT_OBJECT` addresses):
        {"__sF", &g_bionicSF[0]},
        {"stdin", &g_stdinPtr}, {"stdout", &g_stdoutPtr}, {"stderr", &g_stderrPtr},
        {"environ", &g_environVar},
        {"__stack_chk_guard", &g_stackGuardVar},

        // File & descriptor I/O:
        {"__open_2", reinterpret_cast<void*>(&N2OpenChecked)},
        {"__openat_2", reinterpret_cast<void*>(&N2OpenAtChecked)},
        {"read", reinterpret_cast<void*>(&N2Read)},
        {"__read_chk", reinterpret_cast<void*>(&N2ReadChk)},
        {"pread", reinterpret_cast<void*>(&N2Pread)},
        {"pread64", reinterpret_cast<void*>(&N2Pread)},
        {"__pread_chk", reinterpret_cast<void*>(&N2PreadChk)},
        {"write", reinterpret_cast<void*>(&N2Write)},
        {"__write_chk", reinterpret_cast<void*>(&N2WriteChk)},
        {"close", reinterpret_cast<void*>(&N2Close)},
        {"dup", reinterpret_cast<void*>(&N2Dup)},
        {"dup2", reinterpret_cast<void*>(&N2Dup2)},
        {"lseek", reinterpret_cast<void*>(&N2Lseek)},
        {"lseek64", reinterpret_cast<void*>(&N2Lseek)},
        {"fstat", reinterpret_cast<void*>(&N2Fstat)},
        {"fstat64", reinterpret_cast<void*>(&N2Fstat)},
        {"stat", reinterpret_cast<void*>(&N2StatPath)},
        {"stat64", reinterpret_cast<void*>(&N2StatPath)},
        {"lstat", reinterpret_cast<void*>(&N2StatPath)},
        {"lstat64", reinterpret_cast<void*>(&N2StatPath)},
        {"fstatat", reinterpret_cast<void*>(&N2FstatAt)},
        {"fstatat64", reinterpret_cast<void*>(&N2FstatAt)},
        {"statfs", reinterpret_cast<void*>(&N2StatFs)},
        {"statvfs", reinterpret_cast<void*>(&N2StatFs)},
        {"fstatfs", reinterpret_cast<void*>(&N2FstatFs)},
        {"access", reinterpret_cast<void*>(&N2Access)},
        {"faccessat", reinterpret_cast<void*>(&N2FaccessAt)},
        {"mkdir", reinterpret_cast<void*>(&N2Mkdir)},
        {"mkdirat", reinterpret_cast<void*>(&N2MkdirAt)},
        {"rmdir", reinterpret_cast<void*>(&N2Rmdir)},
        {"unlink", reinterpret_cast<void*>(&N2Unlink)},
        {"unlinkat", reinterpret_cast<void*>(&N2UnlinkAt)},
        {"remove", reinterpret_cast<void*>(&N2Remove)},
        {"rename", reinterpret_cast<void*>(&N2Rename)},
        {"renameat", reinterpret_cast<void*>(&N2RenameAt)},
        {"realpath", reinterpret_cast<void*>(&N2Realpath)},
        {"getcwd", reinterpret_cast<void*>(&N2Getcwd)},
        {"chdir", reinterpret_cast<void*>(&N2Chdir)},
        {"ftruncate", reinterpret_cast<void*>(&N2Ftruncate)},
        {"ftruncate64", reinterpret_cast<void*>(&N2Ftruncate)},
        {"truncate", reinterpret_cast<void*>(&N2Truncate)},
        {"fsync", reinterpret_cast<void*>(&N2Fsync)},
        {"mkstemp", reinterpret_cast<void*>(&N2Mkstemp)},
        {"sendfile", reinterpret_cast<void*>(&N2SendFile)},
        {"sendfile64", reinterpret_cast<void*>(&N2SendFile)},
        {"readlink", reinterpret_cast<void*>(&N2ReadLink)},
        {"readlinkat", reinterpret_cast<void*>(&N2ReadLinkAt)},
        {"__readlink_chk", reinterpret_cast<void*>(&N2ReadLinkChk)},
        {"symlink", reinterpret_cast<void*>(&N2Symlink)},
        {"symlinkat", reinterpret_cast<void*>(&N2SymlinkAt)},
        {"link", reinterpret_cast<void*>(&N2Link)},
        {"fchmod", reinterpret_cast<void*>(&N2Fchmod)},
        {"fchmodat", reinterpret_cast<void*>(&N2FchmodAt)},
        {"utime", reinterpret_cast<void*>(&N2Utime)},
        {"utimensat", reinterpret_cast<void*>(&N2UtimensAt)},
        {"posix_fadvise", reinterpret_cast<void*>(&N2PosixFadvise)},
        {"isatty", reinterpret_cast<void*>(&N2Isatty)},
        {"pathconf", reinterpret_cast<void*>(&N2Pathconf)},

        // Streams & directories:
        {"fopen", reinterpret_cast<void*>(&N2Fopen)},
        {"fopen64", reinterpret_cast<void*>(&N2Fopen)},
        {"fdopen", reinterpret_cast<void*>(&N2FdOpen)},
        {"fclose", reinterpret_cast<void*>(&N2Fclose)},
        {"fread", reinterpret_cast<void*>(&N2Fread)},
        {"fwrite", reinterpret_cast<void*>(&N2Fwrite)},
        {"__fwrite_chk", reinterpret_cast<void*>(&N2FwriteChk)},
        {"fflush", reinterpret_cast<void*>(&N2Fflush)},
        {"fseek", reinterpret_cast<void*>(&N2Fseeko)},
        {"fseeko", reinterpret_cast<void*>(&N2Fseeko)},
        {"fseeko64", reinterpret_cast<void*>(&N2Fseeko)},
        {"ftell", reinterpret_cast<void*>(&N2Ftello)},
        {"ftello", reinterpret_cast<void*>(&N2Ftello)},
        {"ftello64", reinterpret_cast<void*>(&N2Ftello)},
        {"rewind", reinterpret_cast<void*>(&N2Rewind)},
        {"feof", reinterpret_cast<void*>(&N2Feof)},
        {"ferror", reinterpret_cast<void*>(&N2Ferror)},
        {"fileno", reinterpret_cast<void*>(&N2Fileno)},
        {"getc", reinterpret_cast<void*>(&N2Getc)},
        {"fgetc", reinterpret_cast<void*>(&N2Getc)},
        {"ungetc", reinterpret_cast<void*>(&N2Ungetc)},
        {"fgets", reinterpret_cast<void*>(&N2Fgets)},
        {"fputc", reinterpret_cast<void*>(&N2Fputc)},
        {"putc", reinterpret_cast<void*>(&N2Fputc)},
        {"putchar", reinterpret_cast<void*>(&N2Putchar)},
        {"fputs", reinterpret_cast<void*>(&N2Fputs)},
        {"puts", reinterpret_cast<void*>(&N2Puts)},
        {"perror", reinterpret_cast<void*>(&N2Perror)},
        {"setbuf", reinterpret_cast<void*>(&N2Setbuf)},
        {"setvbuf", reinterpret_cast<void*>(&N2Setvbuf)},
        {"getwc", reinterpret_cast<void*>(&N2Getwc)},
        {"ungetwc", reinterpret_cast<void*>(&N2Ungetwc)},
        {"fputwc", reinterpret_cast<void*>(&N2Fputwc)},
        {"getline", reinterpret_cast<void*>(&N2Getline)},
        {"opendir", reinterpret_cast<void*>(&N2OpenDir)},
        {"fdopendir", reinterpret_cast<void*>(&N2FdOpenDir)},
        {"readdir", reinterpret_cast<void*>(&N2ReadDir)},
        {"readdir64", reinterpret_cast<void*>(&N2ReadDir)},
        {"rewinddir", reinterpret_cast<void*>(&N2RewindDir)},
        {"closedir", reinterpret_cast<void*>(&N2CloseDir)},

        // Multiplexing (`epoll`, `poll`, `pipe`, `eventfd`, `timerfd`, `inotify`):
        {"pipe", reinterpret_cast<void*>(&N2Pipe)},
        {"pipe2", reinterpret_cast<void*>(&N2Pipe2)},
        {"eventfd", reinterpret_cast<void*>(&N2EventFd)},
        {"timerfd_create", reinterpret_cast<void*>(&N2TimerFdCreate)},
        {"timerfd_settime", reinterpret_cast<void*>(&N2TimerFdSetTime)},
        {"timerfd_gettime", reinterpret_cast<void*>(&N2TimerFdGetTime)},
        {"inotify_init1", reinterpret_cast<void*>(&N2InotifyInit1)},
        {"inotify_add_watch", reinterpret_cast<void*>(&N2InotifyAddWatch)},
        {"inotify_rm_watch", reinterpret_cast<void*>(&N2InotifyRmWatch)},
        {"epoll_create1", reinterpret_cast<void*>(&N2EpollCreate1)},
        {"epoll_create", reinterpret_cast<void*>(&N2EpollCreate1)},
        {"epoll_ctl", reinterpret_cast<void*>(&N2EpollCtl)},
        {"epoll_wait", reinterpret_cast<void*>(&N2EpollWait)},
        {"poll", reinterpret_cast<void*>(&N2Poll)},
        {"__poll_chk", reinterpret_cast<void*>(&N2PollChk)},
        {"ppoll", reinterpret_cast<void*>(&N2Ppoll)},

        // Winsock2 networking:
        {"socket", reinterpret_cast<void*>(&N2Socket)},
        {"socketpair", reinterpret_cast<void*>(&N2SocketPair)},
        {"connect", reinterpret_cast<void*>(&N2Connect)},
        {"bind", reinterpret_cast<void*>(&N2Bind)},
        {"listen", reinterpret_cast<void*>(&N2Listen)},
        {"accept", reinterpret_cast<void*>(&N2Accept)},
        {"accept4", reinterpret_cast<void*>(&N2Accept4)},
        {"send", reinterpret_cast<void*>(&N2Send)},
        {"sendto", reinterpret_cast<void*>(&N2SendTo)},
        {"sendmsg", reinterpret_cast<void*>(&N2SendMsg)},
        {"recv", reinterpret_cast<void*>(&N2Recv)},
        {"recvfrom", reinterpret_cast<void*>(&N2RecvFrom)},
        {"recvmsg", reinterpret_cast<void*>(&N2RecvMsg)},
        {"setsockopt", reinterpret_cast<void*>(&N2SetSockOpt)},
        {"getsockopt", reinterpret_cast<void*>(&N2GetSockOpt)},
        {"getsockname", reinterpret_cast<void*>(&N2GetSockName)},
        {"getpeername", reinterpret_cast<void*>(&N2GetPeerName)},
        {"shutdown", reinterpret_cast<void*>(&N2Shutdown)},
        {"getaddrinfo", reinterpret_cast<void*>(&N2GetAddrInfo)},
        {"freeaddrinfo", reinterpret_cast<void*>(&N2FreeAddrInfo)},
        {"gai_strerror", reinterpret_cast<void*>(&N2GaiStrerror)},
        {"getnameinfo", reinterpret_cast<void*>(&N2GetNameInfo)},
        {"inet_pton", reinterpret_cast<void*>(&N2InetPton)},
        {"inet_ntop", reinterpret_cast<void*>(&N2InetNtop)},
        {"getifaddrs", reinterpret_cast<void*>(&N2GetIfAddrs)},
        {"freeifaddrs", reinterpret_cast<void*>(&N2FreeIfAddrs)},
        {"if_nametoindex", reinterpret_cast<void*>(&N2IfNameToIndex)},
        {"gethostname", reinterpret_cast<void*>(&N2GetHostName)},
        {"__cmsg_nxthdr", reinterpret_cast<void*>(&N2CmsgNxthdr)},

        // W^X VM:
        {"mmap", reinterpret_cast<void*>(&N2Mmap)},
        {"mmap64", reinterpret_cast<void*>(&N2Mmap)},
        {"munmap", reinterpret_cast<void*>(&N2Munmap)},
        {"mprotect", reinterpret_cast<void*>(&N2Mprotect)},
        {"madvise", reinterpret_cast<void*>(&N2Madvise)},
        {"posix_madvise", reinterpret_cast<void*>(&N2PosixMadvise)},
        {"mlock", reinterpret_cast<void*>(&N2Mlock)},
        {"munlock", reinterpret_cast<void*>(&N2Munlock)},

        // Pthread attributes, rwlock, sem, once, affinity:
        {"pthread_attr_init", reinterpret_cast<void*>(&N2AttrInit)},
        {"pthread_attr_destroy", reinterpret_cast<void*>(&N2AttrDestroy)},
        {"pthread_attr_setdetachstate", reinterpret_cast<void*>(&N2AttrSetDetachState)},
        {"pthread_attr_setstacksize", reinterpret_cast<void*>(&N2AttrSetStackSize)},
        {"pthread_attr_getstack", reinterpret_cast<void*>(&N2AttrGetStack)},
        {"pthread_getattr_np", reinterpret_cast<void*>(&N2GetAttrNp)},
        {"pthread_setname_np", reinterpret_cast<void*>(&N2SetNameNp)},
        {"pthread_getschedparam", reinterpret_cast<void*>(&N2GetSchedParam)},
        {"pthread_setschedparam", reinterpret_cast<void*>(&N2SetSchedParam)},
        {"pthread_getcpuclockid", reinterpret_cast<void*>(&N2GetCpuClockId)},
        {"pthread_sigmask", reinterpret_cast<void*>(&N2PthreadSigmask)},
        {"pthread_once", reinterpret_cast<void*>(&N2PthreadOnce)},
        {"pthread_rwlock_init", reinterpret_cast<void*>(&N2RwInit)},
        {"pthread_rwlock_destroy", reinterpret_cast<void*>(&N2RwDestroy)},
        {"pthread_rwlock_rdlock", reinterpret_cast<void*>(&N2RwRdlock)},
        {"pthread_rwlock_wrlock", reinterpret_cast<void*>(&N2RwWrlock)},
        {"pthread_rwlock_unlock", reinterpret_cast<void*>(&N2RwUnlock)},
        {"sem_init", reinterpret_cast<void*>(&N2SemInit)},
        {"sem_destroy", reinterpret_cast<void*>(&N2SemDestroy)},
        {"sem_wait", reinterpret_cast<void*>(&N2SemWait)},
        {"sem_trywait", reinterpret_cast<void*>(&N2SemTryWait)},
        {"sem_post", reinterpret_cast<void*>(&N2SemPost)},
        {"sched_yield", reinterpret_cast<void*>(&N2SchedYield)},
        {"sched_getaffinity", reinterpret_cast<void*>(&N2SchedGetAffinity)},
        {"sched_setaffinity", reinterpret_cast<void*>(&N2SchedSetAffinity)},
        {"__sched_cpucount", reinterpret_cast<void*>(&N2SchedCpuCount)},

        // C++ ABI guards:
        {"__cxa_guard_acquire", reinterpret_cast<void*>(&N2CxaGuardAcquire)},
        {"__cxa_guard_release", reinterpret_cast<void*>(&N2CxaGuardRelease)},
        {"__cxa_guard_abort", reinterpret_cast<void*>(&N2CxaGuardAbort)},
        {"__cxa_pure_virtual", reinterpret_cast<void*>(&N2CxaPureVirtual)},

        // System properties, auxval, time, locale, ctype, wchar, strings, regex, signals:
        {"__system_property_get", reinterpret_cast<void*>(&N2SysPropGet)},
        {"__system_property_find", reinterpret_cast<void*>(&N2SysPropFind)},
        {"getauxval", reinterpret_cast<void*>(&N2GetAuxVal)},
        {"getpagesize", reinterpret_cast<void*>(&N2GetPageSize)},
        {"sysconf", reinterpret_cast<void*>(&N2Sysconf)},
        {"gettimeofday", reinterpret_cast<void*>(&N2GetTimeOfDay)},
        {"time", reinterpret_cast<void*>(&N2Time)},
        {"clock", reinterpret_cast<void*>(&N2Clock)},
        {"localtime_r", reinterpret_cast<void*>(&N2LocaltimeR)},
        {"localtime", reinterpret_cast<void*>(&N2Localtime)},
        {"gmtime_r", reinterpret_cast<void*>(&N2GmtimeR)},
        {"mktime", reinterpret_cast<void*>(&N2Mktime)},
        {"strftime", reinterpret_cast<void*>(&N2Strftime)},
        {"strftime_l", reinterpret_cast<void*>(&N2StrftimeL)},
        {"tzset", reinterpret_cast<void*>(&N2Tzset)},
        {"setlocale", reinterpret_cast<void*>(&N2SetLocale)},
        {"newlocale", reinterpret_cast<void*>(&N2NewLocale)},
        {"uselocale", reinterpret_cast<void*>(&N2UseLocale)},
        {"freelocale", reinterpret_cast<void*>(&N2FreeLocale)},
        {"localeconv", reinterpret_cast<void*>(&N2LocaleConv)},
        {"__ctype_get_mb_cur_max", reinterpret_cast<void*>(&N2MbCurMax)},
        {"iswalpha_l", reinterpret_cast<void*>(&N2IswalphaL)},
        {"iswblank_l", reinterpret_cast<void*>(&N2IswblankL)},
        {"iswcntrl_l", reinterpret_cast<void*>(&N2IswcntrlL)},
        {"iswdigit_l", reinterpret_cast<void*>(&N2IswdigitL)},
        {"iswlower_l", reinterpret_cast<void*>(&N2IswlowerL)},
        {"iswprint_l", reinterpret_cast<void*>(&N2IswprintL)},
        {"iswpunct_l", reinterpret_cast<void*>(&N2IswpunctL)},
        {"iswspace_l", reinterpret_cast<void*>(&N2IswspaceL)},
        {"iswupper_l", reinterpret_cast<void*>(&N2IswupperL)},
        {"iswxdigit_l", reinterpret_cast<void*>(&N2IswxdigitL)},
        {"towlower_l", reinterpret_cast<void*>(&N2TowlowerL)},
        {"towupper_l", reinterpret_cast<void*>(&N2TowupperL)},
        {"btowc", reinterpret_cast<void*>(&N2Btowc)},
        {"wctob", reinterpret_cast<void*>(&N2Wctob)},
        {"mbrlen", reinterpret_cast<void*>(&N2Mbrlen)},
        {"mbrtowc", reinterpret_cast<void*>(&N2Mbrtowc)},
        {"mbtowc", reinterpret_cast<void*>(&N2Mbtowc)},
        {"wcrtomb", reinterpret_cast<void*>(&N2Wcrtomb)},
        {"mbsrtowcs", reinterpret_cast<void*>(&N2Mbsrtowcs)},
        {"mbsnrtowcs", reinterpret_cast<void*>(&N2Mbsnrtowcs)},
        {"wcsnrtombs", reinterpret_cast<void*>(&N2Wcsnrtombs)},
        {"wcslen", reinterpret_cast<void*>(&N2Wcslen)},
        {"wmemchr", reinterpret_cast<void*>(&N2Wmemchr)},
        {"wmemcmp", reinterpret_cast<void*>(&N2Wmemcmp)},
        {"wcscoll_l", reinterpret_cast<void*>(&N2WcscollL)},
        {"wcsxfrm_l", reinterpret_cast<void*>(&N2WcsxfrmL)},
        {"wcstol", reinterpret_cast<void*>(&N2Wcstoll)},
        {"wcstoll", reinterpret_cast<void*>(&N2Wcstoll)},
        {"wcstoul", reinterpret_cast<void*>(&N2Wcstoull)},
        {"wcstoull", reinterpret_cast<void*>(&N2Wcstoull)},
        {"wcstod", reinterpret_cast<void*>(&N2Wcstod)},
        {"wcstof", reinterpret_cast<void*>(&N2Wcstof)},
        {"wcstold", reinterpret_cast<void*>(&N2Wcstod)},
        {"strtod", reinterpret_cast<void*>(&N2Strtod)},
        {"strtof", reinterpret_cast<void*>(&N2Strtof)},
        {"strtold", reinterpret_cast<void*>(&N2Strtod)},
        {"strtold_l", reinterpret_cast<void*>(&N2StrtoldL)},
        {"strtoll_l", reinterpret_cast<void*>(&N2StrtollL)},
        {"strtoull_l", reinterpret_cast<void*>(&N2StrtoullL)},
        {"strtoumax", reinterpret_cast<void*>(&N2StrtoullL)},
        {"atof", reinterpret_cast<void*>(&N2Atof)},
        {"abs", reinterpret_cast<void*>(&N2Abs)},
        {"labs", reinterpret_cast<void*>(&N2Llabs)},
        {"llabs", reinterpret_cast<void*>(&N2Llabs)},
        {"qsort", reinterpret_cast<void*>(&N2Qsort)},
        {"bsearch", reinterpret_cast<void*>(&N2Bsearch)},
        {"strndup", reinterpret_cast<void*>(&N2Strndup)},
        {"strcat", reinterpret_cast<void*>(&N2Strcat)},
        {"strncat", reinterpret_cast<void*>(&N2Strncat)},
        {"strcspn", reinterpret_cast<void*>(&N2Strcspn)},
        {"strspn", reinterpret_cast<void*>(&N2Strspn)},
        {"strpbrk", reinterpret_cast<void*>(&N2Strpbrk)},
        {"strtok_r", reinterpret_cast<void*>(&N2StrtokR)},
        {"strcasecmp", reinterpret_cast<void*>(&N2Strcasecmp)},
        {"strncasecmp", reinterpret_cast<void*>(&N2Strncasecmp)},
        {"strcoll", reinterpret_cast<void*>(&N2StrcollL)},
        {"strcoll_l", reinterpret_cast<void*>(&N2StrcollL)},
        {"strxfrm_l", reinterpret_cast<void*>(&N2StrxfrmL)},
        {"strerror", reinterpret_cast<void*>(&N2Strerror)},
        {"strerror_r", reinterpret_cast<void*>(&N2StrerrorR)},
        {"__gnu_strerror_r", reinterpret_cast<void*>(&N2GnuStrerrorR)},
        {"__memcpy_chk", reinterpret_cast<void*>(&N2MemcpyChk)},
        {"__memmove_chk", reinterpret_cast<void*>(&N2MemmoveChk)},
        {"__strcpy_chk", reinterpret_cast<void*>(&N2StrcpyChk)},
        {"__strchr_chk", reinterpret_cast<void*>(&N2StrchrChk)},
        {"__strlen_chk", reinterpret_cast<void*>(&N2StrlenChk)},
        {"regcomp", reinterpret_cast<void*>(&N2Regcomp)},
        {"regexec", reinterpret_cast<void*>(&N2Regexec)},
        {"regerror", reinterpret_cast<void*>(&N2Regerror)},
        {"regfree", reinterpret_cast<void*>(&N2Regfree)},
        {"arc4random", reinterpret_cast<void*>(&N2Arc4random)},
        {"rand", reinterpret_cast<void*>(&N2Rand)},
        {"rand_r", reinterpret_cast<void*>(&N2RandR)},
        {"sigaction", reinterpret_cast<void*>(&N2SigAction)},
        {"sigemptyset", reinterpret_cast<void*>(&N2SigEmptySet)},
        {"sigfillset", reinterpret_cast<void*>(&N2SigFillSet)},
        {"sigaddset", reinterpret_cast<void*>(&N2SigAddSet)},
        {"sigprocmask", reinterpret_cast<void*>(&N2SigProcmask)},
        {"__libc_current_sigrtmax", reinterpret_cast<void*>(&N2SigRtMax)},
        {"openlog", reinterpret_cast<void*>(&N2OpenLog)},
        {"closelog", reinterpret_cast<void*>(&N2CloseLog)},
        {"getrlimit", reinterpret_cast<void*>(&N2GetRlimit)},
        {"getrusage", reinterpret_cast<void*>(&N2GetRusage)},
        {"setpriority", reinterpret_cast<void*>(&N2SetPriority)},
        {"abort", reinterpret_cast<void*>(&N2Abort)},
        {"__stack_chk_fail", reinterpret_cast<void*>(&N2StackChkFail)},
        {"__assert2", reinterpret_cast<void*>(&N2Assert2)},
        {"android_set_abort_message", reinterpret_cast<void*>(&N2SetAbortMessage)},
        {"_exit", reinterpret_cast<void*>(&N2Exit)},
        {"_Exit", reinterpret_cast<void*>(&N2Exit)},
        {"exit", reinterpret_cast<void*>(&N2Exit)},
        {"raise", reinterpret_cast<void*>(&N2Raise)},
        {"fork", reinterpret_cast<void*>(&N2Enosys)},
        {"execve", reinterpret_cast<void*>(&N2Enosys)},
        {"execlp", reinterpret_cast<void*>(&N2Enosys)},
        {"execvp", reinterpret_cast<void*>(&N2Enosys)},
        {"wait", reinterpret_cast<void*>(&N2Enosys)},
        {"waitpid", reinterpret_cast<void*>(&N2Enosys)},
        {"kill", reinterpret_cast<void*>(&N2Enosys)},
        {"setsid", reinterpret_cast<void*>(&N2Enosys)},
        {"tcgetpgrp", reinterpret_cast<void*>(&N2Enosys)},
        {"getpgrp", reinterpret_cast<void*>(&N2Enosys)},
    };
    for (const auto& s : n2Libc) {
        UpsertSymbol(libc->symbols, s.name, s.address);
        UpsertSymbol(combinedImports, s.name, s.address);
    }

    // 4. Register `libm.so` (`LIBC`):
    std::vector<HostSymbol> libmSyms(std::begin(kLibmSymbols), std::end(kLibmSymbols));
    for (const auto& s : libmSyms) {
        UpsertSymbol(libc->symbols, s.name, s.address);
        UpsertSymbol(combinedImports, s.name, s.address);
    }
    libraries.push_back({"libm.so", std::move(libmSyms), "LIBC"});
    return ext;
}

uint32_t QueryN2FdReady(int fd, uint64_t* nextDeadlineMonoNs) {
    auto ent = GetFd(fd);
    if (!ent) return 4u; // ALOOPER_EVENT_ERROR
    const uint32_t ep = EvalEntryEpollMask(*ent, MonoNowNs(), nextDeadlineMonoNs);
    uint32_t al = 0;
    if (ep & 1u) al |= 1u;  // ALOOPER_EVENT_INPUT
    if (ep & 4u) al |= 2u;  // ALOOPER_EVENT_OUTPUT
    if (ep & 8u) al |= 4u;  // ALOOPER_EVENT_ERROR
    if (ep & 16u) al |= 8u; // ALOOPER_EVENT_HANGUP
    return al;
}

uint64_t RegisterN2WakeListener(std::function<void()> callback) {
    std::lock_guard lock(g_wakeMu);
    const uint64_t id = g_nextListenerId++;
    g_wakeListeners[id] = std::move(callback);
    return id;
}

void UnregisterN2WakeListener(uint64_t listenerId) {
    std::lock_guard lock(g_wakeMu);
    g_wakeListeners.erase(listenerId);
}

void SetGuestSystemProperty(const char* name, const char* value) {
    if (!name || !*name) return;
    std::lock_guard lk(g_propMu);
    g_sysProps[name] = value ? value : "";
}

}  // namespace winapk::guest
