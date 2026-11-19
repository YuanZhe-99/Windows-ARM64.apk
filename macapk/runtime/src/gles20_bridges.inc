// gles20_bridges.inc -- android.opengl.GLES20 + GLUtils JNI bridges and tables.
// Included by native_methods.cpp (macOS) and platform/windows/src/bridge/graphics.cpp (Windows).
// ---------- android.opengl.GLES20 ----------
//
// Direct calls to ANGLE's libGLESv2 symbols. We linked it (see
// CMakeLists.txt with MACAPK_HAS_ANGLE), so glClearColor() etc. just
// resolve at link time. When MACAPK_HAS_ANGLE is undefined every
// method here is a no-op so the build still produces a runnable
// binary; apps just won't see pixels.

#ifdef MACAPK_HAS_ANGLE
#define GL_CALL(stmt) do { stmt; } while (0)
#else
#define GL_CALL(stmt) do { (void)0; } while (0)
#endif

// ---- whole-state ----
// Gated GL-draw trace (MACAPK_TRACE_GLDRAW=1). Diagnostic only: lets
// us see whether the Java GLES20->ANGLE path (used by libGDX-style
// Java renderers like Vector Pinball's GL20Renderer, as opposed to the
// native-libgame GL path used by cocos) actually issues viewport/clear/
// draw calls and whether they raise GL errors. Throttled so it doesn't
// dominate a 60fps log.
static inline bool gldraw_trace() {
    static int v = -1;
    if (v < 0) { const char* e = std::getenv("MACAPK_TRACE_GLDRAW"); v = (e && e[0] == '1') ? 1 : 0; }
    return v == 1;
}
// MACAPK_TRACE_GLVIEWPORT_STACK=1: dump the JAVA stack the FIRST few times a
// DEGENERATE viewport (zero width or height) is applied. A zero viewport clips
// every subsequent draw to nothing, so an app that alternates a real viewport
// with (0,0,0,0) renders half its frames into the void -- but glViewport is
// native, so there is nothing on the Java side to put a breakpoint or a
// printStackTrace in. This walks Thread.currentThread().getStackTrace() over
// JNI to name the CALLER, which is the difference between knowing the viewport
// is zero and knowing WHY. Diagnostic only, off by default, engine-agnostic.
static inline bool glviewport_stack_trace() {
    static int v = -1;
    if (v < 0) { const char* e = std::getenv("MACAPK_TRACE_GLVIEWPORT_STACK"); v = (e && e[0] == '1') ? 1 : 0; }
    return v == 1;
}
static void DumpJavaStack(JNIEnv* env, const char* why) {
    if (!env || env->ExceptionCheck()) return;   // never dump with an exception pending
    jclass cThread = env->FindClass("java/lang/Thread");
    if (!cThread) { env->ExceptionClear(); return; }
    jmethodID mCur = env->GetStaticMethodID(cThread, "currentThread", "()Ljava/lang/Thread;");
    jmethodID mGet = mCur ? env->GetMethodID(cThread, "getStackTrace", "()[Ljava/lang/StackTraceElement;") : nullptr;
    if (!mGet) { env->ExceptionClear(); return; }
    jobject thr = env->CallStaticObjectMethod(cThread, mCur);
    jobjectArray frames = thr ? (jobjectArray) env->CallObjectMethod(thr, mGet) : nullptr;
    if (!frames) { env->ExceptionClear(); return; }
    jclass cSTE = env->FindClass("java/lang/StackTraceElement");
    jmethodID mStr = cSTE ? env->GetMethodID(cSTE, "toString", "()Ljava/lang/String;") : nullptr;
    jsize n = env->GetArrayLength(frames);
    std::fprintf(stderr, "[GLVIEWPORT-STACK] %s\n", why);
    for (jsize i = 0; i < n && i < 24; ++i) {
        jobject f = env->GetObjectArrayElement(frames, i);
        if (!f) continue;
        jstring s = mStr ? (jstring) env->CallObjectMethod(f, mStr) : nullptr;
        if (s) {
            const char* c = env->GetStringUTFChars(s, nullptr);
            if (c) { std::fprintf(stderr, "    at %s\n", c); env->ReleaseStringUTFChars(s, c); }
            env->DeleteLocalRef(s);
        }
        env->DeleteLocalRef(f);
    }
    std::fflush(stderr);
    env->ExceptionClear();
}
void GLES20_glViewport(JNIEnv* env, jclass, jint x, jint y, jint w, jint h) {
#ifdef MACAPK_HAS_ANGLE
    glViewport(x, y, w, h);
    if (gldraw_trace()) std::fprintf(stderr, "[GLDRAW] glViewport(%d,%d,%d,%d)\n", x, y, w, h);
    if ((w <= 0 || h <= 0) && glviewport_stack_trace()) {
        static long zn = 0;
        if (++zn <= 4) {
            char why[128];
            std::snprintf(why, sizeof why, "degenerate glViewport(%d,%d,%d,%d) #%ld", x, y, w, h, zn);
            DumpJavaStack(env, why);
        }
    }
#else
    (void) env;
#endif
}
void GLES20_glClearColor(JNIEnv*, jclass, jfloat r, jfloat g, jfloat b, jfloat a) {
#ifdef MACAPK_HAS_ANGLE
    glClearColor(r, g, b, a);
    if (gldraw_trace()) std::fprintf(stderr, "[GLDRAW] glClearColor(%.3f,%.3f,%.3f,%.3f)\n", r, g, b, a);
#endif
}
void GLES20_glClear(JNIEnv*, jclass, jint mask) {
#ifdef MACAPK_HAS_ANGLE
    glClear((GLbitfield) mask);
    if (gldraw_trace()) {
        static long n = 0;
        if (++n <= 3 || (n % 600) == 0)
            std::fprintf(stderr, "[GLDRAW] glClear(0x%x) #%ld err=0x%x\n", mask, n, glGetError());
    }
#endif
}
void GLES20_glEnable(JNIEnv*, jclass, jint cap) {
#ifdef MACAPK_HAS_ANGLE
    glEnable(cap);
#endif
}
void GLES20_glDisable(JNIEnv*, jclass, jint cap) {
#ifdef MACAPK_HAS_ANGLE
    glDisable(cap);
#endif
}
void GLES20_glBlendFunc(JNIEnv*, jclass, jint sf, jint df) {
#ifdef MACAPK_HAS_ANGLE
    glBlendFunc(sf, df);
#endif
}
void GLES20_glPixelStorei(JNIEnv*, jclass, jint pname, jint param) {
#ifdef MACAPK_HAS_ANGLE
    glPixelStorei(pname, param);
#endif
}
void GLES20_glActiveTexture(JNIEnv*, jclass, jint unit) {
#ifdef MACAPK_HAS_ANGLE
    glActiveTexture(unit);
#endif
}
void GLES20_glBindTexture(JNIEnv*, jclass, jint target, jint tex) {
#ifdef MACAPK_HAS_ANGLE
    glBindTexture(target, tex);
#endif
}
void GLES20_glTexParameteri(JNIEnv*, jclass, jint target, jint pname, jint param) {
#ifdef MACAPK_HAS_ANGLE
    glTexParameteri(target, pname, param);
#endif
}
void GLES20_glBindBuffer(JNIEnv*, jclass, jint target, jint buf) {
#ifdef MACAPK_HAS_ANGLE
    glBindBuffer(target, buf);
#endif
}
void GLES20_glDrawArrays(JNIEnv*, jclass, jint mode, jint first, jint count) {
#ifdef MACAPK_HAS_ANGLE
    glDrawArrays(mode, first, count);
    if (gldraw_trace()) {
        static long n = 0;
        if (++n <= 5 || (n % 600) == 0)
            std::fprintf(stderr, "[GLDRAW] glDrawArrays(mode=0x%x first=%d count=%d) #%ld err=0x%x\n",
                         mode, first, count, n, glGetError());
    }
#endif
}
// MACAPK_TRACE_GLDRAW=1: one-shot snapshot of the ENTIRE draw state the first
// few times a draw call is issued. A draw that reports err=0 while nothing
// lands on screen cannot be diagnosed from the draw call's own arguments: the
// cause is always in the state AROUND it (an unlinked program, an attribute
// array pointed at no buffer, a disabled color write, an index type the
// context does not support). Dumping it once per draw-site is cheaper than a
// dozen speculative round-trips, and it is engine-agnostic.
#ifdef MACAPK_HAS_ANGLE
static void gldraw_probe_fbo(const char* who) {
    GLint vp[4] = {0,0,0,0};
    glGetIntegerv(GL_VIEWPORT, vp);
    int w = vp[2], h = vp[3];
    if (w <= 0 || h <= 0 || (long) w * h > 16L * 1024 * 1024) return;
    std::vector<unsigned char> px((size_t) w * h * 4, 0);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    GLenum e = glGetError();
    long nonblack = 0, nonzero_alpha = 0;
    unsigned rmax = 0, gmax = 0, bmax = 0, amax = 0;
    for (size_t i = 0; i + 3 < px.size(); i += 4) {
        unsigned r = px[i], g = px[i+1], b = px[i+2], a = px[i+3];
        if (r || g || b) ++nonblack;
        if (a) ++nonzero_alpha;
        if (r > rmax) rmax = r;
        if (g > gmax) gmax = g;
        if (b > bmax) bmax = b;
        if (a > amax) amax = a;
    }
    std::fprintf(stderr,
        "[GLPROBE] %s %dx%d readback err=0x%x nonblack=%ld/%ld alpha!=0=%ld max=(%u,%u,%u,%u)\n",
        who, w, h, e, nonblack, (long)((size_t) w * h), nonzero_alpha,
        rmax, gmax, bmax, amax);
}

static void gldraw_dump_state(const char* who, GLenum index_type) {
    GLint prog = 0, vp[4] = {0,0,0,0}, ab = 0, eab = 0, fbo = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &prog);
    glGetIntegerv(GL_VIEWPORT, vp);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &ab);
    glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &eab);
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fbo);
    GLint linked = 0, nattr = 0;
    if (prog) {
        glGetProgramiv(prog, GL_LINK_STATUS, &linked);
        glGetProgramiv(prog, GL_ACTIVE_ATTRIBUTES, &nattr);
    }
    GLboolean cmask[4] = {0,0,0,0};
    glGetBooleanv(GL_COLOR_WRITEMASK, cmask);
    std::fprintf(stderr,
        "[GLSTATE] %s prog=%d linked=%d active_attrs=%d fbo=%d vp=[%d,%d,%d,%d]\n"
        "[GLSTATE]   ARRAY_BUFFER=%d ELEMENT_ARRAY_BUFFER=%d colormask=%d%d%d%d\n"
        "[GLSTATE]   blend=%d depth_test=%d cull=%d scissor=%d dither=%d\n",
        who, prog, linked, nattr, fbo, vp[0], vp[1], vp[2], vp[3],
        ab, eab, cmask[0], cmask[1], cmask[2], cmask[3],
        (int) glIsEnabled(GL_BLEND), (int) glIsEnabled(GL_DEPTH_TEST),
        (int) glIsEnabled(GL_CULL_FACE), (int) glIsEnabled(GL_SCISSOR_TEST),
        (int) glIsEnabled(GL_DITHER));
    GLint bsrc = 0, bdst = 0, beq = 0;
    glGetIntegerv(GL_BLEND_SRC_RGB, &bsrc);
    glGetIntegerv(GL_BLEND_DST_RGB, &bdst);
    glGetIntegerv(GL_BLEND_EQUATION_RGB, &beq);
    std::fprintf(stderr, "[GLSTATE]   blendfunc src=0x%x dst=0x%x eq=0x%x\n", bsrc, bdst, beq);
    GLint abytes = 0, ebytes = 0;
    if (ab)  glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &abytes);
    if (eab) glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &ebytes);
    std::fprintf(stderr, "[GLSTATE]   ARRAY_BUFFER bytes=%d  ELEMENT_ARRAY_BUFFER bytes=%d\n",
                 abytes, ebytes);
    // Per-attribute: enabled, its VBO binding, size/type/stride and the
    // pointer/offset. An attribute enabled with buffer=0 and pointer=0 reads
    // address 0 and is the classic silent-blank shape.
    GLint maxattr = 0;
    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &maxattr);
    if (maxattr > 16) maxattr = 16;
    for (GLint i = 0; i < maxattr; ++i) {
        GLint en = 0;
        glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &en);
        if (!en) continue;
        GLint bufb = 0, sz = 0, ty = 0, st = 0, norm = 0;
        void* ptr = nullptr;
        glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &bufb);
        glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_SIZE, &sz);
        glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_TYPE, &ty);
        glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_STRIDE, &st);
        glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, &norm);
        glGetVertexAttribPointerv(i, GL_VERTEX_ATTRIB_ARRAY_POINTER, &ptr);
        std::fprintf(stderr,
            "[GLSTATE]   attr[%d] buf=%d size=%d type=0x%x norm=%d stride=%d ptr=%ld\n",
            (int) i, bufb, sz, ty, norm, st, (long)(intptr_t) ptr);
    }
    // GL_UNSIGNED_INT indices are NOT core in GLES 2.0: they need
    // GL_OES_element_index_uint. A context without it rejects the draw with
    // GL_INVALID_ENUM, which looks identical to "drew nothing".
    if (index_type == GL_UNSIGNED_INT) {
        const char* ext = (const char*) glGetString(GL_EXTENSIONS);
        const char* ver = (const char*) glGetString(GL_VERSION);
        std::fprintf(stderr,
            "[GLSTATE]   index_type=GL_UNSIGNED_INT version=\"%s\" OES_element_index_uint=%s\n",
            ver ? ver : "?",
            (ext && std::strstr(ext, "GL_OES_element_index_uint")) ? "YES" : "NO");
    }
}
#endif

void GLES20_glDrawElements(JNIEnv*, jclass, jint mode, jint count, jint type, jint offset) {
#ifdef MACAPK_HAS_ANGLE
    if (gldraw_trace()) {
        static long sn = 0;
        if (++sn <= 4) gldraw_dump_state("glDrawElements", (GLenum) type);
    }
    glDrawElements(mode, count, type, reinterpret_cast<const void*>(static_cast<intptr_t>(offset)));
    if (gldraw_trace()) {
        static long n = 0;
        if (++n <= 5 || (n % 600) == 0)
            std::fprintf(stderr, "[GLDRAW] glDrawElements(mode=0x%x count=%d type=0x%x off=%d) #%ld err=0x%x\n",
                         mode, count, type, offset, n, glGetError());
        // THE DISCRIMINATOR. A draw that reports err=0 while the screen stays
        // blank has exactly two possible failure sites: the draw did not write
        // the framebuffer, or something downstream discarded what it wrote.
        // Reading the bound framebuffer back HERE, immediately after the draw
        // and before any present/compositing, separates them in one run.
        static long pn = 0;
        ++pn;
        if (pn >= 21 && pn <= 24) gldraw_probe_fbo("post-glDrawElements");
    }
#endif
}
void GLES20_glEnableVertexAttribArray(JNIEnv*, jclass, jint i) {
#ifdef MACAPK_HAS_ANGLE
    glEnableVertexAttribArray(i);
#endif
}
void GLES20_glDisableVertexAttribArray(JNIEnv*, jclass, jint i) {
#ifdef MACAPK_HAS_ANGLE
    glDisableVertexAttribArray(i);
#endif
}
void GLES20_glVertexAttribPointerOffset(JNIEnv*, jclass,
        jint indx, jint size, jint type, jboolean norm, jint stride, jint offset) {
#ifdef MACAPK_HAS_ANGLE
    glVertexAttribPointer(indx, size, type, norm ? GL_TRUE : GL_FALSE, stride,
                           reinterpret_cast<const void*>(static_cast<intptr_t>(offset)));
#endif
}

