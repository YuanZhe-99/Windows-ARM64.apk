// embed.cpp -- shared-memory RGBA8 frame publishing and duplex pipe IPC for `--embed`.
#include "embed.h"
#include "bridge.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <deque>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>

namespace winapk::bridge {
namespace {

constexpr uint32_t kEmbedMagic = 0x5741504Bu;

std::atomic<bool> g_active{false};
HANDLE g_pipe = INVALID_HANDLE_VALUE;
HANDLE g_readEvent = nullptr;
HANDLE g_writeEvent = nullptr;
HANDLE g_mapping = nullptr;
uint8_t* g_view = nullptr;
size_t g_viewBytes = 0;
std::mutex g_writeMu;
std::mutex g_frameMu;
std::string g_lastBackend;

std::mutex g_cmdMu;
HWND g_hwnd = nullptr;
std::function<void(const EmbedCmd&)> g_dispatch;
std::deque<EmbedCmd> g_cmds;

/// Decodes hex-encoded UTF-8 bytes into UTF-16 for safe whitespace-free transport.
std::wstring DecodeHexUtf8(const std::string& hex) {
    std::string bytes;
    bytes.reserve(hex.size() / 2);
    auto nib = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
        if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
        return -1;
    };
    for (size_t i = 0; i + 1 < hex.size(); i += 2) {
        const int hi = nib(hex[i]), lo = nib(hex[i + 1]);
        if (hi < 0 || lo < 0) break;
        bytes.push_back(static_cast<char>((hi << 4) | lo));
    }
    if (bytes.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, bytes.data(), static_cast<int>(bytes.size()), nullptr, 0);
    if (n <= 0) return {};
    std::wstring out(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, bytes.data(), static_cast<int>(bytes.size()), out.data(), n);
    return out;
}

/// Enqueues one parsed command and wakes the UI thread without blocking if the UI thread is hung.
void Enqueue(EmbedCmd cmd) {
    HWND target = nullptr;
    const bool isStop = cmd.kind == EmbedCmdKind::Stop;
    if (cmd.kind == EmbedCmdKind::Resize && cmd.w > 0 && cmd.h > 0) {
        winapk_bridge_set_phone_size(cmd.w, cmd.h);
    }
    {
        std::lock_guard<std::mutex> lock(g_cmdMu);
        target = g_hwnd;
        g_cmds.push_back(std::move(cmd));
    }
    if (target) {
        if (isStop) PostMessageW(target, WM_CLOSE, 0, 0);
        else PostMessageW(target, kEmbedWindowMessage, 0, 0);
    }
}

/// Parses one newline-terminated command from the front end.
void HandleLine(const std::string& line) {
    std::istringstream in(line);
    std::string op;
    if (!(in >> op)) return;
    EmbedCmd c;
    if (op == "resize") {
        if (in >> c.w >> c.h && c.w > 0 && c.h > 0) { c.kind = EmbedCmdKind::Resize; Enqueue(std::move(c)); }
    } else if (op == "pointer") {
        if (in >> c.action >> c.x >> c.y) { c.kind = EmbedCmdKind::Pointer; Enqueue(std::move(c)); }
    } else if (op == "wheel") {
        if (in >> c.x >> c.y >> c.dx >> c.dy) { c.kind = EmbedCmdKind::Wheel; Enqueue(std::move(c)); }
    } else if (op == "key") {
        if (in >> c.action >> c.w >> c.h >> c.meta) { c.kind = EmbedCmdKind::Key; Enqueue(std::move(c)); }
    } else if (op == "text") {
        std::string hex;
        if (in >> hex) {
            c.kind = EmbedCmdKind::Text;
            c.text = DecodeHexUtf8(hex);
            if (!c.text.empty()) Enqueue(std::move(c));
        }
    } else if (op == "back") {
        c.kind = EmbedCmdKind::Back;
        Enqueue(std::move(c));
    } else if (op == "stop") {
        c.kind = EmbedCmdKind::Stop;
        Enqueue(std::move(c));
    }
}

/// Writes `wire` over the overlapped duplex pipe without blocking against concurrent `ReadFile`.
void WritePipe(const std::string& wire) {
    if (g_pipe == INVALID_HANDLE_VALUE || !g_writeEvent) return;
    OVERLAPPED ov{};
    ov.hEvent = g_writeEvent;
    ResetEvent(g_writeEvent);
    DWORD written = 0;
    if (!WriteFile(g_pipe, wire.data(), static_cast<DWORD>(wire.size()), &written, &ov)) {
        if (GetLastError() == ERROR_IO_PENDING) GetOverlappedResult(g_pipe, &ov, &written, TRUE);
    }
}

/// Reads control lines from the front end until the pipe closes.
void ReaderLoop(HANDLE pipe) {
    std::string buf;
    char chunk[1024];
    for (;;) {
        OVERLAPPED ov{};
        ov.hEvent = g_readEvent;
        ResetEvent(g_readEvent);
        DWORD got = 0;
        BOOL ok = ReadFile(pipe, chunk, sizeof chunk, &got, &ov);
        if (!ok && GetLastError() == ERROR_IO_PENDING) ok = GetOverlappedResult(pipe, &ov, &got, TRUE);
        if (!ok || got == 0) break;
        buf.append(chunk, got);
        size_t nl = 0;
        while ((nl = buf.find('\n')) != std::string::npos) {
            std::string line = buf.substr(0, nl);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            buf.erase(0, nl + 1);
            HandleLine(line);
        }
    }
    // Front end disconnected unexpectedly: ask the app window to close.
    Enqueue(EmbedCmd{EmbedCmdKind::Stop});
}

/// Returns the writable pixel buffer of the next inactive slot, or null when out of bounds.
uint8_t* AcquireSlot(int w, int h, EmbedFrameHeader*& hdr, LONG& nextSlot) {
    if (!g_view || w <= 0 || h <= 0) return nullptr;
    hdr = reinterpret_cast<EmbedFrameHeader*>(g_view);
    if (hdr->magic != kEmbedMagic || hdr->version != 1) return nullptr;
    const uint64_t pixels = static_cast<uint64_t>(w) * static_cast<uint64_t>(h);
    if (pixels == 0 || pixels > hdr->maxPixels) return nullptr;
    const uint64_t slotBytes = static_cast<uint64_t>(hdr->maxPixels) * 4ull;
    if (sizeof(EmbedFrameHeader) + slotBytes * 2ull > g_viewBytes) return nullptr;
    const LONG cur = InterlockedCompareExchange(&hdr->activeSlot, 0, 0);
    nextSlot = (cur == 0) ? 1 : 0;
    return g_view + sizeof(EmbedFrameHeader) + static_cast<size_t>(nextSlot) * static_cast<size_t>(slotBytes);
}

void CommitSlot(EmbedFrameHeader* hdr, LONG slot, int w, int h) {
    hdr->width[slot] = static_cast<uint32_t>(w);
    hdr->height[slot] = static_cast<uint32_t>(h);
    InterlockedExchange(&hdr->activeSlot, slot);
    const LONG seq = InterlockedIncrement(&hdr->seq);
    SendEmbedEvent("frame " + std::to_string(seq) + " " + std::to_string(w) + " " + std::to_string(h));
}

}  // namespace

