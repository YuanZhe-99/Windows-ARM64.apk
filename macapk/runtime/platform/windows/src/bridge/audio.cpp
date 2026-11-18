// audio.cpp -- Native Windows ARM64 WASAPI shared-mode mixer and Media Foundation / WAV / Ogg Vorbis
// clip decoder backing com.macapk.loader.NativeAudio (AudioTrack, SoundPool, MediaPlayer).
//
// Modes (WINAPK_AUDIO_BACKEND):
//   - "wasapi" (default): opens the default Windows eRender endpoint via IMMDeviceEnumerator +
//     IAudioClient shared mode (AUTOCONVERTPCM, IEEE float stereo), automatically rebinding on
//     OnDefaultDeviceChanged / AUDCLNT_E_DEVICE_INVALIDATED and degrading to non-blocking swallow
//     if no audio endpoint exists on the host.
//   - "virtual": runs a 10 ms QPC-paced silent software mixer thread without opening hardware
//     endpoints, exercising ring-buffer drain, resampling, underruns, timestamps and voices.
//   - "none": simulates a host with zero audio output devices (E_NOTFOUND fallback).
#include "audio.h"
#include "bridge.h"

#include <windows.h>
#include <audioclient.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mmdeviceapi.h>
#include <shlwapi.h>
#include <wrl/client.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

extern "C" int stb_vorbis_decode_memory(const unsigned char* mem, int len,
                                        int* channels, int* sample_rate,
                                        short** output);

namespace winapk::bridge {
namespace {

using Microsoft::WRL::ComPtr;

enum {
    ENC_INVALID   = 0,
    ENC_DEFAULT   = 1,
    ENC_PCM_16BIT = 2,
    ENC_PCM_8BIT  = 3,
    ENC_PCM_FLOAT = 4,
};

enum {
    PLAYSTATE_STOPPED = 1,
    PLAYSTATE_PAUSED  = 2,
    PLAYSTATE_PLAYING = 3,
};

enum {
    RATE_OK         = 0,
    RATE_BAD_VALUE  = -2,
    RATE_INVALID_OP = -3,
};

enum StreamMode {
    MODE_RING = 0,
    MODE_CLIP = 1,
};

constexpr int kCanonicalRate = 48000;
constexpr int kCanonicalChannels = 2;
constexpr size_t kMaxVoices = 24;
constexpr float kSpeedMin = 0.01f;
constexpr float kSpeedMax = 20.0f;
constexpr float kPitchMin = 0.25f;
constexpr float kPitchMax = 4.0f;
constexpr float kPitchMinDelta = 0.0001f;

/// Returns monotonic nanoseconds using QueryPerformanceCounter (same clock source as HotSpot nanoTime).
int64_t HostNanoTime() {
    static const LARGE_INTEGER freq = [] {
        LARGE_INTEGER f{};
        QueryPerformanceFrequency(&f);
        return f;
    }();
    LARGE_INTEGER now{};
    QueryPerformanceCounter(&now);
    const int64_t q = now.QuadPart / freq.QuadPart;
    const int64_t r = now.QuadPart % freq.QuadPart;
    return q * 1000000000LL + (r * 1000000000LL) / freq.QuadPart;
}

/// Lock-free single-producer / single-consumer ring buffer of interleaved float32 samples.
struct Ring {
    float* buf = nullptr;
    uint64_t cap = 0;
    uint64_t mask = 0;
    std::atomic<uint64_t> wpos{0};
    std::atomic<uint64_t> rpos{0};
    std::atomic<uint64_t> limit{0};

    ~Ring() { std::free(buf); }

    void Init(uint64_t wantElems) {
        uint64_t c = 64;
        while (c < wantElems) c <<= 1;
        cap = c;
        mask = c - 1;
        buf = static_cast<float*>(std::calloc(cap, sizeof(float)));
        limit.store(wantElems, std::memory_order_relaxed);
    }

    uint32_t Push(const float* src, uint32_t n) {
        if (!buf || n == 0) return 0;
        const uint64_t w = wpos.load(std::memory_order_relaxed);
        const uint64_t r = rpos.load(std::memory_order_acquire);
        uint64_t lim = limit.load(std::memory_order_relaxed);
        if (lim > cap) lim = cap;
        const uint64_t used = w - r;
        const uint64_t space = used < lim ? (lim - used) : 0;
        if (n > space) n = static_cast<uint32_t>(space);
        for (uint32_t i = 0; i < n; ++i) buf[(w + i) & mask] = src[i];
        wpos.store(w + n, std::memory_order_release);
        return n;
    }

    /// Consumes source frames using linear interpolation at `ratio` source frames per output stereo frame.
    uint32_t PopStereoResampled(float* dstStereoAccum, uint32_t wantOutFrames, int srcCh,
                                double ratio, double& frac, float gainL, float gainR,
                                uint64_t& srcFramesConsumed) {
        srcFramesConsumed = 0;
        if (!buf || srcCh <= 0 || wantOutFrames == 0) return 0;
        uint64_t r = rpos.load(std::memory_order_relaxed);
        const uint64_t w = wpos.load(std::memory_order_acquire);
        const uint64_t availFrames = (w - r) / static_cast<uint64_t>(srcCh);
        if (availFrames == 0) return 0;

        uint64_t curFrame = 0;
        uint32_t produced = 0;
        while (produced < wantOutFrames && curFrame < availFrames) {
            const uint64_t f0 = curFrame;
            const uint64_t f1 = (f0 + 1 < availFrames) ? (f0 + 1) : f0;
            const uint64_t base0 = r + f0 * static_cast<uint64_t>(srcCh);
            const uint64_t base1 = r + f1 * static_cast<uint64_t>(srcCh);
            float l0 = buf[base0 & mask];
            float r0 = (srcCh >= 2) ? buf[(base0 + 1) & mask] : l0;
            float l1 = buf[base1 & mask];
            float r1 = (srcCh >= 2) ? buf[(base1 + 1) & mask] : l1;
            const float l = static_cast<float>(l0 + (l1 - l0) * frac) * gainL;
            const float rs = static_cast<float>(r0 + (r1 - r0) * frac) * gainR;
            dstStereoAccum[produced * 2 + 0] += l;
            dstStereoAccum[produced * 2 + 1] += rs;
            ++produced;

            frac += ratio;
            const uint64_t step = static_cast<uint64_t>(frac);
            frac -= static_cast<double>(step);
            curFrame += step;
        }
        if (curFrame > availFrames) {
            curFrame = availFrames;
            frac = 0.0;
        }
        rpos.store(r + curFrame * static_cast<uint64_t>(srcCh), std::memory_order_release);
        srcFramesConsumed = curFrame;
        return produced;
    }
};

struct Clip {
    float* pcm = nullptr;     // interleaved float32 stereo @ kCanonicalRate
    uint64_t frames = 0;
};

struct Stream {
    StreamMode mode = MODE_RING;
    int sampleRate = kCanonicalRate;
    int channels = kCanonicalChannels;
    int encoding = ENC_PCM_16BIT;
    uint32_t frameCount = 0;
    Ring ring;

