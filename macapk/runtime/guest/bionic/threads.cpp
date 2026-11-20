// threads.cpp -- explicit Android LP64 pthread tokens, errno and LLVM emutls descriptors.
// Nothing here changes x18 or TPIDR_EL0, shares a CRT object, or generates executable thunks.
#include "threads.h"

#include <windows.h>
#include <process.h>
#include <malloc.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <iterator>
#include <limits>
#include <map>
#include <mutex>
#include <new>
#include <stdexcept>
#include <vector>

namespace winapk::guest {
namespace {

// Linux/Bionic values, deliberately not the host's errno macros.
constexpr int kAgain = 11, kInvalid = 22, kDeadlock = 35, kNoThread = 3, kNotSupported = 95;
constexpr int kBusy = 16, kPermission = 1, kNoMemory = 12;
constexpr int kNoEntry = 2, kIo = 5, kBadFd = 9, kAccess = 13, kFault = 14, kExists = 17;
constexpr int kNotDirectory = 20, kIsDirectory = 21, kTooManyFiles = 24, kNoSpace = 28;
constexpr int kReadOnly = 30, kNameTooLong = 36, kEncoding = 84;
constexpr int kOverflow = 75;
constexpr size_t kKeys = 128;
using GuestThread = int64_t;  // Android pthread_t is long, not Windows long.
using GuestKey = int32_t;
using Entry = void* (*)(void*);
using Destructor = void (*)(void*);

/// Terminates on a broken guest execution contract instead of unwinding through ELF code.
[[noreturn]] void Fatal(const char* reason) {
    std::fprintf(stderr, "GUEST_ABI_UNSUPPORTED thread runtime: %s\n", reason);
    std::fflush(stderr);
    TerminateProcess(GetCurrentProcess(), 3);
    std::abort();
}

/// Describes one LLVM AArch64 emulated TLS variable, using guest-sized fields only.
struct EmutlsControl {
    uint64_t size, alignment, object;
    const void* initializer;
};
static_assert(sizeof(EmutlsControl) == 32 && offsetof(EmutlsControl, initializer) == 24);

/// Records an allocated key's generation token and guest destructor.
struct Key {
    GuestKey token = 0;
    Destructor destructor = nullptr;
};

/// Keeps a thread-specific value associated with the key generation that set it.
struct Value {
    GuestKey token = 0;
    void* pointer = nullptr;
};

/// Owns one aligned emutls allocation in a single guest context.
struct TlsBlock {
    const EmutlsControl* control;
    void* pointer;
};

/// Keeps the host thread handle and its full pointer-sized return value outside guest memory.
struct Worker {
    ThreadState* runtime;
    GuestThread token;
    Entry entry;
    void* argument;
    HANDLE handle = nullptr;
    void* result = nullptr;
    bool joining = false;
    bool detached = false;
    bool finished = false;
    /// Closes the thread handle only after its owner has waited for exit (or on detach).
    ~Worker() { if (handle) CloseHandle(handle); }
};

/// Owns host-only state for a guest mutex; every field is protected by ThreadState::mutex.
struct MutexState {
    uint64_t generation;
    int kind;  // Android NORMAL=0, RECURSIVE=1, ERRORCHECK=2.
    GuestThread owner = 0;
    uint32_t depth = 0;
    size_t contenders = 0, conditionWaiters = 0;
    std::condition_variable available;

    /// Creates a fresh unlocked mutex record with no host object in guest memory.
    MutexState(uint64_t id, int type) : generation(id), kind(type) {}
};

/// Owns a guest condition's wait queue and dynamic mutex binding, outside guest storage.
struct ConditionState {
    uint64_t generation;
    int clockId = 0;  // 0 = CLOCK_REALTIME, 1 = CLOCK_MONOTONIC
    size_t waiters = 0;
    MutexState* binding = nullptr;
    std::condition_variable changed;

    /// Creates a process-private condition record.
    explicit ConditionState(uint64_t id, int clk = 0) : generation(id), clockId(clk) {}
};

/// Owns one non-inheritable regular-file handle, never represented by a guest fd value.
struct GuestFile {
    HANDLE handle = INVALID_HANDLE_VALUE;
    bool readable = false, writable = false, append = false;
    /// Closes a retained descriptor at runtime teardown after all guest contexts have ended.
    ~GuestFile() { if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle); }
};

/// Describes the Android LP64 timespec without using Windows long or CRT time types.
struct GuestTimespec { int64_t seconds, nanoseconds; };
static_assert(sizeof(GuestTimespec) == 16);

/// Describes Bionic AArch64 stat explicitly, including padding and guest 64-bit longs.
struct GuestStat {
    uint64_t device, inode;
    uint32_t mode, links, uid, gid;
    uint64_t rdevice, padding1;
    int64_t size;
    int32_t blockSize, padding2;
    int64_t blocks;
    GuestTimespec access, modification, change;
    uint32_t unused4, unused5;
};
static_assert(sizeof(GuestStat) == 128 && offsetof(GuestStat, size) == 48 &&
              offsetof(GuestStat, access) == 72 && offsetof(GuestStat, unused4) == 120);

/// Retains one guest C++-ABI exit callback without sharing the host CRT's destructor registry.
struct ExitCallback {
    void (*function)(void*);
    void* argument;
    void* dso;
};

/// Retains guest fork hooks without claiming to implement fork or invoking hooks at registration.
struct ForkHooks {
    void (*prepare)();
    void (*parent)();
    void (*child)();
    void* dso;
};

}  // namespace

struct AllocationEntry {
    void* rawBase = nullptr;
    uint64_t usableSize = 0;
};

/// Holds one runtime's synchronized token tables and exact cleanup counters.
struct ThreadState {
    ModuleSet* modules = nullptr;  // Caller-owned; mappings outlive this runtime's guest threads.
    HANDLE heap = HeapCreate(0, 0, 0);
    std::mutex mutex;
    std::map<void*, AllocationEntry> allocations;
    std::wstring androidRoot;  // Canonical extended-length host path, ending in a backslash.
    std::map<int32_t, std::unique_ptr<GuestFile>> descriptors;
    std::vector<ExitCallback> exits;
    std::vector<ForkHooks> forkHooks;
    bool stopping = false;
    int32_t nextFd = 3;
    std::map<GuestThread, std::unique_ptr<Worker>> workers;
    std::array<Key, kKeys> keys{};
    std::map<void*, std::unique_ptr<MutexState>> mutexes;
    std::map<void*, std::unique_ptr<ConditionState>> conditions;
    uint64_t nextSync = 1;
    GuestThread nextThread = 1;
    GuestKey nextKey = 1;
    std::atomic<size_t> contexts = 0, tlsBlocks = 0;

    /// Releases all remaining guest allocations only after every managed context has ended.
    ~ThreadState() { if (heap) HeapDestroy(heap); }
};

namespace {

/// Holds guest-only state for one host thread, never in a guest hardware thread pointer.
struct GuestThreadContext {
    ThreadState& runtime;
    GuestThread token;
    int32_t error = 0;
    std::string dlError, dlReturned;
    std::array<Value, kKeys> values{};
    std::vector<TlsBlock> tls;