// ---- shader / program ----
jint GLES20_glCreateShader(JNIEnv*, jclass, jint type) {
#ifdef MACAPK_HAS_ANGLE
    return (jint) glCreateShader(type);
#else
    return 0;
#endif
}
void GLES20_glShaderSource(JNIEnv* env, jclass, jint shader, jstring src) {
#ifdef MACAPK_HAS_ANGLE
    if (!src) return;
    const char* s = env->GetStringUTFChars(src, nullptr);
    GLint len = (GLint) std::strlen(s);
    glShaderSource(shader, 1, &s, &len);
    env->ReleaseStringUTFChars(src, s);
#endif
}
void GLES20_glCompileShader(JNIEnv*, jclass, jint shader) {
#ifdef MACAPK_HAS_ANGLE
    glCompileShader(shader);
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
        std::string buf(len > 0 ? len : 1, '\0');
        if (len > 0) glGetShaderInfoLog(shader, len, nullptr, buf.data());
        std::fprintf(stderr, "[E] gles: shader %d compile FAILED: %s\n", (int) shader, buf.c_str());
    }
#endif
}
void GLES20_glGetShaderiv(JNIEnv* env, jclass, jint shader, jint pname,
                          jintArray params, jint offset) {
#ifdef MACAPK_HAS_ANGLE
    if (!params) return;
    GLint v = 0;
    glGetShaderiv(shader, pname, &v);
    env->SetIntArrayRegion(params, offset, 1, reinterpret_cast<const jint*>(&v));
#endif
}
jstring GLES20_glGetShaderInfoLog(JNIEnv* env, jclass, jint shader) {
#ifdef MACAPK_HAS_ANGLE
    GLint len = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
    if (len <= 0) return env->NewStringUTF("");
    std::string buf;
    buf.resize(len);
    glGetShaderInfoLog(shader, len, nullptr, buf.data());
    return env->NewStringUTF(buf.c_str());
#else
    return env->NewStringUTF("");
#endif
}
jint GLES20_glCreateProgram(JNIEnv*, jclass) {
#ifdef MACAPK_HAS_ANGLE
    return (jint) glCreateProgram();
#else
    return 0;
#endif
}
void GLES20_glBindAttribLocation(JNIEnv* env, jclass, jint program, jint index, jstring name_j) {
#ifdef MACAPK_HAS_ANGLE
    if (!name_j) return;
    const char* s = env->GetStringUTFChars(name_j, nullptr);
    glBindAttribLocation(program, index, s);
    env->ReleaseStringUTFChars(name_j, s);
#endif
}
void GLES20_glAttachShader(JNIEnv*, jclass, jint program, jint shader) {
#ifdef MACAPK_HAS_ANGLE
    glAttachShader(program, shader);
#endif
}
void GLES20_glLinkProgram(JNIEnv*, jclass, jint program) {
#ifdef MACAPK_HAS_ANGLE
    glLinkProgram(program);
    GLint ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);
        std::string buf(len > 0 ? len : 1, '\0');
        if (len > 0) glGetProgramInfoLog(program, len, nullptr, buf.data());
        std::fprintf(stderr, "[E] gles: program %d link FAILED: %s\n", (int) program, buf.c_str());
    }
#endif
}
void GLES20_glGetProgramiv(JNIEnv* env, jclass, jint program, jint pname,
                           jintArray params, jint offset) {
#ifdef MACAPK_HAS_ANGLE
    if (!params) return;
    GLint v = 0;
    glGetProgramiv(program, pname, &v);
    jint jv = (jint) v;
    env->SetIntArrayRegion(params, offset, 1, &jv);
    if (pname == GL_LINK_STATUS && v == 0) {
        std::fprintf(stderr, "[E] gles: glGetProgramiv(LINK_STATUS) returned 0 for program %d\n", (int) program);
    }
#endif
}
jstring GLES20_glGetProgramInfoLog(JNIEnv* env, jclass, jint program) {
#ifdef MACAPK_HAS_ANGLE
    GLint len = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);
    if (len <= 0) return env->NewStringUTF("");
    std::string buf;
    buf.resize(len);
    glGetProgramInfoLog(program, len, nullptr, buf.data());
    return env->NewStringUTF(buf.c_str());
#else
    return env->NewStringUTF("");
#endif
}
void GLES20_glUseProgram(JNIEnv*, jclass, jint program) {
#ifdef MACAPK_HAS_ANGLE
    glUseProgram(program);
#endif
}
jint GLES20_glGetUniformLocation(JNIEnv* env, jclass, jint program, jstring name) {
#ifdef MACAPK_HAS_ANGLE
    if (!name) return -1;
    const char* s = env->GetStringUTFChars(name, nullptr);
    GLint loc = glGetUniformLocation(program, s);
    env->ReleaseStringUTFChars(name, s);
    return loc;
#else
    return -1;
#endif
}
jint GLES20_glGetAttribLocation(JNIEnv* env, jclass, jint program, jstring name) {
#ifdef MACAPK_HAS_ANGLE
    if (!name) return -1;
    const char* s = env->GetStringUTFChars(name, nullptr);
    GLint loc = glGetAttribLocation(program, s);
    env->ReleaseStringUTFChars(name, s);
    return loc;
#else
    return -1;
#endif
}
void GLES20_glUniform1i(JNIEnv*, jclass, jint loc, jint x) {
#ifdef MACAPK_HAS_ANGLE
    glUniform1i(loc, x);
#endif
}
void GLES20_glUniform1f(JNIEnv*, jclass, jint loc, jfloat x) {
#ifdef MACAPK_HAS_ANGLE
    glUniform1f(loc, x);
#endif
}
void GLES20_glUniform2f(JNIEnv*, jclass, jint loc, jfloat x, jfloat y) {
#ifdef MACAPK_HAS_ANGLE
    glUniform2f(loc, x, y);
#endif
}
void GLES20_glUniform3f(JNIEnv*, jclass, jint loc, jfloat x, jfloat y, jfloat z) {
#ifdef MACAPK_HAS_ANGLE
    glUniform3f(loc, x, y, z);
#endif
}
void GLES20_glUniform4f(JNIEnv*, jclass, jint loc, jfloat x, jfloat y, jfloat z, jfloat w) {
#ifdef MACAPK_HAS_ANGLE
    glUniform4f(loc, x, y, z, w);
#endif
}
#ifdef MACAPK_HAS_ANGLE
// ---- glGet{Integer,Float,Boolean}v sizing.
//
// The DRIVER decides how many values it writes, and it decides from `pname`
// alone. A fixed 16-slot scratch buffer therefore was not a bound at all: it
// clamped the copy OUT to Java and left the write IN unbounded, so any pname
// whose count exceeds 16 smashed the stack. GL_COMPRESSED_TEXTURE_FORMATS is
// the live case (it returns GL_NUM_COMPRESSED_TEXTURE_FORMATS ints and
// ANGLE-on-Metal reports well over 16 -- our own ETC1Util comment says so);
// GL_SHADER_BINARY_FORMATS and GL_PROGRAM_BINARY_FORMATS are the same shape.
//
// AOSP solves it with getNeededCount(pname), which queries the GL_NUM_* pname
// for the dynamic cases (frameworks/base/core/jni/android_opengl_GLES20.cpp
// :239-317, android16-release) and then refuses the call outright when the
// caller's array is smaller than that. We mirror the COUNT half verbatim but
// not the throw: raising IllegalArgumentException where we previously wrote a
// truncated answer would be a new, app-visible failure for a bug that is ours.
// So we size a heap scratch to the count and still copy out only what fits.
//
// AOSP's own comment notes the switch is incomplete by design (an unrecognised
// pname defaults to 1, and on AOSP that overruns the CALLER's array). We keep
// the historical 16-slot floor for that case and additionally grow the scratch
// to the caller's own remaining length, which is the app's own statement of how
// much it expects. That makes an unknown multi-valued pname corrupt nothing.
#ifndef GL_NUM_COMPRESSED_TEXTURE_FORMATS
#define GL_NUM_COMPRESSED_TEXTURE_FORMATS 0x86A2
#endif
#ifndef GL_NUM_SHADER_BINARY_FORMATS
#define GL_NUM_SHADER_BINARY_FORMATS 0x8DF9
#endif
#ifndef GL_NUM_PROGRAM_BINARY_FORMATS
#define GL_NUM_PROGRAM_BINARY_FORMATS 0x87FE
#endif
#ifndef GL_MAX_VIEWPORT_DIMS
#define GL_MAX_VIEWPORT_DIMS 0x0D3A
#endif
#ifndef GL_PROGRAM_BINARY_FORMATS
#define GL_PROGRAM_BINARY_FORMATS 0x87FF
#endif
static int GlNeededCount(GLint pname) {
    int needed = 1;
    switch (pname) {
        // 2 values.
        case GL_ALIASED_LINE_WIDTH_RANGE:
        case GL_ALIASED_POINT_SIZE_RANGE:
        case GL_DEPTH_RANGE:
        case GL_MAX_VIEWPORT_DIMS:
            needed = 2;
            break;
        // 4 values.
        case GL_BLEND_COLOR:
        case GL_COLOR_CLEAR_VALUE:
        case GL_COLOR_WRITEMASK:
        case GL_SCISSOR_BOX:
        case GL_VIEWPORT:
            needed = 4;
            break;
        // Dynamic: ask GL for the count first. glGetIntegerv is the right call
        // for all three even from the float/boolean paths -- these are counts.
        case GL_COMPRESSED_TEXTURE_FORMATS:
            glGetIntegerv(GL_NUM_COMPRESSED_TEXTURE_FORMATS, &needed);
            break;
        case GL_SHADER_BINARY_FORMATS:
            glGetIntegerv(GL_NUM_SHADER_BINARY_FORMATS, &needed);
            break;
        case GL_PROGRAM_BINARY_FORMATS:
            glGetIntegerv(GL_NUM_PROGRAM_BINARY_FORMATS, &needed);
            break;
        default:
            break;
    }
    if (needed < 1) needed = 1;
    return needed;
}

// Scratch element count that is safe for BOTH the driver's write and the
// caller's expectation. Never below 16, so behaviour for every pname that
// already worked is bit-identical.
static size_t GlScratchCount(GLint pname, jsize avail) {
    size_t n = (size_t) GlNeededCount(pname);
    if (avail > 0 && (size_t) avail > n) n = (size_t) avail;
    if (n < 16u) n = 16u;
    return n;
}

// How many elements to hand BACK to Java. Deliberately NOT the scratch size:
// the scratch is oversized on purpose (it has to survive whatever the driver
// writes), and copying all of it out would blank the caller's array past the
// values the query actually produced -- a real hazard for an app that reuses
// one big int[] across several queries. Keeping the historical 16 floor makes
// this bit-identical to the pre-fix behaviour for every pname whose count is
// 16 or less, i.e. everything that already worked, while still delivering the
// full result for the dynamic large ones (GL_COMPRESSED_TEXTURE_FORMATS and
// friends) that used to be truncated at 16 after smashing the stack.
static jsize GlCopyCount(GLint pname, jsize avail) {
    size_t n = (size_t) GlNeededCount(pname);
    if (n < 16u) n = 16u;
    return (avail > 0 && (size_t) avail < n) ? avail : (jsize) n;
}

#endif  // MACAPK_HAS_ANGLE

void GLES20_glGetIntegerv(JNIEnv* env, jclass, jint pname, jintArray params, jint offset) {
#ifdef MACAPK_HAS_ANGLE
    if (!params || offset < 0) return;
    jsize avail = env->GetArrayLength(params) - offset;
    if (avail <= 0) return;
    std::vector<GLint> v(GlScratchCount(pname, avail), 0);
    glGetIntegerv((GLenum) pname, v.data());
    jsize copy = GlCopyCount(pname, avail);
    if ((size_t) copy > v.size()) copy = (jsize) v.size();
    if (copy > 0) env->SetIntArrayRegion(params, offset, copy,
                                         reinterpret_cast<const jint*>(v.data()));
#endif
}
void GLES20_glGetActiveAttrib(JNIEnv* env, jclass, jint program, jint index, jint bufsize,
                              jintArray lengthArr, jint lengthOff,
                              jintArray sizeArr, jint sizeOff,
                              jintArray typeArr, jint typeOff,
                              jbyteArray nameArr, jint nameOff) {
#ifdef MACAPK_HAS_ANGLE
    GLsizei length = 0;
    GLint   size   = 0;
    GLenum  type   = 0;
    std::vector<char> nameBuf(bufsize > 0 ? bufsize : 1);
    glGetActiveAttrib(program, index, (GLsizei) nameBuf.size(),
                      &length, &size, &type, nameBuf.data());
    if (lengthArr) { jint v = (jint) length; env->SetIntArrayRegion(lengthArr, lengthOff, 1, &v); }
    if (sizeArr)   { jint v = (jint) size;   env->SetIntArrayRegion(sizeArr,   sizeOff,   1, &v); }
    if (typeArr)   { jint v = (jint) type;   env->SetIntArrayRegion(typeArr,   typeOff,   1, &v); }
    if (nameArr) {
        jsize cap = env->GetArrayLength(nameArr);
        jsize copy = std::min<jsize>(cap - nameOff, length + 1);
        if (copy > 0) env->SetByteArrayRegion(nameArr, nameOff, copy,
                                              reinterpret_cast<const jbyte*>(nameBuf.data()));
    }
#endif
}
void GLES20_glGetActiveUniform(JNIEnv* env, jclass, jint program, jint index, jint bufsize,
                               jintArray lengthArr, jint lengthOff,
                               jintArray sizeArr, jint sizeOff,
                               jintArray typeArr, jint typeOff,
                               jbyteArray nameArr, jint nameOff) {
#ifdef MACAPK_HAS_ANGLE
    GLsizei length = 0;
    GLint   size   = 0;
    GLenum  type   = 0;
    std::vector<char> nameBuf(bufsize > 0 ? bufsize : 1);
    glGetActiveUniform(program, index, (GLsizei) nameBuf.size(),
                       &length, &size, &type, nameBuf.data());
    if (lengthArr) { jint v = (jint) length; env->SetIntArrayRegion(lengthArr, lengthOff, 1, &v); }
    if (sizeArr)   { jint v = (jint) size;   env->SetIntArrayRegion(sizeArr,   sizeOff,   1, &v); }
    if (typeArr)   { jint v = (jint) type;   env->SetIntArrayRegion(typeArr,   typeOff,   1, &v); }
    if (nameArr) {
        jsize cap = env->GetArrayLength(nameArr);
        jsize copy = std::min<jsize>(cap - nameOff, length + 1);
        if (copy > 0) env->SetByteArrayRegion(nameArr, nameOff, copy,
                                              reinterpret_cast<const jbyte*>(nameBuf.data()));
    }
#endif
}
void GLES20_glUniformMatrix4fv(JNIEnv* env, jclass, jint loc, jint count,
                               jboolean transpose, jfloatArray values, jint offset) {
#ifdef MACAPK_HAS_ANGLE
    if (!values) return;
    jfloat* p = env->GetFloatArrayElements(values, nullptr);
    static int s = 0;
    if (s++ < 3) {
        std::fprintf(stderr,
            "[I] gles: glUniformMatrix4fv #%d loc=%d xpose=%d M=[%.2f,%.2f,%.2f,%.2f, %.2f,%.2f,%.2f,%.2f, %.2f,%.2f,%.2f,%.2f, %.2f,%.2f,%.2f,%.2f]\n",
            s, (int) loc, (int) transpose,
            p[offset+0], p[offset+1], p[offset+2], p[offset+3],
            p[offset+4], p[offset+5], p[offset+6], p[offset+7],
            p[offset+8], p[offset+9], p[offset+10], p[offset+11],
            p[offset+12], p[offset+13], p[offset+14], p[offset+15]);
        std::fflush(stderr);
    }
    glUniformMatrix4fv(loc, count, transpose ? GL_TRUE : GL_FALSE, p + offset);
    if (gldraw_trace()) {
        // Read the uniform back OUT of the program object. An upload that
        // targets the wrong program (or a stale location) leaves the shader
        // reading an all-zero matrix, which collapses every vertex to the
        // origin -- a silent, error-free blank.
        static int rb = 0;
        if (rb++ < 4) {
            GLint prog = 0;
            glGetIntegerv(GL_CURRENT_PROGRAM, &prog);
            GLfloat got[16] = {0};
            if (prog) glGetUniformfv(prog, loc, got);
            std::fprintf(stderr,
                "[GLUNI] readback prog=%d loc=%d err=0x%x M=[%.2f,%.2f,%.2f,%.2f, %.2f,%.2f,%.2f,%.2f,"
                " %.2f,%.2f,%.2f,%.2f, %.2f,%.2f,%.2f,%.2f]\n",
                prog, (int) loc, glGetError(),
                got[0],got[1],got[2],got[3], got[4],got[5],got[6],got[7],
                got[8],got[9],got[10],got[11], got[12],got[13],got[14],got[15]);
        }
    }
    env->ReleaseFloatArrayElements(values, p, JNI_ABORT);
#endif
}

