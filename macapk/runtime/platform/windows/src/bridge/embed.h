// embed.h -- shared-memory RGBA8 frame output and duplex control pipe for `--embed`.
//
// Host-domain code only. When `winapk run` starts with `--embed <token>`, the front end owns
// `\\.\pipe\winapk-<token>` and `Local\winapk-frame-<token>` (double-buffered top-down RGBA8).
// The app's Win32 window stays hidden (`WS_POPUP`) so an unresponsive app never blocks the
// front end's thread input queue, and moving between a pane and a standalone window never
// restarts the runtime process.
#pragma once

#include <windows.h>
#include <jni.h>

#include <cstdint>
#include <functional>
#include <string>

namespace winapk::bridge {

/// Shared-memory frame header at offset 0 of `Local\winapk-frame-<token>`.
struct EmbedFrameHeader {
    uint32_t magic;          ///< 0x5741504B ('WAPK')
    uint32_t version;        ///< 1
    volatile LONG activeSlot;///< 0 or 1
    volatile LONG seq;       ///< incremented after each published frame
    uint32_t width[2];       ///< slot widths in pixels
    uint32_t height[2];      ///< slot heights in pixels
    uint32_t maxPixels;      ///< capacity per slot in pixels (RGBA8 = 4 bytes/pixel)
    uint32_t reserved[7];    ///< pads header to 64 bytes
};
static_assert(sizeof(EmbedFrameHeader) == 64);

/// Control command sent by the front end over the duplex pipe.
enum class EmbedCmdKind { Resize, Pointer, Wheel, Key, Text, Back, Stop };

/// Parsed control command queued for the hidden app window's UI thread.
struct EmbedCmd {
    EmbedCmdKind kind = EmbedCmdKind::Stop;
    int action = 0;       ///< pointer: 0 down, 2 move, 1 up, 3 cancel; key: 1 down, 0 up
    int w = 0, h = 0;     ///< resize: DIPs; key: vk, unicode
    int meta = 0;         ///< key modifier mask (1 shift, 0x1000 ctrl)
    float x = 0, y = 0;   ///< pointer/wheel position in DIPs from top-left
    float dx = 0, dy = 0; ///< wheel notches
    std::wstring text;    ///< UTF-16 text commit
};

/// Window message posted to wake the UI thread when commands arrive.
constexpr UINT kEmbedWindowMessage = WM_APP + 2;

/// Reports whether `--embed` connected for this process.
bool IsEmbedded();

/// Registers the hidden app window and its UI-thread command handler.
void SetEmbedDispatcher(HWND hwnd, std::function<void(const EmbedCmd&)> fn);

/// Drains queued control commands on the UI thread.
void PumpEmbedCommands();

/// Sends one single-line event (`state`, `backend`, `frame`, `log`) over the control pipe.
void SendEmbedEvent(const std::string& line);

/// Forwards a runtime log line with the calling thread ID when embedded.
void SendEmbedLog(const std::string& line);

/// Publishes a top-down `0xAARRGGBB` software frame into shared memory and notifies the front end.
void PublishEmbedArgb(const uint32_t* argb, int w, int h);

/// Publishes top-down `RGBA8` rows (with `rowPitch` stride) directly from a GPU staging surface.
void PublishEmbedRgbaRows(const uint8_t* topDownRgba, size_t rowPitch, int w, int h);

/// Publishes a bottom-up OpenGL `RGBA8` frame into shared memory (flipped top-down, opaque alpha).
void PublishEmbedGlRgba(const uint8_t* bottomUpRgba, int w, int h);

/// Captures `hwnd` via `PrintWindow` (for non-GL swapchains) and publishes it into shared memory.
void PublishEmbedHwnd(HWND hwnd);

}  // namespace winapk::bridge

/// Connects to `\\.\pipe\winapk-<token>` and `Local\winapk-frame-<token>` before the JVM starts.
extern "C" __declspec(dllexport) jboolean winapk_bridge_connect_embed(const wchar_t* token);