    std::atomic<int> playState{PLAYSTATE_STOPPED};
    std::atomic<bool> released{false};
    std::atomic<float> gainL{1.0f};
    std::atomic<float> gainR{1.0f};
    std::atomic<int> playbackSr{kCanonicalRate};
    std::atomic<float> pbSpeed{1.0f};
    std::atomic<float> pbPitch{1.0f};
    std::atomic<uint64_t> playedFrames{0};
    std::atomic<int64_t> lastRenderNanos{0};
    std::atomic<uint32_t> underruns{0};
    bool inUnderrun = false;
    double ringFrac = 0.0;

    // MODE_CLIP fields (SoundPool voice or MediaPlayer)
    bool ownsClip = false;
    const float* clipPcm = nullptr;
    uint64_t clipFrames = 0;
    std::atomic<uint64_t> clipCursor{0};
    double clipFrac = 0.0;
    std::atomic<int> clipLoop{0};
    std::atomic<float> clipRate{1.0f};
    std::atomic<uint32_t> generation{0};
};

/// Listens for default Windows audio output endpoint changes so WASAPI seamlessly rebinds.
class EndpointNotify final : public IMMNotificationClient {
public:
    explicit EndpointNotify(std::atomic<bool>& flag, std::atomic<uint32_t>& count)
        : rebind_(flag), changes_(count) {}

    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs_; }
    ULONG STDMETHODCALLTYPE Release() override {
        const ULONG r = --refs_;
        if (r == 0) delete this;
        return r;
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override {
        if (!out) return E_POINTER;
        if (iid == __uuidof(IUnknown) || iid == __uuidof(IMMNotificationClient)) {
            *out = static_cast<IMMNotificationClient*>(this);
            AddRef();
            return S_OK;
        }
        *out = nullptr;
        return E_NOINTERFACE;
    }
    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR) override {
        if (flow == eRender && role == eConsole) {
            changes_.fetch_add(1, std::memory_order_relaxed);
            rebind_.store(true, std::memory_order_release);
        }
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR, DWORD) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR, const PROPERTYKEY) override { return S_OK; }

private:
    std::atomic<ULONG> refs_{1};
    std::atomic<bool>& rebind_;
    std::atomic<uint32_t>& changes_;
};

std::mutex g_audioMu;
std::vector<std::unique_ptr<Stream>> g_streams;
std::vector<Clip> g_clips;
std::vector<Stream*> g_voices;
std::atomic<uint32_t> g_voiceRr{0};

bool g_engineInit = false;
std::atomic<bool> g_engineActive{false};  // true when WASAPI or virtual mixer thread drains rings
std::atomic<bool> g_engineStop{false};
std::atomic<bool> g_rebindEndpoint{false};
std::atomic<uint32_t> g_rebindCount{0};
std::atomic<int> g_mixSampleRate{kCanonicalRate};
std::thread g_renderThread;

Stream* StreamOf(int64_t h) {
    std::lock_guard<std::mutex> lk(g_audioMu);
    if (h <= 0 || static_cast<size_t>(h) > g_streams.size()) return nullptr;
    Stream* s = g_streams[static_cast<size_t>(h - 1)].get();
    return (s && !s->released.load(std::memory_order_relaxed)) ? s : nullptr;
}

/// Mixes all active streams and voices into `dstStereo` (`frames` stereo float32 frames at `outRate`).
void MixOutput(float* dstStereo, uint32_t frames, int outRate) {
    std::memset(dstStereo, 0, static_cast<size_t>(frames) * 2 * sizeof(float));
    const int64_t nowNs = HostNanoTime();

    std::vector<Stream*> snapshot;
    {
        std::lock_guard<std::mutex> lk(g_audioMu);
        snapshot.reserve(g_streams.size() + g_voices.size());
        for (const auto& s : g_streams) snapshot.push_back(s.get());
        for (Stream* v : g_voices) snapshot.push_back(v);
    }

    for (Stream* s : snapshot) {
        if (!s || s->released.load(std::memory_order_acquire)) continue;
        if (s->playState.load(std::memory_order_acquire) != PLAYSTATE_PLAYING) continue;
        const float gl = s->gainL.load(std::memory_order_relaxed);
        const float gr = s->gainR.load(std::memory_order_relaxed);

        if (s->mode == MODE_RING) {
            const int sr = std::max(1, s->playbackSr.load(std::memory_order_relaxed));
            const float pitch = s->pbPitch.load(std::memory_order_relaxed);
            const double ratio = (static_cast<double>(sr) * static_cast<double>(pitch)) / static_cast<double>(outRate);
            uint64_t consumed = 0;
            const uint32_t got = s->ring.PopStereoResampled(dstStereo, frames, s->channels,
                                                            ratio, s->ringFrac, gl, gr, consumed);
            if (consumed > 0) {
                s->playedFrames.fetch_add(consumed, std::memory_order_relaxed);
                s->lastRenderNanos.store(nowNs, std::memory_order_relaxed);
                s->inUnderrun = false;
            } else if (got < frames) {
                if (!s->inUnderrun && s->playedFrames.load(std::memory_order_relaxed) > 0) {
                    s->underruns.fetch_add(1, std::memory_order_relaxed);
                    s->inUnderrun = true;
                }
            }
        } else {
            const float* pcm = s->clipPcm;
            const uint64_t total = s->clipFrames;
            if (!pcm || total == 0) continue;
            const double rate = static_cast<double>(s->clipRate.load(std::memory_order_relaxed)) *
                                (static_cast<double>(kCanonicalRate) / static_cast<double>(outRate));
            uint64_t cur = s->clipCursor.load(std::memory_order_relaxed);
            for (uint32_t i = 0; i < frames; ++i) {
                if (cur >= total) {
                    const int lp = s->clipLoop.load(std::memory_order_relaxed);
                    if (lp == -1) {
                        cur = 0;
                    } else if (lp > 0) {
                        s->clipLoop.store(lp - 1, std::memory_order_relaxed);
                        cur = 0;
                    } else {
                        s->playState.store(PLAYSTATE_STOPPED, std::memory_order_release);
                        break;
                    }
                }
                const uint64_t f0 = cur;
                const uint64_t f1 = (f0 + 1 < total) ? (f0 + 1) : f0;
                const float l = static_cast<float>(pcm[f0 * 2 + 0] + (pcm[f1 * 2 + 0] - pcm[f0 * 2 + 0]) * s->clipFrac) * gl;
                const float r = static_cast<float>(pcm[f0 * 2 + 1] + (pcm[f1 * 2 + 1] - pcm[f0 * 2 + 1]) * s->clipFrac) * gr;
                dstStereo[i * 2 + 0] += l;
                dstStereo[i * 2 + 1] += r;
                s->clipFrac += rate;
                const uint64_t step = static_cast<uint64_t>(s->clipFrac);
                s->clipFrac -= static_cast<double>(step);
                cur += step;
            }
            s->clipCursor.store(std::min(cur, total), std::memory_order_relaxed);
        }
    }

    for (uint32_t i = 0; i < frames * 2; ++i) {
        dstStereo[i] = std::clamp(dstStereo[i], -1.0f, 1.0f);
    }
}

