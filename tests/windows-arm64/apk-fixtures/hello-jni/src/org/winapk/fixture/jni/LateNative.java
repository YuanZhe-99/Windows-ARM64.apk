package org.winapk.fixture.jni;

/** Exercises export binding when the declaring class is defined after library initialization. */
final class LateNative {
    /** Prevents constructing the native method holder. */
    private LateNative() {}

    /** Returns 91 from a library already loaded by another class in the same app loader. */
    static native int value();
}