// ---- texture / buffer creation ----
void GLES20_nativeGlGenTextures(JNIEnv* env, jclass, jint n, jintArray out) {
#ifdef MACAPK_HAS_ANGLE
    if (!out || n <= 0) return;
    std::vector<GLuint> tmp(n);
    glGenTextures(n, tmp.data());
    env->SetIntArrayRegion(out, 0, n, reinterpret_cast<const jint*>(tmp.data()));
#else
    static int counter = 1;
    if (!out || n <= 0) return;
    std::vector<jint> tmp(n);
    for (int i = 0; i < n; ++i) tmp[i] = counter++;
    env->SetIntArrayRegion(out, 0, n, tmp.data());
#endif
}
void GLES20_nativeGlGenBuffers(JNIEnv* env, jclass, jint n, jintArray out) {
#ifdef MACAPK_HAS_ANGLE
    if (!out || n <= 0) return;
    std::vector<GLuint> tmp(n);
    glGenBuffers(n, tmp.data());
    env->SetIntArrayRegion(out, 0, n, reinterpret_cast<const jint*>(tmp.data()));
#else
    static int counter = 1;
    if (!out || n <= 0) return;
    std::vector<jint> tmp(n);
    for (int i = 0; i < n; ++i) tmp[i] = counter++;
    env->SetIntArrayRegion(out, 0, n, tmp.data());
#endif
}

// ---- Buffer fast-path for glBufferData / glTex*Image2D / glVertexAttribPointer ----
//
// The hot path for AndEngine: every sprite upload calls glBufferData
// with vertex data, every texture creation calls glTexSubImage2D with
// pixel data. Going through a Java helper that loops element-by-
// element would dominate the JIT'd render path.
//
// Direct buffers (AndEngine's `ByteBuffer.allocateDirect(size)
// .asFloatBuffer()` pattern): GetDirectBufferAddress returns a real
// host pointer, zero-copy. Heap buffers (e.g. IntBuffer.wrap from
// the texture pixel-conversion path): we call .array() and pin via
// the appropriate Get*ArrayElements. ScopedBufferBytes RAII handles
// pin/unpin.
//
// position()/remaining() are honored when the source is a direct
// buffer that's been advanced; they're rare on the hot path but
// AndEngine's scratch buffers do this.

namespace {

// One-time class/method cache. java.nio.Buffer.position() is a final
// method on the abstract Buffer class, so a single methodID resolved
// once is valid across all subclasses. Same for hasArray()/array().
// Hoisting these out of the per-call path saves ~6 JNI helper calls
// per glBufferData/glTex*Image2D/glVertexAttribPointer.
struct BufferCache {
    jclass    cBuffer       = nullptr;   // java.nio.Buffer
    jclass    cFloatBuffer  = nullptr;
    jclass    cIntBuffer    = nullptr;
    jclass    cShortBuffer  = nullptr;
    jclass    cDoubleBuffer = nullptr;
    jclass    cLongBuffer   = nullptr;
    jmethodID mPosition     = nullptr;   // Buffer.position()
    jmethodID mHasArray     = nullptr;   // Buffer.hasArray()
    jmethodID mArray        = nullptr;   // Buffer.array()
    bool      ready         = false;
};
static BufferCache gBufCache;

inline void EnsureBufferCache(JNIEnv* env) {
    if (gBufCache.ready) return;
    auto pin = [&](const char* n) -> jclass {
        jclass c = env->FindClass(n);
        return c ? (jclass) env->NewGlobalRef(c) : nullptr;
    };
    gBufCache.cBuffer       = pin("java/nio/Buffer");
    gBufCache.cFloatBuffer  = pin("java/nio/FloatBuffer");
    gBufCache.cIntBuffer    = pin("java/nio/IntBuffer");
    gBufCache.cShortBuffer  = pin("java/nio/ShortBuffer");
    gBufCache.cDoubleBuffer = pin("java/nio/DoubleBuffer");
    gBufCache.cLongBuffer   = pin("java/nio/LongBuffer");
    if (gBufCache.cBuffer) {
        gBufCache.mPosition = env->GetMethodID(gBufCache.cBuffer, "position", "()I");
        gBufCache.mHasArray = env->GetMethodID(gBufCache.cBuffer, "hasArray", "()Z");
        gBufCache.mArray    = env->GetMethodID(gBufCache.cBuffer, "array",    "()Ljava/lang/Object;");
    }
    gBufCache.ready = true;
}

inline int InferElemSizeCached(JNIEnv* env, jobject buf) {
    if (env->IsInstanceOf(buf, gBufCache.cFloatBuffer))  return 4;
    if (env->IsInstanceOf(buf, gBufCache.cIntBuffer))    return 4;
    if (env->IsInstanceOf(buf, gBufCache.cShortBuffer))  return 2;
    if (env->IsInstanceOf(buf, gBufCache.cDoubleBuffer)) return 8;
    if (env->IsInstanceOf(buf, gBufCache.cLongBuffer))   return 8;
    return 1;
}

struct ScopedBufferBytes {
    JNIEnv* env;
    jobject array_obj = nullptr;   // jXArray or null when direct
    void*   ptr = nullptr;         // position-adjusted pointer handed to GL
    void*   base = nullptr;        // EXACTLY what Get*ArrayElements returned
    int     elem_size = 0;          // 1=byte, 2=short, 4=float (for release path)
    // OUTPUT buffers must set this (v1.0.836). The heap-buffer path pins a
    // COPY of the Java array, and the destructor's default JNI_ABORT means
    // "free the copy and throw away any changes" -- which is right for an
    // input buffer (glBufferData) and silently destroys the result for an
    // output one (glReadPixels). write_back releases with mode 0 instead, so
    // GL's writes are copied back into the Java array. Direct buffers are
    // unaffected either way: they are the real memory, not a copy.
    bool    write_back = false;

    ScopedBufferBytes(JNIEnv* e, jobject buf, bool wb = false) : env(e), write_back(wb) {
        if (!buf) return;
        EnsureBufferCache(env);
        // 1) Direct buffer fast path -- zero-copy.
        void* dptr = env->GetDirectBufferAddress(buf);
        if (dptr) {
            jint p  = env->CallIntMethod(buf, gBufCache.mPosition);
            int  es = InferElemSizeCached(env, buf);
            ptr     = static_cast<char*>(dptr) + (jlong) p * es;
            return;
        }
        // 2) Heap buffer slow path -- pin the underlying array.
        if (!env->CallBooleanMethod(buf, gBufCache.mHasArray)) return;
        jobject arr = env->CallObjectMethod(buf, gBufCache.mArray);
        jint    pos = env->CallIntMethod(buf, gBufCache.mPosition);
        if (!arr) return;
        elem_size = InferElemSizeCached(env, buf);
        if (elem_size == 4) {
            jfloatArray fa = static_cast<jfloatArray>(arr);
            jfloat* fp = env->GetFloatArrayElements(fa, nullptr);
            base = fp;
            ptr = fp + pos;
            array_obj = arr;
        } else if (elem_size == 2) {
            jshortArray sa = static_cast<jshortArray>(arr);
            jshort* sp = env->GetShortArrayElements(sa, nullptr);
            base = sp;
            ptr = sp + pos;
            array_obj = arr;
        } else {
            jbyteArray ba = static_cast<jbyteArray>(arr);
            jbyte* bp = env->GetByteArrayElements(ba, nullptr);
            base = bp;
            ptr = bp + pos;
            array_obj = arr;
        }
    }
    ~ScopedBufferBytes() {
        // Release must be handed the EXACT pointer Get*ArrayElements returned.
        // Passing the position-adjusted `ptr` is undefined behaviour (it can
        // free the wrong block); use the saved base.
        if (!array_obj || !base || !env) return;
        const jint mode = write_back ? 0 : JNI_ABORT;
        if (elem_size == 4) {
            env->ReleaseFloatArrayElements(static_cast<jfloatArray>(array_obj),
                                           static_cast<jfloat*>(base), mode);
        } else if (elem_size == 2) {
            env->ReleaseShortArrayElements(static_cast<jshortArray>(array_obj),
                                           static_cast<jshort*>(base), mode);
        } else {
            env->ReleaseByteArrayElements(static_cast<jbyteArray>(array_obj),
                                          static_cast<jbyte*>(base), mode);
        }
    }
    ScopedBufferBytes(const ScopedBufferBytes&) = delete;
    ScopedBufferBytes& operator=(const ScopedBufferBytes&) = delete;
};
} // namespace

void GLES20_glBufferData_buf(JNIEnv* env, jclass, jint target, jint size, jobject data, jint usage) {
#ifdef MACAPK_HAS_ANGLE
    if (!data) { glBufferData(target, size, nullptr, usage); return; }
    ScopedBufferBytes b(env, data);
    glBufferData(target, size, b.ptr, usage);
    if (gldraw_trace()) {
        static long n = 0;
        if (++n <= 6 || (n >= 21 && n <= 26) || (n % 600) == 0) {
            std::fprintf(stderr, "[GLDRAW] glBufferData(target=0x%x size=%d ptr=%p) #%ld err=0x%x\n",
                         target, size, b.ptr, n, glGetError());
            // For ARRAY_BUFFER (0x8892) dump the first 8 floats so we can
            // see the coordinate range the renderer is uploading.
            if (target == 0x8892 && b.ptr && size >= 32) {
                const float* fp = static_cast<const float*>(b.ptr);
                const unsigned* up = static_cast<const unsigned*>(b.ptr);
                std::fprintf(stderr, "[GLDRAW]   vtx[0..7]=%.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f\n",
                             fp[0], fp[1], fp[2], fp[3], fp[4], fp[5], fp[6], fp[7]);
                std::fprintf(stderr, "[GLDRAW]   hex[0..7]=%08x %08x %08x %08x %08x %08x %08x %08x\n",
                             up[0], up[1], up[2], up[3], up[4], up[5], up[6], up[7]);
                // Coordinate RANGE over the whole upload. Packed colours are
                // non-finite when read as float, so skipping them leaves the
                // real position range -- which answers "is this geometry
                // anywhere near the clip volume" without knowing the stride.
                float lo = 1e30f, hi = -1e30f; long nfin = 0, nnan = 0;
                for (int i = 0; i < size / 4; ++i) {
                    float v = fp[i];
                    if (v != v || v > 1e30f || v < -1e30f) { ++nnan; continue; }
                    ++nfin; if (v < lo) lo = v; if (v > hi) hi = v;
                }
                std::fprintf(stderr, "[GLDRAW]   finite=%ld nonfinite=%ld range=[%.3f .. %.3f]\n",
                             nfin, nnan, lo, hi);
                // Walk the buffer at the CURRENTLY BOUND attribute-0 stride and
                // print the first vertices as (x,y), plus how many consecutive
                // vertices are positionally identical. Triangles built from
                // coincident vertices have zero area: they are drawn, counted,
                // and rasterize to nothing, with no GL error.
                GLint st = 0;
                glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_STRIDE, &st);
                if (st >= 12) {
                    int nv = size / st;
                    std::fprintf(stderr, "[GLVTX] stride=%d verts=%d first8:", st, nv);
                    for (int v = 0; v < nv && v < 8; ++v) {
                        const float* q = reinterpret_cast<const float*>(
                            static_cast<const char*>(b.ptr) + (size_t) v * st);
                        std::fprintf(stderr, " (%.4f,%.4f)", q[0], q[1]);
                    }
                    long dup = 0;
                    for (int v = 1; v < nv; ++v) {
                        const float* a0 = reinterpret_cast<const float*>(
                            static_cast<const char*>(b.ptr) + (size_t)(v-1) * st);
                        const float* a1 = reinterpret_cast<const float*>(
                            static_cast<const char*>(b.ptr) + (size_t) v * st);
                        if (a0[0] == a1[0] && a0[1] == a1[1]) ++dup;
                    }
                    std::fprintf(stderr, "\n[GLVTX] coincident-with-previous: %ld of %d\n", dup, nv - 1);
                }
            }
            // ELEMENT_ARRAY_BUFFER (0x8893): indices are integers, so a float
            // dump is unreadable. Print them as ints; a renderer that packs
            // 32-bit indices is also telling us it needs GL_UNSIGNED_INT.
            if (target == 0x8893 && b.ptr && size >= 32) {
                const unsigned* ip = static_cast<const unsigned*>(b.ptr);
                std::fprintf(stderr, "[GLDRAW]   idx[0..7]=%u %u %u %u %u %u %u %u\n",
                             ip[0], ip[1], ip[2], ip[3], ip[4], ip[5], ip[6], ip[7]);
                // The index RANGE against the vertex buffer's capacity. An
                // index >= the vertex count is out of range: a conformant
                // implementation may drop the whole draw, with no GL error.
                unsigned imin = 0xffffffffu, imax = 0;
                for (int i = 0; i < size / 4; ++i) {
                    if (ip[i] < imin) imin = ip[i];
                    if (ip[i] > imax) imax = ip[i];
                }
                GLint abuf = 0, abytes = 0;
                glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &abuf);
                if (abuf) glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &abytes);
                std::fprintf(stderr,
                    "[GLDRAW]   indices=%d range=[%u..%u]  ARRAY_BUFFER=%d bytes=%d\n",
                    size / 4, imin, imax, abuf, abytes);
            }
        }
    }
#endif
}
// android.opengl.GLUtils RGBA upload helpers.
void GLUtils_nativeTexImage2DRgba(JNIEnv* env, jclass, jint target, jint level, jint internalformat,
                                   jint width, jint height, jint border,
                                   jint format, jint type, jbyteArray pixels) {
#ifdef MACAPK_HAS_ANGLE
    if (!pixels) {
        glTexImage2D(target, level, internalformat, width, height, border, format, type, nullptr);
        return;
    }
    jbyte* p = env->GetByteArrayElements(pixels, nullptr);
    glTexImage2D(target, level, internalformat, width, height, border, format, type, p);
    env->ReleaseByteArrayElements(pixels, p, JNI_ABORT);
#endif
}
void GLUtils_nativeTexSubImage2DRgba(JNIEnv* env, jclass, jint target, jint level,
                                      jint xoffset, jint yoffset, jint width, jint height,
                                      jint format, jint type, jbyteArray pixels) {
#ifdef MACAPK_HAS_ANGLE
    if (!pixels) return;
    jbyte* p = env->GetByteArrayElements(pixels, nullptr);
    glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, p);
    env->ReleaseByteArrayElements(pixels, p, JNI_ABORT);