    /// Creates guest state, optionally activating it immediately for a standalone/worker entry.
    GuestThreadContext(ThreadState& state, GuestThread id, bool activate = true);
    /// Runs key destructors while TLS is alive, then frees TLS before ending the context.
    ~GuestThreadContext();
};

thread_local GuestThreadContext* current = nullptr;
thread_local GuestThreadContext* t_dormantBindingContext = nullptr;
std::atomic<ThreadState*> g_n2AutoState{nullptr};

/// Returns the calling managed context; in N2 mode auto-binds host callback threads on demand.
GuestThreadContext& Context() {
    if (!current) {
        if (ThreadState* autoState = g_n2AutoState.load(std::memory_order_acquire)) {
            if (t_dormantBindingContext && &t_dormantBindingContext->runtime == autoState) {
                return *t_dormantBindingContext;
            }
            thread_local std::unique_ptr<GuestThreadContext> autoCtx;
            if (!autoCtx) {
                GuestThread id = 0;
                {
                    std::lock_guard lock(autoState->mutex);
                    if (!autoState->stopping && autoState->nextThread != std::numeric_limits<GuestThread>::max()) {
                        id = autoState->nextThread++;
                    }
                }
                if (id != 0) {
                    autoCtx = std::make_unique<GuestThreadContext>(*autoState, id, true);
                }
            }
        }
        if (!current) Fatal("shim called without a managed guest context");
    }
    return *current;
}

GuestThreadContext::GuestThreadContext(ThreadState& state, GuestThread id, bool activate) : runtime(state), token(id) {
    if (activate) {
        if (current) Fatal("nested fresh guest context");
        current = this;
    }
    runtime.contexts++;
}

GuestThreadContext::~GuestThreadContext() {
    if (current != this) Fatal("context cleanup requires its active host thread");
    // Snapshot tokens, not table iterators: callbacks may create/delete keys or set values.
    for (int round = 0; round < 4; round++) {
        const auto pending = values;
        for (size_t i = 0; i < kKeys; i++) {
            const Value v = pending[i];
            if (!v.pointer) continue;
            Destructor destructor = nullptr;
            {
                std::lock_guard lock(runtime.mutex);
                if (runtime.keys[i].token != v.token || values[i].token != v.token ||
                    values[i].pointer != v.pointer) continue;
                destructor = runtime.keys[i].destructor;
                values[i].pointer = nullptr;
            }
            if (destructor) destructor(v.pointer);
        }
    }
    for (const TlsBlock& block : tls) {
        _aligned_free(block.pointer);
        runtime.tlsBlocks--;
    }
    runtime.contexts--;
    current = nullptr;
}

/// Returns this context's guest errno address, not the host CRT's errno address.
int32_t* Errno() { return &Context().error; }

/// Returns the guest's opaque 64-bit thread token.
GuestThread Self() { return Context().token; }

/// Compares two Android thread tokens with a fixed integer signature.
int Equal(GuestThread a, GuestThread b) { return a == b; }

/// Fills guest bytes through an audited fixed signature (Android size_t is 64-bit).
void* BytesSet(void* destination, int32_t value, uint64_t size) {
    return std::memset(destination, value, static_cast<size_t>(size));
}

/// Allocates and records guest-owned bytes; the caller holds runtime.mutex.
void* Allocate(ThreadState& runtime, uint64_t size, bool zero, uint64_t align = sizeof(void*)) {
    if (size > INT64_MAX || align == 0 || (align & (align - 1)) != 0) {
        Context().error = kNoMemory;
        return nullptr;
    }
    const uint64_t req = size ? size : 1;
    const uint64_t extra = align > 16 ? align : 0;
    if (req > static_cast<uint64_t>(INT64_MAX) - extra) {
        Context().error = kNoMemory;
        return nullptr;
    }
    void* raw = HeapAlloc(runtime.heap, zero ? HEAP_ZERO_MEMORY : 0, static_cast<SIZE_T>(req + extra));
    if (raw) {
        uintptr_t addr = reinterpret_cast<uintptr_t>(raw);
        if (extra) addr = (addr + align - 1) & ~(static_cast<uintptr_t>(align) - 1);
        void* pointer = reinterpret_cast<void*>(addr);
        try {
            runtime.allocations.emplace(pointer, AllocationEntry{raw, size});
            return pointer;
        } catch (const std::bad_alloc&) {
            HeapFree(runtime.heap, 0, raw);
        }
    }
    Context().error = kNoMemory;
    return nullptr;
}

/// Allocates bytes from this runtime's private heap, returning guest ENOMEM on failure.
void* Malloc(uint64_t size) {
    auto& runtime = Context().runtime;
    std::lock_guard lock(runtime.mutex);
    return Allocate(runtime, size, false);
}

/// Allocates zeroed guest bytes after an explicit unsigned multiplication-overflow check.
void* Calloc(uint64_t count, uint64_t size) {
    if (size && count > UINT64_MAX / size) { Context().error = kNoMemory; return nullptr; }
    auto& runtime = Context().runtime;
    std::lock_guard lock(runtime.mutex);
    return Allocate(runtime, count * size, true);
}

/// Frees only an exact guest-owned allocation; invalid ownership terminates before HeapFree.
void Release(void* pointer) {
    if (!pointer) return;
    auto& runtime = Context().runtime;
    std::lock_guard lock(runtime.mutex);
    const auto it = runtime.allocations.find(pointer);
    if (it == runtime.allocations.end()) Fatal("free of invalid or already freed guest allocation");
    if (!HeapFree(runtime.heap, 0, it->second.rawBase)) Fatal("guest heap release failed");
    runtime.allocations.erase(it);
}

/// Resizes guest bytes transactionally, preserving the old allocation and content on failure.
void* Resize(void* pointer, uint64_t size) {
    auto& runtime = Context().runtime;
    std::lock_guard lock(runtime.mutex);
    if (!pointer) return Allocate(runtime, size, false);
    const auto it = runtime.allocations.find(pointer);
    if (it == runtime.allocations.end()) Fatal("realloc of invalid or already freed guest allocation");
    void* result = nullptr;
    if (size) {
        result = Allocate(runtime, size, false);
        if (!result) return nullptr;
        std::memcpy(result, pointer, static_cast<size_t>(std::min(size, it->second.usableSize)));
    }
    if (!HeapFree(runtime.heap, 0, it->second.rawBase)) Fatal("guest heap release failed");
    runtime.allocations.erase(it);
    return result;
}

/// Copies non-overlapping guest bytes through an audited 64-bit size signature.
void* BytesCopy(void* destination, const void* source, uint64_t size) {
    return std::memcpy(destination, source, static_cast<size_t>(size));
}

/// Moves potentially overlapping guest bytes without exposing any CRT object.
void* BytesMove(void* destination, const void* source, uint64_t size) {
    return std::memmove(destination, source, static_cast<size_t>(size));
}

/// Compares guest bytes as unsigned bytes and returns the ordering sign.
int32_t BytesCompare(const void* first, const void* second, uint64_t size) {
    return std::memcmp(first, second, static_cast<size_t>(size));
}

/// Finds the first matching unsigned byte, bounded by the guest's 64-bit length.
void* BytesFind(const void* source, int32_t value, uint64_t size) {
    return const_cast<void*>(std::memchr(source, value, static_cast<size_t>(size)));
}

/// Returns a terminated byte string's length as guest size_t, without transcoding.
uint64_t StringLength(const char* string) { return std::strlen(string); }

/// Returns the bounded byte length, without reading beyond the supplied maximum.
uint64_t StringBoundedLength(const char* string, uint64_t size) { return strnlen(string, static_cast<size_t>(size)); }

/// Compares terminated byte strings without locale or encoding conversion.
int32_t StringCompare(const char* first, const char* second) { return std::strcmp(first, second); }

/// Compares at most size unsigned bytes, stopping at a string terminator.
int32_t StringBoundedCompare(const char* first, const char* second, uint64_t size) {
    return std::strncmp(first, second, static_cast<size_t>(size));
}

/// Copies a terminated byte string into caller-owned storage.
char* StringCopy(char* destination, const char* source) { return std::strcpy(destination, source); }

/// Copies and pads at most size bytes with standard strncpy termination rules.
char* StringBoundedCopy(char* destination, const char* source, uint64_t size) {
    return std::strncpy(destination, source, static_cast<size_t>(size));
}

/// Finds the first matching byte or the terminator when value converts to zero.
char* StringFind(const char* string, int32_t value) { return const_cast<char*>(std::strchr(string, value)); }

/// Finds the last matching byte, including the terminator.
char* StringReverseFind(const char* string, int32_t value) { return const_cast<char*>(std::strrchr(string, value)); }

/// Finds a byte substring without interpreting UTF-8 code points.
char* StringSubstring(const char* string, const char* needle) { return const_cast<char*>(std::strstr(string, needle)); }

/// Duplicates terminated bytes into the guest private heap, using the same free contract.
char* StringDuplicate(const char* string) {
    const uint64_t size = std::strlen(string) + 1;
    auto* result = static_cast<char*>(Malloc(size));
    if (result) std::memcpy(result, string, static_cast<size_t>(size));
    return result;
}

/// Returns a POSIX-style failure after storing a Linux errno in this managed context.
int32_t IoError(int32_t error) { Context().error = error; return -1; }

/// Writes and flushes one bounded byte log with a fixed signature, never a guest format/va_list.
int32_t AndroidLogWrite(int32_t priority, const char* tag, const char* message) {
    if (priority < 2 || priority > 7 || !tag || !message) return IoError(kInvalid);
    if (strnlen(tag, 128) == 128 || strnlen(message, 4096) == 4096) return IoError(kOverflow);
    if (std::fprintf(stderr, "guest-log %d %s: %s\n", priority, tag, message) < 0 || std::fflush(stderr))
        return IoError(kIo);
    return 1;
}

/// Converts Windows file errors explicitly; unclassified host I/O failures become EIO.
int32_t WindowsIoError(DWORD error) {
    switch (error) {
    case ERROR_FILE_NOT_FOUND: case ERROR_PATH_NOT_FOUND: return IoError(kNoEntry);
    case ERROR_ACCESS_DENIED: case ERROR_SHARING_VIOLATION: case ERROR_LOCK_VIOLATION: return IoError(kAccess);
    case ERROR_ALREADY_EXISTS: case ERROR_FILE_EXISTS: return IoError(kExists);
    case ERROR_INVALID_HANDLE: return IoError(kBadFd);
    case ERROR_NOACCESS: case ERROR_INVALID_USER_BUFFER: return IoError(kFault);
    case ERROR_NOT_ENOUGH_MEMORY: case ERROR_OUTOFMEMORY: return IoError(kNoMemory);
    case ERROR_DISK_FULL: case ERROR_HANDLE_DISK_FULL: return IoError(kNoSpace);
    case ERROR_WRITE_PROTECT: return IoError(kReadOnly);
    case ERROR_DIRECTORY: return IoError(kNotDirectory);
    case ERROR_FILENAME_EXCED_RANGE: return IoError(kNameTooLong);
    case ERROR_INVALID_PARAMETER: case ERROR_NEGATIVE_SEEK: return IoError(kInvalid);
    default: return IoError(kIo);
    }
}

/// Reads a handle's canonical extended-length path without interpreting a guest pathname.
bool FinalPath(HANDLE handle, std::wstring& result) {
    const DWORD size = GetFinalPathNameByHandleW(handle, nullptr, 0, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    if (!size) return false;
    result.resize(size);
    const DWORD length = GetFinalPathNameByHandleW(handle, result.data(), size, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    if (!length || length >= size) return false;
    result.resize(length);
    return true;
}

/// Refuses DOS device basenames even with extensions, before any Windows file open.
bool DeviceName(std::wstring_view part) {
    const auto dot = part.find(L'.');
    const auto base = part.substr(0, dot);
    for (const wchar_t* reserved : {L"CON", L"PRN", L"AUX", L"NUL", L"CONIN$", L"CONOUT$", L"CLOCK$"})
        if (base.size() == std::wcslen(reserved) && !_wcsnicmp(base.data(), reserved, base.size())) return true;
    if (base.size() == 4 && (!_wcsnicmp(base.data(), L"COM", 3) || !_wcsnicmp(base.data(), L"LPT", 3))) {
        const wchar_t number = base[3];
        return (number >= L'1' && number <= L'9') || number == L'\u00b9' || number == L'\u00b2' || number == L'\u00b3';
    }
    return false;
}

/// Maps a strict UTF-8 Android absolute path below the supplied root, refusing reparse components.
bool FilePath(ThreadState& runtime, const char* name, std::wstring& path) {
    if (runtime.androidRoot.empty()) { IoError(kNotSupported); return false; }
    if (!name) { IoError(kFault); return false; }
    const size_t length = strnlen(name, 4097);
    if (!length) { IoError(kNoEntry); return false; }
    if (length > 4096) { IoError(kNameTooLong); return false; }
    const std::string_view input(name, length);
    if (!input.starts_with("/data/") && !input.starts_with("/sdcard/")) { IoError(kAccess); return false; }
    const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name + 1, static_cast<int>(length - 1), nullptr, 0);
    if (!count) { IoError(kEncoding); return false; }
    std::wstring relative(count, L'\0');
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name + 1, static_cast<int>(length - 1), relative.data(), count)) {
        IoError(kEncoding);
        return false;
    }
    path = runtime.androidRoot;
    for (size_t start = 0; start < relative.size();) {
        const size_t slash = relative.find(L'/', start);
        const auto part = std::wstring_view(relative).substr(start, slash == std::wstring::npos ? slash : slash - start);
        if (part.empty() || part == L"." || part == L".." || part.back() == L'.' || part.back() == L' ' || DeviceName(part)) {
            IoError(kAccess);
            return false;
        }
        for (wchar_t character : part)
            if (character < 32 || std::wstring_view(L"\\:*?\"<>|").find(character) != std::wstring_view::npos) {
                IoError(kAccess);
                return false;
            }
        path.append(part);
        const DWORD attributes = GetFileAttributesW(path.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES) {
            const DWORD error = GetLastError();
            if (slash != std::wstring::npos || (error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND)) {
                WindowsIoError(error);
                return false;
            }
        } else {
            if (attributes & FILE_ATTRIBUTE_REPARSE_POINT) { IoError(kAccess); return false; }
            if (slash != std::wstring::npos && !(attributes & FILE_ATTRIBUTE_DIRECTORY)) { IoError(kNotDirectory); return false; }
            if (slash == std::wstring::npos && (attributes & FILE_ATTRIBUTE_DIRECTORY)) { IoError(kIsDirectory); return false; }
        }
        if (slash == std::wstring::npos) return true;
        if (slash + 1 == relative.size()) { IoError(kIsDirectory); return false; }
        path += L'\\';
        start = slash + 1;
    }
    IoError(kAccess);
    return false;
}