void VirtualRenderLoop() {
    constexpr int kRate = kCanonicalRate;
    constexpr uint32_t kPeriodFrames = 480;  // 10 ms @ 48 kHz
    std::vector<float> buf(kPeriodFrames * 2);
    g_mixSampleRate.store(kRate, std::memory_order_relaxed);
    g_engineActive.store(true, std::memory_order_release);
    int64_t nextWakeNs = HostNanoTime();
    while (!g_engineStop.load(std::memory_order_relaxed)) {
        if (g_rebindEndpoint.exchange(false, std::memory_order_acq_rel)) {
            g_rebindCount.fetch_add(1, std::memory_order_relaxed);
            LogLine("[winapk] audio: virtual endpoint rebound");
        }
        MixOutput(buf.data(), kPeriodFrames, kRate);
        nextWakeNs += 10000000LL;
        const int64_t remNs = nextWakeNs - HostNanoTime();
        if (remNs > 1000000LL) Sleep(static_cast<DWORD>(remNs / 1000000LL));
        else if (remNs < -50000000LL) nextWakeNs = HostNanoTime();
    }
}

void WasapiRenderLoop() {
    const HRESULT coHr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    ComPtr<IMMDeviceEnumerator> enumerator;
    ComPtr<EndpointNotify> notify;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                  IID_PPV_ARGS(&enumerator));
    if (SUCCEEDED(hr) && enumerator) {
        notify.Attach(new EndpointNotify(g_rebindEndpoint, g_rebindCount));
        enumerator->RegisterEndpointNotificationCallback(notify.Get());
    }

    HANDLE wakeEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    wchar_t rebindEnv[32]{};
    const DWORD rebindEnvLen = GetEnvironmentVariableW(L"WINAPK_TEST_AUDIO_REBIND_ONCE", rebindEnv, ARRAYSIZE(rebindEnv));
    bool triggerTestRebind = (rebindEnvLen > 0 && rebindEnv[0] == L'1');

    while (!g_engineStop.load(std::memory_order_relaxed)) {
        ComPtr<IMMDevice> device;
        ComPtr<IAudioClient> client;
        ComPtr<IAudioRenderClient> render;
        uint32_t bufferFrames = 0;
        int deviceRate = kCanonicalRate;
        int deviceChannels = 2;

        if (enumerator && SUCCEEDED(enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device)) && device &&
            SUCCEEDED(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                       reinterpret_cast<void**>(client.GetAddressOf()))) && client) {
            WAVEFORMATEX* mixFmt = nullptr;
            if (SUCCEEDED(client->GetMixFormat(&mixFmt)) && mixFmt) {
                deviceRate = mixFmt->nSamplesPerSec > 0 ? static_cast<int>(mixFmt->nSamplesPerSec) : kCanonicalRate;
                CoTaskMemFree(mixFmt);
            }
            WAVEFORMATEX want{};
            want.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
            want.nChannels = 2;
            want.nSamplesPerSec = static_cast<DWORD>(deviceRate);
            want.wBitsPerSample = 32;
            want.nBlockAlign = 8;
            want.nAvgBytesPerSec = want.nSamplesPerSec * want.nBlockAlign;

            const DWORD flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK |
                                AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM |
                                AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;
            if (SUCCEEDED(client->Initialize(AUDCLNT_SHAREMODE_SHARED, flags, 200000 /* 20 ms */, 0, &want, nullptr)) &&
                SUCCEEDED(client->SetEventHandle(wakeEvent)) &&
                SUCCEEDED(client->GetBufferSize(&bufferFrames)) &&
                SUCCEEDED(client->GetService(IID_PPV_ARGS(&render))) &&
                SUCCEEDED(client->Start())) {
                g_mixSampleRate.store(deviceRate, std::memory_order_relaxed);
                g_engineActive.store(true, std::memory_order_release);
                LogLine("[winapk] audio: WASAPI shared output started (" + std::to_string(deviceRate) + " Hz, buffer " +
                        std::to_string(bufferFrames) + " frames)");
            } else {
                client.Reset();
                render.Reset();
            }
        }

        if (!render) {
            // No audio output device currently available: degrade to non-blocking swallow until an endpoint appears.
            g_engineActive.store(false, std::memory_order_release);
            for (int i = 0; i < 20 && !g_engineStop.load(std::memory_order_relaxed) &&
                            !g_rebindEndpoint.exchange(false, std::memory_order_acq_rel); ++i) {
                Sleep(25);
            }
            continue;
        }

        std::vector<float> stereoScratch(std::max<uint32_t>(bufferFrames, 1024) * 2);
        int ticks = 0;
        while (!g_engineStop.load(std::memory_order_relaxed)) {
            if (triggerTestRebind && ++ticks == 3) {
                triggerTestRebind = false;
                g_rebindEndpoint.store(true, std::memory_order_release);
            }
            if (g_rebindEndpoint.exchange(false, std::memory_order_acq_rel)) {
                LogLine("[winapk] audio: default WASAPI endpoint changed; rebinding");
                break;
            }
            WaitForSingleObject(wakeEvent, 15);
            UINT32 padding = 0;
            HRESULT phr = client->GetCurrentPadding(&padding);
            if (phr == AUDCLNT_E_DEVICE_INVALIDATED || FAILED(phr)) {
                LogLine("[winapk] audio: WASAPI endpoint invalidated; rebinding");
                break;
            }
            const UINT32 avail = bufferFrames > padding ? (bufferFrames - padding) : 0;
            if (avail == 0) continue;
            BYTE* outBytes = nullptr;
            if (FAILED(render->GetBuffer(avail, &outBytes)) || !outBytes) break;
            if (stereoScratch.size() < static_cast<size_t>(avail) * 2) stereoScratch.resize(static_cast<size_t>(avail) * 2);
            MixOutput(stereoScratch.data(), avail, deviceRate);
            if (deviceChannels == 2) {
                std::memcpy(outBytes, stereoScratch.data(), static_cast<size_t>(avail) * 2 * sizeof(float));
            }
            if (FAILED(render->ReleaseBuffer(avail, 0))) break;
        }
        if (client) client->Stop();
    }

    if (wakeEvent) CloseHandle(wakeEvent);
    if (enumerator && notify) enumerator->UnregisterEndpointNotificationCallback(notify.Get());
    if (SUCCEEDED(coHr)) CoUninitialize();
}

void EnsureEngineLocked() {
    if (g_engineInit) return;
    g_engineInit = true;
    wchar_t modeBuf[64]{};
    const DWORD n = GetEnvironmentVariableW(L"WINAPK_AUDIO_BACKEND", modeBuf, ARRAYSIZE(modeBuf));
    std::wstring mode(modeBuf, n > 0 && n < ARRAYSIZE(modeBuf) ? n : 0);
    if (mode == L"none") {
        g_engineActive.store(false, std::memory_order_release);
        LogLine("[winapk] audio: backend=none (simulated deviceless fallback)");
        return;
    }
    if (mode == L"virtual") {
        g_renderThread = std::thread(VirtualRenderLoop);
        g_renderThread.detach();
        for (int i = 0; i < 50 && !g_engineActive.load(std::memory_order_acquire); ++i) Sleep(2);
        return;
    }
    g_renderThread = std::thread(WasapiRenderLoop);
    g_renderThread.detach();
    for (int i = 0; i < 100 && !g_engineActive.load(std::memory_order_acquire); ++i) Sleep(2);
}

// ---- Clip decoding (WAV + Ogg Vorbis + Media Foundation) -------------------

