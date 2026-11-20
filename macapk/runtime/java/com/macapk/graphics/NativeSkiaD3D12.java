package com.macapk.graphics;

/**
 * Host Direct3D 12 COM objects used exclusively by Skia's Windows ARM64 Ganesh backend;
 * pointers never cross into guest Android code (PLAN.md §15).
 */
public final class NativeSkiaD3D12 {
    private NativeSkiaD3D12() {}

    /**
     * Creates a hardware IDXGIAdapter1, ID3D12Device and direct ID3D12CommandQueue.
     *
     * <p>Returns: {@code {adapterPtr, devicePtr, queuePtr}}, each carrying one COM reference.
     */
    public static native long[] createAdapterDeviceQueue();

    /** Drains pending GPU work and releases the queue, device and adapter in dependency order. */
    public static native void releaseAdapterDeviceQueue(long adapter, long device, long queue);

    /** Returns the DXGI adapter description string for diagnostic/backend reporting. */
    public static native String adapterName(long adapter);

    /**
     * Shares a Skia Direct3D 12 rendered BGRA8 surface into {@code WinApkCompositor} via a DXGI NT
     * shared handle ({@code D3D12_HEAP_FLAG_SHARED} → {@code ID3D11Device1::OpenSharedResource1}).
     */
    public static native boolean presentSharedLayer(long device, long queue, int layerKey,
                                                    long bgraPixelsPtr, int width, int height,
                                                    int x, int y, boolean zOrderOnTop);

    /** Headless end-to-end verification of D3D12 → DXGI NT shared handle → D3D11 compositor. */
    public static native boolean verifySharedCompositorRoundtrip(long device, long queue,
                                                                 long bgraPixelsPtr,
                                                                 int width, int height,
                                                                 int[] outArgb);
}
