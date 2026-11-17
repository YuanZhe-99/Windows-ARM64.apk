package org.winapk.fixture.gles;

import android.app.Activity;
import android.opengl.GLSurfaceView;
import android.os.Bundle;
import android.util.Log;
import java.nio.ByteBuffer;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

import static android.opengl.GLES20.*;

/** T10.java: a regular DEX APK, with no native libraries or host-only Java calls. */
public final class MainActivity extends Activity implements GLSurfaceView.Renderer {
    private int contexts, frames, width, height;
    private boolean unboundRejected;

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        try {
            glGetString(GL_RENDERER);
        } catch (IllegalStateException expected) {
            unboundRejected = true;
        }
        GLSurfaceView view = new GLSurfaceView(this);
        view.setEGLContextClientVersion(2);
        view.setRenderer(this);
        view.setRenderMode(GLSurfaceView.RENDERMODE_WHEN_DIRTY);
        setContentView(view);
    }

    @Override public void onSurfaceCreated(GL10 gl, EGLConfig config) {
        contexts++;
        String renderer = glGetString(GL_RENDERER);
        if (renderer == null || renderer.isEmpty() || !glGetString(GL_VERSION).startsWith("OpenGL ES "))
            throw new AssertionError("no current EGL context");
        Log.i("T10.java", "renderer=" + renderer);
    }

    @Override public void onSurfaceChanged(GL10 gl, int w, int h) {
        width = w;
        height = h;
        glViewport(0, 0, w, h);
    }

    @Override public void onDrawFrame(GL10 gl) {
        glClearColor(0.25f, 0.5f, 0.75f, 1f);
        glClear(GL_COLOR_BUFFER_BIT);
        glEnable(GL_SCISSOR_TEST);
        glScissor(width / 2, 0, width - width / 2, height);
        glClearColor(0.75f, 0.25f, 0.5f, 1f);
        glClear(GL_COLOR_BUFFER_BIT);
        glDisable(GL_SCISSOR_TEST);

        ByteBuffer pixels = ByteBuffer.allocateDirect(12);
        pixels.position(2);
        glReadPixels(width / 4, height / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        pixels.position(6);
        glReadPixels(3 * width / 4, height / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        boolean invalidRejected = false;
        try {
            glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixels.asReadOnlyBuffer());
        } catch (IllegalArgumentException expected) {
            invalidRejected = true;
        }
        if (!invalidRejected || !unboundRejected || glGetError() != GL_NO_ERROR ||
                pixels.position() != 6 || !isPixel(pixels, 2, 64, 128, 191) || !isPixel(pixels, 6, 191, 64, 128))
            throw new AssertionError("GL state, readback, or refusal contract");
        Log.i("T10.java", "PASS frame=" + (++frames) + " contexts=" + contexts + " size=" + width + "x" + height);
    }

    /** Checks one RGBA8 pixel with at most one unit of rounding in each color channel. */
    private static boolean isPixel(ByteBuffer p, int i, int r, int g, int b) {
        return Math.abs((p.get(i) & 255) - r) <= 1 &&
            Math.abs((p.get(i + 1) & 255) - g) <= 1 &&
            Math.abs((p.get(i + 2) & 255) - b) <= 1 && (p.get(i + 3) & 255) == 255;
    }
}