float* ResampleToCanonical(const float* src, uint64_t srcFrames, int srcCh, int srcRate, uint64_t* outFrames) {
    *outFrames = 0;
    if (!src || srcFrames == 0 || srcCh < 1 || srcRate <= 0) return nullptr;
    const double ratio = static_cast<double>(srcRate) / static_cast<double>(kCanonicalRate);
    uint64_t dstFrames = static_cast<uint64_t>(static_cast<double>(srcFrames) / ratio + 0.5);
    if (dstFrames == 0) dstFrames = 1;
    float* dst = static_cast<float*>(std::calloc(dstFrames * kCanonicalChannels, sizeof(float)));
    if (!dst) return nullptr;
    for (uint64_t f = 0; f < dstFrames; ++f) {
        const double sp = static_cast<double>(f) * ratio;
        const uint64_t i0 = std::min<uint64_t>(static_cast<uint64_t>(sp), srcFrames - 1);
        const double frac = sp - static_cast<double>(i0);
        const uint64_t i1 = (i0 + 1 < srcFrames) ? (i0 + 1) : i0;
        float l = 0.f, r = 0.f;
        if (srcCh == 1) {
            l = r = static_cast<float>(src[i0] + (src[i1] - src[i0]) * frac);
        } else {
            l = static_cast<float>(src[i0 * srcCh + 0] + (src[i1 * srcCh + 0] - src[i0 * srcCh + 0]) * frac);
            r = static_cast<float>(src[i0 * srcCh + 1] + (src[i1 * srcCh + 1] - src[i0 * srcCh + 1]) * frac);
        }
        dst[f * 2 + 0] = l;
        dst[f * 2 + 1] = r;
    }
    *outFrames = dstFrames;
    return dst;
}

bool DecodeWav(const uint8_t* data, size_t len, Clip* out) {
    if (len < 44 || std::memcmp(data, "RIFF", 4) != 0 || std::memcmp(data + 8, "WAVE", 4) != 0) return false;
    uint16_t audioFmt = 0, channels = 0, bitsPerSample = 0, blockAlign = 0;
    uint32_t sampleRate = 0;
    const uint8_t* pcmPtr = nullptr;
    uint32_t pcmBytes = 0;

    size_t off = 12;
    while (off + 8 <= len) {
        uint32_t chunkSize = 0;
        std::memcpy(&chunkSize, data + off + 4, 4);
        const size_t body = off + 8;
        if (chunkSize > len - body) chunkSize = static_cast<uint32_t>(len - body);
        if (std::memcmp(data + off, "fmt ", 4) == 0 && chunkSize >= 16) {
            std::memcpy(&audioFmt, data + body + 0, 2);
            std::memcpy(&channels, data + body + 2, 2);
            std::memcpy(&sampleRate, data + body + 4, 4);
            std::memcpy(&blockAlign, data + body + 12, 2);
            std::memcpy(&bitsPerSample, data + body + 14, 2);
            if (audioFmt == 0xFFFE && chunkSize >= 40) {
                std::memcpy(&audioFmt, data + body + 24, 2);
            }
        } else if (std::memcmp(data + off, "data", 4) == 0) {
            pcmPtr = data + body;
            pcmBytes = chunkSize;
            break;
        }
        off = body + chunkSize + (chunkSize & 1u);
    }
    if (!pcmPtr || pcmBytes == 0 || channels < 1 || channels > 8 || sampleRate < 4000 || sampleRate > 192000) return false;
    const uint32_t bytesPerSample = bitsPerSample / 8;
    if (bytesPerSample == 0 || blockAlign < channels * bytesPerSample) return false;
    const uint64_t frames = pcmBytes / (channels * bytesPerSample);
    if (frames == 0) return false;

    std::vector<float> tmp(frames * channels);
    if (audioFmt == 1 && bitsPerSample == 16) {
        for (uint64_t i = 0; i < frames * channels; ++i) {
            int16_t s = 0;
            std::memcpy(&s, pcmPtr + i * 2, 2);
            tmp[i] = static_cast<float>(s) * (1.0f / 32768.0f);
        }
    } else if (audioFmt == 1 && bitsPerSample == 8) {
        for (uint64_t i = 0; i < frames * channels; ++i) {
            tmp[i] = (static_cast<int>(pcmPtr[i]) - 128) * (1.0f / 128.0f);
        }
    } else if (audioFmt == 3 && bitsPerSample == 32) {
        std::memcpy(tmp.data(), pcmPtr, tmp.size() * sizeof(float));
    } else {
        return false;
    }
    out->pcm = ResampleToCanonical(tmp.data(), frames, channels, static_cast<int>(sampleRate), &out->frames);
    return out->pcm != nullptr;
}

bool DecodeMediaFoundation(const uint8_t* data, size_t len, Clip* out) {
    const HRESULT coHr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const HRESULT mfHr = MFStartup(MF_VERSION, MFSTARTUP_LITE);
    bool ok = false;
    if (SUCCEEDED(mfHr)) {
        ComPtr<IStream> memStream;
        memStream.Attach(SHCreateMemStream(data, static_cast<UINT>(len)));
        ComPtr<IMFByteStream> mfStream;
        ComPtr<IMFSourceReader> reader;
        if (memStream && SUCCEEDED(MFCreateMFByteStreamOnStream(memStream.Get(), &mfStream)) &&
            SUCCEEDED(MFCreateSourceReaderFromByteStream(mfStream.Get(), nullptr, &reader))) {
            ComPtr<IMFMediaType> wantType;
            if (SUCCEEDED(MFCreateMediaType(&wantType)) &&
                SUCCEEDED(wantType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio)) &&
                SUCCEEDED(wantType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_Float)) &&
                SUCCEEDED(reader->SetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM),
                                                      nullptr, wantType.Get()))) {
                ComPtr<IMFMediaType> actual;
                UINT32 ch = 0, sr = 0;
                if (SUCCEEDED(reader->GetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), &actual)) &&
                    SUCCEEDED(actual->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &ch)) &&
                    SUCCEEDED(actual->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &sr)) && ch >= 1 && sr >= 4000) {
                    std::vector<float> samples;
                    for (;;) {
                        DWORD flags = 0;
                        ComPtr<IMFSample> sample;
                        if (FAILED(reader->ReadSample(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM),
                                                      0, nullptr, &flags, nullptr, &sample)))
                            break;
                        if (flags & MF_SOURCE_READERF_ENDOFSTREAM) break;
                        if (!sample) continue;
                        ComPtr<IMFMediaBuffer> mbuf;
                        BYTE* ptr = nullptr;
                        DWORD cb = 0;
                        if (SUCCEEDED(sample->ConvertToContiguousBuffer(&mbuf)) &&
                            SUCCEEDED(mbuf->Lock(&ptr, nullptr, &cb))) {
                            const size_t nfloats = cb / sizeof(float);
                            const auto* fp = reinterpret_cast<const float*>(ptr);
                            samples.insert(samples.end(), fp, fp + nfloats);
                            mbuf->Unlock();
                        }
                        if (samples.size() > 48000u * 2u * 600u) break;  // 10-minute cap
                    }
                    const uint64_t frames = samples.size() / ch;
                    if (frames > 0) {
                        out->pcm = ResampleToCanonical(samples.data(), frames, static_cast<int>(ch),
                                                       static_cast<int>(sr), &out->frames);
                        ok = out->pcm != nullptr;
                    }
                }
            }
        }
        MFShutdown();
    }
    if (SUCCEEDED(coHr)) CoUninitialize();
    return ok;
}