#endif
}

// ---- GLES20.glTexImage2D(..., Buffer): AOSP pass-through (v1.0.1655) ----
// AOSP's generated glue, frameworks/base/core/jni/android_opengl_GLES20.cpp
// android_glTexImage2D__IIIIIIIILjava_nio_Buffer_2 (main, ~:4545), gets the
// Buffer through getPointer (GetPrimitiveArrayCritical for a heap buffer), calls
// glTexImage2D verbatim and releases the pointer. It never reads or writes a
// texture PARAMETER, so a texture the app leaves at the GLES defaults keeps
// MIN_FILTER=NEAREST_MIPMAP_LINEAR and WRAP=REPEAT and, without a mip chain, is
// incomplete and samples (0,0,0,1) on a device.
//
// Until v1.0.1655 this entry point substituted MIN=LINEAR for any *_MIPMAP_*
// filter and WRAP=CLAMP_TO_EDGE for any REPEAT wrap before the upload. That was
// a crutch from the initial commit (9ce85bad), needed only while the Java
// GLES20.glTexParameterf body was EMPTY (fixed v1.0.682; glTexParameterfv real
// since v1.0.836), because apps' own filter/wrap requests then never reached GL.
// Its wrap premise ("REPEAT is invalid for NPOT, samples black") is also false
// here: an ES2 request gets an ES 3.0 ANGLE context with GL_OES_texture_npot,
// where an NPOT REPEAT texture is complete. And it did active harm, measured by
// debug/geh_gles20-teximage2d-param-substitution_gate.sh:
//   - a cube-map FACE upload (TEXTURE_CUBE_MAP_POSITIVE_X+i) raised
//     GL_INVALID_ENUM, because glGetTexParameteriv/glTexParameteri accept only
//     TEXTURE_2D/CUBE_MAP targets, so a valid call left an error behind;
//   - a WRAP=REPEAT the app set explicitly before the upload became CLAMP;
//   - an explicit MIN=LINEAR_MIPMAP_LINEAR plus glGenerateMipmap became LINEAR.
// Our own GLUtils.texImage2D and every native guest glTexImage2D already pass
// through untouched, so this was also inconsistent inside our layer. Every fleet
// app on this bridge (AndEngine, libGDX, Arc, noosa) sets filter and wrap AFTER
// the upload, which overwrote the substitution anyway.
//
// MACAPK_GLES_TEXIMAGE_PASSTHROUGH=0 restores the old substitution exactly (same
// GL calls, same order). Note MACAPK_GL_OFF=texparam/texfilter/texwrap now drops
// the app's filters WITHOUT this safety net (black textures); pair it with
// MACAPK_GLES_TEXIMAGE_PASSTHROUGH=0 to reproduce the pre-v682 behaviour.
// Retirement note: docs/gles20-teximage-param-substitution-retired_2026-09-27.md.
static inline bool gles_teximage_passthrough() {
    static int v = -1;
    if (v < 0) {
        const char* e = std::getenv("MACAPK_GLES_TEXIMAGE_PASSTHROUGH");
        // Exact "0" only, like MACAPK_GL_DEFAULTS (angle_glue.cpp) and the
        // Java !"0".equals(...) convention: "00" or "0x" leave it ON.
        v = (e && e[0] == '0' && e[1] == '\0') ? 0 : 1;
    }
    return v == 1;
}
#ifdef MACAPK_HAS_ANGLE
// The pre-v1.0.1655 substitution, kept verbatim for the =0 arm only.
static void teximage_legacy_param_subst(GLenum target) {
    // GLES2's DEFAULT min filter is GL_NEAREST_MIPMAP_LINEAR, which requires a
    // mipmap chain; without one, sampling returns BLACK. So a safe default is
    // genuinely needed here.
    //
    // v1.0.683: but only REPLACE a mipmap-requiring filter. The previous code
    // unconditionally forced GL_LINEAR, which silently overrode an app's
    // explicit choice. That mattered: Flappy Bird (AndEngine) asks for
    // GL_NEAREST because it is pixel art, and the forced GL_LINEAR blurred
    // every sprite. It went unnoticed for as long as it did because the Java
    // glTexParameterf entry point was an empty no-op until v1.0.682, so the
    // app's request never reached GL at all and the override always won.
    // Honour an explicit non-mipmap filter; substitute only when the current
    // filter would sample black.
    GLint curMin = GL_NEAREST_MIPMAP_LINEAR;
    glGetTexParameteriv(target, GL_TEXTURE_MIN_FILTER, &curMin);
    if (curMin == GL_NEAREST_MIPMAP_NEAREST || curMin == GL_LINEAR_MIPMAP_NEAREST
        || curMin == GL_NEAREST_MIPMAP_LINEAR || curMin == GL_LINEAR_MIPMAP_LINEAR) {
        glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }
    // Wrap: GLES2 defaults to GL_REPEAT, which is invalid for NPOT textures
    // (samples black). Only substitute when it is still at that default.
    GLint curWrapS = GL_REPEAT, curWrapT = GL_REPEAT;
    glGetTexParameteriv(target, GL_TEXTURE_WRAP_S, &curWrapS);
    glGetTexParameteriv(target, GL_TEXTURE_WRAP_T, &curWrapT);
    if (curWrapS == GL_REPEAT) glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    if (curWrapT == GL_REPEAT) glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}
#endif
void GLES20_glTexImage2D_buf(JNIEnv* env, jclass,
        jint target, jint level, jint internalformat,
        jint width, jint height, jint border, jint format, jint type,
        jobject pixels) {
#ifdef MACAPK_HAS_ANGLE
    if (!gles_teximage_passthrough()) teximage_legacy_param_subst((GLenum)target);
    if (!pixels) {
        glTexImage2D(target, level, internalformat, width, height, border, format, type, nullptr);
        return;
    }
    ScopedBufferBytes b(env, pixels);
    glTexImage2D(target, level, internalformat, width, height, border, format, type, b.ptr);
#endif
}

void GLES20_glTexSubImage2D_buf(JNIEnv* env, jclass,
        jint target, jint level, jint xoffset, jint yoffset, jint width, jint height,
        jint format, jint type, jobject pixels) {
#ifdef MACAPK_HAS_ANGLE
    if (!pixels) return;
    ScopedBufferBytes b(env, pixels);
    glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, b.ptr);
#endif
}

void GLES20_glVertexAttribPointer_buf(JNIEnv* env, jclass,
        jint indx, jint size, jint type, jboolean norm, jint stride, jobject buf) {
#ifdef MACAPK_HAS_ANGLE
    // Client-side vertex array. The pointer must remain valid until the
    // next glDrawArrays. AndEngine binds to a real VBO via glBindBuffer
    // before this call, so the pointer is interpreted as an offset and
    // never dereferenced -- ScopedBufferBytes can drop the pin safely.
    // For the rare case where the buffer is genuinely client-side
    // (offset variant not used) the caller MUST keep the buffer alive.
    if (!buf) return;
    ScopedBufferBytes b(env, buf);
    glVertexAttribPointer(indx, size, type, norm ? GL_TRUE : GL_FALSE, stride, b.ptr);
#endif
}

// Client-side INDEX array. AOSP's generated JNI
// (android_glDrawElements__IIILjava_nio_Buffer_2) resolves the Buffer to a raw
// pointer and hands it straight to glDrawElements; ANGLE advertises
// GL_ANGLE_client_arrays, so no ELEMENT_ARRAY_BUFFER needs to be bound.
//
// Unlike glVertexAttribPointer above, the pin does NOT need to outlive the call:
// glDrawElements consumes the indices immediately. libGDX's IndexArray is backed
// by BufferUtils.newUnsafeByteBuffer, i.e. a DIRECT buffer, so ScopedBufferBytes
// takes its zero-copy GetDirectBufferAddress path.
//
// The Java side was an EMPTY BODY until v1.0.834 and therefore discarded every
// draw issued through libGDX's client-array Mesh path, with no GL error and no
// exception. See GLES20.java's comment and docs/libgdx-blank-cluster_2026-07-29.md.
void GLES20_glDrawElements_buf(JNIEnv* env, jclass,
        jint mode, jint count, jint type, jobject indices) {
#ifdef MACAPK_HAS_ANGLE
    if (!indices) { glDrawElements((GLenum)mode, count, (GLenum)type, nullptr); return; }
    ScopedBufferBytes b(env, indices);
    glDrawElements((GLenum)mode, count, (GLenum)type, b.ptr);
    if (gldraw_trace()) {
        static long n = 0;
        if (++n <= 5 || (n % 600) == 0)
            std::fprintf(stderr,
                "[GLDRAW] glDrawElements(buf mode=0x%x count=%d type=0x%x ptr=%p)"
                " #%ld err=0x%x\n", mode, count, type, b.ptr, n, glGetError());
    }
#else
    (void)env; (void)mode; (void)count; (void)type; (void)indices;
#endif
}



// ---- v683 BISECT GATE -------------------------------------------------
// MACAPK_GL_OFF=<comma-separated group names> makes the v682 GLES bridges in
// those groups behave like their pre-v682 EMPTY bodies again, so a regression
// can be bisected at runtime without a rebuild per step.
// groups: state stencil fbo del uniform tex query
static bool gl_grp_off(const char* g) {
    static const char* off = nullptr;
    static bool inited = false;
    if (!inited) { off = std::getenv("MACAPK_GL_OFF"); inited = true; }
    if (!off || !*off) return false;
    const char* p = std::strstr(off, g);
    return p != nullptr;
}
#define GL_GATE(g) do { if (gl_grp_off(g)) return; } while (0)
#define GL_GATE_R(g, v) do { if (gl_grp_off(g)) return (v); } while (0)

