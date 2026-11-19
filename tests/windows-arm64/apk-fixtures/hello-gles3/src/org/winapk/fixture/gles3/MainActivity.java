package org.winapk.fixture.gles3;

import android.app.Activity;
import android.graphics.SurfaceTexture;
import android.opengl.EGL14;
import android.opengl.EGLDisplay;
import android.opengl.EGLSurface;
import android.opengl.GLES20;
import android.opengl.GLES30;
import android.opengl.GLSurfaceView;
import android.os.Bundle;
import android.util.Log;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;
import java.util.concurrent.atomic.AtomicReference;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

/**
 * GPU-P2 fixture: verifies GLES 3.0 (#version 300 es, VAO, UBO, glTexStorage2D, FBO MRT
 * glDrawBuffers, glDrawArraysInstanced) plus an independent worker-thread EGL14 + SurfaceTexture
 * (MacApkGlWindow) session sharing the refcounted ANGLE display.
 */
public final class MainActivity extends Activity implements GLSurfaceView.Renderer {
    private int program, vao, vbo, ubo, fbo, tex;
    private int width, height, frames;
    private boolean egl14WorkerOk;

    private static final String VS =
        "#version 300 es\n" +
        "layout(location = 0) in vec2 aPos;\n" +
        "out vec2 vUv;\n" +
        "void main() {\n" +
        "    float xOffset = float(gl_InstanceID) * 1.0;\n" +
        "    gl_Position = vec4(aPos.x + xOffset, aPos.y, 0.0, 1.0);\n" +
        "    vUv = aPos * 0.5 + 0.5;\n" +
        "}\n";

    private static final String FS =
        "#version 300 es\n" +
        "precision mediump float;\n" +
        "layout(std140) uniform ColorBlock {\n" +
        "    vec4 uColor;\n" +
        "};\n" +
        "in vec2 vUv;\n" +
        "layout(location = 0) out vec4 fragColor;\n" +
        "void main() {\n" +
        "    fragColor = uColor;\n" +
        "}\n";

    @Override
    public void onCreate(Bundle state) {
        super.onCreate(state);
        GLSurfaceView view = new GLSurfaceView(this);
        view.setEGLContextClientVersion(3);
        view.setEGLConfigChooser(8, 8, 8, 8, 16, 8);
        view.setRenderer(this);
        view.setRenderMode(GLSurfaceView.RENDERMODE_WHEN_DIRTY);
        setContentView(view);
    }

    private static int compile(int type, String src) {
        int s = GLES30.glCreateShader(type);
        GLES30.glShaderSource(s, src);
        GLES30.glCompileShader(s);
        int[] ok = new int[1];
        GLES30.glGetShaderiv(s, GLES30.GL_COMPILE_STATUS, ok, 0);
        if (ok[0] == 0) {
            throw new AssertionError("shader compile failed: " + GLES30.glGetShaderInfoLog(s));
        }
        return s;
    }

