// compositor.cpp -- Direct3D 11 / DXGI multi-layer hardware compositor and 3-slot staging ring.
#include "compositor.h"
#include "bridge.h"
#include "embed.h"
#include "guest_graphics.h"

#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

namespace winapk::bridge {
namespace {

using Microsoft::WRL::ComPtr;

constexpr const char kShaderSource[] = R"(
cbuffer QuadCB : register(b0) {
    float4 uRect;  // x0, y0, x1, y1 in NDC (-1..1, y up)
    int    uAlpha; // 0 = opaque, 1 = straight alpha, 2 = premultiplied
    int    uFlipY; // 0 = top-down, 1 = bottom-up (OpenGL)
    int2   uPad;
};

struct VSOut {
    float4 pos : SV_Position;
    float2 uv  : TEXCOORD0;
};

VSOut VSMain(uint id : SV_VertexID) {
    float2 grid = float2((id << 1) & 2, id & 2); // (0,0), (2,0), (0,2), (2,2)
    float2 t = saturate(grid * 0.5);
    VSOut o;
    o.pos = float4(lerp(uRect.x, uRect.z, t.x), lerp(uRect.y, uRect.w, t.y), 0.0, 1.0);
    o.uv  = float2(t.x, uFlipY != 0 ? (1.0 - t.y) : t.y);
    return o;
}

Texture2D    uTex  : register(t0);
SamplerState uSamp : register(s0);

float4 PSMain(VSOut i) : SV_Target {
    float4 c = uTex.Sample(uSamp, i.uv);
    if (uAlpha == 0) return float4(c.rgb, 1.0);
    if (uAlpha == 1) return float4(c.rgb * c.a, c.a);
    return c;
}
)";

struct alignas(16) QuadConstants {
    float rect[4];
    int alphaMode;
    int flipY;
    int pad[2];
};

struct D3dCore {
    std::mutex mu;
    bool attempted = false;
    bool ok = false;
    ComPtr<IDXGIFactory2> factory;
    ComPtr<ID3D11Device> dev;
    ComPtr<ID3D11DeviceContext> ctx;
    ComPtr<ID3D11VertexShader> vs;
    ComPtr<ID3D11PixelShader> ps;
    ComPtr<ID3D11Buffer> cb;
    ComPtr<ID3D11SamplerState> sampler;
    ComPtr<ID3D11BlendState> blend;
    ComPtr<ID3D11RasterizerState> rast;
};

D3dCore g_d3d;

HMODULE LoadD3dCompiler() {
    constexpr DWORD flags = LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32;
    const std::wstring angleDir = PrivateAngleDirectory();
    if (!angleDir.empty()) {
        if (HMODULE m = LoadLibraryExW((std::filesystem::path(angleDir) / L"d3dcompiler_47.dll").c_str(), nullptr, flags)) {
            return m;
        }
    }
    return LoadLibraryExW(L"d3dcompiler_47.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
}

bool EnsureDeviceLocked() {
    if (g_d3d.attempted) return g_d3d.ok;
    g_d3d.attempted = true;

    ComPtr<IDXGIFactory2> factory;
    if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory2), reinterpret_cast<void**>(factory.GetAddressOf())))) {
        return false;
    }

    wchar_t allowWarp[8]{};
    const bool warpAllowed = GetEnvironmentVariableW(L"WINAPK_ALLOW_WARP_COMPOSITOR", allowWarp, 8) > 0 && allowWarp[0] == L'1';

    ComPtr<IDXGIAdapter1> chosen;
    for (UINT idx = 0;; ++idx) {
        ComPtr<IDXGIAdapter1> ad;
        if (factory->EnumAdapters1(idx, ad.GetAddressOf()) == DXGI_ERROR_NOT_FOUND) break;
        DXGI_ADAPTER_DESC1 desc{};
        if (FAILED(ad->GetDesc1(&desc))) continue;
        const bool isSoftware = (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0 ||
                                (desc.VendorId == 0x1414 && desc.DeviceId == 0x008c);
        if (isSoftware && !warpAllowed) continue;
        chosen = ad;
        break;
    }
    if (!chosen) return false;

    const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
    D3D_FEATURE_LEVEL actual{};
    ComPtr<ID3D11Device> dev;
    ComPtr<ID3D11DeviceContext> ctx;
    if (FAILED(D3D11CreateDevice(chosen.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr,
                                 D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels, 2,
                                 D3D11_SDK_VERSION, dev.GetAddressOf(), &actual, ctx.GetAddressOf()))) {
        return false;
    }

    static HMODULE compilerMod = LoadD3dCompiler();
    auto compileFn = compilerMod ? reinterpret_cast<pD3DCompile>(GetProcAddress(compilerMod, "D3DCompile")) : nullptr;
    if (!compileFn) return false;

    ComPtr<ID3DBlob> vsBlob, psBlob, errBlob;
    if (FAILED(compileFn(kShaderSource, sizeof(kShaderSource) - 1, "compositor.hlsl", nullptr, nullptr,
                         "VSMain", "vs_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0,
                         vsBlob.GetAddressOf(), errBlob.GetAddressOf()))) {
        return false;
    }
    if (FAILED(compileFn(kShaderSource, sizeof(kShaderSource) - 1, "compositor.hlsl", nullptr, nullptr,
                         "PSMain", "ps_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0,
                         psBlob.GetAddressOf(), errBlob.GetAddressOf()))) {
        return false;
    }

    ComPtr<ID3D11VertexShader> vs;
    ComPtr<ID3D11PixelShader> ps;
    if (FAILED(dev->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, vs.GetAddressOf())) ||
        FAILED(dev->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, ps.GetAddressOf()))) {
        return false;
    }

    D3D11_BUFFER_DESC cbd{};
    cbd.ByteWidth = sizeof(QuadConstants);
    cbd.Usage = D3D11_USAGE_DYNAMIC;
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    ComPtr<ID3D11Buffer> cb;
    if (FAILED(dev->CreateBuffer(&cbd, nullptr, cb.GetAddressOf()))) return false;

    D3D11_SAMPLER_DESC sd{};
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.MaxLOD = D3D11_FLOAT32_MAX;
    ComPtr<ID3D11SamplerState> samp;
    if (FAILED(dev->CreateSamplerState(&sd, samp.GetAddressOf()))) return false;

    D3D11_BLEND_DESC bd{};
    bd.RenderTarget[0].BlendEnable = TRUE;
    bd.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
    bd.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    ComPtr<ID3D11BlendState> blend;
    if (FAILED(dev->CreateBlendState(&bd, blend.GetAddressOf()))) return false;

    D3D11_RASTERIZER_DESC rd{};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_NONE;
    rd.ScissorEnable = FALSE;
    ComPtr<ID3D11RasterizerState> rast;
    if (FAILED(dev->CreateRasterizerState(&rd, rast.GetAddressOf()))) return false;

    g_d3d.factory = std::move(factory);
    g_d3d.dev = std::move(dev);
    g_d3d.ctx = std::move(ctx);
    g_d3d.vs = std::move(vs);
    g_d3d.ps = std::move(ps);
    g_d3d.cb = std::move(cb);
    g_d3d.sampler = std::move(samp);
    g_d3d.blend = std::move(blend);
    g_d3d.rast = std::move(rast);
    g_d3d.ok = true;
    return true;
}