bool DecodeClip(const uint8_t* data, int len, Clip* out) {
    if (!data || len <= 4 || !out) return false;
    *out = {};
    if (DecodeWav(data, static_cast<size_t>(len), out)) return true;
    if (len >= 4 && std::memcmp(data, "OggS", 4) == 0) {
        int ch = 0, sr = 0;
        short* decoded = nullptr;
        const int frames = stb_vorbis_decode_memory(data, len, &ch, &sr, &decoded);
        if (frames > 0 && decoded && ch >= 1 && sr > 0) {
            std::vector<float> tmp(static_cast<size_t>(frames) * static_cast<size_t>(ch));
            for (size_t i = 0; i < tmp.size(); ++i) tmp[i] = decoded[i] * (1.0f / 32768.0f);
            std::free(decoded);
            out->pcm = ResampleToCanonical(tmp.data(), static_cast<uint64_t>(frames), ch, sr, &out->frames);
            return out->pcm != nullptr;
        }
        if (decoded) std::free(decoded);
    }
    return DecodeMediaFoundation(data, static_cast<size_t>(len), out);
}

// ---- Streaming write helpers -----------------------------------------------

int WriteFloats(Stream* s, const float* src, int n, bool blocking) {
    if (!s || n <= 0) return 0;
    if (!g_engineActive.load(std::memory_order_acquire)) {
        // Headless / deviceless host: advance playedFrames without blocking so callers stay responsive.
        if (s->channels > 0) {
            s->playedFrames.fetch_add(static_cast<uint64_t>(n / s->channels), std::memory_order_relaxed);
            s->lastRenderNanos.store(HostNanoTime(), std::memory_order_relaxed);
        }
        return n;
    }
    if (!blocking) {
        return static_cast<int>(s->ring.Push(src, static_cast<uint32_t>(n)));
    }
    int done = 0;
    int stall = 0;
    while (done < n) {
        if (s->released.load(std::memory_order_relaxed)) break;
        const uint32_t pushed = s->ring.Push(src + done, static_cast<uint32_t>(n - done));
        done += static_cast<int>(pushed);
        if (done < n) {
            if (pushed == 0 && ++stall > 2500) break;
            if (pushed > 0) stall = 0;
            Sleep(2);
        }
    }
    return done;
}

Stream* PickVoiceLocked() {
    for (Stream* v : g_voices) {
        if (v->playState.load(std::memory_order_relaxed) == PLAYSTATE_STOPPED) return v;
    }
    if (g_voices.size() < kMaxVoices) {
        auto v = std::make_unique<Stream>();
        v->mode = MODE_CLIP;
        Stream* raw = v.get();
        g_voices.push_back(raw);
        v.release();  // process-lifetime voice pool, matching upstream audio_backend.mm
        return raw;
    }
    const uint32_t idx = g_voiceRr.fetch_add(1, std::memory_order_relaxed) % static_cast<uint32_t>(g_voices.size());
    return g_voices[idx];
}

Stream* VoiceOfStreamLocked(int streamID) {
    const size_t slot = static_cast<size_t>((streamID >> 16) & 0xFFFF);
    const uint32_t gen = static_cast<uint32_t>(streamID & 0xFFFF);
    if (slot == 0 || slot > g_voices.size()) return nullptr;
    Stream* v = g_voices[slot - 1];
    return (v->generation.load(std::memory_order_relaxed) & 0xFFFF) == gen ? v : nullptr;
}

void CheckSpVolume(float& l, float& r) {
    if (!(l >= 0.f && l <= 1.f) || !(r >= 0.f && r <= 1.f)) {
        l = r = 1.0f;
    }
}

float CheckSpRate(float rate) {
    if (!(rate == rate)) return 0.0f;
    return std::clamp(rate, 0.125f, 8.0f);
}

// ---- JNI entry points: com.macapk.loader.NativeAudio -----------------------

jlong JNICALL AudioCreate(JNIEnv*, jclass, jint sampleRate, jint channels, jint encoding, jint bufferFrames) {
    if (sampleRate < 4000 || sampleRate > 192000 || channels < 1 || channels > 8) return 0;
    if (encoding != ENC_DEFAULT && encoding != ENC_PCM_16BIT && encoding != ENC_PCM_8BIT && encoding != ENC_PCM_FLOAT)
        return 0;
    auto s = std::make_unique<Stream>();
    s->mode = MODE_RING;
    s->sampleRate = sampleRate;
    s->channels = channels;
    s->encoding = (encoding == ENC_DEFAULT) ? ENC_PCM_16BIT : encoding;
    s->playbackSr.store(sampleRate, std::memory_order_relaxed);
    const uint32_t minFrames = static_cast<uint32_t>(std::max(1024, sampleRate / 25));  // >= 40 ms
    s->frameCount = std::max<uint32_t>(bufferFrames > 0 ? static_cast<uint32_t>(bufferFrames) : minFrames, minFrames);
    s->ring.Init(static_cast<uint64_t>(s->frameCount) * static_cast<uint64_t>(channels));

    std::lock_guard<std::mutex> lk(g_audioMu);
    EnsureEngineLocked();
    g_streams.push_back(std::move(s));
    return static_cast<jlong>(g_streams.size());
}

jint JNICALL AudioWriteBytes(JNIEnv* env, jclass, jlong h, jbyteArray data, jint offset, jint size, jboolean blocking) {
    Stream* s = StreamOf(h);
    if (!s || !data || size <= 0 || offset < 0) return 0;
    const jsize arrLen = env->GetArrayLength(data);
    if (offset > arrLen || size > arrLen - offset) return 0;
    std::vector<uint8_t> raw(static_cast<size_t>(size));
    env->GetByteArrayRegion(data, offset, size, reinterpret_cast<jbyte*>(raw.data()));

    constexpr int kChunk = 4096;
    float tmp[kChunk];
    const bool blk = (blocking == JNI_TRUE);
    if (s->encoding == ENC_PCM_FLOAT) {
        const int nfloats = size / 4;
        int consumed = 0;
        while (consumed < nfloats) {
            const int c = std::min(kChunk, nfloats - consumed);
            std::memcpy(tmp, raw.data() + static_cast<size_t>(consumed) * 4, static_cast<size_t>(c) * 4);
            const int w = WriteFloats(s, tmp, c, blk);
            consumed += w;
            if (w < c) break;
        }
        return consumed * 4;
    }
    if (s->encoding == ENC_PCM_8BIT) {
        int consumed = 0;
        while (consumed < size) {
            const int c = std::min(kChunk, size - consumed);
            for (int i = 0; i < c; ++i) tmp[i] = (static_cast<int>(raw[consumed + i]) - 128) * (1.0f / 128.0f);
            const int w = WriteFloats(s, tmp, c, blk);
            consumed += w;
            if (w < c) break;
        }
        return consumed;
    }
    const int nsamp = size / 2;
    int consumed = 0;
    while (consumed < nsamp) {
        const int c = std::min(kChunk, nsamp - consumed);
        for (int i = 0; i < c; ++i) {
            int16_t v = 0;
            std::memcpy(&v, raw.data() + static_cast<size_t>(consumed + i) * 2, 2);
            tmp[i] = static_cast<float>(v) * (1.0f / 32768.0f);
        }
        const int w = WriteFloats(s, tmp, c, blk);
        consumed += w;
        if (w < c) break;
    }
    return consumed * 2;
}