// ===================================================================
// v1.0.682: GLES20 bridges that were EMPTY JAVA BODIES.
//
// The v677 AOSP audit found 106 android.opengl.GLES20 methods with `{}`
// bodies. They returned cleanly and did nothing, so an app's render state
// (depth/cull/blend/colour mask), stencil configuration, framebuffer-object
// setup, uniform ARRAYS (only the scalar forms worked) and every object
// deletion were silently discarded -- and glGetError() reporting GL_NO_ERROR
// meant the app could never detect any of it. These forward to ANGLE exactly
// like the hand-written bridges above.
// ===================================================================
void GLES20_glBlendColor(JNIEnv*, jclass, jfloat r, jfloat g, jfloat b, jfloat a) {
    GL_GATE("state");
#ifdef MACAPK_HAS_ANGLE
    glBlendColor(r,g,b,a);
#endif
}
void GLES20_glBlendEquation(JNIEnv*, jclass, jint mode) {
    GL_GATE("state");
#ifdef MACAPK_HAS_ANGLE
    glBlendEquation((GLenum)mode);
#endif
}
void GLES20_glBlendEquationSeparate(JNIEnv*, jclass, jint rgb, jint a) {
    GL_GATE("state");
#ifdef MACAPK_HAS_ANGLE
    glBlendEquationSeparate((GLenum)rgb,(GLenum)a);
#endif
}
void GLES20_glBlendFuncSeparate(JNIEnv*, jclass, jint sR, jint dR, jint sA, jint dA) {
    GL_GATE("state");
#ifdef MACAPK_HAS_ANGLE
    glBlendFuncSeparate((GLenum)sR,(GLenum)dR,(GLenum)sA,(GLenum)dA);
#endif
}
void GLES20_glClearDepthf(JNIEnv*, jclass, jfloat d) {
    GL_GATE("state");
#ifdef MACAPK_HAS_ANGLE
    glClearDepthf(d);
#endif
}
void GLES20_glClearStencil(JNIEnv*, jclass, jint s) {
    GL_GATE("state");
#ifdef MACAPK_HAS_ANGLE
    glClearStencil(s);
#endif
}
void GLES20_glColorMask(JNIEnv*, jclass, jboolean r, jboolean g, jboolean b, jboolean a) {
    GL_GATE("state");
#ifdef MACAPK_HAS_ANGLE
    glColorMask(r,g,b,a);
#endif
}
void GLES20_glCullFace(JNIEnv*, jclass, jint mode) {
    GL_GATE("state");
#ifdef MACAPK_HAS_ANGLE
    glCullFace((GLenum)mode);
#endif
}
void GLES20_glDepthFunc(JNIEnv*, jclass, jint f) {
    GL_GATE("state");
#ifdef MACAPK_HAS_ANGLE
    glDepthFunc((GLenum)f);
#endif
}
void GLES20_glDepthMask(JNIEnv*, jclass, jboolean flag) {
    GL_GATE("state");
#ifdef MACAPK_HAS_ANGLE
    glDepthMask(flag);
#endif
}
void GLES20_glDepthRangef(JNIEnv*, jclass, jfloat n, jfloat f) {
    GL_GATE("state");
#ifdef MACAPK_HAS_ANGLE
    glDepthRangef(n,f);
#endif
}
void GLES20_glFrontFace(JNIEnv*, jclass, jint mode) {
    GL_GATE("state");
#ifdef MACAPK_HAS_ANGLE
    glFrontFace((GLenum)mode);
#endif
}
void GLES20_glLineWidth(JNIEnv*, jclass, jfloat w) {
    GL_GATE("state");
#ifdef MACAPK_HAS_ANGLE
    glLineWidth(w);
#endif
}
void GLES20_glPolygonOffset(JNIEnv*, jclass, jfloat factor, jfloat units) {
    GL_GATE("state");
#ifdef MACAPK_HAS_ANGLE
    glPolygonOffset(factor,units);
#endif
}
void GLES20_glSampleCoverage(JNIEnv*, jclass, jfloat v, jboolean inv) {
    GL_GATE("state");
#ifdef MACAPK_HAS_ANGLE
    glSampleCoverage(v,inv);
#endif
}
void GLES20_glScissor(JNIEnv*, jclass, jint x, jint y, jint w, jint h) {
    GL_GATE("state");
#ifdef MACAPK_HAS_ANGLE
    glScissor(x,y,w,h);
#endif
}
void GLES20_glHint(JNIEnv*, jclass, jint target, jint mode) {
    GL_GATE("state");
#ifdef MACAPK_HAS_ANGLE
    glHint((GLenum)target,(GLenum)mode);
#endif
}
void GLES20_glFinish(JNIEnv*, jclass) {
    GL_GATE("state");
#ifdef MACAPK_HAS_ANGLE
    glFinish();
#endif
}
void GLES20_glFlush(JNIEnv*, jclass) {
    GL_GATE("state");
#ifdef MACAPK_HAS_ANGLE
    glFlush();
#endif
}
void GLES20_glStencilFunc(JNIEnv*, jclass, jint f, jint ref, jint mask) {
    GL_GATE("stencil");
#ifdef MACAPK_HAS_ANGLE
    glStencilFunc((GLenum)f,ref,(GLuint)mask);
#endif
}
void GLES20_glStencilFuncSeparate(JNIEnv*, jclass, jint face, jint f, jint ref, jint mask) {
    GL_GATE("stencil");
#ifdef MACAPK_HAS_ANGLE
    glStencilFuncSeparate((GLenum)face,(GLenum)f,ref,(GLuint)mask);
#endif
}
void GLES20_glStencilMask(JNIEnv*, jclass, jint mask) {
    GL_GATE("stencil");
#ifdef MACAPK_HAS_ANGLE
    glStencilMask((GLuint)mask);
#endif
}
void GLES20_glStencilMaskSeparate(JNIEnv*, jclass, jint face, jint mask) {
    GL_GATE("stencil");
#ifdef MACAPK_HAS_ANGLE
    glStencilMaskSeparate((GLenum)face,(GLuint)mask);
#endif
}
void GLES20_glStencilOp(JNIEnv*, jclass, jint f, jint zf, jint zp) {
    GL_GATE("stencil");
#ifdef MACAPK_HAS_ANGLE
    glStencilOp((GLenum)f,(GLenum)zf,(GLenum)zp);
#endif
}
void GLES20_glStencilOpSeparate(JNIEnv*, jclass, jint face, jint f, jint zf, jint zp) {
    GL_GATE("stencil");
#ifdef MACAPK_HAS_ANGLE
    glStencilOpSeparate((GLenum)face,(GLenum)f,(GLenum)zf,(GLenum)zp);
#endif
}
void GLES20_glBindFramebuffer(JNIEnv*, jclass, jint t, jint fb) {
    GL_GATE("fbo");
#ifdef MACAPK_HAS_ANGLE
    glBindFramebuffer((GLenum)t,(GLuint)fb);
#endif
}
void GLES20_glBindRenderbuffer(JNIEnv*, jclass, jint t, jint rb) {
    GL_GATE("fbo");
#ifdef MACAPK_HAS_ANGLE
    glBindRenderbuffer((GLenum)t,(GLuint)rb);
#endif
}
void GLES20_glFramebufferRenderbuffer(JNIEnv*, jclass, jint t, jint att, jint rbt, jint rb) {
    GL_GATE("fbo");
#ifdef MACAPK_HAS_ANGLE
    glFramebufferRenderbuffer((GLenum)t,(GLenum)att,(GLenum)rbt,(GLuint)rb);
#endif
}
void GLES20_glFramebufferTexture2D(JNIEnv*, jclass, jint t, jint att, jint tt, jint tex, jint lvl) {
    GL_GATE("fbo");
#ifdef MACAPK_HAS_ANGLE
    glFramebufferTexture2D((GLenum)t,(GLenum)att,(GLenum)tt,(GLuint)tex,lvl);
#endif
}
void GLES20_glRenderbufferStorage(JNIEnv*, jclass, jint t, jint fmt, jint w, jint h) {
    GL_GATE("fbo");
#ifdef MACAPK_HAS_ANGLE
    glRenderbufferStorage((GLenum)t,(GLenum)fmt,w,h);
#endif
}
void GLES20_glGenerateMipmap(JNIEnv*, jclass, jint t) {
    GL_GATE("fbo");
#ifdef MACAPK_HAS_ANGLE
    glGenerateMipmap((GLenum)t);
#endif
}
void GLES20_glDeleteProgram(JNIEnv*, jclass, jint p) {
    GL_GATE("del");
#ifdef MACAPK_HAS_ANGLE
    glDeleteProgram((GLuint)p);
#endif
}
void GLES20_glDeleteShader(JNIEnv*, jclass, jint sh) {
    GL_GATE("del");
#ifdef MACAPK_HAS_ANGLE
    glDeleteShader((GLuint)sh);
#endif
}
void GLES20_glDetachShader(JNIEnv*, jclass, jint p, jint sh) {
    GL_GATE("del");
#ifdef MACAPK_HAS_ANGLE
    glDetachShader((GLuint)p,(GLuint)sh);
#endif
}
void GLES20_glValidateProgram(JNIEnv*, jclass, jint p) {
    GL_GATE("del");
#ifdef MACAPK_HAS_ANGLE
    glValidateProgram((GLuint)p);
#endif
}
void GLES20_glReleaseShaderCompiler(JNIEnv*, jclass) {
    GL_GATE("del");
#ifdef MACAPK_HAS_ANGLE
    glReleaseShaderCompiler();
#endif
}
void GLES20_glTexParameterf(JNIEnv*, jclass, jint t, jint pn, jfloat v) {
    GL_GATE("texparam");
    // finer bisect: 0x2800/0x2801 = MAG/MIN filter, 0x2802/0x2803 = WRAP_S/T
    if ((pn == 0x2800 || pn == 0x2801) && gl_grp_off("texfilter")) return;
    if ((pn == 0x2802 || pn == 0x2803) && gl_grp_off("texwrap")) return;
#ifdef MACAPK_HAS_ANGLE
    if (gldraw_trace()) {
        static long n = 0;
        if (++n <= 40)
            std::fprintf(stderr, "[GLDRAW] glTexParameterf(target=0x%x pname=0x%x value=0x%x/%.1f)\n",
                         t, pn, (unsigned)v, (double)v);
    }
    glTexParameterf((GLenum)t,(GLenum)pn,v);
#endif
}
void GLES20_glCopyTexImage2D(JNIEnv*, jclass, jint t, jint l, jint fmt, jint x, jint y, jint w, jint h, jint b) {
    GL_GATE("texdata");
#ifdef MACAPK_HAS_ANGLE
    glCopyTexImage2D((GLenum)t,l,(GLenum)fmt,x,y,w,h,b);
#endif
}
void GLES20_glCopyTexSubImage2D(JNIEnv*, jclass, jint t, jint l, jint xo, jint yo, jint x, jint y, jint w, jint h) {
    GL_GATE("texdata");
#ifdef MACAPK_HAS_ANGLE
    glCopyTexSubImage2D((GLenum)t,l,xo,yo,x,y,w,h);
#endif
}
void GLES20_glUniform2i(JNIEnv*, jclass, jint loc, jint x, jint y) {
    GL_GATE("uniform");
#ifdef MACAPK_HAS_ANGLE
    glUniform2i(loc,x,y);
#endif
}
void GLES20_glUniform3i(JNIEnv*, jclass, jint loc, jint x, jint y, jint z) {
    GL_GATE("uniform");
#ifdef MACAPK_HAS_ANGLE
    glUniform3i(loc,x,y,z);
#endif
}
void GLES20_glUniform4i(JNIEnv*, jclass, jint loc, jint x, jint y, jint z, jint w) {
    GL_GATE("uniform");
#ifdef MACAPK_HAS_ANGLE
    glUniform4i(loc,x,y,z,w);
#endif
}
void GLES20_glDeleteBuffers_arr(JNIEnv* env, jclass, jint n, jintArray a, jint off) {
    GL_GATE("del");
#ifdef MACAPK_HAS_ANGLE
    if (!a || n <= 0) return;
    std::vector<GLuint> tmp((size_t) n);
    env->GetIntArrayRegion(a, off, n, reinterpret_cast<jint*>(tmp.data()));
    glDeleteBuffers(n, tmp.data());
#endif
}
void GLES20_glDeleteBuffers_buf(JNIEnv* env, jclass, jint n, jobject b) {
    GL_GATE("del");
#ifdef MACAPK_HAS_ANGLE
    if (!b || n <= 0) return;
    ScopedBufferBytes sb(env, b);
    if (sb.ptr) glDeleteBuffers(n, static_cast<const GLuint*>(sb.ptr));
#endif
}
void GLES20_glDeleteTextures_arr(JNIEnv* env, jclass, jint n, jintArray a, jint off) {
    GL_GATE("del");
#ifdef MACAPK_HAS_ANGLE
    if (!a || n <= 0) return;
    std::vector<GLuint> tmp((size_t) n);
    env->GetIntArrayRegion(a, off, n, reinterpret_cast<jint*>(tmp.data()));
    glDeleteTextures(n, tmp.data());
#endif
}
void GLES20_glDeleteTextures_buf(JNIEnv* env, jclass, jint n, jobject b) {
    GL_GATE("del");
#ifdef MACAPK_HAS_ANGLE
    if (!b || n <= 0) return;
    ScopedBufferBytes sb(env, b);
    if (sb.ptr) glDeleteTextures(n, static_cast<const GLuint*>(sb.ptr));
#endif
}
void GLES20_glDeleteFramebuffers_arr(JNIEnv* env, jclass, jint n, jintArray a, jint off) {
    GL_GATE("del");
#ifdef MACAPK_HAS_ANGLE
    if (!a || n <= 0) return;
    std::vector<GLuint> tmp((size_t) n);
    env->GetIntArrayRegion(a, off, n, reinterpret_cast<jint*>(tmp.data()));
    glDeleteFramebuffers(n, tmp.data());
#endif
}
void GLES20_glDeleteFramebuffers_buf(JNIEnv* env, jclass, jint n, jobject b) {
    GL_GATE("del");
#ifdef MACAPK_HAS_ANGLE
    if (!b || n <= 0) return;
    ScopedBufferBytes sb(env, b);
    if (sb.ptr) glDeleteFramebuffers(n, static_cast<const GLuint*>(sb.ptr));
#endif
}
void GLES20_glDeleteRenderbuffers_arr(JNIEnv* env, jclass, jint n, jintArray a, jint off) {
    GL_GATE("del");
#ifdef MACAPK_HAS_ANGLE
    if (!a || n <= 0) return;
    std::vector<GLuint> tmp((size_t) n);
    env->GetIntArrayRegion(a, off, n, reinterpret_cast<jint*>(tmp.data()));
    glDeleteRenderbuffers(n, tmp.data());
#endif
}
void GLES20_glDeleteRenderbuffers_buf(JNIEnv* env, jclass, jint n, jobject b) {
    GL_GATE("del");
#ifdef MACAPK_HAS_ANGLE
    if (!b || n <= 0) return;
    ScopedBufferBytes sb(env, b);
    if (sb.ptr) glDeleteRenderbuffers(n, static_cast<const GLuint*>(sb.ptr));
#endif
}
void GLES20_glUniform1fv_arr(JNIEnv* env, jclass, jint loc, jint count, jfloatArray v, jint off) {
    GL_GATE("uniform");
#ifdef MACAPK_HAS_ANGLE
    if (!v) return;
    jsize len = env->GetArrayLength(v) - off;
    if (len <= 0) return;
    std::vector<GLfloat> tmp((size_t) len);
    env->GetFloatArrayRegion(v, off, len, tmp.data());
    glUniform1fv(loc, count, tmp.data());
#endif
}
void GLES20_glUniform1fv_buf(JNIEnv* env, jclass, jint loc, jint count, jobject v) {
    GL_GATE("uniform");
#ifdef MACAPK_HAS_ANGLE
    if (!v) return;
    ScopedBufferBytes sb(env, v);
    if (sb.ptr) glUniform1fv(loc, count, static_cast<const GLfloat*>(sb.ptr));
#endif
}
void GLES20_glUniform2fv_arr(JNIEnv* env, jclass, jint loc, jint count, jfloatArray v, jint off) {
    GL_GATE("uniform");
#ifdef MACAPK_HAS_ANGLE
    if (!v) return;
    jsize len = env->GetArrayLength(v) - off;
    if (len <= 0) return;
    std::vector<GLfloat> tmp((size_t) len);
    env->GetFloatArrayRegion(v, off, len, tmp.data());
    glUniform2fv(loc, count, tmp.data());
#endif
}
void GLES20_glUniform2fv_buf(JNIEnv* env, jclass, jint loc, jint count, jobject v) {
    GL_GATE("uniform");
#ifdef MACAPK_HAS_ANGLE
    if (!v) return;
    ScopedBufferBytes sb(env, v);
    if (sb.ptr) glUniform2fv(loc, count, static_cast<const GLfloat*>(sb.ptr));
#endif
}
void GLES20_glUniform3fv_arr(JNIEnv* env, jclass, jint loc, jint count, jfloatArray v, jint off) {
    GL_GATE("uniform");
#ifdef MACAPK_HAS_ANGLE
    if (!v) return;
    jsize len = env->GetArrayLength(v) - off;
    if (len <= 0) return;
    std::vector<GLfloat> tmp((size_t) len);
    env->GetFloatArrayRegion(v, off, len, tmp.data());
    glUniform3fv(loc, count, tmp.data());
#endif
}
void GLES20_glUniform3fv_buf(JNIEnv* env, jclass, jint loc, jint count, jobject v) {
    GL_GATE("uniform");
#ifdef MACAPK_HAS_ANGLE
    if (!v) return;
    ScopedBufferBytes sb(env, v);
    if (sb.ptr) glUniform3fv(loc, count, static_cast<const GLfloat*>(sb.ptr));
#endif
}
void GLES20_glUniform4fv_arr(JNIEnv* env, jclass, jint loc, jint count, jfloatArray v, jint off) {
    GL_GATE("uniform");
#ifdef MACAPK_HAS_ANGLE
    if (!v) return;
    jsize len = env->GetArrayLength(v) - off;
    if (len <= 0) return;
    std::vector<GLfloat> tmp((size_t) len);
    env->GetFloatArrayRegion(v, off, len, tmp.data());
    glUniform4fv(loc, count, tmp.data());
#endif
}
void GLES20_glUniform4fv_buf(JNIEnv* env, jclass, jint loc, jint count, jobject v) {
    GL_GATE("uniform");
#ifdef MACAPK_HAS_ANGLE
    if (!v) return;
    ScopedBufferBytes sb(env, v);
    if (sb.ptr) glUniform4fv(loc, count, static_cast<const GLfloat*>(sb.ptr));
#endif
}
void GLES20_glUniform1iv_arr(JNIEnv* env, jclass, jint loc, jint count, jintArray v, jint off) {
    GL_GATE("uniform");
#ifdef MACAPK_HAS_ANGLE
    if (!v) return;
    jsize len = env->GetArrayLength(v) - off;
    if (len <= 0) return;
    std::vector<GLint> tmp((size_t) len);
    env->GetIntArrayRegion(v, off, len, reinterpret_cast<jint*>(tmp.data()));
    glUniform1iv(loc, count, tmp.data());
#endif
}
void GLES20_glUniform1iv_buf(JNIEnv* env, jclass, jint loc, jint count, jobject v) {
    GL_GATE("uniform");
#ifdef MACAPK_HAS_ANGLE
    if (!v) return;
    ScopedBufferBytes sb(env, v);
    if (sb.ptr) glUniform1iv(loc, count, static_cast<const GLint*>(sb.ptr));
#endif
}
void GLES20_glUniform2iv_arr(JNIEnv* env, jclass, jint loc, jint count, jintArray v, jint off) {
    GL_GATE("uniform");
#ifdef MACAPK_HAS_ANGLE
    if (!v) return;
    jsize len = env->GetArrayLength(v) - off;
    if (len <= 0) return;
    std::vector<GLint> tmp((size_t) len);
    env->GetIntArrayRegion(v, off, len, reinterpret_cast<jint*>(tmp.data()));
    glUniform2iv(loc, count, tmp.data());
#endif
}
void GLES20_glUniform2iv_buf(JNIEnv* env, jclass, jint loc, jint count, jobject v) {
    GL_GATE("uniform");
#ifdef MACAPK_HAS_ANGLE
    if (!v) return;
    ScopedBufferBytes sb(env, v);
    if (sb.ptr) glUniform2iv(loc, count, static_cast<const GLint*>(sb.ptr));
#endif
}
void GLES20_glUniform3iv_arr(JNIEnv* env, jclass, jint loc, jint count, jintArray v, jint off) {
    GL_GATE("uniform");
#ifdef MACAPK_HAS_ANGLE
    if (!v) return;
    jsize len = env->GetArrayLength(v) - off;
    if (len <= 0) return;
    std::vector<GLint> tmp((size_t) len);
    env->GetIntArrayRegion(v, off, len, reinterpret_cast<jint*>(tmp.data()));
    glUniform3iv(loc, count, tmp.data());
#endif
}
void GLES20_glUniform3iv_buf(JNIEnv* env, jclass, jint loc, jint count, jobject v) {
    GL_GATE("uniform");
#ifdef MACAPK_HAS_ANGLE
    if (!v) return;
    ScopedBufferBytes sb(env, v);
    if (sb.ptr) glUniform3iv(loc, count, static_cast<const GLint*>(sb.ptr));
#endif
}
void GLES20_glUniform4iv_arr(JNIEnv* env, jclass, jint loc, jint count, jintArray v, jint off) {
    GL_GATE("uniform");
#ifdef MACAPK_HAS_ANGLE
    if (!v) return;
    jsize len = env->GetArrayLength(v) - off;
    if (len <= 0) return;
    std::vector<GLint> tmp((size_t) len);
    env->GetIntArrayRegion(v, off, len, reinterpret_cast<jint*>(tmp.data()));
    glUniform4iv(loc, count, tmp.data());
#endif
}
void GLES20_glUniform4iv_buf(JNIEnv* env, jclass, jint loc, jint count, jobject v) {
    GL_GATE("uniform");
#ifdef MACAPK_HAS_ANGLE
    if (!v) return;
    ScopedBufferBytes sb(env, v);
    if (sb.ptr) glUniform4iv(loc, count, static_cast<const GLint*>(sb.ptr));
#endif
}
void GLES20_glUniformMatrix2fv_arr(JNIEnv* env, jclass, jint loc, jint count, jboolean tr, jfloatArray v, jint off) {
    GL_GATE("uniform");
#ifdef MACAPK_HAS_ANGLE
    if (!v) return;
    jsize len = env->GetArrayLength(v) - off;
    if (len <= 0) return;
    std::vector<GLfloat> tmp((size_t) len);
    env->GetFloatArrayRegion(v, off, len, tmp.data());
    glUniformMatrix2fv(loc, count, tr, tmp.data());
#endif
}
void GLES20_glUniformMatrix2fv_buf(JNIEnv* env, jclass, jint loc, jint count, jboolean tr, jobject v) {
    GL_GATE("uniform");
#ifdef MACAPK_HAS_ANGLE
    if (!v) return;
    ScopedBufferBytes sb(env, v);
    if (sb.ptr) glUniformMatrix2fv(loc, count, tr, static_cast<const GLfloat*>(sb.ptr));
#endif
}
void GLES20_glUniformMatrix3fv_arr(JNIEnv* env, jclass, jint loc, jint count, jboolean tr, jfloatArray v, jint off) {
    GL_GATE("uniform");
#ifdef MACAPK_HAS_ANGLE
    if (!v) return;
    jsize len = env->GetArrayLength(v) - off;
    if (len <= 0) return;
    std::vector<GLfloat> tmp((size_t) len);
    env->GetFloatArrayRegion(v, off, len, tmp.data());
    glUniformMatrix3fv(loc, count, tr, tmp.data());
#endif
}
void GLES20_glUniformMatrix3fv_buf(JNIEnv* env, jclass, jint loc, jint count, jboolean tr, jobject v) {
    GL_GATE("uniform");
#ifdef MACAPK_HAS_ANGLE
    if (!v) return;
    ScopedBufferBytes sb(env, v);
    if (sb.ptr) glUniformMatrix3fv(loc, count, tr, static_cast<const GLfloat*>(sb.ptr));
#endif
}
void GLES20_glCompressedTexImage2D_buf(JNIEnv* env, jclass, jint t, jint l, jint fmt,
        jint w, jint h, jint border, jint imageSize, jobject data) {
    GL_GATE("texdata");
#ifdef MACAPK_HAS_ANGLE
    ScopedBufferBytes sb(env, data);
    glCompressedTexImage2D((GLenum)t, l, (GLenum)fmt, w, h, border, imageSize, sb.ptr);
#endif
}
void GLES20_glCompressedTexSubImage2D_buf(JNIEnv* env, jclass, jint t, jint l, jint xo, jint yo,
        jint w, jint h, jint fmt, jint imageSize, jobject data) {
    GL_GATE("texdata");
#ifdef MACAPK_HAS_ANGLE
    ScopedBufferBytes sb(env, data);
    glCompressedTexSubImage2D((GLenum)t, l, xo, yo, w, h, (GLenum)fmt, imageSize, sb.ptr);
#endif
}
void GLES20_glBufferSubData_buf(JNIEnv* env, jclass, jint target, jint offset, jint size, jobject data) {
    GL_GATE("texdata");
#ifdef MACAPK_HAS_ANGLE
    if (!data) return;
    ScopedBufferBytes sb(env, data);
    if (sb.ptr) glBufferSubData((GLenum)target, offset, size, sb.ptr);
#endif
}
// Real GL error reporting. Returning GL_NO_ERROR unconditionally (the previous
// Java body) meant an app's own error checks could never fire, so a failed
// call looked identical to a successful one.
jint GLES20_glGetError(JNIEnv*, jclass) {
#ifdef MACAPK_HAS_ANGLE
    return (jint) glGetError();
#else
    return 0;
#endif
}
// Real driver strings. GL_EXTENSIONS previously returned "", so every app
// feature-probe ("does this driver have GL_OES_depth_texture?") answered no
// even when ANGLE supports it.
jstring GLES20_glGetString(JNIEnv* env, jclass, jint name) {
#ifdef MACAPK_HAS_ANGLE
    // DIAGNOSTIC GATE (v683): MACAPK_GL_EMPTY_EXT=1 restores the pre-v682
    // behaviour of reporting an EMPTY extension string, to test whether an
    // engine's extension-driven feature selection is what regressed.
    if (name == 0x1F03 /* GL_EXTENSIONS */) {
        static int empty = -1;
        if (empty < 0) { const char* e = std::getenv("MACAPK_GL_EMPTY_EXT");
                         empty = (e && e[0] == '1') ? 1 : 0; }
        if (empty) return env->NewStringUTF("");
    }
    const GLubyte* s = glGetString((GLenum) name);
    if (s) return env->NewStringUTF(reinterpret_cast<const char*>(s));
#endif
    (void) name;
    return env->NewStringUTF("");
}
void GLES20_glGetFloatv(JNIEnv* env, jclass, jint pname, jfloatArray params, jint off) {
    GL_GATE("query");
#ifdef MACAPK_HAS_ANGLE
    if (!params || off < 0) return;
    jsize avail = env->GetArrayLength(params) - off;
    if (avail <= 0) return;
    std::vector<GLfloat> v(GlScratchCount(pname, avail), 0.0f);
    glGetFloatv((GLenum) pname, v.data());
    jsize copy = GlCopyCount(pname, avail);
    if ((size_t) copy > v.size()) copy = (jsize) v.size();
    if (copy > 0) env->SetFloatArrayRegion(params, off, copy, v.data());
#endif
}
void GLES20_glGetBooleanv(JNIEnv* env, jclass, jint pname, jbooleanArray params, jint off) {
    GL_GATE("query");
#ifdef MACAPK_HAS_ANGLE
    if (!params || off < 0) return;
    jsize avail = env->GetArrayLength(params) - off;
    if (avail <= 0) return;
    std::vector<GLboolean> v(GlScratchCount(pname, avail), (GLboolean) 0);
    glGetBooleanv((GLenum) pname, v.data());
    jsize copy = GlCopyCount(pname, avail);
    if ((size_t) copy > v.size()) copy = (jsize) v.size();
    if (copy > 0) env->SetBooleanArrayRegion(params, off, copy,
                                 reinterpret_cast<const jboolean*>(v.data()));
#endif
}
void GLES20_glGetTexParameteriv(JNIEnv* env, jclass, jint t, jint pn, jintArray params, jint off) {
    GL_GATE("query");
#ifdef MACAPK_HAS_ANGLE
    if (!params || off < 0) return;
    // GLES2 tex pnames are single-valued, but GL_TEXTURE_BORDER_COLOR (GLES 3.2
    // / EXT_texture_border_clamp, which ANGLE exposes) makes the driver write 4.
    // Same sizing the float sibling GLES20_glGetTexParameterfv already uses.
    GLint v[4] = {0, 0, 0, 0};
    glGetTexParameteriv((GLenum) t, (GLenum) pn, v);
    jsize avail = env->GetArrayLength(params) - off;
    if (avail > 4) avail = 4;
    if (avail > 0) env->SetIntArrayRegion(params, off, avail,
                                          reinterpret_cast<const jint*>(v));
#endif
}
void GLES20_glGetVertexAttribiv(JNIEnv* env, jclass, jint idx, jint pn, jintArray params, jint off) {
    GL_GATE("query");
#ifdef MACAPK_HAS_ANGLE
    if (!params || off < 0) return;
    // GL_CURRENT_VERTEX_ATTRIB makes the DRIVER write FOUR ints; every other
    // pname writes one. AOSP sizes its bounds check off exactly that switch
    // (frameworks/base/core/jni/android_opengl_GLES20.cpp:3909-3919,
    // android16-release, and identically for the fv form at :3968-3974).
    // A single `GLint v` scratch here was an unconditional stack smash the
    // moment an app asked for GL_CURRENT_VERTEX_ATTRIB -- the float sibling
    // GLES20_glGetVertexAttribfv below already had the right shape.
    GLint v[4] = {0, 0, 0, 0};
    glGetVertexAttribiv((GLuint) idx, (GLenum) pn, v);
    jsize avail = env->GetArrayLength(params) - off;
    if (avail > 4) avail = 4;
    if (avail > 0) env->SetIntArrayRegion(params, off, avail,
                                          reinterpret_cast<const jint*>(v));
#endif
}
void GLES20_glGetBufferParameteriv(JNIEnv* env, jclass, jint t, jint pn, jintArray params, jint off) {
    GL_GATE("query");
#ifdef MACAPK_HAS_ANGLE
    if (!params || off < 0) return;
    if (env->GetArrayLength(params) - off < 1) return;
    GLint v = 0;
    glGetBufferParameteriv((GLenum)t, (GLenum)pn, &v);
    env->SetIntArrayRegion(params, off, 1, reinterpret_cast<const jint*>(&v));
#endif
}
void GLES20_glGetFramebufferAttachmentParameteriv(JNIEnv* env, jclass, jint t, jint att,
        jint pn, jintArray params, jint off) {
    GL_GATE("query");
#ifdef MACAPK_HAS_ANGLE
    if (!params || off < 0) return;
    if (env->GetArrayLength(params) - off < 1) return;
    GLint v = 0;
    glGetFramebufferAttachmentParameteriv((GLenum)t, (GLenum)att, (GLenum)pn, &v);
    env->SetIntArrayRegion(params, off, 1, reinterpret_cast<const jint*>(&v));
#endif
}
void GLES20_glGetRenderbufferParameteriv(JNIEnv* env, jclass, jint t, jint pn,
        jintArray params, jint off) {
    GL_GATE("query");
#ifdef MACAPK_HAS_ANGLE
    if (!params || off < 0) return;
    if (env->GetArrayLength(params) - off < 1) return;
    GLint v = 0;
    glGetRenderbufferParameteriv((GLenum)t, (GLenum)pn, &v);
    env->SetIntArrayRegion(params, off, 1, reinterpret_cast<const jint*>(&v));
#endif
}