/// Opens a regular guest file with an optional integer creation mode in x2, never host varargs.
int32_t FileOpen(const char* name, int32_t flags, uint32_t mode) {
    constexpr int32_t create = 00000100, exclusive = 00000200, truncate = 00001000, append = 00002000;
    constexpr int32_t allowed = 3 | create | exclusive | truncate | append | 00100000 | 02000000;
    if (flags & ~allowed) return IoError(kNotSupported);
    const int access = flags & 3;
    if (access == 3 || ((flags & exclusive) && !(flags & create)) ||
        (!access && (flags & (truncate | append)))) return IoError(kInvalid);
    if ((flags & create) && mode != 0600) return IoError(kNotSupported);
    auto& runtime = Context().runtime;
    std::lock_guard lock(runtime.mutex);
    try {
        if (runtime.descriptors.size() >= 256 || runtime.nextFd == INT32_MAX) return IoError(kTooManyFiles);
        std::wstring path;
        if (!FilePath(runtime, name, path)) return -1;
        auto file = std::make_unique<GuestFile>();
        file->readable = access != 1;
        file->writable = access != 0;
        file->append = (flags & append) != 0;
        const DWORD disposition = flags & create ? flags & exclusive ? CREATE_NEW : OPEN_ALWAYS : OPEN_EXISTING;
        file->handle = CreateFileW(path.c_str(), (file->readable ? GENERIC_READ : 0) | (file->writable ? GENERIC_WRITE : 0),
                                   FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, disposition,
                                   FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
        if (file->handle == INVALID_HANDLE_VALUE) return WindowsIoError(GetLastError());
        FILE_ATTRIBUTE_TAG_INFO attributes{};
        if (!GetFileInformationByHandleEx(file->handle, FileAttributeTagInfo, &attributes, sizeof attributes))
            return WindowsIoError(GetLastError());
        std::wstring final;
        if (!FinalPath(file->handle, final)) return WindowsIoError(GetLastError());
        if (attributes.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT ||
            final.size() <= runtime.androidRoot.size() ||
            _wcsnicmp(final.c_str(), runtime.androidRoot.c_str(), runtime.androidRoot.size())) return IoError(kAccess);
        if (attributes.FileAttributes & FILE_ATTRIBUTE_DIRECTORY) return IoError(kIsDirectory);
        if (GetFileType(file->handle) != FILE_TYPE_DISK) return IoError(kNotSupported);
        // Reserve the fd record before truncating: an allocation failure must not truncate a file.
        const int32_t descriptor = runtime.nextFd++;
        const auto [it, inserted] = runtime.descriptors.emplace(descriptor, std::move(file));
        (void)inserted;
        if ((flags & truncate) && !SetEndOfFile(it->second->handle)) {
            const DWORD error = GetLastError();
            runtime.descriptors.erase(it);
            return WindowsIoError(error);
        }
        return descriptor;
    } catch (const std::bad_alloc&) { return IoError(kNoMemory); }
}

/// Opens a fortified two-argument Bionic call, refusing any flag that would require a mode.
int32_t FileOpenChecked(const char* name, int32_t flags) {
    if (flags & 00000100) return IoError(kInvalid);
    return FileOpen(name, flags, 0);
}

/// Finds a guest fd only in its runtime-owned table; the caller holds runtime.mutex.
GuestFile* Descriptor(ThreadState& runtime, int32_t descriptor) {
    const auto it = runtime.descriptors.find(descriptor);
    if (it != runtime.descriptors.end()) return it->second.get();
    IoError(kBadFd);
    return nullptr;
}

/// Reads regular-file bytes with guest ssize_t, without treating a host HANDLE as an fd.
int64_t FileRead(int32_t descriptor, void* buffer, uint64_t size) {
    auto& runtime = Context().runtime;
    std::lock_guard lock(runtime.mutex);
    auto* file = Descriptor(runtime, descriptor);
    if (!file) return -1;
    if (!file->readable) return IoError(kBadFd);
    if (size > INT64_MAX) return IoError(kInvalid);
    if (size && !buffer) return IoError(kFault);
    if (!size) return 0;
    DWORD read = 0;
    if (!::ReadFile(file->handle, buffer, static_cast<DWORD>(std::min<uint64_t>(size, MAXDWORD)), &read, nullptr))
        return WindowsIoError(GetLastError());
    return read;
}

/// Writes regular-file bytes, targeting EOF atomically for O_APPEND regardless of prior seek.
int64_t FileWrite(int32_t descriptor, const void* buffer, uint64_t size) {
    auto& runtime = Context().runtime;
    std::lock_guard lock(runtime.mutex);
    auto* file = Descriptor(runtime, descriptor);
    if (!file) return -1;
    if (!file->writable) return IoError(kBadFd);
    if (size > INT64_MAX) return IoError(kInvalid);
    if (size && !buffer) return IoError(kFault);
    if (!size) return 0;
    OVERLAPPED atEnd{};
    atEnd.Offset = atEnd.OffsetHigh = MAXDWORD;
    DWORD written = 0;
    if (!::WriteFile(file->handle, buffer, static_cast<DWORD>(std::min<uint64_t>(size, MAXDWORD)), &written,
                     file->append ? &atEnd : nullptr)) return WindowsIoError(GetLastError());
    return written;
}

/// Closes exactly one guest fd and removes its private handle record.
int32_t FileClose(int32_t descriptor) {
    auto& runtime = Context().runtime;
    std::lock_guard lock(runtime.mutex);
    auto* file = Descriptor(runtime, descriptor);
    if (!file) return -1;
    if (!CloseHandle(file->handle)) return WindowsIoError(GetLastError());
    file->handle = INVALID_HANDLE_VALUE;
    runtime.descriptors.erase(descriptor);
    return 0;
}

/// Seeks a regular file with guest 64-bit off_t and explicit Android SEEK_* values.
int64_t FileSeek(int32_t descriptor, int64_t offset, int32_t origin) {
    if (origin < 0 || origin > 2) return IoError(kInvalid);
    auto& runtime = Context().runtime;
    std::lock_guard lock(runtime.mutex);
    auto* file = Descriptor(runtime, descriptor);
    if (!file) return -1;
    LARGE_INTEGER distance{}, position{};
    distance.QuadPart = offset;
    if (!SetFilePointerEx(file->handle, distance, &position, static_cast<DWORD>(origin))) return WindowsIoError(GetLastError());
    return position.QuadPart;
}

/// Converts Windows 100-ns ticks since 1601 to a normalized Unix-epoch guest timespec.
GuestTimespec UnixTime(int64_t ticks) {
    const int64_t unixTicks = ticks - 116444736000000000LL;
    int64_t seconds = unixTicks / 10000000, fraction = unixTicks % 10000000;
    if (fraction < 0) { seconds--; fraction += 10000000; }
    return {seconds, fraction * 100};
}

/// Fills the exact 128-byte guest stat layout from private Windows file information.
int32_t FileStat(int32_t descriptor, GuestStat* result) {
    if (!result) return IoError(kFault);
    auto& runtime = Context().runtime;
    std::lock_guard lock(runtime.mutex);
    auto* file = Descriptor(runtime, descriptor);
    if (!file) return -1;
    BY_HANDLE_FILE_INFORMATION identity{};
    FILE_STANDARD_INFO standard{};
    FILE_BASIC_INFO basic{};
    if (!GetFileInformationByHandle(file->handle, &identity) ||
        !GetFileInformationByHandleEx(file->handle, FileStandardInfo, &standard, sizeof standard) ||
        !GetFileInformationByHandleEx(file->handle, FileBasicInfo, &basic, sizeof basic))
        return WindowsIoError(GetLastError());
    GuestStat stat{};
    stat.device = identity.dwVolumeSerialNumber;
    stat.inode = (static_cast<uint64_t>(identity.nFileIndexHigh) << 32) | identity.nFileIndexLow;
    stat.mode = 0100000 | (basic.FileAttributes & FILE_ATTRIBUTE_READONLY ? 0400 : 0600);
    stat.links = standard.NumberOfLinks;
    stat.size = standard.EndOfFile.QuadPart;
    stat.blockSize = 4096;
    stat.blocks = standard.AllocationSize.QuadPart / 512;
    stat.access = UnixTime(basic.LastAccessTime.QuadPart);
    stat.modification = UnixTime(basic.LastWriteTime.QuadPart);
    stat.change = UnixTime(basic.ChangeTime.QuadPart);
    *result = stat;
    return 0;
}

/// Returns realtime or monotonic in an explicit guest layout; unsupported clocks never fall back.
int32_t ClockGetTime(int32_t clock, GuestTimespec* result) {
    if (!result) return IoError(kFault);
    if (clock == 0) {
        FILETIME time{};
        GetSystemTimePreciseAsFileTime(&time);
        *result = UnixTime(static_cast<int64_t>((static_cast<uint64_t>(time.dwHighDateTime) << 32) | time.dwLowDateTime));
    } else if (clock == 1) {
        ULONGLONG ticks = 0;
        QueryUnbiasedInterruptTimePrecise(&ticks);
        *result = {static_cast<int64_t>(ticks / 10000000), static_cast<int64_t>(ticks % 10000000) * 100};
    } else {
        return IoError(clock < 0 ? kInvalid : kNotSupported);
    }
    return 0;
}

/// Waits a bounded relative duration without guest signals; never synthesizes an interruption.
int32_t NanoSleep(const GuestTimespec* duration, GuestTimespec* remainder) {
    (void)remainder;  // POSIX leaves it untouched on success; this subset delivers no guest signal.
    if (!duration) return IoError(kFault);
    const GuestTimespec requested = *duration;
    if (requested.seconds < 0 || requested.nanoseconds < 0 || requested.nanoseconds >= 1000000000)
        return IoError(kInvalid);
    if (requested.seconds > 60 || (requested.seconds == 60 && requested.nanoseconds)) return IoError(kNotSupported);
    ULONGLONG now = 0;
    QueryUnbiasedInterruptTimePrecise(&now);
    const ULONGLONG deadline = now + static_cast<uint64_t>(requested.seconds) * 10000000 +
                              static_cast<uint64_t>((requested.nanoseconds + 99) / 100);
    while (now < deadline) {
        Sleep(static_cast<DWORD>((deadline - now + 9999) / 10000));
        QueryUnbiasedInterruptTimePrecise(&now);
    }
    return 0;
}

/// Opens an audited guest library with RTLD_NOW|RTLD_LOCAL only; never calls the Windows loader.
void* DlOpen(const char* name, int32_t flags) {
    auto& context = Context();
    try {
        if (!context.runtime.modules || !name || flags != 2) {
            context.dlError = "GUEST_ABI_UNSUPPORTED: dlopen requires a module namespace, basename and RTLD_NOW|RTLD_LOCAL";
            return nullptr;
        }
        LoadError error;
        const uint64_t handle = context.runtime.modules->Open(name, &error);
        if (!handle) context.dlError = std::string(CategoryName(error.category)) + ": " + error.where + ": " + error.reason;
        return reinterpret_cast<void*>(static_cast<uintptr_t>(handle));
    } catch (const std::bad_alloc&) { Fatal("dynamic linker diagnostic allocation failed"); }
}

/// Looks up a guest handle's own/dependency scope; default/global host scopes are not exposed.
void* DlSym(void* handle, const char* name) {
    auto& context = Context();
    try {
        void* result = context.runtime.modules && name ?
            context.runtime.modules->Find(reinterpret_cast<uintptr_t>(handle), name) : nullptr;
        if (!result) context.dlError = "GUEST_SYMBOL_UNRESOLVED: invalid guest handle or export not found";
        return result;
    } catch (const std::bad_alloc&) { Fatal("dynamic linker diagnostic allocation failed"); }
}

/// Drops one explicit guest open reference while retaining code for process-lifetime safety.
int DlClose(void* handle) {
    auto& context = Context();
    try {
        if (context.runtime.modules && context.runtime.modules->Close(reinterpret_cast<uintptr_t>(handle))) return 0;
        context.dlError = "GUEST_ABI_UNSUPPORTED: invalid or already closed guest library handle";
        return -1;
    } catch (const std::bad_alloc&) { Fatal("dynamic linker diagnostic allocation failed"); }
}

/// Returns and clears only this guest thread's pending dynamic-linker diagnostic.
char* DlError() {
    auto& context = Context();
    if (context.dlError.empty()) return nullptr;
    context.dlReturned = std::move(context.dlError);
    context.dlError.clear();
    return context.dlReturned.data();
}

/// Registers a real guest exit callback and DSO identity; returns failure if bounded storage is full.
int32_t CxaAtExit(void (*function)(void*), void* argument, void* dso) {
    if (!function) return IoError(kInvalid);
    auto& runtime = Context().runtime;
    std::lock_guard lock(runtime.mutex);
    if (runtime.exits.size() >= 256) return IoError(kNoMemory);
    try {
        runtime.exits.push_back({function, argument, dso});
        return 0;
    } catch (const std::bad_alloc&) { return IoError(kNoMemory); }
}

/// Calls matching callbacks in reverse order after removing them, allowing safe guest reentry.
void CxaFinalize(void* dso) {
    auto& runtime = Context().runtime;
    for (size_t calls = 0; ; calls++) {
        ExitCallback callback{};
        {
            std::lock_guard lock(runtime.mutex);
            auto found = std::find_if(runtime.exits.rbegin(), runtime.exits.rend(),
                                     [dso](const ExitCallback& entry) { return !dso || entry.dso == dso; });
            if (found == runtime.exits.rend()) {
                std::erase_if(runtime.forkHooks, [dso](const ForkHooks& entry) { return !dso || entry.dso == dso; });
                return;
            }
            if (calls == 4096) Fatal("exit callback iteration limit exceeded");
            callback = *found;
            runtime.exits.erase(std::next(found).base());
        }
        callback.function(callback.argument);
    }
}

/// Stores guest atfork hooks for their DSO; fork itself remains unavailable in this profile.
int32_t RegisterAtFork(void (*prepare)(), void (*parent)(), void (*child)(), void* dso) {
    auto& runtime = Context().runtime;
    std::lock_guard lock(runtime.mutex);
    if (runtime.forkHooks.size() >= 128) return kNoMemory;
    try {
        runtime.forkHooks.push_back({prepare, parent, child, dso});
        return 0;
    } catch (const std::bad_alloc&) { return kNoMemory; }
}

/// Enters a new guest context on a CRT-initialized host worker and publishes its return value.
unsigned __stdcall Start(void* pointer) {
    auto* worker = static_cast<Worker*>(pointer);
    ThreadState* rt = worker->runtime;
    const GuestThread token = worker->token;
    void* res = nullptr;
    {
        GuestThreadContext context(*rt, token);
        res = worker->entry(worker->argument);
    }
    std::lock_guard lock(rt->mutex);
    auto it = rt->workers.find(token);
    if (it != rt->workers.end()) {
        it->second->result = res;
        it->second->finished = true;
        if (it->second->detached) {
            rt->workers.erase(it);
        }
    }
    return 0;
}

/// Creates a default joinable worker, returning Linux errors without changing guest errno.
int Create(GuestThread* token, const void* attributes, Entry entry, void* argument) {
    auto& runtime = Context().runtime;
    if (!token || !entry) return kInvalid;
    if (attributes) return kNotSupported;
    try {
        std::lock_guard lock(runtime.mutex);
        if (runtime.stopping) return kNotSupported;
        if (runtime.nextThread == std::numeric_limits<GuestThread>::max()) return kAgain;
        const GuestThread id = runtime.nextThread++;
        auto worker = std::make_unique<Worker>();
        worker->runtime = &runtime;
        worker->token = id;
        worker->entry = entry;
        worker->argument = argument;
        auto [it, inserted] = runtime.workers.emplace(id, std::move(worker));
        (void)inserted;
        // The table owns the worker before it can start, and no throwing operation follows.
        it->second->handle = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0, Start, it->second.get(), 0, nullptr));
        if (!it->second->handle) {
            runtime.workers.erase(it);
            return kAgain;
        }
        *token = id;
        return 0;
    } catch (const std::bad_alloc&) {
        return kAgain;
    }
}

