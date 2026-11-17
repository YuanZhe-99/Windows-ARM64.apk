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

    /** Returns the real JNI_OnLoad invocation count through its explicit native registration. */
    static native int loadCount();

    /** Shows what loading and calling the library did. */
    @Override
    protected void onCreate(Bundle saved) {
        super.onCreate(saved);
        String result;
        try {
            System.loadLibrary("hello");
            int value = nativeValue(1);
            System.loadLibrary("hello");
            Runtime.getRuntime().loadLibrary("hello");
            String path = getApplicationInfo().nativeLibraryDir + "/" + System.mapLibraryName("hello");
            System.load(path);
            Runtime.getRuntime().load(path);
            boolean refused = false;
            try { System.load(new java.io.File(getFilesDir(), "libhello.so").getAbsolutePath()); }
            catch (UnsatisfiedLinkError expected) {
                refused = expected.getMessage().startsWith("GUEST_ABI_UNSUPPORTED:");
            }
            boolean missing = false;
            try { System.loadLibrary("not-shipped"); }
            catch (UnsatisfiedLinkError expected) {
                missing = expected.getMessage().startsWith("GUEST_ABI_UNSUPPORTED:");
            }
            boolean late = LateNative.value() == 91;
            boolean ok = value == 43 && loadCount() == 1 && refused && missing && late;
            result = "native.value=" + value + " checks=" + (ok ? "PASS" : "FAIL");
        } catch (UnsatisfiedLinkError e) {
            result = "native.error=" + e.getMessage();
        }
        Log.i(TAG, result);
        TextView t = new TextView(this);
        t.setText(result);
        setContentView(t);
    }
}
