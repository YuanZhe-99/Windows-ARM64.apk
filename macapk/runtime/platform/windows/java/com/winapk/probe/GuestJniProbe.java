package com.winapk.probe;

/** Supplies deterministic real-JVM values and exceptions to the T09 guest JNI fixture. */
public class GuestJniProbe {
    public final long value;
    public int count;
    public static boolean initialized;
    private static volatile Object absent;
    private static volatile int zero;

    /** Exposes FindClass initialization as an observable effect without a method lookup. */
    public static class Initialized {
        static { initialized = true; }
    }

    /** Creates an isolated loader so worker FindClass cannot pass via the system loader by accident. */
    public static ClassLoader isolatedLoader() {
        return new java.net.URLClassLoader(new java.net.URL[]{
                GuestJniProbe.class.getProtectionDomain().getCodeSource().getLocation()}, null) {
            /** Defines one valid JVM-only identifier that Java source syntax cannot express. */
            @Override protected Class<?> findClass(String name) throws ClassNotFoundException {
                if (!name.equals("com.winapk.probe.GuestJniProbe")) return super.findClass(name);
                try (java.io.InputStream input = GuestJniProbe.class.getResourceAsStream("GuestJniProbe.class")) {
                    byte[] bytes = input.readAllBytes();
                    int changed = 0;
                    // CONSTANT_Utf8 "Zbad" is shared by the declaration and invocation's NameAndType.
                    for (int i = 0; i + 7 <= bytes.length; i++)
                        if (bytes[i] == 1 && bytes[i + 1] == 0 && bytes[i + 2] == 4 &&
                                bytes[i + 3] == 'Z' && bytes[i + 4] == 'b' &&
                                bytes[i + 5] == 'a' && bytes[i + 6] == 'd') {
                            bytes[i + 3] = '1';
                            changed++;
                        }
                    if (changed != 1) throw new ClassNotFoundException("T09 name mutation count");
                    return defineClass(name, bytes, 0, bytes.length);
                } catch (java.io.IOException error) {
                    throw new ClassNotFoundException(name, error);
                }
            }
        };
    }

    /** Retains a long and promoted float/double values for constructor validation. */
    public GuestJniProbe(long value, float first, double second) {
        this.value = value + (long) first + (long) second;
    }

    /** Returns one exact value from mixed register/stack, narrow, floating and reference arguments. */
    public static double mix(boolean z, byte b, char c, short s, int i, long j, float f, double d,
                             Object object, int a1, int a2, int a3, int a4, int a5, int a6,
                             int a7, int a8, double d1, double d2, double d3, double d4,
                             double d5, double d6, double d7, double d8) {
        if (!z || b != -7 || c != 0x4e2d || s != -123 || i != 29 || j != 4294967297L ||
                f != 1.5f || d != 2.25 || object == null ||
                a1 != 1 || a2 != 2 || a3 != 3 || a4 != 4 || a5 != 5 || a6 != 6 ||
                a7 != 7 || a8 != 8 || d1 != 1 || d2 != 2 || d3 != 3 || d4 != 4 ||
                d5 != 5 || d6 != 6 || d7 != 7 || d8 != 8)
            throw new AssertionError("JNI mixed arguments");
        return 4294967297.75;
    }

    /** Returns an instance value, distinct from the subclass override. */
    public long number(int increment) { return value + increment; }

    /** Changes an observable field for void-call assertions. */
    public void add(int increment) { count += increment; }

    /** Throws an exact pending exception that the guest must observe and clear. */
    public static void fail() { throw new IllegalArgumentException("T09 expected"); }

    /** Returns an exact boolean through its JNI result category. */
    public static boolean booleanValue() { return true; }
    /** Returns an exact signed byte through its JNI result category. */
    public static byte byteValue() { return -7; }
    /** Returns an exact UTF-16 code unit through its JNI result category. */
    public static char charValue() { return '\u4e2d'; }
    /** Returns an exact signed short through its JNI result category. */
    public static short shortValue() { return -123; }
    /** Returns an exact int through its JNI result category. */
    public static int intValue() { return -123456789; }
    /** Returns an exact float through its JNI result category. */
    public static float floatValue() { return 1.5f; }

