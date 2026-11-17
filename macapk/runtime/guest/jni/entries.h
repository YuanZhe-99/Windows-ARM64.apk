#pragma once
#include <cstddef>
#include <cstdint>

namespace winapk::guest {

/// Captures the fixed JNI scalar/reference ABI; no guest or host va_list is represented here.
struct EntryFrame {
    uint64_t integers[8], floating[8];
    const uint64_t* stack;
    uint64_t integerResult, floatingResult;
};
static_assert(sizeof(EntryFrame) == 152 && offsetof(EntryFrame, stack) == 128 &&
              offsetof(EntryFrame, integerResult) == 136 && offsetof(EntryFrame, floatingResult) == 144);

/// Lists process-lifetime PE entry addresses; a closed slot is never recycled.
extern "C" void* winapk_jni_entries[64];
/// Calls one screened fixed-signature guest entry, copying the descriptor's exact stack-slot count.
extern "C" void winapk_jni_invoke(void* entry, EntryFrame* frame, uint64_t stackSlots);
/// Dispatches a PE entry's captured registers to its registered guest method under a thread binding.
extern "C" void winapk_jni_dispatch(uint32_t slot, EntryFrame* frame);

}  // namespace winapk::guest
