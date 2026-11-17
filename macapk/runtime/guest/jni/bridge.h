#pragma once

#include "../loader/elf_loader.h"
#include <jni.h>

namespace winapk::guest {

struct JniState;

/// Owns audited guest JNI tables and the Android-ABI varargs helper for one ClassLoader.
/// Notes: host-only API; destroy after all guest workers and before destroying the JVM.
class JniBridge {
public:
    /// Loads the screened helper and retains loader as a global reference.
    /// Returns: nullptr with loader error or a pending JVM exception on failure.
    static std::unique_ptr<JniBridge> Load(JNIEnv* env, jobject loader,
                                         const std::wstring& helper, LoadError* error);
    /// Releases this thread's proxy and the loader; active proxies on other threads are fatal.
    ~JniBridge();

    /// Returns this thread's guest environment, borrowing env without taking attachment ownership.
    /// Notes: subsequent calls on the same thread reuse the proxy; never accepts a foreign JNIEnv.
    void* Environment(JNIEnv* env);
    /// Returns the private guest JavaVM object, never the host JavaVM address.
    void* Vm() const;
    /// Counts live proxies, including borrowed JVM threads, for lifecycle assertions.
    size_t Environments() const;

private:
    /// Creates the host owner before any guest table becomes reachable.
    JniBridge();
    std::unique_ptr<JniState> state_;
};

}  // namespace winapk::guest