/// Joins one worker without holding the registry lock across guest execution or destruction.
int Join(GuestThread token, void** result) {
    auto& context = Context();
    if (token == context.token) return kDeadlock;
    Worker* worker;
    {
        std::lock_guard lock(context.runtime.mutex);
        auto it = context.runtime.workers.find(token);
        if (it == context.runtime.workers.end()) return kNoThread;
        worker = it->second.get();
        if (worker->joining) return kInvalid;
        worker->joining = true;
    }
    if (WaitForSingleObject(worker->handle, INFINITE) != WAIT_OBJECT_0)
        Fatal("waiting for a managed worker failed");
    if (result) *result = worker->result;
    std::lock_guard lock(context.runtime.mutex);
    context.runtime.workers.erase(token);
    return 0;
}

/// Allocates a generation-tagged key, with the Android limit of 128 simultaneous keys.
int KeyCreate(GuestKey* token, Destructor destructor) {
    if (!token) return kInvalid;
    auto& runtime = Context().runtime;
    std::lock_guard lock(runtime.mutex);
    if (runtime.nextKey == std::numeric_limits<GuestKey>::max()) return kAgain;
    for (auto& key : runtime.keys) {
        if (key.token) continue;
        key = {runtime.nextKey++, destructor};
        *token = key.token;
        return 0;
    }
    return kAgain;
}

/// Deletes a key without calling its destructor or reviving stale per-thread values.
int KeyDelete(GuestKey token) {
    auto& runtime = Context().runtime;
    std::lock_guard lock(runtime.mutex);
    for (auto& key : runtime.keys) {
        if (key.token != token || !token) continue;
        key = {};
        return 0;
    }
    return kInvalid;
}

/// Sets one thread's value only when its generation token still names a live key.
int SetSpecific(GuestKey token, const void* pointer) {
    auto& context = Context();
    std::lock_guard lock(context.runtime.mutex);
    for (size_t i = 0; i < kKeys; i++) {
        if (context.runtime.keys[i].token != token || !token) continue;
        context.values[i] = {token, const_cast<void*>(pointer)};
        return 0;
    }
    return kInvalid;
}

/// Gets one thread's value; invalid/deleted/unset keys have no associated value.
void* GetSpecific(GuestKey token) {
    auto& context = Context();
    std::lock_guard lock(context.runtime.mutex);
    for (size_t i = 0; i < kKeys; i++) {
        if (context.runtime.keys[i].token == token && token && context.values[i].token == token)
            return context.values[i].pointer;
    }
    return nullptr;
}

/// Resolves C emulated TLS from an explicit LLVM descriptor without touching hardware TLS.
void* EmutlsAddress(const EmutlsControl* control) {
    auto& context = Context();
    if (!control || !control->size || !control->alignment ||
        (control->alignment & (control->alignment - 1)) ||
        control->size > SIZE_MAX - control->alignment)
        Fatal("invalid emulated TLS descriptor");
    for (const auto& block : context.tls)
        if (block.control == control) return block.pointer;
    void* pointer = _aligned_malloc(static_cast<size_t>(control->size),
                                   static_cast<size_t>(std::max<uint64_t>(control->alignment, sizeof(void*))));
    if (!pointer) Fatal("emulated TLS allocation failed");
    if (control->initializer) std::memcpy(pointer, control->initializer, static_cast<size_t>(control->size));
    else std::memset(pointer, 0, static_cast<size_t>(control->size));
    try {
        context.tls.push_back({control, pointer});
    } catch (const std::bad_alloc&) {
        _aligned_free(pointer);
        Fatal("emulated TLS registry allocation failed");
    }
    context.runtime.tlsBlocks++;
    return pointer;
}

