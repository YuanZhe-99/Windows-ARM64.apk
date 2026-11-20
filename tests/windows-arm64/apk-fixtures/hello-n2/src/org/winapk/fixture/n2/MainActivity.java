package org.winapk.fixture.n2;

import android.app.Activity;
import android.content.res.AssetManager;
import android.graphics.Bitmap;
import android.os.Bundle;
import android.util.Log;
import android.widget.TextView;
import java.io.File;

/**
 * T16 (Profile N2 engine runtime): verifies `app_lib` ReLinker loading, `libm.so`,
 * `libjnigraphics.so`, `AAssetManager`, `ALooper`/`AChoreographer`, `epoll`/`timerfd`/`eventfd`/TCP,
 * `SYS_futex`, `STT_OBJECT` `RTLD_DEFAULT` `dlsym`, `dladdr`, `dl_iterate_phdr`, `setjmp`/`longjmp`,
 * `libOpenSLES.so`, `AAudio` refusal, and read-only `/system/etc/fonts.xml` + W^X refusals.
 */
public final class MainActivity extends Activity {
    private static final String TAG = "T16.n2";

    private static native String runChecks(AssetManager assets, Bitmap bmp);

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        TextView tv = new TextView(this);
        setContentView(tv);

        try {
            // Exercise ReLinker-style `app_lib` staging path (`Context.getDir("lib", 0)`).
            File appLib = getDir("lib", 0);
            File staged = new File(appLib, "libn2check.so");
            if (!staged.isFile()) {
                throw new IllegalStateException("missing staged app_lib: " + staged);
            }
            System.load(staged.getAbsolutePath());

            Bitmap bmp = Bitmap.createBitmap(4, 4, Bitmap.Config.ARGB_8888);
            String res = runChecks(getAssets(), bmp);
            int px = bmp.getPixel(2, 2);
            // `n2check.c` writes RGBA (0x22, 0x88, 0xCC, 0xFF) -> Android ARGB 0xFF2288CC.
            boolean bmpOk = (px == 0xFF2288CC);
            String summary = res + " bitmap=" + (bmpOk ? "PASS" : String.format("0x%08x", px));
            Log.i(TAG, summary);
            tv.setText(summary);
        } catch (Throwable t) {
            Log.e(TAG, "ERR: " + t.getMessage(), t);
            tv.setText("ERR: " + t);
        }
    }
}