bool IsEmbedded() { return g_active.load(std::memory_order_acquire); }

void SetEmbedDispatcher(HWND hwnd, std::function<void(const EmbedCmd&)> fn) {
    bool pending = false;
    {
        std::lock_guard<std::mutex> lock(g_cmdMu);
        g_hwnd = hwnd;
        g_dispatch = std::move(fn);
        pending = !g_cmds.empty();
    }
    if (hwnd && pending) PostMessageW(hwnd, kEmbedWindowMessage, 0, 0);
}

void PumpEmbedCommands() {
    for (;;) {
        EmbedCmd cmd;
        std::function<void(const EmbedCmd&)> fn;
        {
            std::lock_guard<std::mutex> lock(g_cmdMu);
            if (g_cmds.empty() || !g_dispatch) return;
            cmd = std::move(g_cmds.front());
            g_cmds.pop_front();
            fn = g_dispatch;
        }
        fn(cmd);
    }
}

void SendEmbedEvent(const std::string& line) {
    if (!IsEmbedded() || g_pipe == INVALID_HANDLE_VALUE) return;
    if (line.rfind("backend ", 0) == 0) {
        std::lock_guard<std::mutex> lock(g_writeMu);
        if (g_lastBackend == line) return;
        g_lastBackend = line;
        WritePipe(line + "\n");
        return;
    }
    std::string clean = line;
    for (char& c : clean) if (c == '\r' || c == '\n') c = ' ';
    std::lock_guard<std::mutex> lock(g_writeMu);
    WritePipe(clean + "\n");
}

void SendEmbedLog(const std::string& line) {
    if (!IsEmbedded()) return;
    SendEmbedEvent("log " + std::to_string(GetCurrentThreadId()) + " " + line);
}

void PublishEmbedArgb(const uint32_t* argb, int w, int h) {
    if (!IsEmbedded() || !argb || w <= 0 || h <= 0) return;
    std::lock_guard<std::mutex> lock(g_frameMu);
    EmbedFrameHeader* hdr = nullptr;
    LONG slot = 0;
    uint8_t* dst = AcquireSlot(w, h, hdr, slot);
    if (!dst) return;
    const size_t n = static_cast<size_t>(w) * static_cast<size_t>(h);
    for (size_t i = 0; i < n; ++i) {
        const uint32_t p = argb[i];
        dst[i * 4 + 0] = static_cast<uint8_t>((p >> 16) & 0xff);
        dst[i * 4 + 1] = static_cast<uint8_t>((p >> 8) & 0xff);
        dst[i * 4 + 2] = static_cast<uint8_t>(p & 0xff);
        dst[i * 4 + 3] = 0xff;
    }
    CommitSlot(hdr, slot, w, h);
}

