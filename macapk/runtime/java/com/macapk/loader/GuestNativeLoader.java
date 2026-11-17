package com.macapk.loader;

import java.io.File;
import java.io.IOException;
import java.util.ArrayDeque;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;

/**
 * Routes only Windows guest DEX library calls to the restricted ELF/JNI bridge.
 * Notes: the host and JDK retain their own library loading; this is not a security sandbox.
 */
public final class GuestNativeLoader {
    private static final Set<Class<?>> classes = ConcurrentHashMap.newKeySet();
    private static final ThreadLocal<ArrayDeque<Class<?>>> binding = new ThreadLocal<>();
    private static volatile boolean loaded;

    /** Prevents construction of the host's app-loader service. */
    private GuestNativeLoader() {}

    /** Returns whether the Windows host explicitly enabled restricted guest execution. */
    public static boolean restricted() {
        return "restricted".equals(System.getProperty("winapk.guestNative"));
    }

    /** Returns Android's library naming without invoking the Windows library loader. */
    public static String mapLibraryName(String name) {
        if (name == null) throw new NullPointerException("libname");
        return "lib" + name + ".so";
    }

    /** Loads one basename from the admitted APK directory in the actual caller's loader. */
    public static void loadLibrary(String name) {
        open(mapLibraryName(name), callerLoader());
    }

    /** Loads only an absolute path to a library in the admitted APK directory. */
    public static void load(String path) {
        if (path == null) throw new NullPointerException("filename");
        File requested = new File(path);
        if ("off".equals(System.getProperty("winapk.guestNative")))
            throw new UnsatisfiedLinkError("GUEST_NATIVE_DISABLED: " + requested.getName());
        if (!requested.isAbsolute()) throw new UnsatisfiedLinkError("GUEST_ABI_UNSUPPORTED: absolute library path required");
        try {
            File file = requested.getCanonicalFile();
            File directory = new File(System.getProperty("winapk.guestLibraryDir", "")).getCanonicalFile();
            if (!directory.equals(file.getParentFile()))
                throw new UnsatisfiedLinkError("GUEST_ABI_UNSUPPORTED: library outside admitted APK directory");
            open(file.getName(), callerLoader());
        } catch (IOException error) {
            throw new UnsatisfiedLinkError("GUEST_ABI_UNSUPPORTED: cannot resolve guest library path");
        }
    }

    /** Preserves Runtime.load's null-receiver behavior while redirecting the guest call. */
    public static void runtimeLoad(Runtime runtime, String path) {
        if (runtime == null) throw new NullPointerException("runtime");
        load(path);
    }

    /** Preserves Runtime.loadLibrary's null-receiver behavior while redirecting the guest call. */
    public static void runtimeLoadLibrary(Runtime runtime, String name) {
        if (runtime == null) throw new NullPointerException("runtime");
        loadLibrary(name);
    }

    /** Records a defined app class without initializing it and binds exports once libraries exist. */
    static Class<?> defined(Class<?> type) {
        if (restricted() && type.getClassLoader() instanceof AppClassLoader && classes.add(type) && loaded)
            bind(type);
        return type;
    }

    /** Finds the DEX caller rather than attributing a load to this host-defined helper. */
    private static ClassLoader callerLoader() {
        Class<?> caller = StackWalker.getInstance(StackWalker.Option.RETAIN_CLASS_REFERENCE)
                .walk(frames -> frames.map(StackWalker.StackFrame::getDeclaringClass)
                        .filter(type -> type != GuestNativeLoader.class).findFirst().orElse(null));
        return caller == null ? null : caller.getClassLoader();
    }

    /** Validates names and mode before entering the native library lifecycle. */
    private static void open(String name, ClassLoader loader) {
        if ("off".equals(System.getProperty("winapk.guestNative")))
            throw new UnsatisfiedLinkError("GUEST_NATIVE_DISABLED: " + name);
        if (!restricted() || !(loader instanceof AppClassLoader))
            throw new UnsatisfiedLinkError("GUEST_ABI_UNSUPPORTED: guest library ClassLoader");
        if (name.length() > 128 || !name.matches("[A-Za-z0-9_+.-]+\\.so") || name.contains(".."))
            throw new UnsatisfiedLinkError("GUEST_ABI_UNSUPPORTED: portable library basename required");
        openNative(loader, name);
        loaded = true;
        for (Class<?> type : classes) bind(type);
    }

    /** Defers reflection-induced recursive definitions until the current class has been bound. */
    private static void bind(Class<?> type) {
        ArrayDeque<Class<?>> queue = binding.get();
        if (queue != null) { queue.add(type); return; }
        queue = new ArrayDeque<>();
        binding.set(queue);
        queue.add(type);
        try {
            while (!queue.isEmpty()) bindNative(queue.remove());
        } finally { binding.remove(); }
    }

    /** Opens the screened ELF graph and runs the explicit root's own JNI hook. */
    private static native void openNative(ClassLoader loader, String basename);

    /** Offers a class's native declarations to the bridge's JNI short/long export resolver. */
    private static native void bindNative(Class<?> type);
}
