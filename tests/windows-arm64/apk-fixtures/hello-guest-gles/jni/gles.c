#include <android/native_window_jni.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES3/gl3.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>

static EGLDisplay display;
static EGLConfig config;
static EGLContext context;
static EGLSurface surface;
static ANativeWindow* native;
static GLuint program;
static PFNGLCLEARCOLORPROC clear_color;

typedef struct {
    GLuint src_tex;
    EGLImageKHR image;
    int status;
} WorkerUpload;

/** Compiles one ordinary ES2 stage, or returns zero on a real driver failure. */
static GLuint shader(GLenum stage, const char* source) {
    GLuint id = glCreateShader(stage);
    GLint ok = 0;
    glShaderSource(id, 1, &source, 0);
    glCompileShader(id);
    glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
    if (ok) return id;
    glDeleteShader(id);
    return 0;
}

/**
 * IO worker pthread: creates a shared EGLContext, binds both a 4x4 Pbuffer and surfaceless mode,
 * uploads a texture, wraps it in an EGLImageKHR, and releases its thread binding.
 */
static void* io_upload_worker(void* arg) {
    WorkerUpload* up = (WorkerUpload*)arg;
    const EGLint es2[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    const EGLint pb_attrs[] = { EGL_WIDTH, 4, EGL_HEIGHT, 4, EGL_NONE };
    PFNEGLCREATEIMAGEKHRPROC create_img = (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
    if (!create_img) { up->status = 101; return 0; }

    EGLContext worker_ctx = eglCreateContext(display, config, context, es2);
    EGLSurface pbuf = eglCreatePbufferSurface(display, config, pb_attrs);
    if (!worker_ctx || !pbuf) { up->status = 102; return 0; }
    if (!eglMakeCurrent(display, pbuf, pbuf, worker_ctx)) { up->status = 103; return 0; }

    /* Also verify KHR_surfaceless_context on the worker thread. */
    if (!eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, worker_ctx)) { up->status = 104; return 0; }

    const GLubyte shared_px[4] = {32, 192, 96, 255};
    glGenTextures(1, &up->src_tex);
    glBindTexture(GL_TEXTURE_2D, up->src_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, shared_px);
    glFinish();

    up->image = create_img(display, worker_ctx, EGL_GL_TEXTURE_2D_KHR,
                           (EGLClientBuffer)(uintptr_t)up->src_tex, 0);
    if (up->image == EGL_NO_IMAGE_KHR || glGetError() != GL_NO_ERROR) { up->status = 105; return 0; }

    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroySurface(display, pbuf);
    eglDestroyContext(display, worker_ctx);
    eglReleaseThread();
    up->status = 0;
    return 0;
}

/** Initializes guest EGL 1.5 / GLES 2+3, multi-thread shared contexts, Pbuffer and EGLImageKHR. */
JNIEXPORT jint JNICALL Java_org_winapk_fixture_guestgles_MainActivity_open(JNIEnv* env, jclass type, jobject holder) {
    (void)type;
    const EGLint attributes[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
        0x3142 /* EGL_RECORDABLE_ANDROID */, 1,
        EGL_NONE
    };
    const EGLint fixed_attrs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
        0x3142 /* EGL_RECORDABLE_ANDROID */, 1,
        EGL_NONE
    };
    (void)attributes;
    const EGLint es2[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    const EGLint es3[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
    GLint configs = 0, major = 0, minor = 0;

    glClear(GL_COLOR_BUFFER_BIT);
    if (glGetError() != GL_INVALID_OPERATION || glGetError() != GL_NO_ERROR) return 1;

    display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (!eglInitialize(display, &major, &minor))
        return eglGetError() == EGL_NOT_INITIALIZED ? -1 : 2;
    if (major != 1 || minor != 5 || !eglChooseConfig(display, fixed_attrs, &config, 1, &configs) || configs != 1) return 3;

    const char* ext = eglQueryString(display, EGL_EXTENSIONS);
    if (!ext || !strstr(ext, "EGL_KHR_surfaceless_context") || !strstr(ext, "EGL_KHR_image_base") ||
        eglGetProcAddress("eglCreatePlatformWindowSurface") ||
        eglGetProcAddress("glWinApkUnknownExtension")) return 4;

    clear_color = (PFNGLCLEARCOLORPROC)eglGetProcAddress("glClearColor");
    if (!clear_color || clear_color != &glClearColor) return 5;

    if (eglCreateWindowSurface(display, config, (EGLNativeWindowType)(uintptr_t)UINTPTR_MAX, 0) != EGL_NO_SURFACE ||
        eglGetError() != EGL_BAD_NATIVE_WINDOW) return 7;

    context = eglCreateContext(display, config, EGL_NO_CONTEXT, es2);
    native = ANativeWindow_fromSurface(env, holder);
    surface = eglCreateWindowSurface(display, config, native, 0);
    if (!context || !native || !surface || !eglMakeCurrent(display, surface, surface, context)) return 8;
    if (eglDestroySurface(display, surface) || eglGetError() != EGL_BAD_ACCESS) return 9;

    const GLubyte* version = glGetString(GL_VERSION);
    const GLubyte* extensions = glGetString(GL_EXTENSIONS);
    const GLubyte* renderer = glGetString(GL_RENDERER);
    if (!version || strncmp((const char*)version, "OpenGL ES ", 10) || !extensions || !*extensions ||
        !renderer || !strstr((const char*)renderer, "ANGLE")) return 10;

    /* Verify ES 3.0 availability matches hardware backend (supported on D3D11, refused on Dozen). */
    if (strstr((const char*)renderer, "Direct3D11") || strstr((const char*)renderer, "D3D11")) {
        EGLContext ctx3 = eglCreateContext(display, config, EGL_NO_CONTEXT, es3);
        if (ctx3 == EGL_NO_CONTEXT) return 6;
        if (!eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx3)) return 6;
        const char* v3 = (const char*)glGetString(GL_VERSION);
        GLuint vao = 0;
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
        glDeleteVertexArrays(1, &vao);
        if (!v3 || strncmp(v3, "OpenGL ES 3.", 12) || glGetError() != GL_NO_ERROR) return 6;
        eglMakeCurrent(display, surface, surface, context);
        eglDestroyContext(display, ctx3);
    } else if (strstr((const char*)renderer, "Dozen")) {
        if (eglCreateContext(display, config, EGL_NO_CONTEXT, es3) != EGL_NO_CONTEXT ||
            eglGetError() != EGL_BAD_MATCH || eglGetError() != EGL_SUCCESS) return 6;
    }

    /* Verify multi-thread shared context + Pbuffer + Surfaceless + EGLImageKHR across pthreads. */
    WorkerUpload up = {0, EGL_NO_IMAGE_KHR, -1};
    pthread_t th;
    if (pthread_create(&th, 0, &io_upload_worker, &up) != 0) return 15;
    pthread_join(th, 0);
    if (up.status != 0) return up.status;

    PFNGLEGLIMAGETARGETTEXTURE2DOESPROC img_tex =
        (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");
    PFNEGLDESTROYIMAGEKHRPROC destroy_img =
        (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
    if (!img_tex || !destroy_img) return 16;

    GLuint dst_tex = 0, img_fbo = 0;
    GLubyte img_read[4] = {0};
    glGenTextures(1, &dst_tex);
    glBindTexture(GL_TEXTURE_2D, dst_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    img_tex(GL_TEXTURE_2D, (GLeglImageOES)up.image);
    glGenFramebuffers(1, &img_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, img_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, dst_tex, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) return 17;
    glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, img_read);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &img_fbo);
    glDeleteTextures(1, &dst_tex);
    glDeleteTextures(1, &up.src_tex);
    destroy_img(display, up.image);
    const GLubyte want_shared[4] = {32, 192, 96, 255};
    if (memcmp(img_read, want_shared, 4) != 0 || glGetError() != GL_NO_ERROR) return 18;

    GLuint v = shader(GL_VERTEX_SHADER, "attribute vec2 p;void main(){gl_Position=vec4(p,0.,1.);}");
    GLuint f = shader(GL_FRAGMENT_SHADER, "precision mediump float;void main(){gl_FragColor=vec4(1.,0.,0.,0.5);}");
    if (!v || !f) return 11;
    program = glCreateProgram();
    glAttachShader(program, v);
    glAttachShader(program, f);
    glBindAttribLocation(program, 0, "p");
    glLinkProgram(program);
    glDeleteShader(v);
    glDeleteShader(f);
    GLint linked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) return 12;

    const GLubyte texel[] = {64, 128, 192, 255};
    GLubyte copy[4] = {0};
    GLuint texture, fbo;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, texel);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) return 13;
    glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, copy);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &texture);
    return !memcmp(copy, texel, sizeof copy) && glGetError() == GL_NO_ERROR ? 0 : 14;
}

