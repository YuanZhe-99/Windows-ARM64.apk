package org.winapk.fixture.surface;

import android.app.Activity;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

/** T10.surface uses only Android APIs and a normally linked API-30 guest ELF. */
public final class MainActivity extends Activity {
    private static final String TAG = "T10.surface";
    private static native long open(Surface surface, Surface unrelated);
    private static native long dimensions();
    private static native boolean released(Surface surface);
    private static native boolean replace(Surface oldSurface, Surface newSurface);

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        System.loadLibrary("surfacecheck");
        final Handler main = new Handler(Looper.getMainLooper());
        SurfaceView first = new SurfaceView(this);
        Surface unrelated = new SurfaceView(this).getHolder().getSurface();
        first.getHolder().addCallback(new SurfaceHolder.Callback() {
            private long initial;
            private boolean changed;

            @Override public void surfaceCreated(SurfaceHolder holder) {
                if (open(holder.getSurface(), unrelated) == 0) throw new AssertionError("native acquisition");
            }

            @Override public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
                long extent = dimensions();
                if (extent <= 0) throw new AssertionError("native extent");
                if (initial == 0) {
                    initial = extent;
                    logExtent("initial", extent);
                } else if (!changed && extent != initial) {
                    changed = true;
                    logExtent("resized", extent);
                    Surface old = holder.getSurface();
                    old.release();
                    if (!released(old)) throw new AssertionError("Java release destroyed a native reference or allowed conversion");
                    main.post(new Runnable() {
                        @Override public void run() { installReplacement(old); }
                    });
                }
            }

            @Override public void surfaceDestroyed(SurfaceHolder holder) {}
        });
        setContentView(first);
    }

    /** Replaces the attached view on the Activity looper and inspects both attachment generations. */
    private void installReplacement(Surface old) {
        SurfaceView next = new SurfaceView(this);
        next.getHolder().addCallback(new SurfaceHolder.Callback() {
            @Override public void surfaceCreated(SurfaceHolder holder) {
                if (!replace(old, holder.getSurface())) throw new AssertionError("attachment ownership");
                Log.i(TAG, "PASS release/replace/stale");
            }
            @Override public void surfaceChanged(SurfaceHolder holder, int format, int w, int h) {}
            @Override public void surfaceDestroyed(SurfaceHolder holder) {}
        });
        setContentView(next);
    }

    /** Logs the actual guest dimensions for comparison with the Win32 client area. */
    private static void logExtent(String phase, long extent) {
        Log.i(TAG, phase + "=" + (extent >>> 32) + "x" + (int)extent);
    }
}