jint JNICALL AudioWriteShorts(JNIEnv* env, jclass, jlong h, jshortArray data, jint offset, jint size, jboolean blocking) {
    Stream* s = StreamOf(h);
    if (!s || !data || size <= 0 || offset < 0) return 0;
    const jsize arrLen = env->GetArrayLength(data);
    if (offset > arrLen || size > arrLen - offset) return 0;
    std::vector<jshort> raw(static_cast<size_t>(size));
    env->GetShortArrayRegion(data, offset, size, raw.data());

    constexpr int kChunk = 4096;
    float tmp[kChunk];
    int consumed = 0;
    while (consumed < size) {
        const int c = std::min(kChunk, size - consumed);
        for (int i = 0; i < c; ++i) tmp[i] = static_cast<float>(raw[consumed + i]) * (1.0f / 32768.0f);
        const int w = WriteFloats(s, tmp, c, blocking == JNI_TRUE);
        consumed += w;
        if (w < c) break;
    }
    return consumed;
}

jint JNICALL AudioWriteFloats(JNIEnv* env, jclass, jlong h, jfloatArray data, jint offset, jint size, jboolean blocking) {
    Stream* s = StreamOf(h);
    if (!s || !data || size <= 0 || offset < 0) return 0;
    const jsize arrLen = env->GetArrayLength(data);
    if (offset > arrLen || size > arrLen - offset) return 0;
    std::vector<float> raw(static_cast<size_t>(size));
    env->GetFloatArrayRegion(data, offset, size, raw.data());
    return WriteFloats(s, raw.data(), size, blocking == JNI_TRUE);
}

void JNICALL AudioPlay(JNIEnv*, jclass, jlong h) {
    if (Stream* s = StreamOf(h)) s->playState.store(PLAYSTATE_PLAYING, std::memory_order_release);
}

void JNICALL AudioPause(JNIEnv*, jclass, jlong h) {
    if (Stream* s = StreamOf(h)) s->playState.store(PLAYSTATE_PAUSED, std::memory_order_release);
}

void JNICALL AudioStop(JNIEnv*, jclass, jlong h) {
    if (Stream* s = StreamOf(h)) {
        s->playState.store(PLAYSTATE_STOPPED, std::memory_order_release);
        s->playedFrames.store(0, std::memory_order_relaxed);
        s->ringFrac = 0.0;
    }
}

void JNICALL AudioFlush(JNIEnv*, jclass, jlong h) {
    if (Stream* s = StreamOf(h)) {
        const uint64_t w = s->ring.wpos.load(std::memory_order_acquire);
        s->ring.rpos.store(w, std::memory_order_release);
        s->playedFrames.store(0, std::memory_order_relaxed);
        s->ringFrac = 0.0;
    }
}

void JNICALL AudioRelease(JNIEnv*, jclass, jlong h) {
    if (Stream* s = StreamOf(h)) {
        s->released.store(true, std::memory_order_release);
        s->playState.store(PLAYSTATE_STOPPED, std::memory_order_release);
    }
}

void JNICALL AudioSetVolume(JNIEnv*, jclass, jlong h, jfloat l, jfloat r) {
    if (Stream* s = StreamOf(h)) {
        s->gainL.store(l, std::memory_order_relaxed);
        s->gainR.store(r, std::memory_order_relaxed);
    }
}

jint JNICALL AudioGetPlayState(JNIEnv*, jclass, jlong h) {
    Stream* s = StreamOf(h);
    return s ? s->playState.load(std::memory_order_relaxed) : PLAYSTATE_STOPPED;
}

jlong JNICALL AudioGetHeadPos(JNIEnv*, jclass, jlong h) {
    Stream* s = StreamOf(h);
    return s ? static_cast<jlong>(s->playedFrames.load(std::memory_order_relaxed)) : 0;
}

jboolean JNICALL AudioGetTimestamp(JNIEnv* env, jclass, jlong h, jlongArray out) {
    if (!out || env->GetArrayLength(out) < 2) return JNI_FALSE;
    Stream* s = StreamOf(h);
    if (!s) return JNI_FALSE;
    const uint64_t frames = s->playedFrames.load(std::memory_order_relaxed);
    if (frames == 0) return JNI_FALSE;
    const jlong vals[2] = {static_cast<jlong>(frames), s->lastRenderNanos.load(std::memory_order_relaxed)};
    env->SetLongArrayRegion(out, 0, 2, vals);
    return JNI_TRUE;
}

jint JNICALL AudioSetPlaybackRate(JNIEnv*, jclass, jlong h, jint rate) {
    Stream* s = StreamOf(h);
    if (!s || s->mode != MODE_RING) return RATE_INVALID_OP;
    if (rate == s->playbackSr.load(std::memory_order_relaxed)) return RATE_OK;
    if (rate <= 0) return RATE_BAD_VALUE;
    const double pitch = static_cast<double>(s->pbPitch.load(std::memory_order_relaxed));
    if (static_cast<double>(rate) * pitch > static_cast<double>(g_mixSampleRate.load(std::memory_order_relaxed)) * 256.0)
        return RATE_BAD_VALUE;
    s->playbackSr.store(rate, std::memory_order_relaxed);
    return RATE_OK;
}

jint JNICALL AudioGetPlaybackRate(JNIEnv*, jclass, jlong h) {
    Stream* s = StreamOf(h);
    if (!s) return 0;
    const int r = s->playbackSr.load(std::memory_order_relaxed);
    return r > 0 ? r : s->sampleRate;
}

jint JNICALL AudioSetPlaybackParams(JNIEnv*, jclass, jlong h, jfloat speed, jfloat pitch) {
    Stream* s = StreamOf(h);
    if (!s || s->mode != MODE_RING) return RATE_INVALID_OP;
    if (!(speed == speed) || !(pitch == pitch)) return RATE_BAD_VALUE;
    if (pitch < kPitchMin || pitch > kPitchMax || speed < kSpeedMin || speed > kSpeedMax) return RATE_BAD_VALUE;
    const float effectiveSpeed = speed / std::max(pitch, kPitchMinDelta);
    // Independent pitch-preserving time stretching is not implemented on Windows; refuse rather than chipmunking.
    if (std::fabs(effectiveSpeed - 1.0f) >= 0.0001f) return RATE_BAD_VALUE;
    s->pbPitch.store(pitch, std::memory_order_relaxed);
    s->pbSpeed.store(speed, std::memory_order_relaxed);
    return RATE_OK;
}

void JNICALL AudioGetPlaybackParams(JNIEnv* env, jclass, jlong h, jfloatArray out) {
    if (!out || env->GetArrayLength(out) < 2) return;
    Stream* s = StreamOf(h);
    const jfloat vals[2] = {
        s ? s->pbSpeed.load(std::memory_order_relaxed) : 1.0f,
        s ? s->pbPitch.load(std::memory_order_relaxed) : 1.0f,
    };
    env->SetFloatArrayRegion(out, 0, 2, vals);
}

jint JNICALL AudioGetBufferCapacity(JNIEnv*, jclass, jlong h) {
    Stream* s = StreamOf(h);
    return s ? static_cast<jint>(s->frameCount) : 0;
}