/** Draws and presents one blended triangle with a deterministic center-pixel readback. */
static int render(EGLint w, EGLint h) {
    static const GLfloat vertices[] = {-1.f, -1.f, 3.f, -1.f, -1.f, 3.f};
    glViewport(0, 0, w, h);
    clear_color(0.f, 0.f, 1.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(program);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, vertices);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glDisableVertexAttribArray(0);
    glDisable(GL_BLEND);
    GLubyte pixel[4] = {0};
    glReadPixels(w/2, h/2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    if (pixel[0] < 127 || pixel[0] > 129 || pixel[1] || pixel[2] < 126 || pixel[2] > 129 ||
        pixel[3] < 190 || pixel[3] > 192 || glGetError() != GL_NO_ERROR) return -4;
    return eglSwapBuffers(display, surface) ? 0 : -eglGetError();
}

/** Measures the live extent or verifies that swap is refused while it is zero. */
JNIEXPORT jlong JNICALL Java_org_winapk_fixture_guestgles_MainActivity_probeExtent(JNIEnv* env, jclass type) {
    (void)env;
    (void)type;
    EGLint w, h;
    if (!eglQuerySurface(display, surface, EGL_WIDTH, &w) || !eglQuerySurface(display, surface, EGL_HEIGHT, &h)) return -1;
    if (w && h) return ((jlong)w << 32) | (uint32_t)h;
    if (w || h || ANativeWindow_getWidth(native) || ANativeWindow_getHeight(native) ||
        eglSwapBuffers(display, surface) || eglGetError() != EGL_BAD_SURFACE) return -7;
    return 0;
}

/** Draws on the current context, optionally replacing the resized surface and tearing down. */
JNIEXPORT jlong JNICALL Java_org_winapk_fixture_guestgles_MainActivity_draw(JNIEnv* env, jclass type, jboolean finish) {
    (void)env;
    (void)type;
    EGLint w, h;
    if (!eglQuerySurface(display, surface, EGL_WIDTH, &w) || !eglQuerySurface(display, surface, EGL_HEIGHT, &h) ||
        w != ANativeWindow_getWidth(native) || h != ANativeWindow_getHeight(native)) return -1;
    int status = render(w, h);
    if (status) return status;
    if (finish) {
        EGLSurface old = surface;
        if (!eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT) ||
            !eglDestroySurface(display, old) || eglSwapBuffers(display, old) ||
            eglGetError() != EGL_BAD_SURFACE) return -2;
        surface = eglCreateWindowSurface(display, config, native, 0);
        if (!surface || surface == old || !eglMakeCurrent(display, surface, surface, context)) return -3;
        status = render(w, h);
        if (status) return status - 10;
        glDeleteProgram(program);
        if (!eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT) ||
            !eglDestroySurface(display, surface) || !eglDestroyContext(display, context) ||
            !eglTerminate(display)) return -6;
        ANativeWindow_release(native);
    }
    return ((jlong)w << 32) | (uint32_t)h;
}
