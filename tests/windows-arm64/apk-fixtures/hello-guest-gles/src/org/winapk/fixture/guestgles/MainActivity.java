package org.winapk.fixture.guestgles;

import android.app.Activity;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

/** T11: sends a real Android holder Surface through JNI to ordinary NDK EGL/GLES calls. */
public final class MainActivity extends Activity implements SurfaceHolder.Callback {
    private static final String TAG = "T11";
    private static native int open(Surface surface);
    private static native long draw(boolean finish);
    private static native long probeExtent();
    private final Handler main = new Handler(Looper.getMainLooper());
    private final Runnable restoreCheck = new Runnable() {
        /** Checks the EGL extent on the same thread that owns its binding. */
        @Override public void run() { checkRestore(); }
    };
    private long first;
    private boolean stopped;
    private boolean minimized;
    private boolean restored;
    private long restoreDeadline;

    /** Observes the host extent until the driver draw can resume, without relying on focus events. */
    private void checkRestore() {
        if (stopped) return;
        long extent = probeExtent();
        if (extent < 0 || SystemClock.uptimeMillis() >= restoreDeadline) {
            Log.e(TAG, "FAIL minimize/restore extent=" + extent);
            stopped = true;
            return;
        }
        if (extent == 0 && !minimized) {
            minimized = true;
            Log.i(TAG, "PASS minimized=EGL_BAD_SURFACE");
        } else if (extent > 0 && minimized) {
            long drawn = draw(false);
            restored = drawn == first && extent == first;
            stopped = !restored;
            if (restored) Log.i(TAG, "PASS restored=" + describe(drawn));
            else Log.e(TAG, "FAIL restore draw=" + drawn);
            return;
        }
        main.postDelayed(restoreCheck, 100);
    }

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        System.loadLibrary("guestgles");
        SurfaceView view = new SurfaceView(this);
        view.getHolder().addCallback(this);
        setContentView(view);
    }

    @Override public void surfaceCreated(SurfaceHolder holder) {
        int result = open(holder.getSurface());
        if (result == -1) {
            Log.i(TAG, "EGL_NOT_INITIALIZED");
            stopped = true;
        } else if (result != 0) {
            Log.e(TAG, "FAIL open=" + result);
            stopped = true;
        }
    }

    @Override public void surfaceChanged(SurfaceHolder holder, int format, int w, int h) {
        if (stopped) return;
        if (first != 0 && !restored) return;
        if (first != 0 && (first >>> 32) == w && (int)first == h) return;
        long extent = draw(first != 0);
        if (extent < 0) {
            Log.e(TAG, "FAIL draw=" + extent);
            stopped = true;
            return;
        }
        if (first == 0) {
            first = extent;
            Log.i(TAG, "PASS initial=" + describe(extent));
            restoreDeadline = SystemClock.uptimeMillis() + 12000;
            main.post(restoreCheck);
        } else {
            Log.i(TAG, "PASS recreated=" + describe(extent));
            stopped = true;
        }
    }

    @Override public void surfaceDestroyed(SurfaceHolder holder) {}

    /** Formats the native bridge's measured client extent. */
    private static String describe(long extent) { return (extent >>> 32) + "x" + (int)extent; }
}