void PublishEmbedRgbaRows(const uint8_t* topDownRgba, size_t rowPitch, int w, int h) {
    if (!IsEmbedded() || !topDownRgba || w <= 0 || h <= 0) return;
    const size_t dstStride = static_cast<size_t>(w) * 4;
    if (rowPitch < dstStride) return;
    std::lock_guard<std::mutex> lock(g_frameMu);
    EmbedFrameHeader* hdr = nullptr;
    LONG slot = 0;
    uint8_t* dst = AcquireSlot(w, h, hdr, slot);
    if (!dst) return;
    if (rowPitch == dstStride) {
        std::memcpy(dst, topDownRgba, dstStride * static_cast<size_t>(h));
    } else {
        for (int y = 0; y < h; ++y) {
            std::memcpy(dst + static_cast<size_t>(y) * dstStride,
                        topDownRgba + static_cast<size_t>(y) * rowPitch, dstStride);
        }
    }
    CommitSlot(hdr, slot, w, h);
}

void PublishEmbedGlRgba(const uint8_t* bottomUpRgba, int w, int h) {
    if (!IsEmbedded() || !bottomUpRgba || w <= 0 || h <= 0) return;
    std::lock_guard<std::mutex> lock(g_frameMu);
    EmbedFrameHeader* hdr = nullptr;
    LONG slot = 0;
    uint8_t* dst = AcquireSlot(w, h, hdr, slot);
    if (!dst) return;
    const size_t stride = static_cast<size_t>(w) * 4;
    for (int y = 0; y < h; ++y) {
        const uint8_t* srcRow = bottomUpRgba + static_cast<size_t>(h - 1 - y) * stride;
        uint8_t* dstRow = dst + static_cast<size_t>(y) * stride;
        std::memcpy(dstRow, srcRow, stride);
        for (int x = 0; x < w; ++x) dstRow[x * 4 + 3] = 0xff;
    }
    CommitSlot(hdr, slot, w, h);
}

void PublishEmbedHwnd(HWND hwnd) {
    if (!IsEmbedded() || !hwnd) return;
    RECT rc{};
    if (!GetClientRect(hwnd, &rc) || rc.right <= 0 || rc.bottom <= 0) return;
    const int w = rc.right, h = rc.bottom;
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h;  // top-down BGRA
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HDC mem = CreateCompatibleDC(nullptr);
    if (!mem) return;
    HBITMAP bmp = CreateDIBSection(mem, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (bmp && bits) {
        HGDIOBJ old = SelectObject(mem, bmp);
        if (PrintWindow(hwnd, mem, 3)) PublishEmbedArgb(static_cast<const uint32_t*>(bits), w, h);
        SelectObject(mem, old);
    }
    if (bmp) DeleteObject(bmp);
    DeleteDC(mem);
}

}  // namespace winapk::bridge

extern "C" __declspec(dllexport) jboolean winapk_bridge_connect_embed(const wchar_t* token) {
    using namespace winapk::bridge;
    if (!token || !*token) return JNI_FALSE;
    const std::wstring pipeName = std::wstring(L"\\\\.\\pipe\\winapk-") + token;
    const std::wstring mapName = std::wstring(L"Local\\winapk-frame-") + token;

    HANDLE pipe = CreateFileW(pipeName.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                              OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
    if (pipe == INVALID_HANDLE_VALUE) return JNI_FALSE;

    HANDLE readEv = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    HANDLE writeEv = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!readEv || !writeEv) {
        if (readEv) CloseHandle(readEv);
        if (writeEv) CloseHandle(writeEv);
        CloseHandle(pipe);
        return JNI_FALSE;
    }

    HANDLE mapping = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, mapName.c_str());
    if (!mapping) { CloseHandle(readEv); CloseHandle(writeEv); CloseHandle(pipe); return JNI_FALSE; }

    auto* view = static_cast<uint8_t*>(MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, 0));
    if (!view) { CloseHandle(mapping); CloseHandle(readEv); CloseHandle(writeEv); CloseHandle(pipe); return JNI_FALSE; }

    MEMORY_BASIC_INFORMATION mbi{};
    if (!VirtualQuery(view, &mbi, sizeof mbi) || mbi.RegionSize < sizeof(EmbedFrameHeader)) {
        UnmapViewOfFile(view); CloseHandle(mapping); CloseHandle(readEv); CloseHandle(writeEv); CloseHandle(pipe); return JNI_FALSE;
    }
    auto* hdr = reinterpret_cast<EmbedFrameHeader*>(view);
    if (hdr->magic != 0x5741504Bu || hdr->version != 1 || hdr->maxPixels == 0) {
        UnmapViewOfFile(view); CloseHandle(mapping); CloseHandle(readEv); CloseHandle(writeEv); CloseHandle(pipe); return JNI_FALSE;
    }

    g_pipe = pipe;
    g_readEvent = readEv;
    g_writeEvent = writeEv;
    g_mapping = mapping;
    g_view = view;
    g_viewBytes = mbi.RegionSize;
    g_active.store(true, std::memory_order_release);

    SendEmbedEvent("hello " + std::to_string(GetCurrentProcessId()));
    std::thread(ReaderLoop, pipe).detach();
    return JNI_TRUE;
}
