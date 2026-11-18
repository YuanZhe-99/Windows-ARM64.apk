// audio.h -- Windows ARM64 WASAPI audio backend for com.macapk.loader.NativeAudio (PLAN.md §15).
#pragma once

#include <jni.h>

namespace winapk::bridge {

/// Registers all 43 methods of com.macapk.loader.NativeAudio onto the JVM `env` belongs to.
/// Returns: true when the class is present and every method bound cleanly.
bool RegisterAudio(JNIEnv* env);

}  // namespace winapk::bridge