// ===========================================================================
// v1.0.836 -- the empty-bodied GLES20 no-op sweep.
//
// Everything below replaces a Java method whose body was `{}` (or, for the
// glGen*/glCheckFramebufferStatus/glIsEnabled group, a FABRICATED answer).
// AOSP declares every one of them native. The failure mode this closes has no
// signature at all -- no GL error, no exception, no log line -- which is why
// glDrawElements(...,Buffer) sat broken long enough to blank three libGDX apps
// (v1.0.834). See docs/gles20-noop-sweep_2026-07-29.md.
//
// House conventions followed here: GL_GATE("query") on the read-back paths,
// #ifdef MACAPK_HAS_ANGLE around every GL call, and out-parameters bounded by
// the CALLER's array length (GetArrayLength) rather than trusted blindly.
// ===========================================================================

// Copy exactly n floats out of a Java float[] at `off`, or return false if the
// array cannot supply them. Checking the length ourselves (instead of letting
// GetFloatArrayRegion throw) keeps us from returning into Java with a pending
// ArrayIndexOutOfBoundsException that the caller never expected.
static bool TakeFloats(JNIEnv* env, jfloatArray arr, jint off, int n, GLfloat* out) {
    if (!arr || off < 0) return false;
    if (env->GetArrayLength(arr) - off < n) return false;
    env->GetFloatArrayRegion(arr, off, n, out);
    return true;
}

// ---- Constant vertex attributes. A constant attribute (one with no enabled
// glVertexAttribPointer array behind it) takes its value from exactly these
// calls; dropping them left the attribute at the GL default (0,0,0,1), so
// geometry drew black / degenerate with a clean log. ----
void GLES20_glVertexAttrib1f(JNIEnv*, jclass, jint i, jfloat x) {
#ifdef MACAPK_HAS_ANGLE
    glVertexAttrib1f((GLuint) i, x);
#endif
}
void GLES20_glVertexAttrib2f(JNIEnv*, jclass, jint i, jfloat x, jfloat y) {
#ifdef MACAPK_HAS_ANGLE
    glVertexAttrib2f((GLuint) i, x, y);
#endif
}
void GLES20_glVertexAttrib3f(JNIEnv*, jclass, jint i, jfloat x, jfloat y, jfloat z) {
#ifdef MACAPK_HAS_ANGLE
    glVertexAttrib3f((GLuint) i, x, y, z);
#endif
}
void GLES20_glVertexAttrib4f(JNIEnv*, jclass, jint i, jfloat x, jfloat y, jfloat z, jfloat w) {
#ifdef MACAPK_HAS_ANGLE
    glVertexAttrib4f((GLuint) i, x, y, z, w);
#endif
}
void GLES20_glVertexAttrib1fv_arr(JNIEnv* env, jclass, jint i, jfloatArray v, jint off) {
#ifdef MACAPK_HAS_ANGLE
    GLfloat f[1]; if (TakeFloats(env, v, off, 1, f)) glVertexAttrib1fv((GLuint) i, f);
#else
    (void) env; (void) i; (void) v; (void) off;
#endif
}
void GLES20_glVertexAttrib2fv_arr(JNIEnv* env, jclass, jint i, jfloatArray v, jint off) {
#ifdef MACAPK_HAS_ANGLE
    GLfloat f[2]; if (TakeFloats(env, v, off, 2, f)) glVertexAttrib2fv((GLuint) i, f);
#else
    (void) env; (void) i; (void) v; (void) off;
#endif
}
void GLES20_glVertexAttrib3fv_arr(JNIEnv* env, jclass, jint i, jfloatArray v, jint off) {
#ifdef MACAPK_HAS_ANGLE
    GLfloat f[3]; if (TakeFloats(env, v, off, 3, f)) glVertexAttrib3fv((GLuint) i, f);
#else
    (void) env; (void) i; (void) v; (void) off;
#endif
}
void GLES20_glVertexAttrib4fv_arr(JNIEnv* env, jclass, jint i, jfloatArray v, jint off) {
#ifdef MACAPK_HAS_ANGLE
    GLfloat f[4]; if (TakeFloats(env, v, off, 4, f)) glVertexAttrib4fv((GLuint) i, f);
#else
    (void) env; (void) i; (void) v; (void) off;
#endif
}

// ---- glTexParameterfv: the array-form sibling of the scalar glTexParameterf
// that was fixed in v1.0.682. GLES2 pnames are single-valued; GL_TEXTURE_
// BORDER_COLOR (an EXT/GLES3.2 addition) takes 4, so accept up to 4. ----
void GLES20_glTexParameterfv_arr(JNIEnv* env, jclass, jint t, jint pn, jfloatArray v, jint off) {
#ifdef MACAPK_HAS_ANGLE
    GLfloat f[4] = {0, 0, 0, 0};
    if (!v || off < 0) return;
    jsize avail = env->GetArrayLength(v) - off;
    if (avail <= 0) return;
    jsize take = avail > 4 ? 4 : avail;
    env->GetFloatArrayRegion(v, off, take, f);
    glTexParameterfv((GLenum) t, (GLenum) pn, f);
#else
    (void) env; (void) t; (void) pn; (void) v; (void) off;
#endif
}