// Android LP64 pthread_mutex_t / pthread_cond_t: 10 / 12 int32 words, alignment 4.
// Guest bytes contain only these private markers, never a host lock or an address.
constexpr uint32_t kMutexMarker = 0x57414d58, kConditionMarker = 0x57414356, kDestroyed = 0xffffffff;

/// Builds a guest-sized generation marker, with every reserved word explicitly zero.
template <size_t N>
std::array<uint32_t, N> Marker(uint32_t tag, uint64_t generation = 0) {
    std::array<uint32_t, N> words{};
    words[0] = tag;
    words[1] = static_cast<uint32_t>(generation);
    words[2] = static_cast<uint32_t>(generation >> 32);
    return words;
}

/// Reads opaque guest bytes without treating Android storage as a C++ host lock object.
template <size_t N>
std::array<uint32_t, N> Words(const void* object) {
    std::array<uint32_t, N> words;
    std::memcpy(words.data(), object, sizeof words);
    return words;
}

/// Writes only the guest-sized private marker, using its four-byte-aligned byte representation.
template <size_t N>
void Stamp(void* object, uint32_t tag, uint64_t generation = 0) {
    const auto words = Marker<N>(tag, generation);
    std::memcpy(object, words.data(), sizeof words);
}

/// Decodes NDK static initializer words, refusing unknown storage and unsupported features.
template <size_t N>
int StaticKind(const std::array<uint32_t, N>& words, int* kind, bool allowN2Cond = false) {
    if (std::any_of(words.begin() + 1, words.end(), [](uint32_t word) { return word != 0; })) return kInvalid;
    if constexpr (N == 10) {
        if (words[0] == 0 || words[0] == 0x4000 || words[0] == 0x8000) {
            *kind = static_cast<int>(words[0] >> 14);
            return 0;
        }
        if (words[0] == 0x2000 || words[0] == 0x6000 || words[0] == 0xa000) return kNotSupported;
    } else {
        if (words[0] == 0) { *kind = 0; return 0; }
        if (allowN2Cond && (words[0] == 1 || words[0] == 2)) { *kind = 1; return 0; }
        if (words[0] <= 3) return kNotSupported;  // process-shared and/or monotonic
    }
    return kInvalid;
}

/// Looks up or lazily creates a mutex record while the caller holds the registry lock.
MutexState* GetMutex(ThreadState& runtime, void* object, int* error) {
    if (!object) { *error = kInvalid; return nullptr; }
    const auto words = Words<10>(object);
    auto it = runtime.mutexes.find(object);
    if (it != runtime.mutexes.end()) {
        if (words == Marker<10>(kMutexMarker, it->second->generation)) return it->second.get();
        *error = kInvalid;
        return nullptr;
    }
    int kind;
    *error = StaticKind(words, &kind);
    if (*error) return nullptr;
    if (runtime.nextSync == UINT64_MAX) { *error = kAgain; return nullptr; }
    const uint64_t generation = runtime.nextSync++;
    auto mutex = std::make_unique<MutexState>(generation, kind);
    auto* result = mutex.get();
    runtime.mutexes.emplace(object, std::move(mutex));
    Stamp<10>(object, kMutexMarker, generation);
    return result;
}

/// Looks up or lazily creates an untimed condition while the registry lock is held.
ConditionState* GetCondition(ThreadState& runtime, void* object, int* error) {
    if (!object) { *error = kInvalid; return nullptr; }
    const auto words = Words<12>(object);
    auto it = runtime.conditions.find(object);
    if (it != runtime.conditions.end()) {
        if (words == Marker<12>(kConditionMarker, it->second->generation)) return it->second.get();
        *error = kInvalid;
        return nullptr;
    }
    int kind;
    const bool n2 = runtime.modules && runtime.modules->IsN2Engine();
    *error = StaticKind(words, &kind, n2);
    if (*error) return nullptr;
    if (runtime.nextSync == UINT64_MAX) { *error = kAgain; return nullptr; }
    const uint64_t generation = runtime.nextSync++;
    auto condition = std::make_unique<ConditionState>(generation, kind);
    auto* result = condition.get();
    runtime.conditions.emplace(object, std::move(condition));
    Stamp<12>(object, kConditionMarker, generation);
    return result;
}

/// Initializes a normal guest mutex, preserving storage on unsupported attributes or allocation failure.
int MutexInit(void* object, const void* attributes) {
    auto& runtime = Context().runtime;
    if (!object) return kInvalid;
    if (attributes) return kNotSupported;
    try {
        std::lock_guard lock(runtime.mutex);
        if (runtime.mutexes.contains(object)) return kBusy;
        if (runtime.nextSync == UINT64_MAX) return kAgain;
        const uint64_t generation = runtime.nextSync++;
        auto mutex = std::make_unique<MutexState>(generation, 0);
        runtime.mutexes.emplace(object, std::move(mutex));
        Stamp<10>(object, kMutexMarker, generation);
        return 0;
    } catch (const std::bad_alloc&) { return kNoMemory; }
}

/// Acquires logical mutex ownership; pins contended records until their waits end.
int Acquire(void* object, bool attempt) {
    auto& context = Context();
    try {
        std::unique_lock lock(context.runtime.mutex);
        int error = 0;
        auto* mutex = GetMutex(context.runtime, object, &error);
        if (!mutex) return error;
        if (mutex->owner == context.token && mutex->kind == 1) {
            if (mutex->depth == 2048) return kAgain;  // Bionic non-PI recursion counter: 11 bits, depth minus one.
            mutex->depth++;
            return 0;
        }
        if (attempt && mutex->owner) return kBusy;
        if (mutex->owner == context.token && mutex->kind == 2) return kDeadlock;
        mutex->contenders++;
        while (mutex->owner) mutex->available.wait(lock);
        mutex->contenders--;
        mutex->owner = context.token;
        mutex->depth = 1;
        return 0;
    } catch (const std::bad_alloc&) { return kNoMemory; }
      catch (const std::system_error&) { Fatal("host mutex wait failed"); }
}

/// Blocks for a guest mutex, preserving normal/recursive/error-checking relock semantics.
int MutexLock(void* object) { return Acquire(object, false); }

/// Attempts a guest mutex acquisition without waiting, returning Linux EBUSY when owned.
int MutexTryLock(void* object) { return Acquire(object, true); }

/// Releases one level of guest ownership and wakes a contender only on the final unlock.
int MutexUnlock(void* object) {
    auto& context = Context();
    try {
        std::lock_guard lock(context.runtime.mutex);
        int error = 0;
        auto* mutex = GetMutex(context.runtime, object, &error);
        if (!mutex) return error;
        if (mutex->owner != context.token) return kPermission;
        if (--mutex->depth == 0) {
            mutex->owner = 0;
            mutex->available.notify_one();
        }
        return 0;
    } catch (const std::bad_alloc&) { return kNoMemory; }
}

/// Destroys only an unowned mutex with no lock/condition waiters; leaves a tombstone in guest storage.
int MutexDestroy(void* object) {
    auto& runtime = Context().runtime;
    if (!object) return kInvalid;
    std::lock_guard lock(runtime.mutex);
    auto it = runtime.mutexes.find(object);
    if (it == runtime.mutexes.end()) {
        int kind;
        if (int error = StaticKind(Words<10>(object), &kind)) return error;
    } else {
        auto& mutex = *it->second;
        if (Words<10>(object) != Marker<10>(kMutexMarker, mutex.generation)) return kInvalid;
        if (mutex.owner || mutex.contenders || mutex.conditionWaiters) return kBusy;
        runtime.mutexes.erase(it);
    }
    Stamp<10>(object, kDestroyed);
    return 0;
}

/// Initializes an untimed process-private condition without sharing host lock bytes.
int ConditionInit(void* object, const void* attributes) {
    auto& runtime = Context().runtime;
    if (!object) return kInvalid;
    if (attributes) return kNotSupported;
    try {
        std::lock_guard lock(runtime.mutex);
        if (runtime.conditions.contains(object)) return kBusy;
        if (runtime.nextSync == UINT64_MAX) return kAgain;
        const uint64_t generation = runtime.nextSync++;
        auto condition = std::make_unique<ConditionState>(generation);
        runtime.conditions.emplace(object, std::move(condition));
        Stamp<12>(object, kConditionMarker, generation);
        return 0;
    } catch (const std::bad_alloc&) { return kNoMemory; }
}

/// Atomically releases a guest mutex for a condition wait and reacquires it before returning.
int ConditionWait(void* object, void* mutexObject) {
    auto& context = Context();
    try {
        std::unique_lock lock(context.runtime.mutex);
        int error = 0;
        auto* mutex = GetMutex(context.runtime, mutexObject, &error);
        if (!mutex) return error;
        if (mutex->owner != context.token) return kPermission;
        if (mutex->depth != 1) return kNotSupported;
        auto* condition = GetCondition(context.runtime, object, &error);
        if (!condition) return error;
        if (condition->waiters && condition->binding != mutex) return kInvalid;
        condition->binding = mutex;
        condition->waiters++;
        mutex->conditionWaiters++;
        mutex->owner = 0;
        mutex->depth = 0;
        mutex->available.notify_one();
        condition->changed.wait(lock);  // Predicate belongs to the guest; spurious wakes are legal.
        while (mutex->owner) mutex->available.wait(lock);
        mutex->owner = context.token;
        mutex->depth = 1;
        mutex->conditionWaiters--;
        if (--condition->waiters == 0) condition->binding = nullptr;
        return 0;
    } catch (const std::bad_alloc&) { return kNoMemory; }
      catch (const std::system_error&) { Fatal("host condition wait failed"); }
}

/// Wakes one or all current condition waiters without retaining a notification for future waits.
int Wake(void* object, bool all) {
    auto& runtime = Context().runtime;
    try {
        std::lock_guard lock(runtime.mutex);
        int error = 0;
        auto* condition = GetCondition(runtime, object, &error);
        if (!condition) return error;
        if (all) condition->changed.notify_all();
        else condition->changed.notify_one();
        return 0;
    } catch (const std::bad_alloc&) { return kNoMemory; }
}

/// Signals one guest condition waiter, if any.
int ConditionSignal(void* object) { return Wake(object, false); }

/// Broadcasts to every current guest condition waiter.
int ConditionBroadcast(void* object) { return Wake(object, true); }

