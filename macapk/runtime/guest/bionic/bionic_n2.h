// bionic_n2.h -- Profile N2 Bionic (`libc.so`, `libdl.so`, `libm.so`) engine runtime extensions:
// complete C99/POSIX math, virtual FD table (`epoll`, `poll`, `eventfd`, `timerfd`, `pipe2`,
// `inotify`, Winsock2 sockets, files & directories), `FILE*` streams, `mmap` (W^X), `rwlock`/`sem`,
// system properties, `getauxval`, locale/ctype/wchar, and guest-ABI formatting/setjmp helper.
#pragma once

#include "../loader/elf_loader.h"
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace winapk::guest {

/// Owns the loaded `libwinapk-bionic-n2.so` guest helper and N2 system library tables.
class N2BionicExtension {
public:
    ~N2BionicExtension();

    /// Loads `libwinapk-bionic-n2.so` and upgrades `libraries` (`libc.so`, `libdl.so`, `liblog.so`)
    /// plus appends `libm.so` for profile N2.
    static std::unique_ptr<N2BionicExtension> Install(
        const std::wstring& n2HelperPath,
        const std::wstring& androidRoot,
        std::vector<BuiltinLibrary>& libraries,
        std::vector<HostSymbol>& combinedImports,
        LoadError* error);

private:
    std::unique_ptr<Module> helper_;
};

/// Queries readiness of a virtual N2 fd (`ALOOPER_EVENT_INPUT = 1`, `ALOOPER_EVENT_OUTPUT = 2`,
/// `ALOOPER_EVENT_ERROR = 4`, `ALOOPER_EVENT_HANGUP = 8`) and returns any armed `timerfd` deadline
/// (monotonic ns, or `UINT64_MAX` if none) so `ALooper_pollOnce` sleeps until exact expiration.
uint32_t QueryN2FdReady(int fd, uint64_t* nextDeadlineMonoNs = nullptr);

/// Registers/unregisters a wake callback invoked whenever any N2 `eventfd`, `pipe`, or `timerfd`
/// transitions to readable or is rearmed (`ALooper` integration).
uint64_t RegisterN2WakeListener(std::function<void()> callback);
void UnregisterN2WakeListener(uint64_t listenerId);

/// Stores a system property pushed from Java (`SystemProperties`) or queried by `__system_property_get`.
void SetGuestSystemProperty(const char* name, const char* value);

}  // namespace winapk::guest