jint JNICALL AudioSetBufferSize(JNIEnv*, jclass, jlong h, jint frames) {
    Stream* s = StreamOf(h);
    if (!s || s->channels <= 0) return 0;
    constexpr uint32_t kMin = 16;
    uint32_t want = frames < 0 ? 0u : static_cast<uint32_t>(frames);
    if (s->frameCount < kMin) want = s->frameCount;
    else want = std::clamp(want, kMin, s->frameCount);
    s->ring.limit.store(static_cast<uint64_t>(want) * static_cast<uint64_t>(s->channels), std::memory_order_relaxed);
    return static_cast<jint>(want);
}

jint JNICALL AudioGetUnderruns(JNIEnv*, jclass, jlong h) {
    Stream* s = StreamOf(h);
    return s ? static_cast<jint>(s->underruns.load(std::memory_order_relaxed)) : 0;
}

// ---- SoundPool JNI ---------------------------------------------------------

jint JNICALL SpLoad(JNIEnv* env, jclass, jbyteArray data, jint len) {
    if (!data || len <= 0 || len > env->GetArrayLength(data)) return 0;
    std::vector<uint8_t> raw(static_cast<size_t>(len));
    env->GetByteArrayRegion(data, 0, len, reinterpret_cast<jbyte*>(raw.data()));
    Clip c{};
    if (!DecodeClip(raw.data(), len, &c)) return 0;
    std::lock_guard<std::mutex> lk(g_audioMu);
    EnsureEngineLocked();
    g_clips.push_back(c);
    return static_cast<jint>(g_clips.size());
}

jint JNICALL SpPlay(JNIEnv*, jclass, jint soundID, jfloat left, jfloat right, jint loop, jfloat rate) {
    CheckSpVolume(left, right);
    const float srate = CheckSpRate(rate);
    std::lock_guard<std::mutex> lk(g_audioMu);
    const size_t idx = static_cast<size_t>(soundID - 1);
    if (soundID <= 0 || idx >= g_clips.size() || !g_clips[idx].pcm) return 0;
    Stream* v = PickVoiceLocked();
    if (!v) return 0;
    v->playState.store(PLAYSTATE_STOPPED, std::memory_order_release);
    v->clipPcm = g_clips[idx].pcm;
    v->clipFrames = g_clips[idx].frames;
    v->clipCursor.store(0, std::memory_order_relaxed);
    v->clipFrac = 0.0;
    v->clipLoop.store(loop, std::memory_order_relaxed);
    v->clipRate.store(srate > 0.0f ? srate : 1.0f, std::memory_order_relaxed);
    v->gainL.store(left, std::memory_order_relaxed);
    v->gainR.store(right, std::memory_order_relaxed);
    const uint32_t gen = v->generation.fetch_add(1, std::memory_order_relaxed) + 1;
    size_t slot = 0;
    for (; slot < g_voices.size(); ++slot) if (g_voices[slot] == v) break;
    v->playState.store(PLAYSTATE_PLAYING, std::memory_order_release);
    return static_cast<jint>(((slot + 1) << 16) | (gen & 0xFFFF));
}

void JNICALL SpStop(JNIEnv*, jclass, jint streamID) {
    std::lock_guard<std::mutex> lk(g_audioMu);
    if (Stream* v = VoiceOfStreamLocked(streamID)) v->playState.store(PLAYSTATE_STOPPED, std::memory_order_release);
}

void JNICALL SpPause(JNIEnv*, jclass, jint streamID) {
    std::lock_guard<std::mutex> lk(g_audioMu);
    Stream* v = VoiceOfStreamLocked(streamID);
    if (v && v->playState.load(std::memory_order_relaxed) == PLAYSTATE_PLAYING)
        v->playState.store(PLAYSTATE_PAUSED, std::memory_order_release);
}

void JNICALL SpResume(JNIEnv*, jclass, jint streamID) {
    std::lock_guard<std::mutex> lk(g_audioMu);
    Stream* v = VoiceOfStreamLocked(streamID);
    if (v && v->playState.load(std::memory_order_relaxed) == PLAYSTATE_PAUSED)
        v->playState.store(PLAYSTATE_PLAYING, std::memory_order_release);
}

void JNICALL SpSetVolume(JNIEnv*, jclass, jint streamID, jfloat l, jfloat r) {
    CheckSpVolume(l, r);
    std::lock_guard<std::mutex> lk(g_audioMu);
    if (Stream* v = VoiceOfStreamLocked(streamID)) {
        v->gainL.store(l, std::memory_order_relaxed);
        v->gainR.store(r, std::memory_order_relaxed);
    }
}

void JNICALL SpSetLoop(JNIEnv*, jclass, jint streamID, jint loop) {
    std::lock_guard<std::mutex> lk(g_audioMu);
    if (Stream* v = VoiceOfStreamLocked(streamID)) v->clipLoop.store(loop, std::memory_order_relaxed);
}

void JNICALL SpSetRate(JNIEnv*, jclass, jint streamID, jfloat rate) {
    const float srate = CheckSpRate(rate);
    if (srate <= 0.0f) return;
    std::lock_guard<std::mutex> lk(g_audioMu);
    if (Stream* v = VoiceOfStreamLocked(streamID)) v->clipRate.store(srate, std::memory_order_relaxed);
}

jboolean JNICALL SpUnload(JNIEnv*, jclass, jint soundID) {
    std::lock_guard<std::mutex> lk(g_audioMu);
    const size_t idx = static_cast<size_t>(soundID - 1);
    if (soundID <= 0 || idx >= g_clips.size() || !g_clips[idx].pcm) return JNI_FALSE;
    for (Stream* v : g_voices) {
        if (v->clipPcm == g_clips[idx].pcm) {
            v->playState.store(PLAYSTATE_STOPPED, std::memory_order_release);
            v->clipPcm = nullptr;
            v->clipFrames = 0;
        }
    }
    std::free(g_clips[idx].pcm);
    g_clips[idx] = {};
    return JNI_TRUE;
}

void JNICALL SpAutoPause(JNIEnv*, jclass) {
    std::lock_guard<std::mutex> lk(g_audioMu);
    for (Stream* v : g_voices) {
        if (v->playState.load(std::memory_order_relaxed) == PLAYSTATE_PLAYING)
            v->playState.store(PLAYSTATE_PAUSED, std::memory_order_release);
    }
}

void JNICALL SpAutoResume(JNIEnv*, jclass) {
    std::lock_guard<std::mutex> lk(g_audioMu);
    for (Stream* v : g_voices) {
        if (v->playState.load(std::memory_order_relaxed) == PLAYSTATE_PAUSED)
            v->playState.store(PLAYSTATE_PLAYING, std::memory_order_release);
    }
}

// ---- MediaPlayer JNI -------------------------------------------------------

jlong JNICALL MpCreate(JNIEnv*, jclass) {
    auto s = std::make_unique<Stream>();
    s->mode = MODE_CLIP;
    s->ownsClip = true;
    std::lock_guard<std::mutex> lk(g_audioMu);
    EnsureEngineLocked();
    g_streams.push_back(std::move(s));
    return static_cast<jlong>(g_streams.size());
}

