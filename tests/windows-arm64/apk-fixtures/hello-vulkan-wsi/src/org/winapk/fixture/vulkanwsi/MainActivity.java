package org.winapk.fixture.vulkanwsi;

import android.app.Activity;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

/** Exercises Android WSI using the holder of a real Activity window. */
public final class MainActivity extends Activity {
    private static final String TAG = "T13.wsi";
    private static native int open(Surface surface);
    private static native long extent();
    private static native int present(boolean resized);
    private static native boolean replace(Surface surface);

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        System.loadLibrary("guestwsi");
        final Handler main = new Handler(Looper.getMainLooper());
        SurfaceView first = new SurfaceView(this);
        first.getHolder().addCallback(new SurfaceHolder.Callback() {
            private long initial;
            private boolean stopped;
            @Override public void surfaceCreated(SurfaceHolder holder) {
                int status = open(holder.getSurface());
                if (status == 0) return;
                stopped = true;
                if (status == -1) Log.i(TAG, "VULKAN_UNAVAILABLE");
                else Log.e(TAG, "FAIL open=" + status);
            }
            @Override public void surfaceChanged(SurfaceHolder holder, int format, int w, int h) {
                if (stopped) return;
                long size = extent();
                if (size <= 0) {
                    stopped = true;
                    Log.e(TAG, "FAIL extent=" + size);
                } else if (initial == 0) {
                    int status = present(false);
                    if (status != 0) {
                        stopped = true;
                        Log.e(TAG, "FAIL initial-present=" + status);
                        return;
                    }
                    initial = size;
                    logSize("initial", size);
                } else if (size != initial) {
                    stopped = true;
                    int status = present(true);
                    if (status != 0) {
                        Log.e(TAG, "FAIL resized-present=" + status);
                        return;
                    }
                    logSize("resized", size);
                    main.post(new Runnable() {
                        @Override public void run() { installReplacement(); }
                    });
                }
            }
            @Override public void surfaceDestroyed(SurfaceHolder holder) {}
        });
        setContentView(first);
    }

    /** Checks that old WSI fails and the next attached view can create its own surface. */
    private void installReplacement() {
        SurfaceView next = new SurfaceView(this);
        next.getHolder().addCallback(new SurfaceHolder.Callback() {
            @Override public void surfaceCreated(SurfaceHolder holder) {
                if (replace(holder.getSurface())) Log.i(TAG, "PASS replacement");
                else Log.e(TAG, "FAIL replacement");
            }
            @Override public void surfaceChanged(SurfaceHolder holder, int format, int w, int h) {}
            @Override public void surfaceDestroyed(SurfaceHolder holder) {}
        });
        setContentView(next);
    }

    /** Logs client pixels as reported by the ICD. */
    private static void logSize(String phase, long size) {
        Log.i(TAG, phase + "=" + (size >>> 32) + "x" + (int)size);
    }
}