// ---- Read-back queries whose out-parameter was never written. Each of these
// handed the caller back its own zeros. ----
void GLES20_glGetTexParameterfv(JNIEnv* env, jclass, jint t, jint pn, jfloatArray params, jint off) {
    GL_GATE("query");
#ifdef MACAPK_HAS_ANGLE
    if (!params || off < 0) return;
    GLfloat v[4] = {0, 0, 0, 0};
    glGetTexParameterfv((GLenum) t, (GLenum) pn, v);
    jsize avail = env->GetArrayLength(params) - off;
    if (avail > 4) avail = 4;
    if (avail > 0) env->SetFloatArrayRegion(params, off, avail, v);
#endif
}
void GLES20_glGetVertexAttribfv(JNIEnv* env, jclass, jint idx, jint pn, jfloatArray params, jint off) {
    GL_GATE("query");
#ifdef MACAPK_HAS_ANGLE
    if (!params || off < 0) return;
    // GL_CURRENT_VERTEX_ATTRIB returns 4 floats; every other pname returns 1.
    GLfloat v[4] = {0, 0, 0, 0};
    glGetVertexAttribfv((GLuint) idx, (GLenum) pn, v);
    jsize avail = env->GetArrayLength(params) - off;
    if (avail > 4) avail = 4;
    if (avail > 0) env->SetFloatArrayRegion(params, off, avail, v);
#endif
}
// glGetUniformfv/iv write as many components as the UNIFORM has (a mat4 is 16
// floats, a uniform array can be far more), and there is no cheap way to learn
// that count from a location alone. So we do what AOSP does: hand GL a pointer
// into the caller's own array and let the caller own the sizing, exactly as on
// real Android. Pinning the array (rather than a fixed scratch buffer) is what
// makes that safe -- a fixed buffer would be a stack overflow waiting for the
// first `uniform mat4 bones[32]` introspection call.
void GLES20_glGetUniformfv(JNIEnv* env, jclass, jint program, jint loc, jfloatArray params, jint off) {
    GL_GATE("query");
#ifdef MACAPK_HAS_ANGLE
    if (!params || off < 0 || env->GetArrayLength(params) <= off) return;
    jfloat* p = env->GetFloatArrayElements(params, nullptr);
    if (!p) return;
    glGetUniformfv((GLuint) program, loc, p + off);
    env->ReleaseFloatArrayElements(params, p, 0);   // mode 0 = copy back
#endif
}
void GLES20_glGetUniformiv(JNIEnv* env, jclass, jint program, jint loc, jintArray params, jint off) {
    GL_GATE("query");
#ifdef MACAPK_HAS_ANGLE
    if (!params || off < 0 || env->GetArrayLength(params) <= off) return;
    jint* p = env->GetIntArrayElements(params, nullptr);
    if (!p) return;
    glGetUniformiv((GLuint) program, loc, reinterpret_cast<GLint*>(p + off));
    env->ReleaseIntArrayElements(params, p, 0);
#endif
}
// range[2] + precision[1], both spec-fixed sizes. An engine that queries this
// to decide whether to emit `highp` was reading range={0,0} and concluding high
// precision is unavailable.
void GLES20_glGetShaderPrecisionFormat(JNIEnv* env, jclass, jint stype, jint ptype,
        jintArray range, jint roff, jintArray precision, jint poff) {
    GL_GATE("query");
#ifdef MACAPK_HAS_ANGLE
    GLint r[2] = {0, 0};
    GLint p = 0;
    glGetShaderPrecisionFormat((GLenum) stype, (GLenum) ptype, r, &p);
    if (range && roff >= 0) {
        jsize avail = env->GetArrayLength(range) - roff;
        if (avail > 2) avail = 2;
        if (avail > 0) env->SetIntArrayRegion(range, roff, avail, reinterpret_cast<const jint*>(r));
    }
    if (precision && poff >= 0 && env->GetArrayLength(precision) - poff >= 1)
        env->SetIntArrayRegion(precision, poff, 1, reinterpret_cast<const jint*>(&p));
#else
    (void) env; (void) stype; (void) ptype; (void) range; (void) roff; (void) precision; (void) poff;
#endif
}
void GLES20_glGetAttachedShaders(JNIEnv* env, jclass, jint program, jint maxcount,
        jintArray count, jint coff, jintArray shaders, jint soff) {
    GL_GATE("query");
#ifdef MACAPK_HAS_ANGLE
    if (maxcount < 0) return;
    // Bound by BOTH the caller's declared maxcount and the real capacity of the
    // out-array, so a caller that lies about maxcount cannot make GL overrun it.
    jsize cap = (shaders && soff >= 0) ? env->GetArrayLength(shaders) - soff : 0;
    if (cap < 0) cap = 0;
    jsize want = (maxcount < cap) ? maxcount : cap;
    std::vector<GLuint> ids(want > 0 ? (size_t) want : 1, 0);
    GLsizei n = 0;
    glGetAttachedShaders((GLuint) program, (GLsizei) want, &n, ids.data());
    if (n > want) n = (GLsizei) want;
    if (shaders && n > 0)
        env->SetIntArrayRegion(shaders, soff, n, reinterpret_cast<const jint*>(ids.data()));
    if (count && coff >= 0 && env->GetArrayLength(count) - coff >= 1) {
        jint jn = (jint) n;
        env->SetIntArrayRegion(count, coff, 1, &jn);
    }
#else
    (void) env; (void) program; (void) maxcount; (void) count; (void) coff; (void) shaders; (void) soff;
#endif
}
void GLES20_glGetShaderSource(JNIEnv* env, jclass, jint shader, jint bufsize,
        jintArray length, jint loff, jbyteArray source, jint soff) {
    GL_GATE("query");
#ifdef MACAPK_HAS_ANGLE
    if (bufsize <= 0) return;
    jsize cap = (source && soff >= 0) ? env->GetArrayLength(source) - soff : 0;
    if (cap <= 0) return;
    jsize want = (bufsize < cap) ? bufsize : cap;
    std::vector<char> buf((size_t) want, 0);
    GLsizei n = 0;
    glGetShaderSource((GLuint) shader, (GLsizei) want, &n, buf.data());
    if (n > want) n = (GLsizei) want;
    if (n > 0) env->SetByteArrayRegion(source, soff, n, reinterpret_cast<const jbyte*>(buf.data()));
    if (length && loff >= 0 && env->GetArrayLength(length) - loff >= 1) {
        jint jn = (jint) n;
        env->SetIntArrayRegion(length, loff, 1, &jn);
    }
#else
    (void) env; (void) shader; (void) bufsize; (void) length; (void) loff; (void) source; (void) soff;
#endif
}

// ---- glReadPixels. AOSP's generated JNI resolves the Buffer to a raw pointer
// with NO size check: the app owns the sizing, and an undersized buffer
// corrupts memory on real Android too. We match that contract rather than
// inventing a truncation Android does not do -- but the write_back flag is
// mandatory here, because on the heap-buffer path ScopedBufferBytes pins a COPY
// and the default JNI_ABORT release would silently discard every pixel GL just
// wrote.
//
// This binding is NOT the one Mac.apk's own render instrument uses: self-capture
// and the GL frame dumps call C++ glReadPixels directly from angle_glue.cpp /
// cocoa_bridge.mm, so nothing here can move a render verdict. ----
void GLES20_glReadPixels_buf(JNIEnv* env, jclass, jint x, jint y, jint w, jint h,
        jint format, jint type, jobject pixels) {
#ifdef MACAPK_HAS_ANGLE
    if (!pixels || w <= 0 || h <= 0) return;
    ScopedBufferBytes b(env, pixels, /*write_back=*/true);
    if (!b.ptr) return;
    glReadPixels(x, y, w, h, (GLenum) format, (GLenum) type, b.ptr);
    if (gldraw_trace()) {
        static long n = 0;
        if (++n <= 5 || (n % 600) == 0)
            std::fprintf(stderr, "[GLDRAW] glReadPixels(%d,%d,%dx%d fmt=0x%x type=0x%x)"
                                 " #%ld err=0x%x\n", x, y, w, h, format, type, n, glGetError());
    }
#else
    (void) env; (void) x; (void) y; (void) w; (void) h; (void) format; (void) type; (void) pixels;
#endif
}

// ---- glShaderBinary. Bridged precisely BECAUSE ANGLE supports zero binary
// formats: the real entry point raises GL_INVALID_ENUM, which is what makes a
// precompiled-shader loader fall back to source. The old silent no-op reported
// success and the fallback never ran. ----
void GLES20_glShaderBinary_arr(JNIEnv* env, jclass, jint n, jintArray shaders, jint off,
        jint binaryformat, jobject binary, jint length) {
#ifdef MACAPK_HAS_ANGLE
    if (n <= 0 || !shaders || off < 0) return;
    jsize cap = env->GetArrayLength(shaders) - off;
    if (cap <= 0) return;
    jsize want = (n < cap) ? n : cap;
    std::vector<jint> ids((size_t) want, 0);
    env->GetIntArrayRegion(shaders, off, want, ids.data());
    ScopedBufferBytes b(env, binary);
    glShaderBinary((GLsizei) want, reinterpret_cast<const GLuint*>(ids.data()),
                   (GLenum) binaryformat, b.ptr, (GLsizei) length);
#else
    (void) env; (void) n; (void) shaders; (void) off; (void) binaryformat; (void) binary; (void) length;
#endif
}

// ---- The fabricated-answer group. These were not empty bodies; they were
// worse -- they returned a plausible LIE.
//
// glGenFramebuffers/glGenRenderbuffers handed out an invented `nextName++`.
// GLES2 requires glBindFramebuffer to raise GL_INVALID_OPERATION for a name
// that was never generated, so with the real native glBindFramebuffer already
// in place, the FBO could never actually be bound -- and
// glCheckFramebufferStatus returning GL_FRAMEBUFFER_COMPLETE unconditionally
// hid exactly that, because libGDX's GLFrameBuffer.build() trusts it and
// throws only on a non-COMPLETE answer. ----
void GLES20_nativeGlGenFramebuffers(JNIEnv* env, jclass, jint n, jintArray out) {
#ifdef MACAPK_HAS_ANGLE
    if (n <= 0 || !out) return;
    jsize cap = env->GetArrayLength(out);
    if (cap < n) return;
    std::vector<GLuint> ids((size_t) n, 0);
    glGenFramebuffers((GLsizei) n, ids.data());
    env->SetIntArrayRegion(out, 0, n, reinterpret_cast<const jint*>(ids.data()));
#else
    (void) env; (void) n; (void) out;
#endif
}
void GLES20_nativeGlGenRenderbuffers(JNIEnv* env, jclass, jint n, jintArray out) {
#ifdef MACAPK_HAS_ANGLE
    if (n <= 0 || !out) return;
    jsize cap = env->GetArrayLength(out);
    if (cap < n) return;
    std::vector<GLuint> ids((size_t) n, 0);
    glGenRenderbuffers((GLsizei) n, ids.data());
    env->SetIntArrayRegion(out, 0, n, reinterpret_cast<const jint*>(ids.data()));
#else
    (void) env; (void) n; (void) out;
#endif
}
jint GLES20_glCheckFramebufferStatus(JNIEnv*, jclass, jint target) {
#ifdef MACAPK_HAS_ANGLE
    return (jint) glCheckFramebufferStatus((GLenum) target);
#else
    (void) target;
    return 0x8CD5 /* GL_FRAMEBUFFER_COMPLETE */;
#endif
}
// A hardcoded `false` broke the save/restore idiom outright: every
// `boolean was = glIsEnabled(GL_BLEND); ...; if (was) glEnable(GL_BLEND);`
// restored the capability to OFF.
jboolean GLES20_glIsEnabled(JNIEnv*, jclass, jint cap) {
#ifdef MACAPK_HAS_ANGLE
    return glIsEnabled((GLenum) cap) ? JNI_TRUE : JNI_FALSE;
#else
    (void) cap;
    return JNI_FALSE;
#endif
}