/// Destroys only a condition with no waiter still referencing its host record.
int ConditionDestroy(void* object) {
    auto& runtime = Context().runtime;
    if (!object) return kInvalid;
    std::lock_guard lock(runtime.mutex);
    auto it = runtime.conditions.find(object);
    if (it == runtime.conditions.end()) {
        int kind;
        const bool n2 = runtime.modules && runtime.modules->IsN2Engine();
        if (int error = StaticKind(Words<12>(object), &kind, n2)) return error;
    } else {
        auto& condition = *it->second;
        if (Words<12>(object) != Marker<12>(kConditionMarker, condition.generation)) return kInvalid;
        if (condition.waiters) return kBusy;
        runtime.conditions.erase(it);
    }
    Stamp<12>(object, kDestroyed);
    return 0;
}

// ---- N2 Pthread, Timed Condition, Attribute, and DL Extensions ----

constexpr int kTimedOut = 110; // Linux ETIMEDOUT

struct GuestPthreadAttr {
    uint32_t flags;
    uint32_t pad;
    void* stack_base;
    uint64_t stack_size;
    uint64_t guard_size;
    int32_t sched_policy;
    int32_t sched_priority;
    char reserved[16];
};
static_assert(sizeof(GuestPthreadAttr) == 56);

int N2PthreadCreate(GuestThread* token, const GuestPthreadAttr* attrs, Entry entry, void* argument) {
    auto& runtime = Context().runtime;
    if (!token || !entry) return kInvalid;
    const bool detached = attrs && (attrs->flags & 1u) != 0;
    const unsigned stackSize = (attrs && attrs->stack_size >= 65536 && attrs->stack_size <= (64ull << 20))
        ? static_cast<unsigned>(attrs->stack_size) : 0;
    try {
        std::lock_guard lock(runtime.mutex);
        if (runtime.stopping) return kNotSupported;
        if (runtime.nextThread == std::numeric_limits<GuestThread>::max()) return kAgain;
        const GuestThread id = runtime.nextThread++;
        auto worker = std::make_unique<Worker>();
        worker->runtime = &runtime;
        worker->token = id;
        worker->entry = entry;
        worker->argument = argument;
        worker->detached = detached;
        auto [it, inserted] = runtime.workers.emplace(id, std::move(worker));
        (void)inserted;
        it->second->handle = reinterpret_cast<HANDLE>(
            _beginthreadex(nullptr, stackSize, Start, it->second.get(), 0, nullptr));
        if (!it->second->handle) {
            runtime.workers.erase(it);
            return kAgain;
        }
        *token = id;
        return 0;
    } catch (const std::bad_alloc&) {
        return kAgain;
    }
}

int N2PthreadDetach(GuestThread token) {
    auto& runtime = Context().runtime;
    std::lock_guard lock(runtime.mutex);
    auto it = runtime.workers.find(token);
    if (it == runtime.workers.end()) return kNoThread;
    if (it->second->joining || it->second->detached) return kInvalid;
    if (it->second->finished) {
        runtime.workers.erase(it);
    } else {
        it->second->detached = true;
    }
    return 0;
}

int N2MutexAttrInit(int64_t* attr) { if (!attr) return kInvalid; *attr = 0; return 0; }
int N2MutexAttrDestroy(int64_t* attr) { return attr ? 0 : kInvalid; }
int N2MutexAttrSetType(int64_t* attr, int type) {
    if (!attr || type < 0 || type > 2) return kInvalid;
    *attr = (*attr & ~3LL) | type;
    return 0;
}
int N2MutexAttrGetType(const int64_t* attr, int* type) {
    if (!attr || !type) return kInvalid;
    *type = static_cast<int>(*attr & 3LL);
    return 0;
}

int N2MutexInit(void* object, const int64_t* attrs) {
    auto& runtime = Context().runtime;
    if (!object) return kInvalid;
    const int kind = attrs ? static_cast<int>(*attrs & 3LL) : 0;
    if (kind < 0 || kind > 2) return kInvalid;
    try {
        std::lock_guard lock(runtime.mutex);
        if (runtime.mutexes.contains(object)) return kBusy;
        if (runtime.nextSync == UINT64_MAX) return kAgain;
        const uint64_t generation = runtime.nextSync++;
        runtime.mutexes.emplace(object, std::make_unique<MutexState>(generation, kind));
        Stamp<10>(object, kMutexMarker, generation);
        return 0;
    } catch (const std::bad_alloc&) { return kNoMemory; }
}

int N2CondAttrInit(int64_t* attr) { if (!attr) return kInvalid; *attr = 0; return 0; }
int N2CondAttrDestroy(int64_t* attr) { return attr ? 0 : kInvalid; }
int N2CondAttrSetClock(int64_t* attr, int clockId) {
    if (!attr || (clockId != 0 && clockId != 1)) return kInvalid;
    *attr = (*attr & ~2LL) | (clockId == 1 ? 2LL : 0LL);
    return 0;
}
int N2CondAttrGetClock(const int64_t* attr, int* clockId) {
    if (!attr || !clockId) return kInvalid;
    *clockId = (*attr & 2LL) ? 1 : 0;
    return 0;
}

int N2CondInit(void* object, const int64_t* attrs) {
    auto& runtime = Context().runtime;
    if (!object) return kInvalid;
    const int clk = (attrs && (*attrs & 2LL)) ? 1 : 0;
    try {
        std::lock_guard lock(runtime.mutex);
        if (runtime.conditions.contains(object)) return kBusy;
        if (runtime.nextSync == UINT64_MAX) return kAgain;
        const uint64_t generation = runtime.nextSync++;
        runtime.conditions.emplace(object, std::make_unique<ConditionState>(generation, clk));
        Stamp<12>(object, kConditionMarker, generation);
        return 0;
    } catch (const std::bad_alloc&) { return kNoMemory; }
}

int N2CondTimedWaitWithClock(void* object, void* mutexObject, const GuestTimespec* abstime, int overrideClock) {
    if (!abstime) return ConditionWait(object, mutexObject);
    if (abstime->nanoseconds < 0 || abstime->nanoseconds >= 1000000000LL) return kInvalid;
    auto& context = Context();
    try {
        std::unique_lock lock(context.runtime.mutex);
        int error = 0;
        auto* mutex = GetMutex(context.runtime, mutexObject, &error);
        if (!mutex) return error;
        if (mutex->owner != context.token) return kPermission;
        const uint32_t savedDepth = mutex->depth;
        auto* condition = GetCondition(context.runtime, object, &error);
        if (!condition) return error;
        if (condition->waiters && condition->binding != mutex) return kInvalid;

        const int clk = overrideClock >= 0 ? overrideClock : condition->clockId;
        GuestTimespec now{};
        ClockGetTime(clk, &now);
        int64_t diffSec = abstime->seconds - now.seconds;
        int64_t diffNs = abstime->nanoseconds - now.nanoseconds;
        if (diffNs < 0) { diffSec--; diffNs += 1000000000LL; }
        if (diffSec < 0) return kTimedOut;
        const auto waitDur = std::chrono::seconds(std::min<int64_t>(diffSec, 86400 * 365)) +
                             std::chrono::nanoseconds(diffNs);

        condition->binding = mutex;
        condition->waiters++;
        mutex->conditionWaiters++;
        mutex->owner = 0;
        mutex->depth = 0;
        mutex->available.notify_one();
        const auto cvStatus = condition->changed.wait_for(lock, waitDur);
        while (mutex->owner) mutex->available.wait(lock);
        mutex->owner = context.token;
        mutex->depth = savedDepth;
        mutex->conditionWaiters--;
        if (--condition->waiters == 0) condition->binding = nullptr;
        return cvStatus == std::cv_status::timeout ? kTimedOut : 0;
    } catch (const std::bad_alloc&) { return kNoMemory; }
      catch (const std::system_error&) { Fatal("host timed condition wait failed"); }
}

int N2CondTimedWait(void* object, void* mutexObject, const GuestTimespec* abstime) {
    return N2CondTimedWaitWithClock(object, mutexObject, abstime, -1);
}
int N2CondClockWait(void* object, void* mutexObject, int clk, const GuestTimespec* abstime) {
    if (clk != 0 && clk != 1) return kInvalid;
    return N2CondTimedWaitWithClock(object, mutexObject, abstime, clk);
}

int N2ClockGetTime(int32_t clock, GuestTimespec* result) {
    if (!result) return IoError(kFault);
    // 0=REALTIME, 5=REALTIME_COARSE; 1=MONOTONIC, 4=MONOTONIC_RAW, 6=MONOTONIC_COARSE, 7=BOOTTIME
    if (clock == 0 || clock == 5) return ClockGetTime(0, result);
    if (clock == 1 || clock == 4 || clock == 6 || clock == 7) return ClockGetTime(1, result);
    if (clock == 2 || clock == 3) { // PROCESS_CPUTIME_ID / THREAD_CPUTIME_ID
        FILETIME create{}, exit{}, kernel{}, user{};
        const BOOL ok = clock == 3
            ? GetThreadTimes(GetCurrentThread(), &create, &exit, &kernel, &user)
            : GetProcessTimes(GetCurrentProcess(), &create, &exit, &kernel, &user);
        if (!ok) return IoError(kInvalid);
        const uint64_t ticks = ((static_cast<uint64_t>(kernel.dwHighDateTime) << 32) | kernel.dwLowDateTime) +
                               ((static_cast<uint64_t>(user.dwHighDateTime) << 32) | user.dwLowDateTime);
        *result = {static_cast<int64_t>(ticks / 10000000ull), static_cast<int64_t>((ticks % 10000000ull) * 100ull)};
        return 0;
    }
    return IoError(kInvalid);
}

int N2NanoSleep(const GuestTimespec* duration, GuestTimespec* remainder) {
    if (remainder) *remainder = {0, 0};
    if (!duration) return IoError(kFault);
    if (duration->seconds < 0 || duration->nanoseconds < 0 || duration->nanoseconds >= 1000000000LL)
        return IoError(kInvalid);
    const uint64_t totalMs = static_cast<uint64_t>(std::min<int64_t>(duration->seconds, 3600)) * 1000ull +
                             static_cast<uint64_t>((duration->nanoseconds + 999999LL) / 1000000LL);
    Sleep(static_cast<DWORD>(std::min<uint64_t>(totalMs, 3600000ull)));
    return 0;
}