struct LayerSlot {
    int width = 0, height = 0;
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    LayerAlpha alpha = LayerAlpha::Opaque;
    bool bottomUp = false;
    bool hasContent = false;
    SurfaceLayerPlacement placement{};
    ComPtr<ID3D11Texture2D> tex;
    ComPtr<ID3D11ShaderResourceView> srv;
};

struct WindowCompositor {
    HWND hwnd = nullptr;
    LayerSlot ui{};
    std::map<int, LayerSlot> surfaces;

    // Offscreen RGBA8 target + 3-slot staging ring for `--embed`
    int rtW = 0, rtH = 0;
    ComPtr<ID3D11Texture2D> rtTex;
    ComPtr<ID3D11RenderTargetView> rtView;
    ComPtr<ID3D11Texture2D> staging[3];
    bool stagingValid[3]{false, false, false};
    uint64_t frameIndex = 0;

    // Standalone DXGI flip swapchain (allocated only when surface layers are active)
    int swapW = 0, swapH = 0;
    ComPtr<IDXGISwapChain1> swapChain;
    ComPtr<ID3D11RenderTargetView> swapRtv;
};

std::map<HWND, std::unique_ptr<WindowCompositor>> g_windows;

bool EnsureLayerTexture(LayerSlot& slot, int w, int h, DXGI_FORMAT fmt) {
    if (slot.tex && slot.width == w && slot.height == h && slot.format == fmt) return true;
    slot.tex.Reset();
    slot.srv.Reset();
    D3D11_TEXTURE2D_DESC td{};
    td.Width = static_cast<UINT>(w);
    td.Height = static_cast<UINT>(h);
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = fmt;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    if (FAILED(g_d3d.dev->CreateTexture2D(&td, nullptr, slot.tex.GetAddressOf()))) return false;
    if (FAILED(g_d3d.dev->CreateShaderResourceView(slot.tex.Get(), nullptr, slot.srv.GetAddressOf()))) {
        slot.tex.Reset();
        return false;
    }
    slot.width = w;
    slot.height = h;
    slot.format = fmt;
    return true;
}