const JNINativeMethod kGles20Methods[] = {
    {(char*)"glBlendColor", (char*)"(FFFF)V", (void*)GLES20_glBlendColor},
    {(char*)"glBlendEquation", (char*)"(I)V", (void*)GLES20_glBlendEquation},
    {(char*)"glBlendEquationSeparate", (char*)"(II)V", (void*)GLES20_glBlendEquationSeparate},
    {(char*)"glBlendFuncSeparate", (char*)"(IIII)V", (void*)GLES20_glBlendFuncSeparate},
    {(char*)"glClearDepthf", (char*)"(F)V", (void*)GLES20_glClearDepthf},
    {(char*)"glClearStencil", (char*)"(I)V", (void*)GLES20_glClearStencil},
    {(char*)"glColorMask", (char*)"(ZZZZ)V", (void*)GLES20_glColorMask},
    {(char*)"glCullFace", (char*)"(I)V", (void*)GLES20_glCullFace},
    {(char*)"glDepthFunc", (char*)"(I)V", (void*)GLES20_glDepthFunc},
    {(char*)"glDepthMask", (char*)"(Z)V", (void*)GLES20_glDepthMask},
    {(char*)"glDepthRangef", (char*)"(FF)V", (void*)GLES20_glDepthRangef},
    {(char*)"glFrontFace", (char*)"(I)V", (void*)GLES20_glFrontFace},
    {(char*)"glLineWidth", (char*)"(F)V", (void*)GLES20_glLineWidth},
    {(char*)"glPolygonOffset", (char*)"(FF)V", (void*)GLES20_glPolygonOffset},
    {(char*)"glSampleCoverage", (char*)"(FZ)V", (void*)GLES20_glSampleCoverage},
    {(char*)"glScissor", (char*)"(IIII)V", (void*)GLES20_glScissor},
    {(char*)"glHint", (char*)"(II)V", (void*)GLES20_glHint},
    {(char*)"glFinish", (char*)"()V", (void*)GLES20_glFinish},
    {(char*)"glFlush", (char*)"()V", (void*)GLES20_glFlush},
    {(char*)"glStencilFunc", (char*)"(III)V", (void*)GLES20_glStencilFunc},
    {(char*)"glStencilFuncSeparate", (char*)"(IIII)V", (void*)GLES20_glStencilFuncSeparate},
    {(char*)"glStencilMask", (char*)"(I)V", (void*)GLES20_glStencilMask},
    {(char*)"glStencilMaskSeparate", (char*)"(II)V", (void*)GLES20_glStencilMaskSeparate},
    {(char*)"glStencilOp", (char*)"(III)V", (void*)GLES20_glStencilOp},
    {(char*)"glStencilOpSeparate", (char*)"(IIII)V", (void*)GLES20_glStencilOpSeparate},
    {(char*)"glBindFramebuffer", (char*)"(II)V", (void*)GLES20_glBindFramebuffer},
    {(char*)"glBindRenderbuffer", (char*)"(II)V", (void*)GLES20_glBindRenderbuffer},
    {(char*)"glFramebufferRenderbuffer", (char*)"(IIII)V", (void*)GLES20_glFramebufferRenderbuffer},
    {(char*)"glFramebufferTexture2D", (char*)"(IIIII)V", (void*)GLES20_glFramebufferTexture2D},
    {(char*)"glRenderbufferStorage", (char*)"(IIII)V", (void*)GLES20_glRenderbufferStorage},
    {(char*)"glGenerateMipmap", (char*)"(I)V", (void*)GLES20_glGenerateMipmap},
    {(char*)"glDeleteProgram", (char*)"(I)V", (void*)GLES20_glDeleteProgram},
    {(char*)"glDeleteShader", (char*)"(I)V", (void*)GLES20_glDeleteShader},
    {(char*)"glDetachShader", (char*)"(II)V", (void*)GLES20_glDetachShader},
    {(char*)"glValidateProgram", (char*)"(I)V", (void*)GLES20_glValidateProgram},
    {(char*)"glReleaseShaderCompiler", (char*)"()V", (void*)GLES20_glReleaseShaderCompiler},
    {(char*)"glTexParameterf", (char*)"(IIF)V", (void*)GLES20_glTexParameterf},
    {(char*)"glCopyTexImage2D", (char*)"(IIIIIIII)V", (void*)GLES20_glCopyTexImage2D},
    {(char*)"glCopyTexSubImage2D", (char*)"(IIIIIIII)V", (void*)GLES20_glCopyTexSubImage2D},
    {(char*)"glUniform2i", (char*)"(III)V", (void*)GLES20_glUniform2i},
    {(char*)"glUniform3i", (char*)"(IIII)V", (void*)GLES20_glUniform3i},
    {(char*)"glUniform4i", (char*)"(IIIII)V", (void*)GLES20_glUniform4i},
    {(char*)"glDeleteBuffers", (char*)"(I[II)V", (void*)GLES20_glDeleteBuffers_arr},
    {(char*)"glDeleteBuffers", (char*)"(ILjava/nio/IntBuffer;)V", (void*)GLES20_glDeleteBuffers_buf},
    {(char*)"glDeleteTextures", (char*)"(I[II)V", (void*)GLES20_glDeleteTextures_arr},
    {(char*)"glDeleteTextures", (char*)"(ILjava/nio/IntBuffer;)V", (void*)GLES20_glDeleteTextures_buf},
    {(char*)"glDeleteFramebuffers", (char*)"(I[II)V", (void*)GLES20_glDeleteFramebuffers_arr},
    {(char*)"glDeleteFramebuffers", (char*)"(ILjava/nio/IntBuffer;)V", (void*)GLES20_glDeleteFramebuffers_buf},
    {(char*)"glDeleteRenderbuffers", (char*)"(I[II)V", (void*)GLES20_glDeleteRenderbuffers_arr},
    {(char*)"glDeleteRenderbuffers", (char*)"(ILjava/nio/IntBuffer;)V", (void*)GLES20_glDeleteRenderbuffers_buf},
    {(char*)"glUniform1fv", (char*)"(II[FI)V", (void*)GLES20_glUniform1fv_arr},
    {(char*)"glUniform1fv", (char*)"(IILjava/nio/FloatBuffer;)V", (void*)GLES20_glUniform1fv_buf},
    {(char*)"glUniform2fv", (char*)"(II[FI)V", (void*)GLES20_glUniform2fv_arr},
    {(char*)"glUniform2fv", (char*)"(IILjava/nio/FloatBuffer;)V", (void*)GLES20_glUniform2fv_buf},
    {(char*)"glUniform3fv", (char*)"(II[FI)V", (void*)GLES20_glUniform3fv_arr},
    {(char*)"glUniform3fv", (char*)"(IILjava/nio/FloatBuffer;)V", (void*)GLES20_glUniform3fv_buf},
    {(char*)"glUniform4fv", (char*)"(II[FI)V", (void*)GLES20_glUniform4fv_arr},
    {(char*)"glUniform4fv", (char*)"(IILjava/nio/FloatBuffer;)V", (void*)GLES20_glUniform4fv_buf},
    {(char*)"glUniform1iv", (char*)"(II[II)V", (void*)GLES20_glUniform1iv_arr},
    {(char*)"glUniform1iv", (char*)"(IILjava/nio/IntBuffer;)V", (void*)GLES20_glUniform1iv_buf},
    {(char*)"glUniform2iv", (char*)"(II[II)V", (void*)GLES20_glUniform2iv_arr},
    {(char*)"glUniform2iv", (char*)"(IILjava/nio/IntBuffer;)V", (void*)GLES20_glUniform2iv_buf},
    {(char*)"glUniform3iv", (char*)"(II[II)V", (void*)GLES20_glUniform3iv_arr},
    {(char*)"glUniform3iv", (char*)"(IILjava/nio/IntBuffer;)V", (void*)GLES20_glUniform3iv_buf},
    {(char*)"glUniform4iv", (char*)"(II[II)V", (void*)GLES20_glUniform4iv_arr},
    {(char*)"glUniform4iv", (char*)"(IILjava/nio/IntBuffer;)V", (void*)GLES20_glUniform4iv_buf},
    {(char*)"glUniformMatrix2fv", (char*)"(IIZ[FI)V", (void*)GLES20_glUniformMatrix2fv_arr},
    {(char*)"glUniformMatrix2fv", (char*)"(IIZLjava/nio/FloatBuffer;)V", (void*)GLES20_glUniformMatrix2fv_buf},
    {(char*)"glUniformMatrix3fv", (char*)"(IIZ[FI)V", (void*)GLES20_glUniformMatrix3fv_arr},
    {(char*)"glUniformMatrix3fv", (char*)"(IIZLjava/nio/FloatBuffer;)V", (void*)GLES20_glUniformMatrix3fv_buf},
    {(char*)"glCompressedTexImage2D", (char*)"(IIIIIIILjava/nio/Buffer;)V", (void*)GLES20_glCompressedTexImage2D_buf},
    {(char*)"glCompressedTexSubImage2D", (char*)"(IIIIIIIILjava/nio/Buffer;)V", (void*)GLES20_glCompressedTexSubImage2D_buf},
    {(char*)"glBufferSubData", (char*)"(IIILjava/nio/Buffer;)V", (void*)GLES20_glBufferSubData_buf},
    {(char*)"glGetError", (char*)"()I", (void*)GLES20_glGetError},
    {(char*)"glGetString", (char*)"(I)Ljava/lang/String;", (void*)GLES20_glGetString},
    {(char*)"glGetFloatv", (char*)"(I[FI)V", (void*)GLES20_glGetFloatv},
    {(char*)"glGetBooleanv", (char*)"(I[ZI)V", (void*)GLES20_glGetBooleanv},
    {(char*)"glGetTexParameteriv", (char*)"(II[II)V", (void*)GLES20_glGetTexParameteriv},
    {(char*)"glGetVertexAttribiv", (char*)"(II[II)V", (void*)GLES20_glGetVertexAttribiv},
    {(char*)"glGetBufferParameteriv", (char*)"(II[II)V", (void*)GLES20_glGetBufferParameteriv},
    {(char*)"glGetFramebufferAttachmentParameteriv", (char*)"(III[II)V", (void*)GLES20_glGetFramebufferAttachmentParameteriv},
    {(char*)"glGetRenderbufferParameteriv", (char*)"(II[II)V", (void*)GLES20_glGetRenderbufferParameteriv},
    {(char*)"glViewport",            (char*)"(IIII)V",          (void*)GLES20_glViewport},
    {(char*)"glClearColor",          (char*)"(FFFF)V",          (void*)GLES20_glClearColor},
    {(char*)"glClear",               (char*)"(I)V",             (void*)GLES20_glClear},
    {(char*)"glEnable",              (char*)"(I)V",             (void*)GLES20_glEnable},
    {(char*)"glDisable",             (char*)"(I)V",             (void*)GLES20_glDisable},
    {(char*)"glBlendFunc",           (char*)"(II)V",            (void*)GLES20_glBlendFunc},
    {(char*)"glPixelStorei",         (char*)"(II)V",            (void*)GLES20_glPixelStorei},
    {(char*)"glActiveTexture",       (char*)"(I)V",             (void*)GLES20_glActiveTexture},
    {(char*)"glBindTexture",         (char*)"(II)V",            (void*)GLES20_glBindTexture},
    {(char*)"glTexParameteri",       (char*)"(III)V",           (void*)GLES20_glTexParameteri},
    {(char*)"glBindBuffer",          (char*)"(II)V",            (void*)GLES20_glBindBuffer},
    {(char*)"glDrawArrays",          (char*)"(III)V",           (void*)GLES20_glDrawArrays},
    {(char*)"glDrawElements",        (char*)"(IIII)V",          (void*)GLES20_glDrawElements},
    {(char*)"glEnableVertexAttribArray",  (char*)"(I)V",        (void*)GLES20_glEnableVertexAttribArray},
    {(char*)"glDisableVertexAttribArray", (char*)"(I)V",        (void*)GLES20_glDisableVertexAttribArray},
    {(char*)"glVertexAttribPointer", (char*)"(IIIZII)V",        (void*)GLES20_glVertexAttribPointerOffset},
    {(char*)"glCreateShader",        (char*)"(I)I",             (void*)GLES20_glCreateShader},
    {(char*)"glShaderSource",        (char*)"(ILjava/lang/String;)V", (void*)GLES20_glShaderSource},
    {(char*)"glCompileShader",       (char*)"(I)V",             (void*)GLES20_glCompileShader},
    {(char*)"glGetShaderiv",         (char*)"(II[II)V",         (void*)GLES20_glGetShaderiv},
    {(char*)"glGetShaderInfoLog",    (char*)"(I)Ljava/lang/String;", (void*)GLES20_glGetShaderInfoLog},
    {(char*)"glCreateProgram",       (char*)"()I",              (void*)GLES20_glCreateProgram},
    {(char*)"glAttachShader",        (char*)"(II)V",            (void*)GLES20_glAttachShader},
    {(char*)"glBindAttribLocation",  (char*)"(IILjava/lang/String;)V", (void*)GLES20_glBindAttribLocation},
    {(char*)"glLinkProgram",         (char*)"(I)V",             (void*)GLES20_glLinkProgram},
    {(char*)"glGetProgramiv",        (char*)"(II[II)V",         (void*)GLES20_glGetProgramiv},
    {(char*)"glGetProgramInfoLog",   (char*)"(I)Ljava/lang/String;", (void*)GLES20_glGetProgramInfoLog},
    {(char*)"glUseProgram",          (char*)"(I)V",             (void*)GLES20_glUseProgram},
    {(char*)"glGetUniformLocation",  (char*)"(ILjava/lang/String;)I", (void*)GLES20_glGetUniformLocation},
    {(char*)"glGetAttribLocation",   (char*)"(ILjava/lang/String;)I", (void*)GLES20_glGetAttribLocation},
    {(char*)"glUniform1i",           (char*)"(II)V",            (void*)GLES20_glUniform1i},
    {(char*)"glUniform1f",           (char*)"(IF)V",            (void*)GLES20_glUniform1f},
    {(char*)"glUniform2f",           (char*)"(IFF)V",           (void*)GLES20_glUniform2f},
    {(char*)"glUniform3f",           (char*)"(IFFF)V",          (void*)GLES20_glUniform3f},
    {(char*)"glUniform4f",           (char*)"(IFFFF)V",         (void*)GLES20_glUniform4f},
    {(char*)"glUniformMatrix4fv",    (char*)"(IIZ[FI)V",        (void*)GLES20_glUniformMatrix4fv},
    {(char*)"glGetIntegerv",         (char*)"(I[II)V",          (void*)GLES20_glGetIntegerv},
    {(char*)"glGetActiveAttrib",     (char*)"(III[II[II[II[BI)V", (void*)GLES20_glGetActiveAttrib},
    {(char*)"glGetActiveUniform",    (char*)"(III[II[II[II[BI)V", (void*)GLES20_glGetActiveUniform},
    {(char*)"nativeGlGenTextures",   (char*)"(I[I)V",           (void*)GLES20_nativeGlGenTextures},
    {(char*)"nativeGlGenBuffers",    (char*)"(I[I)V",           (void*)GLES20_nativeGlGenBuffers},
    {(char*)"glBufferData",          (char*)"(IILjava/nio/Buffer;I)V",      (void*)GLES20_glBufferData_buf},
    {(char*)"glTexImage2D",          (char*)"(IIIIIIIILjava/nio/Buffer;)V", (void*)GLES20_glTexImage2D_buf},
    {(char*)"glTexSubImage2D",       (char*)"(IIIIIIIILjava/nio/Buffer;)V", (void*)GLES20_glTexSubImage2D_buf},
    {(char*)"glVertexAttribPointer", (char*)"(IIIZILjava/nio/Buffer;)V",    (void*)GLES20_glVertexAttribPointer_buf},
    {(char*)"glDrawElements",        (char*)"(IIILjava/nio/Buffer;)V",      (void*)GLES20_glDrawElements_buf},

    // ---- v1.0.836: the empty-bodied / fabricated-answer sweep. Each row here
    // has a matching `native` declaration added to GLES20.java in the SAME
    // change: Bind() aborts the process if RegisterNatives fails, and a native
    // registered against a non-native Java declaration raises NoSuchMethodError,
    // so these two halves can never be landed separately. ----
    {(char*)"glVertexAttrib1f",      (char*)"(IF)V",                        (void*)GLES20_glVertexAttrib1f},
    {(char*)"glVertexAttrib2f",      (char*)"(IFF)V",                       (void*)GLES20_glVertexAttrib2f},
    {(char*)"glVertexAttrib3f",      (char*)"(IFFF)V",                      (void*)GLES20_glVertexAttrib3f},
    {(char*)"glVertexAttrib4f",      (char*)"(IFFFF)V",                     (void*)GLES20_glVertexAttrib4f},
    {(char*)"glVertexAttrib1fv",     (char*)"(I[FI)V",                      (void*)GLES20_glVertexAttrib1fv_arr},
    {(char*)"glVertexAttrib2fv",     (char*)"(I[FI)V",                      (void*)GLES20_glVertexAttrib2fv_arr},
    {(char*)"glVertexAttrib3fv",     (char*)"(I[FI)V",                      (void*)GLES20_glVertexAttrib3fv_arr},
    {(char*)"glVertexAttrib4fv",     (char*)"(I[FI)V",                      (void*)GLES20_glVertexAttrib4fv_arr},
    {(char*)"glTexParameterfv",      (char*)"(II[FI)V",                     (void*)GLES20_glTexParameterfv_arr},
    {(char*)"glGetTexParameterfv",   (char*)"(II[FI)V",                     (void*)GLES20_glGetTexParameterfv},
    {(char*)"glGetVertexAttribfv",   (char*)"(II[FI)V",                     (void*)GLES20_glGetVertexAttribfv},
    {(char*)"glGetUniformfv",        (char*)"(II[FI)V",                     (void*)GLES20_glGetUniformfv},
    {(char*)"glGetUniformiv",        (char*)"(II[II)V",                     (void*)GLES20_glGetUniformiv},
    {(char*)"glGetShaderPrecisionFormat", (char*)"(II[II[II)V",             (void*)GLES20_glGetShaderPrecisionFormat},
    {(char*)"glGetAttachedShaders",  (char*)"(II[II[II)V",                  (void*)GLES20_glGetAttachedShaders},
    {(char*)"glGetShaderSource",     (char*)"(II[II[BI)V",                  (void*)GLES20_glGetShaderSource},
    {(char*)"glReadPixels",          (char*)"(IIIIIILjava/nio/Buffer;)V",   (void*)GLES20_glReadPixels_buf},
    {(char*)"glShaderBinary",        (char*)"(I[IIILjava/nio/Buffer;I)V",   (void*)GLES20_glShaderBinary_arr},
    {(char*)"nativeGlGenFramebuffers",  (char*)"(I[I)V",                    (void*)GLES20_nativeGlGenFramebuffers},
    {(char*)"nativeGlGenRenderbuffers", (char*)"(I[I)V",                    (void*)GLES20_nativeGlGenRenderbuffers},
    {(char*)"glCheckFramebufferStatus", (char*)"(I)I",                      (void*)GLES20_glCheckFramebufferStatus},
    {(char*)"glIsEnabled",           (char*)"(I)Z",                         (void*)GLES20_glIsEnabled},
};

const JNINativeMethod kGLUtilsMethods[] = {
    {(char*)"nativeTexImage2DRgba",  (char*)"(IIIIIIII[B)V",    (void*)GLUtils_nativeTexImage2DRgba},
    {(char*)"nativeTexSubImage2DRgba", (char*)"(IIIIIIII[B)V",  (void*)GLUtils_nativeTexSubImage2DRgba},
};

