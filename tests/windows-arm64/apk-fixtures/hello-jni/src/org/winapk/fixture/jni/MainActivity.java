package org.winapk.fixture.jni;

import android.app.Activity;
import android.os.Bundle;
import android.util.Log;
import android.widget.TextView;

/**
 * Loads libhello.so and calls it, logging the result under the tag "JNI" as
 * "native.value=<n>" or "native.error=<message>" (scripts/windows/test.ps1 asserts on it).
 */
public class MainActivity extends Activity {
    private static final String TAG = "JNI";

    /** Returns 42 + x, from libhello.so. */
    static native int nativeValue(int x);

    /** Shows what loading and calling the library did. */
    @Override
    protected void onCreate(Bundle saved) {
        super.onCreate(saved);
        String result;
        try {
            System.loadLibrary("hello");
            result = "native.value=" + nativeValue(1);
        } catch (UnsatisfiedLinkError e) {
            result = "native.error=" + e.getMessage();
        }
        Log.i(TAG, result);
        TextView t = new TextView(this);
        t.setText(result);
        setContentView(t);
    }
}
