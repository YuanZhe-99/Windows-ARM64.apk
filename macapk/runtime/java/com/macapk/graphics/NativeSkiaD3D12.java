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
}