void* N2DlOpen(const char* name, int32_t flags) {
    auto& context = Context();
    if (!context.runtime.modules) {
        context.dlError = "dlopen: no guest module namespace";
        return nullptr;
    }
    if (!name || !*name) {
        return reinterpret_cast<void*>(static_cast<uintptr_t>(1)); // main program handle -> RTLD_DEFAULT-like
    }
    std::string_view sv(name);
    if (sv.find("..") != std::string_view::npos) {
        context.dlError = "dlopen: invalid relative path traversal";
        return nullptr;
    }
    const size_t slash = sv.find_last_of("/\\");
    const std::string_view base = slash == std::string_view::npos ? sv : sv.substr(slash + 1);
    if (flags & 4 /* RTLD_NOLOAD */) {
        const uint64_t existing = context.runtime.modules->FindLoaded(base);
        if (!existing) context.dlError = "dlopen(RTLD_NOLOAD): library not loaded";
        return reinterpret_cast<void*>(static_cast<uintptr_t>(existing));
    }
    LoadError error;
    const uint64_t handle = context.runtime.modules->Open(base, &error);
    if (!handle) {
        context.dlError = std::string(CategoryName(error.category)) + ": " + error.where + ": " + error.reason;
    }
    return reinterpret_cast<void*>(static_cast<uintptr_t>(handle));
}

void* N2DlSym(void* handle, const char* name) {
    auto& context = Context();
    if (!context.runtime.modules || !name || !*name) {
        context.dlError = "dlsym: invalid symbol name";
        return nullptr;
    }
    const auto hVal = reinterpret_cast<uintptr_t>(handle);
    void* result = nullptr;
    // On Android ARM64 LP64: RTLD_DEFAULT == 0 (nullptr), RTLD_NEXT == (void*)-1L.
    if (hVal == 0 || hVal == UINTPTR_MAX) {
        result = context.runtime.modules->FindDefault(name);
    } else {
        result = context.runtime.modules->Find(hVal, name);
        if (!result && hVal == 1) result = context.runtime.modules->FindDefault(name);
    }
    if (!result) context.dlError = std::string("undefined symbol: ") + name;
    return result;
}

int N2DlAddr(const void* addr, GuestDlInfo* info) {
    auto& context = Context();
    if (!context.runtime.modules || !addr || !info) return 0;
    return context.runtime.modules->DlAddr(reinterpret_cast<uintptr_t>(addr), info) ? 1 : 0;
}

int N2DlIteratePhdr(int (*callback)(GuestDlPhdrInfo*, uint64_t, void*), void* data) {
    auto& context = Context();
    if (!context.runtime.modules || !callback) return 0;
    return context.runtime.modules->IteratePhdr(callback, data);
}

}  // namespace