bool EnsureEmbedTargets(WindowCompositor& wc, int w, int h) {
    if (wc.rtTex && wc.rtW == w && wc.rtH == h) return true;
    wc.rtTex.Reset();
    wc.rtView.Reset();
    for (int i = 0; i < 3; ++i) {
        wc.staging[i].Reset();
        wc.stagingValid[i] = false;
    }
    wc.frameIndex = 0;

    D3D11_TEXTURE2D_DESC rtd{};
    rtd.Width = static_cast<UINT>(w);
    rtd.Height = static_cast<UINT>(h);
    rtd.MipLevels = 1;
    rtd.ArraySize = 1;
    rtd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    rtd.SampleDesc.Count = 1;
    rtd.Usage = D3D11_USAGE_DEFAULT;
    rtd.BindFlags = D3D11_BIND_RENDER_TARGET;
    if (FAILED(g_d3d.dev->CreateTexture2D(&rtd, nullptr, wc.rtTex.GetAddressOf()))) return false;
    if (FAILED(g_d3d.dev->CreateRenderTargetView(wc.rtTex.Get(), nullptr, wc.rtView.GetAddressOf()))) return false;

    D3D11_TEXTURE2D_DESC stdDesc = rtd;
    stdDesc.Usage = D3D11_USAGE_STAGING;
    stdDesc.BindFlags = 0;
    stdDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    for (int i = 0; i < 3; ++i) {
        if (FAILED(g_d3d.dev->CreateTexture2D(&stdDesc, nullptr, wc.staging[i].GetAddressOf()))) return false;
    }
    wc.rtW = w;
    wc.rtH = h;
    return true;
}

bool EnsureSwapChain(WindowCompositor& wc, int w, int h) {
    if (wc.swapChain && wc.swapW == w && wc.swapH == h && wc.swapRtv) return true;
    wc.swapRtv.Reset();
    if (!wc.swapChain) {
        DXGI_SWAP_CHAIN_DESC1 scd{};
        scd.Width = static_cast<UINT>(w);
        scd.Height = static_cast<UINT>(h);
        scd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        scd.SampleDesc.Count = 1;
        scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        scd.BufferCount = 2;
        scd.Scaling = DXGI_SCALING_STRETCH;
        scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        scd.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
        if (FAILED(g_d3d.factory->CreateSwapChainForHwnd(g_d3d.dev.Get(), wc.hwnd, &scd, nullptr, nullptr,
                                                         wc.swapChain.GetAddressOf()))) {
            return false;
        }
        g_d3d.factory->MakeWindowAssociation(wc.hwnd, DXGI_MWA_NO_ALT_ENTER);
    } else if (FAILED(wc.swapChain->ResizeBuffers(2, static_cast<UINT>(w), static_cast<UINT>(h),
                                                  DXGI_FORMAT_R8G8B8A8_UNORM, 0))) {
        return false;
    }
    ComPtr<ID3D11Texture2D> back;
    if (FAILED(wc.swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(back.GetAddressOf()))) ||
        FAILED(g_d3d.dev->CreateRenderTargetView(back.Get(), nullptr, wc.swapRtv.GetAddressOf()))) {
        return false;
    }
    wc.swapW = w;
    wc.swapH = h;
    return true;
}