    /** Enters the guest through an explicitly registered, mixed fixed-signature PE adapter. */
    public static native double nativeMix(boolean z, byte b, char c, short s, int i, long j, float f, double d,
            Object object, int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8,
            double d1, double d2, double d3, double d4, double d5, double d6, double d7, double d8);
    /** Enters an instance native method and returns an untruncated guest long. */
    public native long nativeNumber(int increment);
    /** Enters a guest method with object/array tokens and an object result. */
    public static native String nativeEcho(String text, int[] values);
    /** Reenters Java from a guest native method, allowing another JVM-to-guest invocation. */
    public static native int nativeReenter(int depth);
    /** Returns a float through the native FP result register. */
    public static native float nativeFloat(float value);
    /** Leaves a pending Java exception in a guest native method. */
    public static native void nativeThrow();
    /** Resolves the normal JNI short name from an explicitly loaded root. */
    public static native long exportSimple(long value);
    /** Resolves the int overload's JNI long name. */
    public static native int exportOverload(int value);
    /** Resolves the long overload's distinct JNI long name. */
    public static native long exportOverload(long value);
    /** Resolves UTF-16 JNI escaping for an underscore and a non-ASCII method name. */
    public static native long export_中(long value);
    /** Resolves a native export in the root's dependency scope without calling that dependency's JNI hook. */
    public static native int exportDependency();
    /** Resolves array/object descriptor escaping in a long JNI name. */
    public static native Object exportDescriptor(int[] values, String text);
    /** Keeps the explicit JNI_OnLoad registration instead of a conflicting Java_* export. */
    public static native int loadRegistered(int value);
    /** Must be refused by the bridge if its JNI_OnLoad returned an invalid version. */
    public static native int badVersionRegistered();
    /** Becomes JVM-valid "1bad" in the isolated fixture; JNI escaping must refuse symbol lookup. */
    private static native int Zbad();

    /** Checks short/long/Unicode names, dependency scope, and registration precedence with real JVM calls. */
    public static boolean runExports() {
        if (exportSimple(-4294967297L) != -4294967298L || exportOverload(7) != 107 ||
                exportOverload(4294967297L) != 8589934593L || export_中(4294967297L) != 4294967300L ||
                exportDependency() != 3141 || loadRegistered(9) != 16 ||
                exportDescriptor(new int[]{7}, "descriptor") != "descriptor") return false;
        try {
            Zbad();
            return false;
        } catch (UnsatisfiedLinkError expected) {
            return true;
        }
    }

    /** Completes a nested JVM/guest chain with a deterministic result. */
    public static int roundtrip(int depth) { return depth == 0 ? 7 : nativeReenter(depth - 1) + 1; }

    /** Runs actual JVM-to-guest native calls outside the host-controlled guest case entry. */
    public static boolean runNatives() {
        GuestJniProbe object = new GuestJniProbe(4294967297L, 2.0f, 3.0);
        if (nativeMix(true, (byte)-7, '\u4e2d', (short)-123, 29, 4294967297L, 1.5f, 2.25, object,
                1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8) != 4294967297.75 ||
                object.nativeNumber(7) != 4294967309L || nativeFloat(1.5f) != 2.75f ||
                nativeReenter(5) != 12) return false;
        int[] values = {-3, 42, 99};
        String text = "中\u0000文";
        if (nativeEcho(text, values) != text || values[1] != 77) return false;
        try {
            nativeThrow();
            return false;
        } catch (IllegalArgumentException expected) {
            return expected.getMessage().equals("T09 native expected");
        }
    }

    /** Confirms bridge teardown removed JVM bindings, rather than leaving a callable stale PE entry. */
    public static boolean afterUnregister() {
        try {
            nativeFloat(1.5f);
            return false;
        } catch (UnsatisfiedLinkError expected) {
            return true;
        }
    }

    /** Exercises implicit Java faults on a hot method while guest executable ranges are registered. */
    private static int implicit(int value) {
        try {
            if ((value & 1) == 0) return absent.hashCode();
            return value / zero;
        } catch (NullPointerException | ArithmeticException expected) {
            return 1;
        }
    }

    /** Confirms Java/JIT-owned faults retain normal JVM handling, including after compilation. */
    public static boolean vmFaults() {
        int count = 0;
        for (int i = 0; i < 20000; i++) count += implicit(i);
        return count == 20000;
    }

    /** Returns reference and primitive array arguments without any intermediate conversion. */
    public static String arrays(String text, int[] values) {
        if (!text.equals("中\u0000文") || values.length != 3 || values[0] != -3 ||
                values[1] != 42 || values[2] != 99) throw new AssertionError("JNI references");
        return text;
    }

    /** Supplies a subclass to distinguish virtual from nonvirtual JNI calls. */
    public static class Child extends GuestJniProbe {
        /** Creates the same deterministic base state. */
        public Child(long value, float first, double second) { super(value, first, second); }
        /** Overrides the value so nonvirtual dispatch cannot accidentally pass as virtual. */
        @Override public long number(int increment) { return super.number(increment) + 100; }
    }
}