jboolean JNICALL MpSetSource(JNIEnv* env, jclass, jlong h, jbyteArray data, jint len) {
    Stream* s = StreamOf(h);
    if (!s || !data || len <= 0 || len > env->GetArrayLength(data)) return JNI_FALSE;
    std::vector<uint8_t> raw(static_cast<size_t>(len));
    env->GetByteArrayRegion(data, 0, len, reinterpret_cast<jbyte*>(raw.data()));
    Clip c{};
    if (!DecodeClip(raw.data(), len, &c)) return JNI_FALSE;
    std::lock_guard<std::mutex> lk(g_audioMu);
    s->playState.store(PLAYSTATE_STOPPED, std::memory_order_release);
    const float* old = s->clipPcm;
    s->clipPcm = c.pcm;
    s->clipFrames = c.frames;
    s->clipCursor.store(0, std::memory_order_relaxed);
    s->clipFrac = 0.0;
    if (old && s->ownsClip) std::free(const_cast<float*>(old));
    return JNI_TRUE;
}

void JNICALL MpStart(JNIEnv*, jclass, jlong h) {
    if (Stream* s = StreamOf(h)) s->playState.store(PLAYSTATE_PLAYING, std::memory_order_release);
}

void JNICALL MpPause(JNIEnv*, jclass, jlong h) {
    if (Stream* s = StreamOf(h)) s->playState.store(PLAYSTATE_PAUSED, std::memory_order_release);
}

void JNICALL MpStop(JNIEnv*, jclass, jlong h) {
    if (Stream* s = StreamOf(h)) {
        s->playState.store(PLAYSTATE_STOPPED, std::memory_order_release);
        s->clipCursor.store(0, std::memory_order_relaxed);
    }
}

void JNICALL MpSeek(JNIEnv*, jclass, jlong h, jint ms) {
    if (Stream* s = StreamOf(h)) {
        const uint64_t frame = ms <= 0 ? 0 : static_cast<uint64_t>(static_cast<double>(ms) * 0.001 * kCanonicalRate);
        s->clipCursor.store(std::min(frame, s->clipFrames), std::memory_order_relaxed);
    }
}

void JNICALL MpSetLooping(JNIEnv*, jclass, jlong h, jboolean loop) {
    if (Stream* s = StreamOf(h)) s->clipLoop.store(loop ? -1 : 0, std::memory_order_relaxed);
}

void JNICALL MpSetVolume(JNIEnv*, jclass, jlong h, jfloat l, jfloat r) {
    if (Stream* s = StreamOf(h)) {
        s->gainL.store(l, std::memory_order_relaxed);
        s->gainR.store(r, std::memory_order_relaxed);
    }
}

jint JNICALL MpDuration(JNIEnv*, jclass, jlong h) {
    Stream* s = StreamOf(h);
    if (!s || s->clipFrames == 0) return 0;
    return static_cast<jint>(static_cast<double>(s->clipFrames) / kCanonicalRate * 1000.0);
}

jint JNICALL MpPosition(JNIEnv*, jclass, jlong h) {
    Stream* s = StreamOf(h);
    if (!s) return 0;
    return static_cast<jint>(static_cast<double>(s->clipCursor.load(std::memory_order_relaxed)) / kCanonicalRate * 1000.0);
}

jboolean JNICALL MpIsPlaying(JNIEnv*, jclass, jlong h) {
    Stream* s = StreamOf(h);
    return (s && s->playState.load(std::memory_order_relaxed) == PLAYSTATE_PLAYING) ? JNI_TRUE : JNI_FALSE;
}

void JNICALL MpRelease(JNIEnv*, jclass, jlong h) {
    Stream* s = StreamOf(h);
    if (!s) return;
    std::lock_guard<std::mutex> lk(g_audioMu);
    s->released.store(true, std::memory_order_release);
    s->playState.store(PLAYSTATE_STOPPED, std::memory_order_release);
}

#define M(name, sig, fn) {const_cast<char*>(name), const_cast<char*>(sig), reinterpret_cast<void*>(fn)}
const JNINativeMethod kAudioMethods[] = {
    M("nativeCreate", "(IIII)J", AudioCreate),
    M("nativeWriteBytes", "(J[BIIZ)I", AudioWriteBytes),
    M("nativeWriteShorts", "(J[SIIZ)I", AudioWriteShorts),
    M("nativeWriteFloats", "(J[FIIZ)I", AudioWriteFloats),
    M("nativePlay", "(J)V", AudioPlay),
    M("nativePause", "(J)V", AudioPause),
    M("nativeStop", "(J)V", AudioStop),
    M("nativeFlush", "(J)V", AudioFlush),
    M("nativeRelease", "(J)V", AudioRelease),
    M("nativeSetVolume", "(JFF)V", AudioSetVolume),
    M("nativeGetPlayState", "(J)I", AudioGetPlayState),
    M("nativeGetPlaybackHeadPosition", "(J)J", AudioGetHeadPos),
    M("nativeGetTimestamp", "(J[J)Z", AudioGetTimestamp),
    M("nativeSetPlaybackRate", "(JI)I", AudioSetPlaybackRate),
    M("nativeGetPlaybackRate", "(J)I", AudioGetPlaybackRate),
    M("nativeSetPlaybackParams", "(JFF)I", AudioSetPlaybackParams),
    M("nativeGetPlaybackParams", "(J[F)V", AudioGetPlaybackParams),
    M("nativeSpLoad", "([BI)I", SpLoad),
    M("nativeSpPlay", "(IFFIF)I", SpPlay),
    M("nativeSpStop", "(I)V", SpStop),
    M("nativeSpPause", "(I)V", SpPause),
    M("nativeSpResume", "(I)V", SpResume),
    M("nativeSpSetVolume", "(IFF)V", SpSetVolume),
    M("nativeSpSetLoop", "(II)V", SpSetLoop),
    M("nativeSpSetRate", "(IF)V", SpSetRate),
    M("nativeSpUnload", "(I)Z", SpUnload),
    M("nativeSpAutoPause", "()V", SpAutoPause),
    M("nativeSpAutoResume", "()V", SpAutoResume),
    M("nativeMpCreate", "()J", MpCreate),
    M("nativeMpSetSource", "(J[BI)Z", MpSetSource),
    M("nativeMpStart", "(J)V", MpStart),
    M("nativeMpPause", "(J)V", MpPause),
    M("nativeMpStop", "(J)V", MpStop),
    M("nativeMpSeek", "(JI)V", MpSeek),
    M("nativeMpSetLooping", "(JZ)V", MpSetLooping),
    M("nativeMpSetVolume", "(JFF)V", MpSetVolume),
    M("nativeMpDuration", "(J)I", MpDuration),
    M("nativeMpPosition", "(J)I", MpPosition),
    M("nativeMpIsPlaying", "(J)Z", MpIsPlaying),
    M("nativeMpRelease", "(J)V", MpRelease),
    M("nativeGetBufferCapacity", "(J)I", AudioGetBufferCapacity),
    M("nativeSetBufferSize", "(JI)I", AudioSetBufferSize),
    M("nativeGetUnderrunCount", "(J)I", AudioGetUnderruns),
};
#undef M

}  // namespace

bool RegisterAudio(JNIEnv* env) {
    return Bind(env, "com/macapk/loader/NativeAudio", kAudioMethods, static_cast<int>(std::size(kAudioMethods)));
}

}  // namespace winapk::bridge