ThreadRuntime::ThreadRuntime(ModuleSet* modules, const std::wstring& androidRoot) : state_(std::make_unique<ThreadState>()) {
    if (!state_->heap) throw std::bad_alloc();
    state_->modules = modules;
    if (!androidRoot.empty()) {
        GuestFile root;
        root.handle = CreateFileW(androidRoot.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                  nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
        BY_HANDLE_FILE_INFORMATION information{};
        if (root.handle == INVALID_HANDLE_VALUE || !GetFileInformationByHandle(root.handle, &information) ||
            !(information.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || !FinalPath(root.handle, state_->androidRoot))
            throw std::runtime_error("Android file root is not an accessible directory");
        if (state_->androidRoot.back() != L'\\') state_->androidRoot += L'\\';
    }
}

ThreadRuntime::~ThreadRuntime() {
    ThreadState* expected = state_.get();
    g_n2AutoState.compare_exchange_strong(expected, nullptr, std::memory_order_acq_rel);
    // Check both before and after callbacks; never release executable code or host locks still in use.
    const auto check = [&] {
        if (!state_->workers.empty() || state_->contexts != 0)
            Fatal("runtime destroyed with unjoined workers or active contexts");
        for (const auto& [object, mutex] : state_->mutexes) {
            (void)object;
            if (mutex->owner || mutex->contenders || mutex->conditionWaiters)
                Fatal("runtime destroyed with owned or waited-on synchronization objects");
        }
        for (const auto& [object, condition] : state_->conditions) {
            (void)object;
            if (condition->waiters) Fatal("runtime destroyed with owned or waited-on synchronization objects");
        }
    };
    {
        std::lock_guard lock(state_->mutex);
        check();
        state_->stopping = true;
    }
    for (int pass = 0; pass < 4; pass++) {
        GuestThread token;
        {
            std::lock_guard lock(state_->mutex);
            if (state_->exits.empty()) break;
            if (state_->nextThread == std::numeric_limits<GuestThread>::max()) Fatal("thread token space exhausted");
            token = state_->nextThread++;
        }
        GuestThreadContext context(*state_, token);
        CxaFinalize(nullptr);
    }
    std::lock_guard lock(state_->mutex);
    if (!state_->exits.empty()) Fatal("exit callbacks rearmed during context teardown");
    check();
}

int32_t Usleep(uint32_t usec) {
    const GuestTimespec ts{static_cast<int64_t>(usec / 1000000u), static_cast<int64_t>(usec % 1000000u) * 1000};
    return NanoSleep(&ts, nullptr);
}

int32_t GuestGetPid() {
    (void)Context();
    return static_cast<int32_t>(GetCurrentProcessId());
}

int32_t GuestGetTid() {
    (void)Context();
    return static_cast<int32_t>(GetCurrentThreadId());
}

std::atomic<const char* (*)(const char*)> g_guestEnvLookup{nullptr};

const char* GuestGetenv(const char* name) {
    (void)Context();
    if (!name || !*name) return nullptr;
    if (auto fn = g_guestEnvLookup.load(std::memory_order_acquire)) return fn(name);
    return nullptr;
}

/// Guest LP64 integer parsers: Android ARM64 `long` is 64-bit, never Windows 32-bit `long`.
int32_t GuestAtoi(const char* s) {
    (void)Context();
    return s ? static_cast<int32_t>(std::strtol(s, nullptr, 10)) : 0;
}

int64_t GuestAtol(const char* s) {
    (void)Context();
    return s ? static_cast<int64_t>(std::strtoll(s, nullptr, 10)) : 0;
}

int64_t GuestStrtol(const char* s, char** endptr, int32_t base) {
    (void)Context();
    if (!s || (base != 0 && (base < 2 || base > 36))) {
        if (endptr) *endptr = const_cast<char*>(s);
        *Errno() = kInvalid;
        return 0;
    }
    errno = 0;
    const long long val = std::strtoll(s, endptr, base);
    if (errno == ERANGE) *Errno() = 34;  // Bionic ERANGE
    return static_cast<int64_t>(val);
}

uint64_t GuestStrtoul(const char* s, char** endptr, int32_t base) {
    (void)Context();
    if (!s || (base != 0 && (base < 2 || base > 36))) {
        if (endptr) *endptr = const_cast<char*>(s);
        *Errno() = kInvalid;
        return 0;
    }
    errno = 0;
    const unsigned long long val = std::strtoull(s, endptr, base);
    if (errno == ERANGE) *Errno() = 34;  // Bionic ERANGE
    return static_cast<uint64_t>(val);
}

void ThreadRuntime::SetEnvLookup(const char* (*lookup)(const char*)) {
    g_guestEnvLookup.store(lookup, std::memory_order_release);
}

std::span<const HostSymbol> ThreadRuntime::Imports() {
    static const HostSymbol imports[] = {
        {"__errno", reinterpret_cast<void*>(&Errno)},
        {"__cxa_atexit", reinterpret_cast<void*>(&CxaAtExit)},
        {"__cxa_finalize", reinterpret_cast<void*>(&CxaFinalize)},
        {"__register_atfork", reinterpret_cast<void*>(&RegisterAtFork)},
        {"pthread_create", reinterpret_cast<void*>(&Create)},
        {"pthread_join", reinterpret_cast<void*>(&Join)},
        {"pthread_self", reinterpret_cast<void*>(&Self)},
        {"pthread_equal", reinterpret_cast<void*>(&Equal)},
        {"pthread_key_create", reinterpret_cast<void*>(&KeyCreate)},
        {"pthread_key_delete", reinterpret_cast<void*>(&KeyDelete)},
        {"pthread_setspecific", reinterpret_cast<void*>(&SetSpecific)},
        {"pthread_getspecific", reinterpret_cast<void*>(&GetSpecific)},
        {"__emutls_get_address", reinterpret_cast<void*>(&EmutlsAddress)},
        {"memset", reinterpret_cast<void*>(&BytesSet)},
        {"malloc", reinterpret_cast<void*>(&Malloc)},
        {"calloc", reinterpret_cast<void*>(&Calloc)},
        {"realloc", reinterpret_cast<void*>(&Resize)},
        {"free", reinterpret_cast<void*>(&Release)},
        {"memcpy", reinterpret_cast<void*>(&BytesCopy)},
        {"memmove", reinterpret_cast<void*>(&BytesMove)},
        {"memcmp", reinterpret_cast<void*>(&BytesCompare)},
        {"memchr", reinterpret_cast<void*>(&BytesFind)},
        {"strlen", reinterpret_cast<void*>(&StringLength)},
        {"strnlen", reinterpret_cast<void*>(&StringBoundedLength)},
        {"strcmp", reinterpret_cast<void*>(&StringCompare)},
        {"strncmp", reinterpret_cast<void*>(&StringBoundedCompare)},
        {"strcpy", reinterpret_cast<void*>(&StringCopy)},
        {"strncpy", reinterpret_cast<void*>(&StringBoundedCopy)},
        {"strchr", reinterpret_cast<void*>(&StringFind)},
        {"strrchr", reinterpret_cast<void*>(&StringReverseFind)},
        {"strstr", reinterpret_cast<void*>(&StringSubstring)},
        {"strdup", reinterpret_cast<void*>(&StringDuplicate)},
        {"__android_log_write", reinterpret_cast<void*>(&AndroidLogWrite)},
        {"open", reinterpret_cast<void*>(&FileOpen)},
        {"open64", reinterpret_cast<void*>(&FileOpen)},
        {"__open_2", reinterpret_cast<void*>(&FileOpenChecked)},
        {"read", reinterpret_cast<void*>(&FileRead)},
        {"write", reinterpret_cast<void*>(&FileWrite)},
        {"close", reinterpret_cast<void*>(&FileClose)},
        {"lseek", reinterpret_cast<void*>(&FileSeek)},
        {"lseek64", reinterpret_cast<void*>(&FileSeek)},
        {"fstat", reinterpret_cast<void*>(&FileStat)},
        {"fstat64", reinterpret_cast<void*>(&FileStat)},
        {"clock_gettime", reinterpret_cast<void*>(&ClockGetTime)},
        {"nanosleep", reinterpret_cast<void*>(&NanoSleep)},
        {"usleep", reinterpret_cast<void*>(&Usleep)},
        {"getpid", reinterpret_cast<void*>(&GuestGetPid)},
        {"gettid", reinterpret_cast<void*>(&GuestGetTid)},
        {"getenv", reinterpret_cast<void*>(&GuestGetenv)},
        {"atoi", reinterpret_cast<void*>(&GuestAtoi)},
        {"atol", reinterpret_cast<void*>(&GuestAtol)},
        {"strtol", reinterpret_cast<void*>(&GuestStrtol)},
        {"strtoll", reinterpret_cast<void*>(&GuestStrtol)},
        {"strtoul", reinterpret_cast<void*>(&GuestStrtoul)},
        {"strtoull", reinterpret_cast<void*>(&GuestStrtoul)},
        {"dlopen", reinterpret_cast<void*>(&DlOpen)},
        {"dlsym", reinterpret_cast<void*>(&DlSym)},
        {"dlclose", reinterpret_cast<void*>(&DlClose)},
        {"dlerror", reinterpret_cast<void*>(&DlError)},
        {"pthread_mutex_init", reinterpret_cast<void*>(&MutexInit)},
        {"pthread_mutex_lock", reinterpret_cast<void*>(&MutexLock)},
        {"pthread_mutex_trylock", reinterpret_cast<void*>(&MutexTryLock)},
        {"pthread_mutex_unlock", reinterpret_cast<void*>(&MutexUnlock)},
        {"pthread_mutex_destroy", reinterpret_cast<void*>(&MutexDestroy)},
        {"pthread_cond_init", reinterpret_cast<void*>(&ConditionInit)},
        {"pthread_cond_wait", reinterpret_cast<void*>(&ConditionWait)},
        {"pthread_cond_signal", reinterpret_cast<void*>(&ConditionSignal)},
        {"pthread_cond_broadcast", reinterpret_cast<void*>(&ConditionBroadcast)},
        {"pthread_cond_destroy", reinterpret_cast<void*>(&ConditionDestroy)},
    };
    return imports;
}

void ThreadRuntime::EnableN2AutoBind() {
    g_n2AutoState.store(state_.get(), std::memory_order_release);
}

int32_t* GuestErrnoPtr() { return Errno(); }
void* GuestHeapMalloc(uint64_t size) { return Malloc(size); }
void* GuestHeapCalloc(uint64_t count, uint64_t size) { return Calloc(count, size); }
void* GuestHeapRealloc(void* ptr, uint64_t size) { return Resize(ptr, size); }
void GuestHeapFree(void* ptr) { Release(ptr); }

int GuestPosixMemalign(void** memptr, uint64_t alignment, uint64_t size) {
    if (!memptr || alignment < sizeof(void*) || (alignment & (alignment - 1)) != 0) return kInvalid;
    auto& runtime = Context().runtime;
    std::lock_guard lock(runtime.mutex);
    const int32_t savedErr = Context().error;
    void* p = Allocate(runtime, size, false, alignment);
    Context().error = savedErr; // POSIX: posix_memalign returns error directly without setting errno.
    if (!p) return kNoMemory;
    *memptr = p;
    return 0;
}

void* GuestAlignedAlloc(uint64_t alignment, uint64_t size) {
    if (alignment == 0 || (alignment & (alignment - 1)) != 0 || (size % alignment) != 0) {
        Context().error = kInvalid;
        return nullptr;
    }
    auto& runtime = Context().runtime;
    std::lock_guard lock(runtime.mutex);
    return Allocate(runtime, size, false, std::max<uint64_t>(alignment, sizeof(void*)));
}

uint64_t GuestMallocUsableSize(const void* ptr) {
    if (!ptr) return 0;
    auto& runtime = Context().runtime;
    std::lock_guard lock(runtime.mutex);
    auto it = runtime.allocations.find(const_cast<void*>(ptr));
    return it != runtime.allocations.end() ? it->second.usableSize : 0;
}

ModuleSet* GuestActiveModules() { return Context().runtime.modules; }
const std::wstring& GuestActiveAndroidRoot() { return Context().runtime.androidRoot; }
int32_t GuestLogWriteRaw(int32_t priority, const char* tag, const char* message) {
    if (!tag) tag = "app";
    if (!message) message = "";
    const int32_t p = std::clamp<int32_t>(priority, 2, 7);
    std::fprintf(stderr, "guest-log %d %s: %.*s\n", p, tag, 4095, message);
    std::fflush(stderr);
    return 1;
}

std::span<const HostSymbol> ThreadRuntime::N2Overrides() {
    static const HostSymbol overrides[] = {
        {"pthread_create", reinterpret_cast<void*>(&N2PthreadCreate)},
        {"pthread_detach", reinterpret_cast<void*>(&N2PthreadDetach)},
        {"pthread_mutex_init", reinterpret_cast<void*>(&N2MutexInit)},
        {"pthread_mutexattr_init", reinterpret_cast<void*>(&N2MutexAttrInit)},
        {"pthread_mutexattr_destroy", reinterpret_cast<void*>(&N2MutexAttrDestroy)},
        {"pthread_mutexattr_settype", reinterpret_cast<void*>(&N2MutexAttrSetType)},
        {"pthread_mutexattr_gettype", reinterpret_cast<void*>(&N2MutexAttrGetType)},
        {"pthread_cond_init", reinterpret_cast<void*>(&N2CondInit)},
        {"pthread_cond_timedwait", reinterpret_cast<void*>(&N2CondTimedWait)},
        {"pthread_cond_clockwait", reinterpret_cast<void*>(&N2CondClockWait)},
        {"pthread_condattr_init", reinterpret_cast<void*>(&N2CondAttrInit)},
        {"pthread_condattr_destroy", reinterpret_cast<void*>(&N2CondAttrDestroy)},
        {"pthread_condattr_setclock", reinterpret_cast<void*>(&N2CondAttrSetClock)},
        {"pthread_condattr_getclock", reinterpret_cast<void*>(&N2CondAttrGetClock)},
        {"clock_gettime", reinterpret_cast<void*>(&N2ClockGetTime)},
        {"nanosleep", reinterpret_cast<void*>(&N2NanoSleep)},
        {"posix_memalign", reinterpret_cast<void*>(&GuestPosixMemalign)},
        {"aligned_alloc", reinterpret_cast<void*>(&GuestAlignedAlloc)},
        {"malloc_usable_size", reinterpret_cast<void*>(&GuestMallocUsableSize)},
        {"dlopen", reinterpret_cast<void*>(&N2DlOpen)},
        {"dlsym", reinterpret_cast<void*>(&N2DlSym)},
        {"dladdr", reinterpret_cast<void*>(&N2DlAddr)},
        {"dl_iterate_phdr", reinterpret_cast<void*>(&N2DlIteratePhdr)},
    };
    return overrides;
}

/// Holds one thread-affine binding and keeps owned storage distinct from a borrowed worker context.
struct BindingState {
    DWORD owner = GetCurrentThreadId();
    std::unique_ptr<GuestThreadContext> owned;
    GuestThreadContext* context = nullptr;
    size_t depth = 0;
    bool closing = false;
};

ThreadBinding::ThreadBinding(ThreadState& state) : state_(std::make_unique<BindingState>()) {
    if (current) {
        if (&current->runtime != &state) Fatal("binding a different active runtime");
        state_->context = current;
        return;
    }
    GuestThread token;
    {
        std::lock_guard lock(state.mutex);
        if (state.stopping) Fatal("binding a stopping runtime");
        if (state.nextThread == std::numeric_limits<GuestThread>::max()) Fatal("thread token space exhausted");
        token = state.nextThread++;
    }
    state_->owned = std::make_unique<GuestThreadContext>(state, token, false);
    state_->context = state_->owned.get();
    t_dormantBindingContext = state_->context;
}

ThreadBinding::~ThreadBinding() { Close(); }

void ThreadBinding::Close() {
    if (state_->owner != GetCurrentThreadId()) Fatal("binding closed on a different host thread");
    if (!state_->context) return;
    if (state_->depth || (state_->owned && current)) Fatal("binding closed during an active call");
    state_->closing = true;
    if (t_dormantBindingContext == state_->context) t_dormantBindingContext = nullptr;
    if (state_->owned) {
        current = state_->context;
        state_->owned.reset();  // Key callbacks may reenter Call while context/proxy are still valid.
    }
    state_->context = nullptr;
}

void* ThreadBinding::Call(Entry entry, void* argument) {
    if (state_->owner != GetCurrentThreadId()) Fatal("binding called on a different host thread");
    auto* context = state_->context;
    if (!context || (state_->closing && current != context)) Fatal("calling a closed binding");
    if (current && current != context) Fatal("entering a different active guest context");
    /// Restores the previous activation after an ordinary or nested host-controlled call.
    struct Activation {
        BindingState& state;
        GuestThreadContext* previous = current;
        /// Activates the binding without changing its retained errno/TLS/key storage.
        explicit Activation(BindingState& binding) : state(binding) { current = state.context; state.depth++; }
        /// Deactivates only this invocation, leaving any enclosing callback's activation intact.
        ~Activation() { state.depth--; current = previous; }
    } activation(*state_);
    return entry(argument);
}

std::shared_ptr<ThreadBinding> ThreadRuntime::Bind() {
    // Weak entries do not extend runtime lifetime; expired entries are replaced on the same thread.
    thread_local std::map<ThreadState*, std::weak_ptr<ThreadBinding>> bindings;
    auto& weak = bindings[state_.get()];
    if (auto existing = weak.lock()) return existing;
    auto binding = std::shared_ptr<ThreadBinding>(new ThreadBinding(*state_));
    weak = binding;
    return binding;
}

void* ThreadRuntime::Call(Entry entry, void* argument) {
    GuestThread token;
    {
        std::lock_guard lock(state_->mutex);
        if (state_->nextThread == std::numeric_limits<GuestThread>::max()) Fatal("thread token space exhausted");
        token = state_->nextThread++;
    }
    GuestThreadContext context(*state_, token);
    return entry(argument);
}

bool ThreadRuntime::Idle() const {
    std::lock_guard lock(state_->mutex);
    return state_->workers.empty() && state_->contexts == 0 && state_->tlsBlocks == 0 &&
           state_->mutexes.empty() && state_->conditions.empty() && state_->allocations.empty() && state_->descriptors.empty() &&
           std::all_of(state_->keys.begin(), state_->keys.end(), [](const Key& key) { return key.token == 0; });
}

}  // namespace winapk::guest
