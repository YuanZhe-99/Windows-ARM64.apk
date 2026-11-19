// skia_d3d12.h -- Windows ARM64 Direct3D 12 device/queue bridge for Skia Ganesh AGSL/RenderEffect.
#pragma once

#include <jni.h>

namespace winapk::bridge {

/// Registers com.macapk.graphics.NativeSkiaD3D12 on the JVM `env` belongs to.
bool RegisterSkia(JNIEnv* env);

}  // namespace winapk::bridge
