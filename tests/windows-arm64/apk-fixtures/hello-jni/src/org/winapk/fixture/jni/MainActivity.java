package org.winapk.fixture.jni;

import android.app.Activity;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.view.ViewTreeObserver;
import android.widget.Button;
import android.widget.LinearLayout;
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

    /** Calls Java with strings, arrays, floating/long values and a nested native call. */
    static native String nativeMatrix(String text, int[] values, double real, long wide);
    /** Checks mixed fixed-signature long/float/double parameters and a double result. */
    static native double nativeNumbers(long wide, float small, double real);
    /** Checks an instance receiver and a 64-bit integer result. */
    native long instanceValue(long value);
    /** Propagates a guest-thrown Java exception to this Activity. */
    static native void nativeThrowBack();
    /** Returns the native method exported only by a DT_NEEDED dependency. */
    static native int dependencyValue();
    /** Observes rejected-version hook side effects through the successful root. */
    static native int badVersionCount();
    /** Starts, attaches, callbacks from, detaches and joins one guest-created worker. */
    static native int nativeWorker();
    /** Attempts an unsupported JNI monitor operation and must terminate the runtime process. */
    static native void nativeUnsupported();

    /** Mutates the passed array and reenters a guest export while called from native code. */
    static String matrixCallback(String text, int[] values, double real, long wide) {
        if (!"JNI".equals(text) || values.length != 1 || values[0] != 7 ||
                real != 3.25 || wide != 4294967297L || nativeValue(2) != 44)
            throw new AssertionError("T09 callback arguments");
        values[0] = 99;
        return text + ":nested";
    }

    /** Supplies a pending exception for guest code to identify, clear and continue. */
    static void throwExpected() { throw new IllegalStateException("T09 callback expected"); }

    /** Asserts the remaining G3 APK values without replacing them with a window-only smoke check. */
    private boolean matrix() {
        int[] values = {7};
        boolean calls = "JNI:nested".equals(nativeMatrix("JNI", values, 3.25, 4294967297L)) && values[0] == 99;
        boolean numbers = nativeNumbers(4294967297L, 1.5f, -2.25) == 9.75 && instanceValue(4294967297L) == 4294967298L;
        boolean exception = false;
        try { nativeThrowBack(); }
        catch (IllegalArgumentException expected) { exception = "T09 expected".equals(expected.getMessage()); }
        boolean dependencies = dependencyValue() == 3141;
        System.loadLibrary("dependency");
        System.loadLibrary("dependency");
        dependencies = dependencies && dependencyValue() == 3142 && loadCount() == 1;
        boolean version = badVersionCount() == 0;
        for (int i = 0; i < 2; i++) {
            try { System.loadLibrary("version"); version = false; }
            catch (UnsatisfiedLinkError expected) {
                version = version && expected.getMessage().startsWith("GUEST_ABI_UNSUPPORTED:") &&
                        expected.getMessage().contains("JNI_OnLoad version");
            }
        }
        version = version && badVersionCount() == 1;
        boolean worker = nativeWorker() == 45;
        Log.i(TAG, "matrix.calls=" + calls + " numbers=" + numbers + " exception=" + exception +
                " dependency=" + dependencies + " version=" + version + " worker=" + worker);
        return calls && numbers && exception && dependencies && version && worker;
    }

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
            result = "native.value=" + value + " checks=" + (ok ? "PASS" : "FAIL") +
                    " matrix=" + (matrix() ? "PASS" : "FAIL");
        } catch (UnsatisfiedLinkError e) {
            result = "native.error=" + e.getMessage();
        }
        Log.i(TAG, result);
        TextView t = new TextView(this);
        t.setText(result);
        LinearLayout content = new LinearLayout(this);
        content.setOrientation(LinearLayout.VERTICAL);
        content.addView(t);
        final Button unsupported = new Button(this);
        unsupported.setText("Unsupported JNI (expected process failure)");
        unsupported.setOnClickListener(new View.OnClickListener() {
            /** Exercises one real unsupported guest JNI call from normal Activity input. */
            @Override public void onClick(View view) { nativeUnsupported(); }
        });
        content.addView(unsupported);
        content.getViewTreeObserver().addOnGlobalLayoutListener(new ViewTreeObserver.OnGlobalLayoutListener() {
            /** Publishes actual pixel bounds so the test can activate the refusal through window input. */
            @Override public void onGlobalLayout() {
                int[] at = new int[2];
                unsupported.getLocationInWindow(at);
                Log.i(TAG, "refusal.bounds=" + at[0] + "," + at[1] + "," +
                        (at[0] + unsupported.getWidth()) + "," + (at[1] + unsupported.getHeight()));
            }
        });
        setContentView(content);
    }
}