void DrawLayerQuad(const LayerSlot& slot, int x, int y, int w, int h, int targetW, int targetH, LayerAlpha alphaOverride) {
    if (!slot.hasContent || !slot.srv || w <= 0 || h <= 0 || targetW <= 0 || targetH <= 0) return;

    // Convert top-left client pixel rect [x, y, x+w, y+h] into NDC [-1..1] with Y pointing up:
    // top of quad (uv.y = 0 -> t.y = 0) sits at y0 = 1 - 2*y/targetH;
    // bottom of quad (uv.y = 1 -> t.y = 1) sits at y1 = 1 - 2*(y+h)/targetH.
    const float x0 = (2.0f * static_cast<float>(x) / static_cast<float>(targetW)) - 1.0f;
    const float x1 = (2.0f * static_cast<float>(x + w) / static_cast<float>(targetW)) - 1.0f;
    const float y0 = 1.0f - (2.0f * static_cast<float>(y) / static_cast<float>(targetH));
    const float y1 = 1.0f - (2.0f * static_cast<float>(y + h) / static_cast<float>(targetH));

    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(g_d3d.ctx->Map(g_d3d.cb.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) return;
    auto* cb = static_cast<QuadConstants*>(mapped.pData);
    cb->rect[0] = x0;
    cb->rect[1] = y0;
    cb->rect[2] = x1;
    cb->rect[3] = y1;
    cb->alphaMode = alphaOverride == LayerAlpha::Opaque ? 0 : (alphaOverride == LayerAlpha::Straight ? 1 : 2);
    cb->flipY = slot.bottomUp ? 1 : 0;
    cb->pad[0] = cb->pad[1] = 0;
    g_d3d.ctx->Unmap(g_d3d.cb.Get(), 0);

    ID3D11ShaderResourceView* srvs[] = {slot.srv.Get()};
    g_d3d.ctx->PSSetShaderResources(0, 1, srvs);
    g_d3d.ctx->Draw(4, 0);
    ID3D11ShaderResourceView* nullSrvs[] = {nullptr};
    g_d3d.ctx->PSSetShaderResources(0, 1, nullSrvs);
}

void CompositeToRtv(WindowCompositor& wc, ID3D11RenderTargetView* rtv, int targetW, int targetH) {
    const float clear[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    g_d3d.ctx->ClearRenderTargetView(rtv, clear);

    D3D11_VIEWPORT vp{0.0f, 0.0f, static_cast<float>(targetW), static_cast<float>(targetH), 0.0f, 1.0f};
    g_d3d.ctx->RSSetViewports(1, &vp);
    g_d3d.ctx->RSSetState(g_d3d.rast.Get());
    ID3D11RenderTargetView* rtvs[] = {rtv};
    g_d3d.ctx->OMSetRenderTargets(1, rtvs, nullptr);
    const float blendFactor[4] = {0, 0, 0, 0};
    g_d3d.ctx->OMSetBlendState(g_d3d.blend.Get(), blendFactor, 0xffffffffu);
    g_d3d.ctx->IASetInputLayout(nullptr);
    g_d3d.ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    g_d3d.ctx->VSSetShader(g_d3d.vs.Get(), nullptr, 0);
    g_d3d.ctx->PSSetShader(g_d3d.ps.Get(), nullptr, 0);
    ID3D11Buffer* cbs[] = {g_d3d.cb.Get()};
    g_d3d.ctx->VSSetConstantBuffers(0, 1, cbs);
    g_d3d.ctx->PSSetConstantBuffers(0, 1, cbs);
    ID3D11SamplerState* samps[] = {g_d3d.sampler.Get()};
    g_d3d.ctx->PSSetSamplers(0, 1, samps);

    bool hasBelow = false;
    // 1. Below-window SurfaceViews
    for (const auto& [key, layer] : wc.surfaces) {
        if (!layer.hasContent || !layer.placement.visible || layer.placement.zOrderOnTop) continue;
        hasBelow = true;
        const int lw = layer.placement.width > 0 ? layer.placement.width : targetW;
        const int lh = layer.placement.height > 0 ? layer.placement.height : targetH;
        DrawLayerQuad(layer, layer.placement.x, layer.placement.y, lw, lh, targetW, targetH, layer.alpha);
    }

    // 2. Main Java UI layer (preserves straight-alpha punchHole cutouts when a below layer exists)
    if (wc.ui.hasContent) {
        DrawLayerQuad(wc.ui, 0, 0, targetW, targetH, targetW, targetH,
                      hasBelow ? LayerAlpha::Straight : LayerAlpha::Opaque);
    }

    // 3. Above-window SurfaceViews (`setZOrderOnTop(true)`)
    for (const auto& [key, layer] : wc.surfaces) {
        if (!layer.hasContent || !layer.placement.visible || !layer.placement.zOrderOnTop) continue;
        const int lw = layer.placement.width > 0 ? layer.placement.width : targetW;
        const int lh = layer.placement.height > 0 ? layer.placement.height : targetH;
        DrawLayerQuad(layer, layer.placement.x, layer.placement.y, lw, lh, targetW, targetH, layer.alpha);
    }

    ID3D11RenderTargetView* nullRtv[] = {nullptr};
    g_d3d.ctx->OMSetRenderTargets(1, nullRtv, nullptr);
}

bool RenderAndPublishLocked(WindowCompositor& wc) {
    RECT rc{};
    int targetW = 0, targetH = 0;
    if (wc.hwnd && GetClientRect(wc.hwnd, &rc) && rc.right > 0 && rc.bottom > 0) {
        targetW = rc.right;
        targetH = rc.bottom;
    } else if (wc.ui.hasContent) {
        targetW = wc.ui.width;
        targetH = wc.ui.height;
    } else if (!wc.surfaces.empty()) {
        const auto& first = wc.surfaces.begin()->second;
        targetW = first.width;
        targetH = first.height;
    }
    if (targetW <= 0 || targetH <= 0) return false;

    if (IsEmbedded()) {
        if (!EnsureEmbedTargets(wc, targetW, targetH)) return false;
        CompositeToRtv(wc, wc.rtView.Get(), targetW, targetH);

        const int writeSlot = static_cast<int>(wc.frameIndex % 3);
        g_d3d.ctx->CopyResource(wc.staging[writeSlot].Get(), wc.rtTex.Get());
        wc.stagingValid[writeSlot] = true;

        // Asynchronous 3-buffer readback: try the oldest in-flight slot without blocking the GPU;
        // fall back to immediate map on the first 2 frames (or if GPU lags >2 frames) so first-frame
        // latency is zero.
        int readSlot = writeSlot;
        UINT mapFlags = 0;
        const int oldestSlot = static_cast<int>((wc.frameIndex + 1) % 3);
        if (wc.frameIndex >= 2 && wc.stagingValid[oldestSlot]) {
            D3D11_MAPPED_SUBRESOURCE asyncMap{};
            if (SUCCEEDED(g_d3d.ctx->Map(wc.staging[oldestSlot].Get(), 0, D3D11_MAP_READ,
                                         D3D11_MAP_FLAG_DO_NOT_WAIT, &asyncMap))) {
                PublishEmbedRgbaRows(static_cast<const uint8_t*>(asyncMap.pData), asyncMap.RowPitch, targetW, targetH);
                g_d3d.ctx->Unmap(wc.staging[oldestSlot].Get(), 0);
                ++wc.frameIndex;
                return true;
            }
        }
        D3D11_MAPPED_SUBRESOURCE syncMap{};
        if (SUCCEEDED(g_d3d.ctx->Map(wc.staging[readSlot].Get(), 0, D3D11_MAP_READ, mapFlags, &syncMap))) {
            PublishEmbedRgbaRows(static_cast<const uint8_t*>(syncMap.pData), syncMap.RowPitch, targetW, targetH);
            g_d3d.ctx->Unmap(wc.staging[readSlot].Get(), 0);
            ++wc.frameIndex;
            return true;
        }
        return false;
    }

    // Standalone window: only bind DXGI swapchain when at least one surface layer is active, so
    // pure-Java windows keep zero-overhead GDI paint and direct HWND probes never collide.
    if (wc.surfaces.empty() && !wc.swapChain) return false;
    if (!EnsureSwapChain(wc, targetW, targetH)) return false;
    CompositeToRtv(wc, wc.swapRtv.Get(), targetW, targetH);
    return SUCCEEDED(wc.swapChain->Present(0, 0));
}

WindowCompositor& GetOrCreateLocked(HWND hwnd) {
    auto& ptr = g_windows[hwnd];
    if (!ptr) {
        ptr = std::make_unique<WindowCompositor>();
        ptr->hwnd = hwnd;
    }
    return *ptr;
}

}  // namespace

bool CompositorAvailable() {
    std::lock_guard lock(g_d3d.mu);
    return EnsureDeviceLocked();
}

bool CompositorSubmitUiArgb(HWND hwnd, const uint32_t* argb, int w, int h,
                            int dirtyX, int dirtyY, int dirtyW, int dirtyH) {
    if (!hwnd || !argb || w <= 0 || h <= 0) return false;
    std::lock_guard lock(g_d3d.mu);
    if (!EnsureDeviceLocked()) return false;
    WindowCompositor& wc = GetOrCreateLocked(hwnd);

    // Pure-Java standalone window with no surface layers: leave standalone paint to GDI unless
    // embedded or multi-layer compositing is already active.
    if (!IsEmbedded() && wc.surfaces.empty() && !wc.swapChain) return false;

    const bool hadSame = wc.ui.tex && wc.ui.width == w && wc.ui.height == h;
    if (!EnsureLayerTexture(wc.ui, w, h, DXGI_FORMAT_B8G8R8A8_UNORM)) return false;

    if (hadSame && dirtyW > 0 && dirtyH > 0) {
        const int x0 = std::clamp(dirtyX, 0, w);
        const int y0 = std::clamp(dirtyY, 0, h);
        const int x1 = std::clamp(dirtyX + dirtyW, x0, w);
        const int y1 = std::clamp(dirtyY + dirtyH, y0, h);
        if (x1 > x0 && y1 > y0) {
            D3D11_BOX box{static_cast<UINT>(x0), static_cast<UINT>(y0), 0,
                          static_cast<UINT>(x1), static_cast<UINT>(y1), 1};
            const uint32_t* subSrc = argb + static_cast<size_t>(y0) * static_cast<size_t>(w) + x0;
            g_d3d.ctx->UpdateSubresource(wc.ui.tex.Get(), 0, &box, subSrc, static_cast<UINT>(w * 4), 0);
        }
    } else {
        g_d3d.ctx->UpdateSubresource(wc.ui.tex.Get(), 0, nullptr, argb, static_cast<UINT>(w * 4), 0);
    }
    wc.ui.alpha = LayerAlpha::Straight;
    wc.ui.bottomUp = false;
    wc.ui.hasContent = true;
    return RenderAndPublishLocked(wc);
}

bool CompositorSubmitSurfaceBytes(HWND hwnd, int layerKey, const void* data, size_t rowPitch,
                                  int w, int h, LayerPixels fmt, LayerAlpha alpha, bool bottomUp,
                                  const SurfaceLayerPlacement& placement) {
    if (!hwnd || !data || w <= 0 || h <= 0 || rowPitch < static_cast<size_t>(w) * 4) return false;
    std::lock_guard lock(g_d3d.mu);
    if (!EnsureDeviceLocked()) return false;
    WindowCompositor& wc = GetOrCreateLocked(hwnd);
    LayerSlot& slot = wc.surfaces[layerKey];
    const DXGI_FORMAT dxgiFmt = fmt == LayerPixels::Rgba8 ? DXGI_FORMAT_R8G8B8A8_UNORM : DXGI_FORMAT_B8G8R8A8_UNORM;
    if (!EnsureLayerTexture(slot, w, h, dxgiFmt)) return false;
    g_d3d.ctx->UpdateSubresource(slot.tex.Get(), 0, nullptr, data, static_cast<UINT>(rowPitch), 0);
    slot.alpha = alpha;
    slot.bottomUp = bottomUp;
    slot.placement = placement;
    slot.hasContent = true;
    return RenderAndPublishLocked(wc);
}

bool CompositorSubmitSurfaceSharedTexture(HWND hwnd, int layerKey, HANDLE dxgiSharedHandle,
                                          int w, int h, LayerAlpha alpha, bool bottomUp,
                                          const SurfaceLayerPlacement& placement) {
    if (!hwnd || !dxgiSharedHandle || w <= 0 || h <= 0) return false;
    std::lock_guard lock(g_d3d.mu);
    if (!EnsureDeviceLocked()) return false;

    ComPtr<ID3D11Texture2D> sharedTex;
    if (FAILED(g_d3d.dev->OpenSharedResource(dxgiSharedHandle, __uuidof(ID3D11Texture2D),
                                             reinterpret_cast<void**>(sharedTex.GetAddressOf())))) {
        return false;
    }
    D3D11_TEXTURE2D_DESC sd{};
    sharedTex->GetDesc(&sd);
    DXGI_FORMAT fmt = sd.Format;
    if (fmt == DXGI_FORMAT_B8G8R8A8_TYPELESS) fmt = DXGI_FORMAT_B8G8R8A8_UNORM;
    else if (fmt == DXGI_FORMAT_R8G8B8A8_TYPELESS) fmt = DXGI_FORMAT_R8G8B8A8_UNORM;

    WindowCompositor& wc = GetOrCreateLocked(hwnd);
    LayerSlot& slot = wc.surfaces[layerKey];
    if (!EnsureLayerTexture(slot, static_cast<int>(sd.Width), static_cast<int>(sd.Height), fmt)) return false;
    g_d3d.ctx->CopyResource(slot.tex.Get(), sharedTex.Get());
    slot.alpha = alpha;
    slot.bottomUp = bottomUp;
    slot.placement = placement;
    slot.hasContent = true;
    return RenderAndPublishLocked(wc);
}

void CompositorUpdateSurfacePlacement(HWND hwnd, int layerKey, const SurfaceLayerPlacement& placement, bool drop) {
    std::lock_guard lock(g_d3d.mu);
    auto winIt = g_windows.find(hwnd);
    if (winIt == g_windows.end()) return;
    WindowCompositor& wc = *winIt->second;
    auto it = wc.surfaces.find(layerKey);
    if (it == wc.surfaces.end()) return;
    if (drop) {
        wc.surfaces.erase(it);
    } else {
        it->second.placement = placement;
    }
    RenderAndPublishLocked(wc);
}

void CompositorClearSurfaceLayers(HWND hwnd) {
    std::lock_guard lock(g_d3d.mu);
    auto winIt = g_windows.find(hwnd);
    if (winIt == g_windows.end()) return;
    winIt->second->surfaces.clear();
}

bool CompositorPresentWindow(HWND hwnd, int clientW, int clientH) {
    if (!hwnd || clientW <= 0 || clientH <= 0) return false;
    std::lock_guard lock(g_d3d.mu);
    auto winIt = g_windows.find(hwnd);
    if (winIt == g_windows.end()) return false;
    WindowCompositor& wc = *winIt->second;
    if (!IsEmbedded() && wc.surfaces.empty() && !wc.swapChain) return false;
    return RenderAndPublishLocked(wc);
}

void CompositorDestroyWindow(HWND hwnd) {
    std::lock_guard lock(g_d3d.mu);
    g_windows.erase(hwnd);
}

}  // namespace winapk::bridge
