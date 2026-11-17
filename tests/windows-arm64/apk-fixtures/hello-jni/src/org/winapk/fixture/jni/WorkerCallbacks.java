package org.winapk.fixture.jni;

/** Loads for the first time through JNI FindClass on an attached guest worker. */
final class WorkerCallbacks {
    /** Prevents constructing the callback holder. */
    private WorkerCallbacks() {}

    /** Reenters the already bound guest method from the worker's Java callback. */
    static int value() { return MainActivity.nativeValue(3); }
}