    private static void verifyWorkerEgl14SurfaceTexture() throws Exception {
        final AtomicReference<Throwable> err = new AtomicReference<>();
        final AtomicReference<Boolean> frameArrived = new AtomicReference<>(Boolean.FALSE);
        final android.os.HandlerThread cbThread = new android.os.HandlerThread("st-cb");
        cbThread.start();
        final android.os.Handler cbHandler = new android.os.Handler(cbThread.getLooper());
        final java.util.concurrent.CountDownLatch latch = new java.util.concurrent.CountDownLatch(1);
        Thread worker = new Thread(new Runnable() {
            @Override public void run() {
            SurfaceTexture st = null;
            EGLDisplay dpy = EGL14.EGL_NO_DISPLAY;
            android.opengl.EGLContext ctx = EGL14.EGL_NO_CONTEXT;
            EGLSurface surf = EGL14.EGL_NO_SURFACE;
            try {
                st = new SurfaceTexture(0);
                st.setDefaultBufferSize(16, 16);
                st.setOnFrameAvailableListener(new SurfaceTexture.OnFrameAvailableListener() {
                    @Override public void onFrameAvailable(SurfaceTexture t) {
                        frameArrived.set(Boolean.TRUE);
                        latch.countDown();
                    }
                }, cbHandler);

                dpy = EGL14.eglGetDisplay(EGL14.EGL_DEFAULT_DISPLAY);
                if (dpy == EGL14.EGL_NO_DISPLAY) throw new AssertionError("eglGetDisplay failed");
                int[] ver = new int[2];
                if (!EGL14.eglInitialize(dpy, ver, 0, ver, 1)) {
                    throw new AssertionError("eglInitialize failed: " + EGL14.eglGetError());
                }
                // Include EGL_RECORDABLE_ANDROID (0x3142) to verify Android attribute filtering.
                int[] cfgAttrs = {
                    EGL14.EGL_RED_SIZE, 8,
                    EGL14.EGL_GREEN_SIZE, 8,
                    EGL14.EGL_BLUE_SIZE, 8,
                    EGL14.EGL_ALPHA_SIZE, 8,
                    EGL14.EGL_RENDERABLE_TYPE, EGL14.EGL_OPENGL_ES2_BIT,
                    0x3142, 1,
                    EGL14.EGL_NONE
                };
                android.opengl.EGLConfig[] cfgs = new android.opengl.EGLConfig[1];
                int[] num = new int[1];
                if (!EGL14.eglChooseConfig(dpy, cfgAttrs, 0, cfgs, 0, 1, num, 0) || num[0] < 1) {
                    throw new AssertionError("eglChooseConfig failed: " + EGL14.eglGetError());
                }
                int[] ctxAttrs = {EGL14.EGL_CONTEXT_CLIENT_VERSION, 2, EGL14.EGL_NONE};
                ctx = EGL14.eglCreateContext(dpy, cfgs[0], EGL14.EGL_NO_CONTEXT, ctxAttrs, 0);
                if (ctx == EGL14.EGL_NO_CONTEXT) throw new AssertionError("eglCreateContext failed");

                int[] surfAttrs = {EGL14.EGL_NONE};
                surf = EGL14.eglCreateWindowSurface(dpy, cfgs[0], st, surfAttrs, 0);
                if (surf == EGL14.EGL_NO_SURFACE) {
                    throw new AssertionError("eglCreateWindowSurface(SurfaceTexture) failed: " + EGL14.eglGetError());
                }
                if (!EGL14.eglMakeCurrent(dpy, surf, surf, ctx)) {
                    throw new AssertionError("eglMakeCurrent failed");
                }
                GLES20.glViewport(0, 0, 16, 16);
                GLES20.glClearColor(220f / 255f, 90f / 255f, 40f / 255f, 1f);
                GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT);

                ByteBuffer px = ByteBuffer.allocateDirect(4);
                GLES20.glReadPixels(8, 8, 1, 1, GLES20.GL_RGBA, GLES20.GL_UNSIGNED_BYTE, px);
                int r = px.get(0) & 0xff, g = px.get(1) & 0xff, b = px.get(2) & 0xff;
                if (Math.abs(r - 220) > 1 || Math.abs(g - 90) > 1 || Math.abs(b - 40) > 1) {
                    throw new AssertionError("worker EGL14 pixel mismatch: " + r + "," + g + "," + b);
                }
                if (!EGL14.eglSwapBuffers(dpy, surf)) {
                    throw new AssertionError("eglSwapBuffers(SurfaceTexture) failed");
                }
                latch.await(5, java.util.concurrent.TimeUnit.SECONDS);
                EGL14.eglMakeCurrent(dpy, EGL14.EGL_NO_SURFACE, EGL14.EGL_NO_SURFACE, EGL14.EGL_NO_CONTEXT);
            } catch (Throwable t) {
                err.set(t);
            } finally {
                if (dpy != EGL14.EGL_NO_DISPLAY) {
                    if (surf != EGL14.EGL_NO_SURFACE) EGL14.eglDestroySurface(dpy, surf);
                    if (ctx != EGL14.EGL_NO_CONTEXT) EGL14.eglDestroyContext(dpy, ctx);
                    // Must decrement refcount without tearing down the main GLSurfaceView's display!
                    EGL14.eglTerminate(dpy);
                }
                if (st != null) st.release();
            }
            }
        }, "egl14-st-worker");
        worker.start();
        worker.join(10000);
        cbThread.quitSafely();
        if (worker.isAlive()) throw new AssertionError("worker timed out");
        if (err.get() != null) throw new AssertionError("worker failed", err.get());
        if (!Boolean.TRUE.equals(frameArrived.get())) {
            throw new AssertionError("SurfaceTexture did not receive queued frame");
        }
    }

    @Override
    public void onSurfaceCreated(GL10 gl, EGLConfig config) {
        String ver = GLES30.glGetString(GLES30.GL_VERSION);
        if (ver == null || !ver.startsWith("OpenGL ES 3.")) {
            throw new AssertionError("expected OpenGL ES 3.x context, got: " + ver);
        }
        try {
            verifyWorkerEgl14SurfaceTexture();
            egl14WorkerOk = true;
        } catch (Exception e) {
            throw new RuntimeException(e);
        }

        int vs = compile(GLES30.GL_VERTEX_SHADER, VS);
        int fs = compile(GLES30.GL_FRAGMENT_SHADER, FS);
        program = GLES30.glCreateProgram();
        GLES30.glAttachShader(program, vs);
        GLES30.glAttachShader(program, fs);
        GLES30.glLinkProgram(program);
        int[] linked = new int[1];
        GLES30.glGetProgramiv(program, GLES30.GL_LINK_STATUS, linked, 0);
        if (linked[0] == 0) {
            throw new AssertionError("link failed: " + GLES30.glGetProgramInfoLog(program));
        }
        GLES30.glDeleteShader(vs);
        GLES30.glDeleteShader(fs);

        int blockIdx = GLES30.glGetUniformBlockIndex(program, "ColorBlock");
        GLES30.glUniformBlockBinding(program, blockIdx, 0);

        int[] ids = new int[3];
        GLES30.glGenVertexArrays(1, ids, 0);
        vao = ids[0];
        GLES30.glGenBuffers(2, ids, 1);
        vbo = ids[1];
        ubo = ids[2];

        // Left half [-1..0] drawn in 2 instances (instance 0: [-1..0], instance 1: [0..1]) covering full quad!
        float[] quad = {
            -1f, -1f,  0f, -1f,  -1f,  1f,
             0f, -1f,  0f,  1f,  -1f,  1f
        };
        FloatBuffer vb = ByteBuffer.allocateDirect(quad.length * 4).order(ByteOrder.nativeOrder()).asFloatBuffer();
        vb.put(quad).position(0);

        GLES30.glBindVertexArray(vao);
        GLES30.glBindBuffer(GLES30.GL_ARRAY_BUFFER, vbo);
        GLES30.glBufferData(GLES30.GL_ARRAY_BUFFER, quad.length * 4, vb, GLES30.GL_STATIC_DRAW);
        GLES30.glEnableVertexAttribArray(0);
        GLES30.glVertexAttribPointer(0, 2, GLES30.GL_FLOAT, false, 0, 0);
        GLES30.glBindVertexArray(0);

        float[] col = {32f / 255f, 160f / 255f, 224f / 255f, 1.0f};
        FloatBuffer ub = ByteBuffer.allocateDirect(16).order(ByteOrder.nativeOrder()).asFloatBuffer();
        ub.put(col).position(0);
        GLES30.glBindBuffer(GLES30.GL_UNIFORM_BUFFER, ubo);
        GLES30.glBufferData(GLES30.GL_UNIFORM_BUFFER, 16, ub, GLES30.GL_STATIC_DRAW);
        GLES30.glBindBufferBase(GLES30.GL_UNIFORM_BUFFER, 0, ubo);

        int[] fboIds = new int[2];
        GLES30.glGenTextures(1, fboIds, 0);
        tex = fboIds[0];
        GLES30.glGenFramebuffers(1, fboIds, 1);
        fbo = fboIds[1];
        GLES30.glBindTexture(GLES30.GL_TEXTURE_2D, tex);
        GLES30.glTexStorage2D(GLES30.GL_TEXTURE_2D, 1, GLES30.GL_RGBA8, 32, 32);
        GLES30.glBindFramebuffer(GLES30.GL_FRAMEBUFFER, fbo);
        GLES30.glFramebufferTexture2D(GLES30.GL_FRAMEBUFFER, GLES30.GL_COLOR_ATTACHMENT0, GLES30.GL_TEXTURE_2D, tex, 0);
        GLES30.glDrawBuffers(1, new int[] {GLES30.GL_COLOR_ATTACHMENT0}, 0);
        if (GLES30.glCheckFramebufferStatus(GLES30.GL_FRAMEBUFFER) != GLES30.GL_FRAMEBUFFER_COMPLETE) {
            throw new AssertionError("GLES3 FBO incomplete");
        }
        GLES30.glBindFramebuffer(GLES30.GL_FRAMEBUFFER, 0);
    }

    @Override
    public void onSurfaceChanged(GL10 gl, int w, int h) {
        width = w;
        height = h;
    }

    @Override
    public void onDrawFrame(GL10 gl) {
        GLES30.glUseProgram(program);
        GLES30.glBindVertexArray(vao);

        // 1. Render into GLES3 immutable-storage FBO and blit to default framebuffer.
        GLES30.glBindFramebuffer(GLES30.GL_FRAMEBUFFER, fbo);
        GLES30.glViewport(0, 0, 32, 32);
        GLES30.glClearColor(0f, 0f, 0f, 1f);
        GLES30.glClear(GLES30.GL_COLOR_BUFFER_BIT);
        GLES30.glDrawArraysInstanced(GLES30.GL_TRIANGLES, 0, 6, 2);

        GLES30.glBindFramebuffer(GLES30.GL_READ_FRAMEBUFFER, fbo);
        GLES30.glBindFramebuffer(GLES30.GL_DRAW_FRAMEBUFFER, 0);
        GLES30.glBlitFramebuffer(0, 0, 32, 32, 0, 0, width, height, GLES30.GL_COLOR_BUFFER_BIT, GLES30.GL_NEAREST);
        GLES30.glBindFramebuffer(GLES30.GL_FRAMEBUFFER, 0);

        ByteBuffer px = ByteBuffer.allocateDirect(8);
        GLES30.glReadPixels(width / 4, height / 2, 1, 1, GLES30.GL_RGBA, GLES30.GL_UNSIGNED_BYTE, px);
        px.position(4);
        GLES30.glReadPixels(3 * width / 4, height / 2, 1, 1, GLES30.GL_RGBA, GLES30.GL_UNSIGNED_BYTE, px);
        int err = GLES30.glGetError();
        if (!egl14WorkerOk || err != GLES30.GL_NO_ERROR || !match(px, 0) || !match(px, 4)) {
            throw new AssertionError("GLES3/EGL14 check failed err=" + err);
        }
        Log.i("T10.gles3", "PASS frame=" + (++frames) + " egl14Worker=true size=" + width + "x" + height);
    }

    private static boolean match(ByteBuffer p, int i) {
        return Math.abs((p.get(i) & 0xff) - 32) <= 1 &&
               Math.abs((p.get(i + 1) & 0xff) - 160) <= 1 &&
               Math.abs((p.get(i + 2) & 0xff) - 224) <= 1 &&
               (p.get(i + 3) & 0xff) == 255;
    }
}
