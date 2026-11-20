// ndk_platform.h -- Profile N2 Android NDK platform libraries on Windows ARM64:
//   - `libandroid.so` extensions (`ALooper_*`, `AChoreographer_*`, `AAssetManager_*`)
//   - `libjnigraphics.so` (`AndroidBitmap_*`)
//   - `libOpenSLES.so` & `libaaudio.so` (WASAPI PCM buffer-queue audio output)
#pragma once

#include "bridge.h"
#include "../../../../guest/loader/elf_loader.h"
#include <vector>

namespace winapk::bridge {

/// Appends `ALooper_*`, `AChoreographer_*`, and `AAssetManager_*` exports into `libandroid`
/// and adds `libjnigraphics.so`, `libOpenSLES.so`, and `libaaudio.so` to `libraries`.
void AddNdkPlatformLibraries(guest::BuiltinLibrary& libandroid,
                             std::vector<guest::BuiltinLibrary>& libraries);

/// Registers `com.macapk.loader.NativeLooper` so Java `Looper.loop()` and guest `ALooper_addFd`
/// share one per-thread native looper (`MACAPK_MAIN_NATIVE_POLL` & `MACAPK_CHOREO_VSYNC_THREADS`).
bool RegisterNativeLooper(JNIEnv* env);

/// Delivers one display vsync tick (in `System.nanoTime()` epoch) to per-thread loopers waiting
/// in `NativeLooper.scheduleVsync`.
void DeliverNativeLooperVsync();

}  // namespace winapk::bridge
