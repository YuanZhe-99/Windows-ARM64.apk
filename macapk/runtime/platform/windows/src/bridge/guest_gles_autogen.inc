// Auto-generated from Khronos <GLES/gl.h>, <GLES2/gl2.h>, <GLES2/gl2ext.h>, <GLES3/gl3*.h>.
// Direct calling-thread ARM64 guest GLES wrappers (no host-callback or variadic parameters).

void GL_APIENTRY G_glAlphaFunc(GLenum func, GLfloat ref) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum func, GLfloat ref)>(ResolveAngleGl("glAlphaFunc"));
    if (fn) fn(func, ref); else NoteGlUnsupported();
}
void GL_APIENTRY G_glClearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)>(ResolveAngleGl("glClearColor"));
    if (fn) fn(red, green, blue, alpha); else NoteGlUnsupported();
}
void GL_APIENTRY G_glClearDepthf(GLfloat d) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLfloat d)>(ResolveAngleGl("glClearDepthf"));
    if (fn) fn(d); else NoteGlUnsupported();
}
void GL_APIENTRY G_glClipPlanef(GLenum p, const GLfloat *eqn) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum p, const GLfloat *eqn)>(ResolveAngleGl("glClipPlanef"));
    if (fn) fn(p, eqn); else NoteGlUnsupported();
}
void GL_APIENTRY G_glColor4f(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)>(ResolveAngleGl("glColor4f"));
    if (fn) fn(red, green, blue, alpha); else NoteGlUnsupported();
}
void GL_APIENTRY G_glDepthRangef(GLfloat n, GLfloat f) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLfloat n, GLfloat f)>(ResolveAngleGl("glDepthRangef"));
    if (fn) fn(n, f); else NoteGlUnsupported();
}
void GL_APIENTRY G_glFogf(GLenum pname, GLfloat param) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum pname, GLfloat param)>(ResolveAngleGl("glFogf"));
    if (fn) fn(pname, param); else NoteGlUnsupported();
}
void GL_APIENTRY G_glFogfv(GLenum pname, const GLfloat *params) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum pname, const GLfloat *params)>(ResolveAngleGl("glFogfv"));
    if (fn) fn(pname, params); else NoteGlUnsupported();
}
void GL_APIENTRY G_glFrustumf(GLfloat l, GLfloat r, GLfloat b, GLfloat t, GLfloat n, GLfloat f) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLfloat l, GLfloat r, GLfloat b, GLfloat t, GLfloat n, GLfloat f)>(ResolveAngleGl("glFrustumf"));
    if (fn) fn(l, r, b, t, n, f); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetClipPlanef(GLenum plane, GLfloat *equation) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum plane, GLfloat *equation)>(ResolveAngleGl("glGetClipPlanef"));
    if (fn) fn(plane, equation); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetFloatv(GLenum pname, GLfloat *data) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum pname, GLfloat *data)>(ResolveAngleGl("glGetFloatv"));
    if (fn) fn(pname, data); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetLightfv(GLenum light, GLenum pname, GLfloat *params) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum light, GLenum pname, GLfloat *params)>(ResolveAngleGl("glGetLightfv"));
    if (fn) fn(light, pname, params); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetMaterialfv(GLenum face, GLenum pname, GLfloat *params) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum face, GLenum pname, GLfloat *params)>(ResolveAngleGl("glGetMaterialfv"));
    if (fn) fn(face, pname, params); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetTexEnvfv(GLenum target, GLenum pname, GLfloat *params) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLenum pname, GLfloat *params)>(ResolveAngleGl("glGetTexEnvfv"));
    if (fn) fn(target, pname, params); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetTexParameterfv(GLenum target, GLenum pname, GLfloat *params) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLenum pname, GLfloat *params)>(ResolveAngleGl("glGetTexParameterfv"));
    if (fn) fn(target, pname, params); else NoteGlUnsupported();
}
void GL_APIENTRY G_glLightModelf(GLenum pname, GLfloat param) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum pname, GLfloat param)>(ResolveAngleGl("glLightModelf"));
    if (fn) fn(pname, param); else NoteGlUnsupported();
}
void GL_APIENTRY G_glLightModelfv(GLenum pname, const GLfloat *params) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum pname, const GLfloat *params)>(ResolveAngleGl("glLightModelfv"));
    if (fn) fn(pname, params); else NoteGlUnsupported();
}
void GL_APIENTRY G_glLightf(GLenum light, GLenum pname, GLfloat param) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum light, GLenum pname, GLfloat param)>(ResolveAngleGl("glLightf"));
    if (fn) fn(light, pname, param); else NoteGlUnsupported();
}
void GL_APIENTRY G_glLightfv(GLenum light, GLenum pname, const GLfloat *params) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum light, GLenum pname, const GLfloat *params)>(ResolveAngleGl("glLightfv"));
    if (fn) fn(light, pname, params); else NoteGlUnsupported();
}
void GL_APIENTRY G_glLineWidth(GLfloat width) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLfloat width)>(ResolveAngleGl("glLineWidth"));
    if (fn) fn(width); else NoteGlUnsupported();
}
void GL_APIENTRY G_glLoadMatrixf(const GLfloat *m) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(const GLfloat *m)>(ResolveAngleGl("glLoadMatrixf"));
    if (fn) fn(m); else NoteGlUnsupported();
}
void GL_APIENTRY G_glMaterialf(GLenum face, GLenum pname, GLfloat param) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum face, GLenum pname, GLfloat param)>(ResolveAngleGl("glMaterialf"));
    if (fn) fn(face, pname, param); else NoteGlUnsupported();
}
void GL_APIENTRY G_glMaterialfv(GLenum face, GLenum pname, const GLfloat *params) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum face, GLenum pname, const GLfloat *params)>(ResolveAngleGl("glMaterialfv"));
    if (fn) fn(face, pname, params); else NoteGlUnsupported();
}
void GL_APIENTRY G_glMultMatrixf(const GLfloat *m) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(const GLfloat *m)>(ResolveAngleGl("glMultMatrixf"));
    if (fn) fn(m); else NoteGlUnsupported();
}
void GL_APIENTRY G_glMultiTexCoord4f(GLenum target, GLfloat s, GLfloat t, GLfloat r, GLfloat q) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLfloat s, GLfloat t, GLfloat r, GLfloat q)>(ResolveAngleGl("glMultiTexCoord4f"));
    if (fn) fn(target, s, t, r, q); else NoteGlUnsupported();
}
void GL_APIENTRY G_glNormal3f(GLfloat nx, GLfloat ny, GLfloat nz) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLfloat nx, GLfloat ny, GLfloat nz)>(ResolveAngleGl("glNormal3f"));
    if (fn) fn(nx, ny, nz); else NoteGlUnsupported();
}
void GL_APIENTRY G_glOrthof(GLfloat l, GLfloat r, GLfloat b, GLfloat t, GLfloat n, GLfloat f) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLfloat l, GLfloat r, GLfloat b, GLfloat t, GLfloat n, GLfloat f)>(ResolveAngleGl("glOrthof"));
    if (fn) fn(l, r, b, t, n, f); else NoteGlUnsupported();
}
void GL_APIENTRY G_glPointParameterf(GLenum pname, GLfloat param) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum pname, GLfloat param)>(ResolveAngleGl("glPointParameterf"));
    if (fn) fn(pname, param); else NoteGlUnsupported();
}
void GL_APIENTRY G_glPointParameterfv(GLenum pname, const GLfloat *params) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum pname, const GLfloat *params)>(ResolveAngleGl("glPointParameterfv"));
    if (fn) fn(pname, params); else NoteGlUnsupported();
}
void GL_APIENTRY G_glPointSize(GLfloat size) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLfloat size)>(ResolveAngleGl("glPointSize"));
    if (fn) fn(size); else NoteGlUnsupported();
}
void GL_APIENTRY G_glPolygonOffset(GLfloat factor, GLfloat units) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLfloat factor, GLfloat units)>(ResolveAngleGl("glPolygonOffset"));
    if (fn) fn(factor, units); else NoteGlUnsupported();
}
void GL_APIENTRY G_glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLfloat angle, GLfloat x, GLfloat y, GLfloat z)>(ResolveAngleGl("glRotatef"));
    if (fn) fn(angle, x, y, z); else NoteGlUnsupported();
}
void GL_APIENTRY G_glScalef(GLfloat x, GLfloat y, GLfloat z) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLfloat x, GLfloat y, GLfloat z)>(ResolveAngleGl("glScalef"));
    if (fn) fn(x, y, z); else NoteGlUnsupported();
}
void GL_APIENTRY G_glTexEnvf(GLenum target, GLenum pname, GLfloat param) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLenum pname, GLfloat param)>(ResolveAngleGl("glTexEnvf"));
    if (fn) fn(target, pname, param); else NoteGlUnsupported();
}
void GL_APIENTRY G_glTexEnvfv(GLenum target, GLenum pname, const GLfloat *params) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLenum pname, const GLfloat *params)>(ResolveAngleGl("glTexEnvfv"));
    if (fn) fn(target, pname, params); else NoteGlUnsupported();
}
void GL_APIENTRY G_glTexParameterf(GLenum target, GLenum pname, GLfloat param) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLenum pname, GLfloat param)>(ResolveAngleGl("glTexParameterf"));
    if (fn) fn(target, pname, param); else NoteGlUnsupported();
}
void GL_APIENTRY G_glTexParameterfv(GLenum target, GLenum pname, const GLfloat *params) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLenum pname, const GLfloat *params)>(ResolveAngleGl("glTexParameterfv"));
    if (fn) fn(target, pname, params); else NoteGlUnsupported();
}
void GL_APIENTRY G_glTranslatef(GLfloat x, GLfloat y, GLfloat z) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLfloat x, GLfloat y, GLfloat z)>(ResolveAngleGl("glTranslatef"));
    if (fn) fn(x, y, z); else NoteGlUnsupported();
}
void GL_APIENTRY G_glActiveTexture(GLenum texture) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum texture)>(ResolveAngleGl("glActiveTexture"));
    if (fn) fn(texture); else NoteGlUnsupported();
}
void GL_APIENTRY G_glAlphaFuncx(GLenum func, GLfixed ref) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum func, GLfixed ref)>(ResolveAngleGl("glAlphaFuncx"));
    if (fn) fn(func, ref); else NoteGlUnsupported();
}
void GL_APIENTRY G_glBindBuffer(GLenum target, GLuint buffer) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLuint buffer)>(ResolveAngleGl("glBindBuffer"));
    if (fn) fn(target, buffer); else NoteGlUnsupported();
}
void GL_APIENTRY G_glBindTexture(GLenum target, GLuint texture) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLuint texture)>(ResolveAngleGl("glBindTexture"));
    if (fn) fn(target, texture); else NoteGlUnsupported();
}
void GL_APIENTRY G_glBlendFunc(GLenum sfactor, GLenum dfactor) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum sfactor, GLenum dfactor)>(ResolveAngleGl("glBlendFunc"));
    if (fn) fn(sfactor, dfactor); else NoteGlUnsupported();
}
void GL_APIENTRY G_glBufferData(GLenum target, GLsizeiptr size, const void *data, GLenum usage) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLsizeiptr size, const void *data, GLenum usage)>(ResolveAngleGl("glBufferData"));
    if (fn) fn(target, size, data, usage); else NoteGlUnsupported();
}
void GL_APIENTRY G_glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void *data) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLintptr offset, GLsizeiptr size, const void *data)>(ResolveAngleGl("glBufferSubData"));
    if (fn) fn(target, offset, size, data); else NoteGlUnsupported();
}
void GL_APIENTRY G_glClear(GLbitfield mask) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLbitfield mask)>(ResolveAngleGl("glClear"));
    if (fn) fn(mask); else NoteGlUnsupported();
}
void GL_APIENTRY G_glClearColorx(GLfixed red, GLfixed green, GLfixed blue, GLfixed alpha) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLfixed red, GLfixed green, GLfixed blue, GLfixed alpha)>(ResolveAngleGl("glClearColorx"));
    if (fn) fn(red, green, blue, alpha); else NoteGlUnsupported();
}
void GL_APIENTRY G_glClearDepthx(GLfixed depth) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLfixed depth)>(ResolveAngleGl("glClearDepthx"));
    if (fn) fn(depth); else NoteGlUnsupported();
}
void GL_APIENTRY G_glClearStencil(GLint s) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLint s)>(ResolveAngleGl("glClearStencil"));
    if (fn) fn(s); else NoteGlUnsupported();
}
void GL_APIENTRY G_glClientActiveTexture(GLenum texture) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum texture)>(ResolveAngleGl("glClientActiveTexture"));
    if (fn) fn(texture); else NoteGlUnsupported();
}
void GL_APIENTRY G_glClipPlanex(GLenum plane, const GLfixed *equation) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum plane, const GLfixed *equation)>(ResolveAngleGl("glClipPlanex"));
    if (fn) fn(plane, equation); else NoteGlUnsupported();
}
void GL_APIENTRY G_glColor4ub(GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha)>(ResolveAngleGl("glColor4ub"));
    if (fn) fn(red, green, blue, alpha); else NoteGlUnsupported();
}
void GL_APIENTRY G_glColor4x(GLfixed red, GLfixed green, GLfixed blue, GLfixed alpha) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLfixed red, GLfixed green, GLfixed blue, GLfixed alpha)>(ResolveAngleGl("glColor4x"));
    if (fn) fn(red, green, blue, alpha); else NoteGlUnsupported();
}
void GL_APIENTRY G_glColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha)>(ResolveAngleGl("glColorMask"));
    if (fn) fn(red, green, blue, alpha); else NoteGlUnsupported();
}
void GL_APIENTRY G_glColorPointer(GLint size, GLenum type, GLsizei stride, const void *pointer) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLint size, GLenum type, GLsizei stride, const void *pointer)>(ResolveAngleGl("glColorPointer"));
    if (fn) fn(size, type, stride, pointer); else NoteGlUnsupported();
}
void GL_APIENTRY G_glCompressedTexImage2D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void *data) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void *data)>(ResolveAngleGl("glCompressedTexImage2D"));
    if (fn) fn(target, level, internalformat, width, height, border, imageSize, data); else NoteGlUnsupported();
}
void GL_APIENTRY G_glCompressedTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *data) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *data)>(ResolveAngleGl("glCompressedTexSubImage2D"));
    if (fn) fn(target, level, xoffset, yoffset, width, height, format, imageSize, data); else NoteGlUnsupported();
}
void GL_APIENTRY G_glCopyTexImage2D(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border)>(ResolveAngleGl("glCopyTexImage2D"));
    if (fn) fn(target, level, internalformat, x, y, width, height, border); else NoteGlUnsupported();
}
void GL_APIENTRY G_glCopyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height)>(ResolveAngleGl("glCopyTexSubImage2D"));
    if (fn) fn(target, level, xoffset, yoffset, x, y, width, height); else NoteGlUnsupported();
}
void GL_APIENTRY G_glCullFace(GLenum mode) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum mode)>(ResolveAngleGl("glCullFace"));
    if (fn) fn(mode); else NoteGlUnsupported();
}
void GL_APIENTRY G_glDeleteBuffers(GLsizei n, const GLuint *buffers) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLsizei n, const GLuint *buffers)>(ResolveAngleGl("glDeleteBuffers"));
    if (fn) fn(n, buffers); else NoteGlUnsupported();
}
void GL_APIENTRY G_glDeleteTextures(GLsizei n, const GLuint *textures) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLsizei n, const GLuint *textures)>(ResolveAngleGl("glDeleteTextures"));
    if (fn) fn(n, textures); else NoteGlUnsupported();
}
void GL_APIENTRY G_glDepthFunc(GLenum func) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum func)>(ResolveAngleGl("glDepthFunc"));
    if (fn) fn(func); else NoteGlUnsupported();
}
void GL_APIENTRY G_glDepthMask(GLboolean flag) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLboolean flag)>(ResolveAngleGl("glDepthMask"));
    if (fn) fn(flag); else NoteGlUnsupported();
}
void GL_APIENTRY G_glDepthRangex(GLfixed n, GLfixed f) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLfixed n, GLfixed f)>(ResolveAngleGl("glDepthRangex"));
    if (fn) fn(n, f); else NoteGlUnsupported();
}
void GL_APIENTRY G_glDisable(GLenum cap) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum cap)>(ResolveAngleGl("glDisable"));
    if (fn) fn(cap); else NoteGlUnsupported();
}
void GL_APIENTRY G_glDisableClientState(GLenum array) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum array)>(ResolveAngleGl("glDisableClientState"));
    if (fn) fn(array); else NoteGlUnsupported();
}
void GL_APIENTRY G_glDrawArrays(GLenum mode, GLint first, GLsizei count) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum mode, GLint first, GLsizei count)>(ResolveAngleGl("glDrawArrays"));
    if (fn) fn(mode, first, count); else NoteGlUnsupported();
}
void GL_APIENTRY G_glDrawElements(GLenum mode, GLsizei count, GLenum type, const void *indices) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum mode, GLsizei count, GLenum type, const void *indices)>(ResolveAngleGl("glDrawElements"));
    if (fn) fn(mode, count, type, indices); else NoteGlUnsupported();
}
void GL_APIENTRY G_glEnable(GLenum cap) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum cap)>(ResolveAngleGl("glEnable"));
    if (fn) fn(cap); else NoteGlUnsupported();
}
void GL_APIENTRY G_glEnableClientState(GLenum array) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum array)>(ResolveAngleGl("glEnableClientState"));
    if (fn) fn(array); else NoteGlUnsupported();
}
void GL_APIENTRY G_glFinish() {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)()>(ResolveAngleGl("glFinish"));
    if (fn) fn(); else NoteGlUnsupported();
}
void GL_APIENTRY G_glFlush() {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)()>(ResolveAngleGl("glFlush"));
    if (fn) fn(); else NoteGlUnsupported();
}
void GL_APIENTRY G_glFogx(GLenum pname, GLfixed param) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum pname, GLfixed param)>(ResolveAngleGl("glFogx"));
    if (fn) fn(pname, param); else NoteGlUnsupported();
}
void GL_APIENTRY G_glFogxv(GLenum pname, const GLfixed *param) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum pname, const GLfixed *param)>(ResolveAngleGl("glFogxv"));
    if (fn) fn(pname, param); else NoteGlUnsupported();
}
void GL_APIENTRY G_glFrontFace(GLenum mode) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum mode)>(ResolveAngleGl("glFrontFace"));
    if (fn) fn(mode); else NoteGlUnsupported();
}
void GL_APIENTRY G_glFrustumx(GLfixed l, GLfixed r, GLfixed b, GLfixed t, GLfixed n, GLfixed f) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLfixed l, GLfixed r, GLfixed b, GLfixed t, GLfixed n, GLfixed f)>(ResolveAngleGl("glFrustumx"));
    if (fn) fn(l, r, b, t, n, f); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetBooleanv(GLenum pname, GLboolean *data) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum pname, GLboolean *data)>(ResolveAngleGl("glGetBooleanv"));
    if (fn) fn(pname, data); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetBufferParameteriv(GLenum target, GLenum pname, GLint *params) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLenum pname, GLint *params)>(ResolveAngleGl("glGetBufferParameteriv"));
    if (fn) fn(target, pname, params); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetClipPlanex(GLenum plane, GLfixed *equation) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum plane, GLfixed *equation)>(ResolveAngleGl("glGetClipPlanex"));
    if (fn) fn(plane, equation); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGenBuffers(GLsizei n, GLuint *buffers) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLsizei n, GLuint *buffers)>(ResolveAngleGl("glGenBuffers"));
    if (fn) fn(n, buffers); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGenTextures(GLsizei n, GLuint *textures) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLsizei n, GLuint *textures)>(ResolveAngleGl("glGenTextures"));
    if (fn) fn(n, textures); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetFixedv(GLenum pname, GLfixed *data) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum pname, GLfixed *data)>(ResolveAngleGl("glGetFixedv"));
    if (fn) fn(pname, data); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetIntegerv(GLenum pname, GLint *data) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum pname, GLint *data)>(ResolveAngleGl("glGetIntegerv"));
    if (fn) fn(pname, data); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetLightxv(GLenum light, GLenum pname, GLfixed *params) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum light, GLenum pname, GLfixed *params)>(ResolveAngleGl("glGetLightxv"));
    if (fn) fn(light, pname, params); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetMaterialxv(GLenum face, GLenum pname, GLfixed *params) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum face, GLenum pname, GLfixed *params)>(ResolveAngleGl("glGetMaterialxv"));
    if (fn) fn(face, pname, params); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetPointerv(GLenum pname, void **params) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum pname, void **params)>(ResolveAngleGl("glGetPointerv"));
    if (fn) fn(pname, params); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetTexEnviv(GLenum target, GLenum pname, GLint *params) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLenum pname, GLint *params)>(ResolveAngleGl("glGetTexEnviv"));
    if (fn) fn(target, pname, params); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetTexEnvxv(GLenum target, GLenum pname, GLfixed *params) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLenum pname, GLfixed *params)>(ResolveAngleGl("glGetTexEnvxv"));
    if (fn) fn(target, pname, params); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetTexParameteriv(GLenum target, GLenum pname, GLint *params) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLenum pname, GLint *params)>(ResolveAngleGl("glGetTexParameteriv"));
    if (fn) fn(target, pname, params); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetTexParameterxv(GLenum target, GLenum pname, GLfixed *params) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLenum pname, GLfixed *params)>(ResolveAngleGl("glGetTexParameterxv"));
    if (fn) fn(target, pname, params); else NoteGlUnsupported();
}
void GL_APIENTRY G_glHint(GLenum target, GLenum mode) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLenum mode)>(ResolveAngleGl("glHint"));
    if (fn) fn(target, mode); else NoteGlUnsupported();
}
GLboolean GL_APIENTRY G_glIsBuffer(GLuint buffer) {
    if (!GuestGlReady()) return static_cast<GLboolean>(0);
    static auto fn = reinterpret_cast<GLboolean (GL_APIENTRY *)(GLuint buffer)>(ResolveAngleGl("glIsBuffer"));
    if (fn) return fn(buffer);
    NoteGlUnsupported();
    return static_cast<GLboolean>(0);
}
GLboolean GL_APIENTRY G_glIsEnabled(GLenum cap) {
    if (!GuestGlReady()) return static_cast<GLboolean>(0);
    static auto fn = reinterpret_cast<GLboolean (GL_APIENTRY *)(GLenum cap)>(ResolveAngleGl("glIsEnabled"));
    if (fn) return fn(cap);
    NoteGlUnsupported();
    return static_cast<GLboolean>(0);
}
GLboolean GL_APIENTRY G_glIsTexture(GLuint texture) {
    if (!GuestGlReady()) return static_cast<GLboolean>(0);
    static auto fn = reinterpret_cast<GLboolean (GL_APIENTRY *)(GLuint texture)>(ResolveAngleGl("glIsTexture"));
    if (fn) return fn(texture);
    NoteGlUnsupported();
    return static_cast<GLboolean>(0);
}
void GL_APIENTRY G_glLightModelx(GLenum pname, GLfixed param) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum pname, GLfixed param)>(ResolveAngleGl("glLightModelx"));
    if (fn) fn(pname, param); else NoteGlUnsupported();
}
void GL_APIENTRY G_glLightModelxv(GLenum pname, const GLfixed *param) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum pname, const GLfixed *param)>(ResolveAngleGl("glLightModelxv"));
    if (fn) fn(pname, param); else NoteGlUnsupported();
}
void GL_APIENTRY G_glLightx(GLenum light, GLenum pname, GLfixed param) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum light, GLenum pname, GLfixed param)>(ResolveAngleGl("glLightx"));
    if (fn) fn(light, pname, param); else NoteGlUnsupported();
}
void GL_APIENTRY G_glLightxv(GLenum light, GLenum pname, const GLfixed *params) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum light, GLenum pname, const GLfixed *params)>(ResolveAngleGl("glLightxv"));
    if (fn) fn(light, pname, params); else NoteGlUnsupported();
}
void GL_APIENTRY G_glLineWidthx(GLfixed width) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLfixed width)>(ResolveAngleGl("glLineWidthx"));
    if (fn) fn(width); else NoteGlUnsupported();
}
void GL_APIENTRY G_glLoadIdentity() {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)()>(ResolveAngleGl("glLoadIdentity"));
    if (fn) fn(); else NoteGlUnsupported();
}
void GL_APIENTRY G_glLoadMatrixx(const GLfixed *m) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(const GLfixed *m)>(ResolveAngleGl("glLoadMatrixx"));
    if (fn) fn(m); else NoteGlUnsupported();
}
void GL_APIENTRY G_glLogicOp(GLenum opcode) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum opcode)>(ResolveAngleGl("glLogicOp"));
    if (fn) fn(opcode); else NoteGlUnsupported();
}
void GL_APIENTRY G_glMaterialx(GLenum face, GLenum pname, GLfixed param) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum face, GLenum pname, GLfixed param)>(ResolveAngleGl("glMaterialx"));
    if (fn) fn(face, pname, param); else NoteGlUnsupported();
}
void GL_APIENTRY G_glMaterialxv(GLenum face, GLenum pname, const GLfixed *param) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum face, GLenum pname, const GLfixed *param)>(ResolveAngleGl("glMaterialxv"));
    if (fn) fn(face, pname, param); else NoteGlUnsupported();
}
void GL_APIENTRY G_glMatrixMode(GLenum mode) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum mode)>(ResolveAngleGl("glMatrixMode"));
    if (fn) fn(mode); else NoteGlUnsupported();
}
void GL_APIENTRY G_glMultMatrixx(const GLfixed *m) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(const GLfixed *m)>(ResolveAngleGl("glMultMatrixx"));
    if (fn) fn(m); else NoteGlUnsupported();
}
void GL_APIENTRY G_glMultiTexCoord4x(GLenum texture, GLfixed s, GLfixed t, GLfixed r, GLfixed q) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum texture, GLfixed s, GLfixed t, GLfixed r, GLfixed q)>(ResolveAngleGl("glMultiTexCoord4x"));
    if (fn) fn(texture, s, t, r, q); else NoteGlUnsupported();
}
void GL_APIENTRY G_glNormal3x(GLfixed nx, GLfixed ny, GLfixed nz) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLfixed nx, GLfixed ny, GLfixed nz)>(ResolveAngleGl("glNormal3x"));
    if (fn) fn(nx, ny, nz); else NoteGlUnsupported();
}
void GL_APIENTRY G_glNormalPointer(GLenum type, GLsizei stride, const void *pointer) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum type, GLsizei stride, const void *pointer)>(ResolveAngleGl("glNormalPointer"));
    if (fn) fn(type, stride, pointer); else NoteGlUnsupported();
}
void GL_APIENTRY G_glOrthox(GLfixed l, GLfixed r, GLfixed b, GLfixed t, GLfixed n, GLfixed f) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLfixed l, GLfixed r, GLfixed b, GLfixed t, GLfixed n, GLfixed f)>(ResolveAngleGl("glOrthox"));
    if (fn) fn(l, r, b, t, n, f); else NoteGlUnsupported();
}
void GL_APIENTRY G_glPixelStorei(GLenum pname, GLint param) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum pname, GLint param)>(ResolveAngleGl("glPixelStorei"));
    if (fn) fn(pname, param); else NoteGlUnsupported();
}
void GL_APIENTRY G_glPointParameterx(GLenum pname, GLfixed param) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum pname, GLfixed param)>(ResolveAngleGl("glPointParameterx"));
    if (fn) fn(pname, param); else NoteGlUnsupported();
}
void GL_APIENTRY G_glPointParameterxv(GLenum pname, const GLfixed *params) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum pname, const GLfixed *params)>(ResolveAngleGl("glPointParameterxv"));
    if (fn) fn(pname, params); else NoteGlUnsupported();
}
void GL_APIENTRY G_glPointSizex(GLfixed size) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLfixed size)>(ResolveAngleGl("glPointSizex"));
    if (fn) fn(size); else NoteGlUnsupported();
}
void GL_APIENTRY G_glPolygonOffsetx(GLfixed factor, GLfixed units) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLfixed factor, GLfixed units)>(ResolveAngleGl("glPolygonOffsetx"));
    if (fn) fn(factor, units); else NoteGlUnsupported();
}
void GL_APIENTRY G_glPopMatrix() {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)()>(ResolveAngleGl("glPopMatrix"));
    if (fn) fn(); else NoteGlUnsupported();
}
void GL_APIENTRY G_glPushMatrix() {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)()>(ResolveAngleGl("glPushMatrix"));
    if (fn) fn(); else NoteGlUnsupported();
}
void GL_APIENTRY G_glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void *pixels) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void *pixels)>(ResolveAngleGl("glReadPixels"));
    if (fn) fn(x, y, width, height, format, type, pixels); else NoteGlUnsupported();
}
void GL_APIENTRY G_glRotatex(GLfixed angle, GLfixed x, GLfixed y, GLfixed z) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLfixed angle, GLfixed x, GLfixed y, GLfixed z)>(ResolveAngleGl("glRotatex"));
    if (fn) fn(angle, x, y, z); else NoteGlUnsupported();
}
void GL_APIENTRY G_glSampleCoverage(GLfloat value, GLboolean invert) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLfloat value, GLboolean invert)>(ResolveAngleGl("glSampleCoverage"));
    if (fn) fn(value, invert); else NoteGlUnsupported();
}
void GL_APIENTRY G_glSampleCoveragex(GLclampx value, GLboolean invert) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLclampx value, GLboolean invert)>(ResolveAngleGl("glSampleCoveragex"));
    if (fn) fn(value, invert); else NoteGlUnsupported();
}
void GL_APIENTRY G_glScalex(GLfixed x, GLfixed y, GLfixed z) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLfixed x, GLfixed y, GLfixed z)>(ResolveAngleGl("glScalex"));
    if (fn) fn(x, y, z); else NoteGlUnsupported();
}
void GL_APIENTRY G_glScissor(GLint x, GLint y, GLsizei width, GLsizei height) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLint x, GLint y, GLsizei width, GLsizei height)>(ResolveAngleGl("glScissor"));
    if (fn) fn(x, y, width, height); else NoteGlUnsupported();
}
void GL_APIENTRY G_glShadeModel(GLenum mode) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum mode)>(ResolveAngleGl("glShadeModel"));
    if (fn) fn(mode); else NoteGlUnsupported();
}
void GL_APIENTRY G_glStencilFunc(GLenum func, GLint ref, GLuint mask) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum func, GLint ref, GLuint mask)>(ResolveAngleGl("glStencilFunc"));
    if (fn) fn(func, ref, mask); else NoteGlUnsupported();
}
void GL_APIENTRY G_glStencilMask(GLuint mask) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint mask)>(ResolveAngleGl("glStencilMask"));
    if (fn) fn(mask); else NoteGlUnsupported();
}
void GL_APIENTRY G_glStencilOp(GLenum fail, GLenum zfail, GLenum zpass) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum fail, GLenum zfail, GLenum zpass)>(ResolveAngleGl("glStencilOp"));
    if (fn) fn(fail, zfail, zpass); else NoteGlUnsupported();
}
void GL_APIENTRY G_glTexCoordPointer(GLint size, GLenum type, GLsizei stride, const void *pointer) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLint size, GLenum type, GLsizei stride, const void *pointer)>(ResolveAngleGl("glTexCoordPointer"));
    if (fn) fn(size, type, stride, pointer); else NoteGlUnsupported();
}
void GL_APIENTRY G_glTexEnvi(GLenum target, GLenum pname, GLint param) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLenum pname, GLint param)>(ResolveAngleGl("glTexEnvi"));
    if (fn) fn(target, pname, param); else NoteGlUnsupported();
}
void GL_APIENTRY G_glTexEnvx(GLenum target, GLenum pname, GLfixed param) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLenum pname, GLfixed param)>(ResolveAngleGl("glTexEnvx"));
    if (fn) fn(target, pname, param); else NoteGlUnsupported();
}
void GL_APIENTRY G_glTexEnviv(GLenum target, GLenum pname, const GLint *params) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLenum pname, const GLint *params)>(ResolveAngleGl("glTexEnviv"));
    if (fn) fn(target, pname, params); else NoteGlUnsupported();
}
void GL_APIENTRY G_glTexEnvxv(GLenum target, GLenum pname, const GLfixed *params) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLenum pname, const GLfixed *params)>(ResolveAngleGl("glTexEnvxv"));
    if (fn) fn(target, pname, params); else NoteGlUnsupported();
}
void GL_APIENTRY G_glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void *pixels) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void *pixels)>(ResolveAngleGl("glTexImage2D"));
    if (fn) fn(target, level, internalformat, width, height, border, format, type, pixels); else NoteGlUnsupported();
}
void GL_APIENTRY G_glTexParameteri(GLenum target, GLenum pname, GLint param) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLenum pname, GLint param)>(ResolveAngleGl("glTexParameteri"));
    if (fn) fn(target, pname, param); else NoteGlUnsupported();
}
void GL_APIENTRY G_glTexParameterx(GLenum target, GLenum pname, GLfixed param) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLenum pname, GLfixed param)>(ResolveAngleGl("glTexParameterx"));
    if (fn) fn(target, pname, param); else NoteGlUnsupported();
}
void GL_APIENTRY G_glTexParameteriv(GLenum target, GLenum pname, const GLint *params) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLenum pname, const GLint *params)>(ResolveAngleGl("glTexParameteriv"));
    if (fn) fn(target, pname, params); else NoteGlUnsupported();
}
void GL_APIENTRY G_glTexParameterxv(GLenum target, GLenum pname, const GLfixed *params) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLenum pname, const GLfixed *params)>(ResolveAngleGl("glTexParameterxv"));
    if (fn) fn(target, pname, params); else NoteGlUnsupported();
}
void GL_APIENTRY G_glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels)>(ResolveAngleGl("glTexSubImage2D"));
    if (fn) fn(target, level, xoffset, yoffset, width, height, format, type, pixels); else NoteGlUnsupported();
}
void GL_APIENTRY G_glTranslatex(GLfixed x, GLfixed y, GLfixed z) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLfixed x, GLfixed y, GLfixed z)>(ResolveAngleGl("glTranslatex"));
    if (fn) fn(x, y, z); else NoteGlUnsupported();
}
void GL_APIENTRY G_glVertexPointer(GLint size, GLenum type, GLsizei stride, const void *pointer) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLint size, GLenum type, GLsizei stride, const void *pointer)>(ResolveAngleGl("glVertexPointer"));
    if (fn) fn(size, type, stride, pointer); else NoteGlUnsupported();
}
void GL_APIENTRY G_glViewport(GLint x, GLint y, GLsizei width, GLsizei height) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLint x, GLint y, GLsizei width, GLsizei height)>(ResolveAngleGl("glViewport"));
    if (fn) fn(x, y, width, height); else NoteGlUnsupported();
}
void GL_APIENTRY G_glPointSizePointerOES(GLenum type, GLsizei stride, const void *pointer) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum type, GLsizei stride, const void *pointer)>(ResolveAngleGl("glPointSizePointerOES"));
    if (fn) fn(type, stride, pointer); else NoteGlUnsupported();
}
void GL_APIENTRY G_glAttachShader(GLuint program, GLuint shader) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint program, GLuint shader)>(ResolveAngleGl("glAttachShader"));
    if (fn) fn(program, shader); else NoteGlUnsupported();
}
void GL_APIENTRY G_glBindAttribLocation(GLuint program, GLuint index, const GLchar *name) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint program, GLuint index, const GLchar *name)>(ResolveAngleGl("glBindAttribLocation"));
    if (fn) fn(program, index, name); else NoteGlUnsupported();
}
void GL_APIENTRY G_glBindFramebuffer(GLenum target, GLuint framebuffer) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLuint framebuffer)>(ResolveAngleGl("glBindFramebuffer"));
    if (fn) fn(target, framebuffer); else NoteGlUnsupported();
}
void GL_APIENTRY G_glBindRenderbuffer(GLenum target, GLuint renderbuffer) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLuint renderbuffer)>(ResolveAngleGl("glBindRenderbuffer"));
    if (fn) fn(target, renderbuffer); else NoteGlUnsupported();
}
void GL_APIENTRY G_glBlendColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)>(ResolveAngleGl("glBlendColor"));
    if (fn) fn(red, green, blue, alpha); else NoteGlUnsupported();
}
void GL_APIENTRY G_glBlendEquation(GLenum mode) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum mode)>(ResolveAngleGl("glBlendEquation"));
    if (fn) fn(mode); else NoteGlUnsupported();
}
void GL_APIENTRY G_glBlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum modeRGB, GLenum modeAlpha)>(ResolveAngleGl("glBlendEquationSeparate"));
    if (fn) fn(modeRGB, modeAlpha); else NoteGlUnsupported();
}
void GL_APIENTRY G_glBlendFuncSeparate(GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha)>(ResolveAngleGl("glBlendFuncSeparate"));
    if (fn) fn(sfactorRGB, dfactorRGB, sfactorAlpha, dfactorAlpha); else NoteGlUnsupported();
}
GLenum GL_APIENTRY G_glCheckFramebufferStatus(GLenum target) {
    if (!GuestGlReady()) return static_cast<GLenum>(0);
    static auto fn = reinterpret_cast<GLenum (GL_APIENTRY *)(GLenum target)>(ResolveAngleGl("glCheckFramebufferStatus"));
    if (fn) return fn(target);
    NoteGlUnsupported();
    return static_cast<GLenum>(0);
}
void GL_APIENTRY G_glCompileShader(GLuint shader) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint shader)>(ResolveAngleGl("glCompileShader"));
    if (fn) fn(shader); else NoteGlUnsupported();
}
GLuint GL_APIENTRY G_glCreateProgram() {
    if (!GuestGlReady()) return static_cast<GLuint>(0);
    static auto fn = reinterpret_cast<GLuint (GL_APIENTRY *)()>(ResolveAngleGl("glCreateProgram"));
    if (fn) return fn();
    NoteGlUnsupported();
    return static_cast<GLuint>(0);
}
GLuint GL_APIENTRY G_glCreateShader(GLenum type) {
    if (!GuestGlReady()) return static_cast<GLuint>(0);
    static auto fn = reinterpret_cast<GLuint (GL_APIENTRY *)(GLenum type)>(ResolveAngleGl("glCreateShader"));
    if (fn) return fn(type);
    NoteGlUnsupported();
    return static_cast<GLuint>(0);
}
void GL_APIENTRY G_glDeleteFramebuffers(GLsizei n, const GLuint *framebuffers) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLsizei n, const GLuint *framebuffers)>(ResolveAngleGl("glDeleteFramebuffers"));
    if (fn) fn(n, framebuffers); else NoteGlUnsupported();
}
void GL_APIENTRY G_glDeleteProgram(GLuint program) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint program)>(ResolveAngleGl("glDeleteProgram"));
    if (fn) fn(program); else NoteGlUnsupported();
}
void GL_APIENTRY G_glDeleteRenderbuffers(GLsizei n, const GLuint *renderbuffers) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLsizei n, const GLuint *renderbuffers)>(ResolveAngleGl("glDeleteRenderbuffers"));
    if (fn) fn(n, renderbuffers); else NoteGlUnsupported();
}
void GL_APIENTRY G_glDeleteShader(GLuint shader) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint shader)>(ResolveAngleGl("glDeleteShader"));
    if (fn) fn(shader); else NoteGlUnsupported();
}
void GL_APIENTRY G_glDetachShader(GLuint program, GLuint shader) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint program, GLuint shader)>(ResolveAngleGl("glDetachShader"));
    if (fn) fn(program, shader); else NoteGlUnsupported();
}
void GL_APIENTRY G_glDisableVertexAttribArray(GLuint index) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint index)>(ResolveAngleGl("glDisableVertexAttribArray"));
    if (fn) fn(index); else NoteGlUnsupported();
}
void GL_APIENTRY G_glEnableVertexAttribArray(GLuint index) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint index)>(ResolveAngleGl("glEnableVertexAttribArray"));
    if (fn) fn(index); else NoteGlUnsupported();
}
void GL_APIENTRY G_glFramebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer)>(ResolveAngleGl("glFramebufferRenderbuffer"));
    if (fn) fn(target, attachment, renderbuffertarget, renderbuffer); else NoteGlUnsupported();
}
void GL_APIENTRY G_glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)>(ResolveAngleGl("glFramebufferTexture2D"));
    if (fn) fn(target, attachment, textarget, texture, level); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGenerateMipmap(GLenum target) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target)>(ResolveAngleGl("glGenerateMipmap"));
    if (fn) fn(target); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGenFramebuffers(GLsizei n, GLuint *framebuffers) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLsizei n, GLuint *framebuffers)>(ResolveAngleGl("glGenFramebuffers"));
    if (fn) fn(n, framebuffers); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGenRenderbuffers(GLsizei n, GLuint *renderbuffers) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLsizei n, GLuint *renderbuffers)>(ResolveAngleGl("glGenRenderbuffers"));
    if (fn) fn(n, renderbuffers); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetActiveAttrib(GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, GLchar *name) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, GLchar *name)>(ResolveAngleGl("glGetActiveAttrib"));
    if (fn) fn(program, index, bufSize, length, size, type, name); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetActiveUniform(GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, GLchar *name) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, GLchar *name)>(ResolveAngleGl("glGetActiveUniform"));
    if (fn) fn(program, index, bufSize, length, size, type, name); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetAttachedShaders(GLuint program, GLsizei maxCount, GLsizei *count, GLuint *shaders) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint program, GLsizei maxCount, GLsizei *count, GLuint *shaders)>(ResolveAngleGl("glGetAttachedShaders"));
    if (fn) fn(program, maxCount, count, shaders); else NoteGlUnsupported();
}
GLint GL_APIENTRY G_glGetAttribLocation(GLuint program, const GLchar *name) {
    if (!GuestGlReady()) return static_cast<GLint>(0);
    static auto fn = reinterpret_cast<GLint (GL_APIENTRY *)(GLuint program, const GLchar *name)>(ResolveAngleGl("glGetAttribLocation"));
    if (fn) return fn(program, name);
    NoteGlUnsupported();
    return static_cast<GLint>(0);
}
void GL_APIENTRY G_glGetFramebufferAttachmentParameteriv(GLenum target, GLenum attachment, GLenum pname, GLint *params) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLenum attachment, GLenum pname, GLint *params)>(ResolveAngleGl("glGetFramebufferAttachmentParameteriv"));
    if (fn) fn(target, attachment, pname, params); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetProgramiv(GLuint program, GLenum pname, GLint *params) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint program, GLenum pname, GLint *params)>(ResolveAngleGl("glGetProgramiv"));
    if (fn) fn(program, pname, params); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetProgramInfoLog(GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog)>(ResolveAngleGl("glGetProgramInfoLog"));
    if (fn) fn(program, bufSize, length, infoLog); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetRenderbufferParameteriv(GLenum target, GLenum pname, GLint *params) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLenum pname, GLint *params)>(ResolveAngleGl("glGetRenderbufferParameteriv"));
    if (fn) fn(target, pname, params); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetShaderiv(GLuint shader, GLenum pname, GLint *params) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint shader, GLenum pname, GLint *params)>(ResolveAngleGl("glGetShaderiv"));
    if (fn) fn(shader, pname, params); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetShaderInfoLog(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog)>(ResolveAngleGl("glGetShaderInfoLog"));
    if (fn) fn(shader, bufSize, length, infoLog); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetShaderPrecisionFormat(GLenum shadertype, GLenum precisiontype, GLint *range, GLint *precision) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum shadertype, GLenum precisiontype, GLint *range, GLint *precision)>(ResolveAngleGl("glGetShaderPrecisionFormat"));
    if (fn) fn(shadertype, precisiontype, range, precision); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetShaderSource(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *source) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *source)>(ResolveAngleGl("glGetShaderSource"));
    if (fn) fn(shader, bufSize, length, source); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetUniformfv(GLuint program, GLint location, GLfloat *params) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint program, GLint location, GLfloat *params)>(ResolveAngleGl("glGetUniformfv"));
    if (fn) fn(program, location, params); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetUniformiv(GLuint program, GLint location, GLint *params) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint program, GLint location, GLint *params)>(ResolveAngleGl("glGetUniformiv"));
    if (fn) fn(program, location, params); else NoteGlUnsupported();
}
GLint GL_APIENTRY G_glGetUniformLocation(GLuint program, const GLchar *name) {
    if (!GuestGlReady()) return static_cast<GLint>(0);
    static auto fn = reinterpret_cast<GLint (GL_APIENTRY *)(GLuint program, const GLchar *name)>(ResolveAngleGl("glGetUniformLocation"));
    if (fn) return fn(program, name);
    NoteGlUnsupported();
    return static_cast<GLint>(0);
}
void GL_APIENTRY G_glGetVertexAttribfv(GLuint index, GLenum pname, GLfloat *params) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint index, GLenum pname, GLfloat *params)>(ResolveAngleGl("glGetVertexAttribfv"));
    if (fn) fn(index, pname, params); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetVertexAttribiv(GLuint index, GLenum pname, GLint *params) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint index, GLenum pname, GLint *params)>(ResolveAngleGl("glGetVertexAttribiv"));
    if (fn) fn(index, pname, params); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetVertexAttribPointerv(GLuint index, GLenum pname, void **pointer) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint index, GLenum pname, void **pointer)>(ResolveAngleGl("glGetVertexAttribPointerv"));
    if (fn) fn(index, pname, pointer); else NoteGlUnsupported();
}
GLboolean GL_APIENTRY G_glIsFramebuffer(GLuint framebuffer) {
    if (!GuestGlReady()) return static_cast<GLboolean>(0);
    static auto fn = reinterpret_cast<GLboolean (GL_APIENTRY *)(GLuint framebuffer)>(ResolveAngleGl("glIsFramebuffer"));
    if (fn) return fn(framebuffer);
    NoteGlUnsupported();
    return static_cast<GLboolean>(0);
}
GLboolean GL_APIENTRY G_glIsProgram(GLuint program) {
    if (!GuestGlReady()) return static_cast<GLboolean>(0);
    static auto fn = reinterpret_cast<GLboolean (GL_APIENTRY *)(GLuint program)>(ResolveAngleGl("glIsProgram"));
    if (fn) return fn(program);
    NoteGlUnsupported();
    return static_cast<GLboolean>(0);
}
GLboolean GL_APIENTRY G_glIsRenderbuffer(GLuint renderbuffer) {
    if (!GuestGlReady()) return static_cast<GLboolean>(0);
    static auto fn = reinterpret_cast<GLboolean (GL_APIENTRY *)(GLuint renderbuffer)>(ResolveAngleGl("glIsRenderbuffer"));
    if (fn) return fn(renderbuffer);
    NoteGlUnsupported();
    return static_cast<GLboolean>(0);
}
GLboolean GL_APIENTRY G_glIsShader(GLuint shader) {
    if (!GuestGlReady()) return static_cast<GLboolean>(0);
    static auto fn = reinterpret_cast<GLboolean (GL_APIENTRY *)(GLuint shader)>(ResolveAngleGl("glIsShader"));
    if (fn) return fn(shader);
    NoteGlUnsupported();
    return static_cast<GLboolean>(0);
}
void GL_APIENTRY G_glLinkProgram(GLuint program) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint program)>(ResolveAngleGl("glLinkProgram"));
    if (fn) fn(program); else NoteGlUnsupported();
}
void GL_APIENTRY G_glReleaseShaderCompiler() {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)()>(ResolveAngleGl("glReleaseShaderCompiler"));
    if (fn) fn(); else NoteGlUnsupported();
}
void GL_APIENTRY G_glRenderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLenum internalformat, GLsizei width, GLsizei height)>(ResolveAngleGl("glRenderbufferStorage"));
    if (fn) fn(target, internalformat, width, height); else NoteGlUnsupported();
}
void GL_APIENTRY G_glShaderBinary(GLsizei count, const GLuint *shaders, GLenum binaryFormat, const void *binary, GLsizei length) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLsizei count, const GLuint *shaders, GLenum binaryFormat, const void *binary, GLsizei length)>(ResolveAngleGl("glShaderBinary"));
    if (fn) fn(count, shaders, binaryFormat, binary, length); else NoteGlUnsupported();
}
void GL_APIENTRY G_glShaderSource(GLuint shader, GLsizei count, const GLchar *const*string, const GLint *length) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint shader, GLsizei count, const GLchar *const*string, const GLint *length)>(ResolveAngleGl("glShaderSource"));
    if (fn) fn(shader, count, string, length); else NoteGlUnsupported();
}
void GL_APIENTRY G_glStencilFuncSeparate(GLenum face, GLenum func, GLint ref, GLuint mask) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum face, GLenum func, GLint ref, GLuint mask)>(ResolveAngleGl("glStencilFuncSeparate"));
    if (fn) fn(face, func, ref, mask); else NoteGlUnsupported();
}
void GL_APIENTRY G_glStencilMaskSeparate(GLenum face, GLuint mask) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum face, GLuint mask)>(ResolveAngleGl("glStencilMaskSeparate"));
    if (fn) fn(face, mask); else NoteGlUnsupported();
}
void GL_APIENTRY G_glStencilOpSeparate(GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass)>(ResolveAngleGl("glStencilOpSeparate"));
    if (fn) fn(face, sfail, dpfail, dppass); else NoteGlUnsupported();
}
void GL_APIENTRY G_glUniform1f(GLint location, GLfloat v0) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLint location, GLfloat v0)>(ResolveAngleGl("glUniform1f"));
    if (fn) fn(location, v0); else NoteGlUnsupported();
}
void GL_APIENTRY G_glUniform1fv(GLint location, GLsizei count, const GLfloat *value) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLint location, GLsizei count, const GLfloat *value)>(ResolveAngleGl("glUniform1fv"));
    if (fn) fn(location, count, value); else NoteGlUnsupported();
}
void GL_APIENTRY G_glUniform1i(GLint location, GLint v0) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLint location, GLint v0)>(ResolveAngleGl("glUniform1i"));
    if (fn) fn(location, v0); else NoteGlUnsupported();
}
void GL_APIENTRY G_glUniform1iv(GLint location, GLsizei count, const GLint *value) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLint location, GLsizei count, const GLint *value)>(ResolveAngleGl("glUniform1iv"));
    if (fn) fn(location, count, value); else NoteGlUnsupported();
}
void GL_APIENTRY G_glUniform2f(GLint location, GLfloat v0, GLfloat v1) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLint location, GLfloat v0, GLfloat v1)>(ResolveAngleGl("glUniform2f"));
    if (fn) fn(location, v0, v1); else NoteGlUnsupported();
}
void GL_APIENTRY G_glUniform2fv(GLint location, GLsizei count, const GLfloat *value) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLint location, GLsizei count, const GLfloat *value)>(ResolveAngleGl("glUniform2fv"));
    if (fn) fn(location, count, value); else NoteGlUnsupported();
}
void GL_APIENTRY G_glUniform2i(GLint location, GLint v0, GLint v1) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLint location, GLint v0, GLint v1)>(ResolveAngleGl("glUniform2i"));
    if (fn) fn(location, v0, v1); else NoteGlUnsupported();
}
void GL_APIENTRY G_glUniform2iv(GLint location, GLsizei count, const GLint *value) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLint location, GLsizei count, const GLint *value)>(ResolveAngleGl("glUniform2iv"));
    if (fn) fn(location, count, value); else NoteGlUnsupported();
}
void GL_APIENTRY G_glUniform3f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2)>(ResolveAngleGl("glUniform3f"));
    if (fn) fn(location, v0, v1, v2); else NoteGlUnsupported();
}
void GL_APIENTRY G_glUniform3fv(GLint location, GLsizei count, const GLfloat *value) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLint location, GLsizei count, const GLfloat *value)>(ResolveAngleGl("glUniform3fv"));
    if (fn) fn(location, count, value); else NoteGlUnsupported();
}
void GL_APIENTRY G_glUniform3i(GLint location, GLint v0, GLint v1, GLint v2) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLint location, GLint v0, GLint v1, GLint v2)>(ResolveAngleGl("glUniform3i"));
    if (fn) fn(location, v0, v1, v2); else NoteGlUnsupported();
}
void GL_APIENTRY G_glUniform3iv(GLint location, GLsizei count, const GLint *value) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLint location, GLsizei count, const GLint *value)>(ResolveAngleGl("glUniform3iv"));
    if (fn) fn(location, count, value); else NoteGlUnsupported();
}
void GL_APIENTRY G_glUniform4f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3)>(ResolveAngleGl("glUniform4f"));
    if (fn) fn(location, v0, v1, v2, v3); else NoteGlUnsupported();
}
void GL_APIENTRY G_glUniform4fv(GLint location, GLsizei count, const GLfloat *value) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLint location, GLsizei count, const GLfloat *value)>(ResolveAngleGl("glUniform4fv"));
    if (fn) fn(location, count, value); else NoteGlUnsupported();
}
void GL_APIENTRY G_glUniform4i(GLint location, GLint v0, GLint v1, GLint v2, GLint v3) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLint location, GLint v0, GLint v1, GLint v2, GLint v3)>(ResolveAngleGl("glUniform4i"));
    if (fn) fn(location, v0, v1, v2, v3); else NoteGlUnsupported();
}
void GL_APIENTRY G_glUniform4iv(GLint location, GLsizei count, const GLint *value) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLint location, GLsizei count, const GLint *value)>(ResolveAngleGl("glUniform4iv"));
    if (fn) fn(location, count, value); else NoteGlUnsupported();
}
void GL_APIENTRY G_glUniformMatrix2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)>(ResolveAngleGl("glUniformMatrix2fv"));
    if (fn) fn(location, count, transpose, value); else NoteGlUnsupported();
}
void GL_APIENTRY G_glUniformMatrix3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)>(ResolveAngleGl("glUniformMatrix3fv"));
    if (fn) fn(location, count, transpose, value); else NoteGlUnsupported();
}
void GL_APIENTRY G_glUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)>(ResolveAngleGl("glUniformMatrix4fv"));
    if (fn) fn(location, count, transpose, value); else NoteGlUnsupported();
}
void GL_APIENTRY G_glUseProgram(GLuint program) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint program)>(ResolveAngleGl("glUseProgram"));
    if (fn) fn(program); else NoteGlUnsupported();
}
void GL_APIENTRY G_glValidateProgram(GLuint program) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint program)>(ResolveAngleGl("glValidateProgram"));
    if (fn) fn(program); else NoteGlUnsupported();
}
void GL_APIENTRY G_glVertexAttrib1f(GLuint index, GLfloat x) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint index, GLfloat x)>(ResolveAngleGl("glVertexAttrib1f"));
    if (fn) fn(index, x); else NoteGlUnsupported();
}
void GL_APIENTRY G_glVertexAttrib1fv(GLuint index, const GLfloat *v) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint index, const GLfloat *v)>(ResolveAngleGl("glVertexAttrib1fv"));
    if (fn) fn(index, v); else NoteGlUnsupported();
}
void GL_APIENTRY G_glVertexAttrib2f(GLuint index, GLfloat x, GLfloat y) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint index, GLfloat x, GLfloat y)>(ResolveAngleGl("glVertexAttrib2f"));
    if (fn) fn(index, x, y); else NoteGlUnsupported();
}
void GL_APIENTRY G_glVertexAttrib2fv(GLuint index, const GLfloat *v) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint index, const GLfloat *v)>(ResolveAngleGl("glVertexAttrib2fv"));
    if (fn) fn(index, v); else NoteGlUnsupported();
}
void GL_APIENTRY G_glVertexAttrib3f(GLuint index, GLfloat x, GLfloat y, GLfloat z) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint index, GLfloat x, GLfloat y, GLfloat z)>(ResolveAngleGl("glVertexAttrib3f"));
    if (fn) fn(index, x, y, z); else NoteGlUnsupported();
}
void GL_APIENTRY G_glVertexAttrib3fv(GLuint index, const GLfloat *v) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint index, const GLfloat *v)>(ResolveAngleGl("glVertexAttrib3fv"));
    if (fn) fn(index, v); else NoteGlUnsupported();
}
void GL_APIENTRY G_glVertexAttrib4f(GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w)>(ResolveAngleGl("glVertexAttrib4f"));
    if (fn) fn(index, x, y, z, w); else NoteGlUnsupported();
}
void GL_APIENTRY G_glVertexAttrib4fv(GLuint index, const GLfloat *v) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint index, const GLfloat *v)>(ResolveAngleGl("glVertexAttrib4fv"));
    if (fn) fn(index, v); else NoteGlUnsupported();
}
void GL_APIENTRY G_glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer)>(ResolveAngleGl("glVertexAttribPointer"));
    if (fn) fn(index, size, type, normalized, stride, pointer); else NoteGlUnsupported();
}
void GL_APIENTRY G_glReadBuffer(GLenum src) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum src)>(ResolveAngleGl("glReadBuffer"));
    if (fn) fn(src); else NoteGlUnsupported();
}
void GL_APIENTRY G_glDrawRangeElements(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const void *indices) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const void *indices)>(ResolveAngleGl("glDrawRangeElements"));
    if (fn) fn(mode, start, end, count, type, indices); else NoteGlUnsupported();
}
void GL_APIENTRY G_glTexImage3D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const void *pixels) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const void *pixels)>(ResolveAngleGl("glTexImage3D"));
    if (fn) fn(target, level, internalformat, width, height, depth, border, format, type, pixels); else NoteGlUnsupported();
}
void GL_APIENTRY G_glTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void *pixels) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void *pixels)>(ResolveAngleGl("glTexSubImage3D"));
    if (fn) fn(target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels); else NoteGlUnsupported();
}
void GL_APIENTRY G_glCopyTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height)>(ResolveAngleGl("glCopyTexSubImage3D"));
    if (fn) fn(target, level, xoffset, yoffset, zoffset, x, y, width, height); else NoteGlUnsupported();
}
void GL_APIENTRY G_glCompressedTexImage3D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const void *data) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const void *data)>(ResolveAngleGl("glCompressedTexImage3D"));
    if (fn) fn(target, level, internalformat, width, height, depth, border, imageSize, data); else NoteGlUnsupported();
}
void GL_APIENTRY G_glCompressedTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void *data) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void *data)>(ResolveAngleGl("glCompressedTexSubImage3D"));
    if (fn) fn(target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, data); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGenQueries(GLsizei n, GLuint *ids) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLsizei n, GLuint *ids)>(ResolveAngleGl("glGenQueries"));
    if (fn) fn(n, ids); else NoteGlUnsupported();
}
void GL_APIENTRY G_glDeleteQueries(GLsizei n, const GLuint *ids) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLsizei n, const GLuint *ids)>(ResolveAngleGl("glDeleteQueries"));
    if (fn) fn(n, ids); else NoteGlUnsupported();
}
GLboolean GL_APIENTRY G_glIsQuery(GLuint id) {
    if (!GuestGlReady()) return static_cast<GLboolean>(0);
    static auto fn = reinterpret_cast<GLboolean (GL_APIENTRY *)(GLuint id)>(ResolveAngleGl("glIsQuery"));
    if (fn) return fn(id);
    NoteGlUnsupported();
    return static_cast<GLboolean>(0);
}
void GL_APIENTRY G_glBeginQuery(GLenum target, GLuint id) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLuint id)>(ResolveAngleGl("glBeginQuery"));
    if (fn) fn(target, id); else NoteGlUnsupported();
}
void GL_APIENTRY G_glEndQuery(GLenum target) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target)>(ResolveAngleGl("glEndQuery"));
    if (fn) fn(target); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetQueryiv(GLenum target, GLenum pname, GLint *params) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLenum pname, GLint *params)>(ResolveAngleGl("glGetQueryiv"));
    if (fn) fn(target, pname, params); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetQueryObjectuiv(GLuint id, GLenum pname, GLuint *params) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint id, GLenum pname, GLuint *params)>(ResolveAngleGl("glGetQueryObjectuiv"));
    if (fn) fn(id, pname, params); else NoteGlUnsupported();
}
GLboolean GL_APIENTRY G_glUnmapBuffer(GLenum target) {
    if (!GuestGlReady()) return static_cast<GLboolean>(0);
    static auto fn = reinterpret_cast<GLboolean (GL_APIENTRY *)(GLenum target)>(ResolveAngleGl("glUnmapBuffer"));
    if (fn) return fn(target);
    NoteGlUnsupported();
    return static_cast<GLboolean>(0);
}
void GL_APIENTRY G_glGetBufferPointerv(GLenum target, GLenum pname, void **params) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLenum pname, void **params)>(ResolveAngleGl("glGetBufferPointerv"));
    if (fn) fn(target, pname, params); else NoteGlUnsupported();
}
void GL_APIENTRY G_glDrawBuffers(GLsizei n, const GLenum *bufs) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLsizei n, const GLenum *bufs)>(ResolveAngleGl("glDrawBuffers"));
    if (fn) fn(n, bufs); else NoteGlUnsupported();
}
void GL_APIENTRY G_glUniformMatrix2x3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)>(ResolveAngleGl("glUniformMatrix2x3fv"));
    if (fn) fn(location, count, transpose, value); else NoteGlUnsupported();
}
void GL_APIENTRY G_glUniformMatrix3x2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)>(ResolveAngleGl("glUniformMatrix3x2fv"));
    if (fn) fn(location, count, transpose, value); else NoteGlUnsupported();
}
void GL_APIENTRY G_glUniformMatrix2x4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)>(ResolveAngleGl("glUniformMatrix2x4fv"));
    if (fn) fn(location, count, transpose, value); else NoteGlUnsupported();
}
void GL_APIENTRY G_glUniformMatrix4x2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)>(ResolveAngleGl("glUniformMatrix4x2fv"));
    if (fn) fn(location, count, transpose, value); else NoteGlUnsupported();
}
void GL_APIENTRY G_glUniformMatrix3x4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)>(ResolveAngleGl("glUniformMatrix3x4fv"));
    if (fn) fn(location, count, transpose, value); else NoteGlUnsupported();
}
void GL_APIENTRY G_glUniformMatrix4x3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)>(ResolveAngleGl("glUniformMatrix4x3fv"));
    if (fn) fn(location, count, transpose, value); else NoteGlUnsupported();
}
void GL_APIENTRY G_glBlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter)>(ResolveAngleGl("glBlitFramebuffer"));
    if (fn) fn(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter); else NoteGlUnsupported();
}
void GL_APIENTRY G_glRenderbufferStorageMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height)>(ResolveAngleGl("glRenderbufferStorageMultisample"));
    if (fn) fn(target, samples, internalformat, width, height); else NoteGlUnsupported();
}
void GL_APIENTRY G_glFramebufferTextureLayer(GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer)>(ResolveAngleGl("glFramebufferTextureLayer"));
    if (fn) fn(target, attachment, texture, level, layer); else NoteGlUnsupported();
}
void * GL_APIENTRY G_glMapBufferRange(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access) {
    if (!GuestGlReady()) return static_cast<void *>(0);
    static auto fn = reinterpret_cast<void * (GL_APIENTRY *)(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access)>(ResolveAngleGl("glMapBufferRange"));
    if (fn) return fn(target, offset, length, access);
    NoteGlUnsupported();
    return static_cast<void *>(0);
}
void GL_APIENTRY G_glFlushMappedBufferRange(GLenum target, GLintptr offset, GLsizeiptr length) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLintptr offset, GLsizeiptr length)>(ResolveAngleGl("glFlushMappedBufferRange"));
    if (fn) fn(target, offset, length); else NoteGlUnsupported();
}
void GL_APIENTRY G_glBindVertexArray(GLuint array) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint array)>(ResolveAngleGl("glBindVertexArray"));
    if (fn) fn(array); else NoteGlUnsupported();
}
void GL_APIENTRY G_glDeleteVertexArrays(GLsizei n, const GLuint *arrays) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLsizei n, const GLuint *arrays)>(ResolveAngleGl("glDeleteVertexArrays"));
    if (fn) fn(n, arrays); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGenVertexArrays(GLsizei n, GLuint *arrays) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLsizei n, GLuint *arrays)>(ResolveAngleGl("glGenVertexArrays"));
    if (fn) fn(n, arrays); else NoteGlUnsupported();
}
GLboolean GL_APIENTRY G_glIsVertexArray(GLuint array) {
    if (!GuestGlReady()) return static_cast<GLboolean>(0);
    static auto fn = reinterpret_cast<GLboolean (GL_APIENTRY *)(GLuint array)>(ResolveAngleGl("glIsVertexArray"));
    if (fn) return fn(array);
    NoteGlUnsupported();
    return static_cast<GLboolean>(0);
}
void GL_APIENTRY G_glGetIntegeri_v(GLenum target, GLuint index, GLint *data) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLuint index, GLint *data)>(ResolveAngleGl("glGetIntegeri_v"));
    if (fn) fn(target, index, data); else NoteGlUnsupported();
}
void GL_APIENTRY G_glBeginTransformFeedback(GLenum primitiveMode) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum primitiveMode)>(ResolveAngleGl("glBeginTransformFeedback"));
    if (fn) fn(primitiveMode); else NoteGlUnsupported();
}
void GL_APIENTRY G_glEndTransformFeedback() {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)()>(ResolveAngleGl("glEndTransformFeedback"));
    if (fn) fn(); else NoteGlUnsupported();
}
void GL_APIENTRY G_glBindBufferRange(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size)>(ResolveAngleGl("glBindBufferRange"));
    if (fn) fn(target, index, buffer, offset, size); else NoteGlUnsupported();
}
void GL_APIENTRY G_glBindBufferBase(GLenum target, GLuint index, GLuint buffer) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLuint index, GLuint buffer)>(ResolveAngleGl("glBindBufferBase"));
    if (fn) fn(target, index, buffer); else NoteGlUnsupported();
}
void GL_APIENTRY G_glTransformFeedbackVaryings(GLuint program, GLsizei count, const GLchar *const*varyings, GLenum bufferMode) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint program, GLsizei count, const GLchar *const*varyings, GLenum bufferMode)>(ResolveAngleGl("glTransformFeedbackVaryings"));
    if (fn) fn(program, count, varyings, bufferMode); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetTransformFeedbackVarying(GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLsizei *size, GLenum *type, GLchar *name) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLsizei *size, GLenum *type, GLchar *name)>(ResolveAngleGl("glGetTransformFeedbackVarying"));
    if (fn) fn(program, index, bufSize, length, size, type, name); else NoteGlUnsupported();
}
void GL_APIENTRY G_glVertexAttribIPointer(GLuint index, GLint size, GLenum type, GLsizei stride, const void *pointer) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint index, GLint size, GLenum type, GLsizei stride, const void *pointer)>(ResolveAngleGl("glVertexAttribIPointer"));
    if (fn) fn(index, size, type, stride, pointer); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetVertexAttribIiv(GLuint index, GLenum pname, GLint *params) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint index, GLenum pname, GLint *params)>(ResolveAngleGl("glGetVertexAttribIiv"));
    if (fn) fn(index, pname, params); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetVertexAttribIuiv(GLuint index, GLenum pname, GLuint *params) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint index, GLenum pname, GLuint *params)>(ResolveAngleGl("glGetVertexAttribIuiv"));
    if (fn) fn(index, pname, params); else NoteGlUnsupported();
}
void GL_APIENTRY G_glVertexAttribI4i(GLuint index, GLint x, GLint y, GLint z, GLint w) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint index, GLint x, GLint y, GLint z, GLint w)>(ResolveAngleGl("glVertexAttribI4i"));
    if (fn) fn(index, x, y, z, w); else NoteGlUnsupported();
}
void GL_APIENTRY G_glVertexAttribI4ui(GLuint index, GLuint x, GLuint y, GLuint z, GLuint w) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint index, GLuint x, GLuint y, GLuint z, GLuint w)>(ResolveAngleGl("glVertexAttribI4ui"));
    if (fn) fn(index, x, y, z, w); else NoteGlUnsupported();
}
void GL_APIENTRY G_glVertexAttribI4iv(GLuint index, const GLint *v) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint index, const GLint *v)>(ResolveAngleGl("glVertexAttribI4iv"));
    if (fn) fn(index, v); else NoteGlUnsupported();
}
void GL_APIENTRY G_glVertexAttribI4uiv(GLuint index, const GLuint *v) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint index, const GLuint *v)>(ResolveAngleGl("glVertexAttribI4uiv"));
    if (fn) fn(index, v); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetUniformuiv(GLuint program, GLint location, GLuint *params) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint program, GLint location, GLuint *params)>(ResolveAngleGl("glGetUniformuiv"));
    if (fn) fn(program, location, params); else NoteGlUnsupported();
}
GLint GL_APIENTRY G_glGetFragDataLocation(GLuint program, const GLchar *name) {
    if (!GuestGlReady()) return static_cast<GLint>(0);
    static auto fn = reinterpret_cast<GLint (GL_APIENTRY *)(GLuint program, const GLchar *name)>(ResolveAngleGl("glGetFragDataLocation"));
    if (fn) return fn(program, name);
    NoteGlUnsupported();
    return static_cast<GLint>(0);
}
void GL_APIENTRY G_glUniform1ui(GLint location, GLuint v0) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLint location, GLuint v0)>(ResolveAngleGl("glUniform1ui"));
    if (fn) fn(location, v0); else NoteGlUnsupported();
}
void GL_APIENTRY G_glUniform2ui(GLint location, GLuint v0, GLuint v1) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLint location, GLuint v0, GLuint v1)>(ResolveAngleGl("glUniform2ui"));
    if (fn) fn(location, v0, v1); else NoteGlUnsupported();
}
void GL_APIENTRY G_glUniform3ui(GLint location, GLuint v0, GLuint v1, GLuint v2) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLint location, GLuint v0, GLuint v1, GLuint v2)>(ResolveAngleGl("glUniform3ui"));
    if (fn) fn(location, v0, v1, v2); else NoteGlUnsupported();
}
void GL_APIENTRY G_glUniform4ui(GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3)>(ResolveAngleGl("glUniform4ui"));
    if (fn) fn(location, v0, v1, v2, v3); else NoteGlUnsupported();
}
void GL_APIENTRY G_glUniform1uiv(GLint location, GLsizei count, const GLuint *value) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLint location, GLsizei count, const GLuint *value)>(ResolveAngleGl("glUniform1uiv"));
    if (fn) fn(location, count, value); else NoteGlUnsupported();
}
void GL_APIENTRY G_glUniform2uiv(GLint location, GLsizei count, const GLuint *value) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLint location, GLsizei count, const GLuint *value)>(ResolveAngleGl("glUniform2uiv"));
    if (fn) fn(location, count, value); else NoteGlUnsupported();
}
void GL_APIENTRY G_glUniform3uiv(GLint location, GLsizei count, const GLuint *value) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLint location, GLsizei count, const GLuint *value)>(ResolveAngleGl("glUniform3uiv"));
    if (fn) fn(location, count, value); else NoteGlUnsupported();
}
void GL_APIENTRY G_glUniform4uiv(GLint location, GLsizei count, const GLuint *value) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLint location, GLsizei count, const GLuint *value)>(ResolveAngleGl("glUniform4uiv"));
    if (fn) fn(location, count, value); else NoteGlUnsupported();
}
void GL_APIENTRY G_glClearBufferiv(GLenum buffer, GLint drawbuffer, const GLint *value) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum buffer, GLint drawbuffer, const GLint *value)>(ResolveAngleGl("glClearBufferiv"));
    if (fn) fn(buffer, drawbuffer, value); else NoteGlUnsupported();
}
void GL_APIENTRY G_glClearBufferuiv(GLenum buffer, GLint drawbuffer, const GLuint *value) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum buffer, GLint drawbuffer, const GLuint *value)>(ResolveAngleGl("glClearBufferuiv"));
    if (fn) fn(buffer, drawbuffer, value); else NoteGlUnsupported();
}
void GL_APIENTRY G_glClearBufferfv(GLenum buffer, GLint drawbuffer, const GLfloat *value) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum buffer, GLint drawbuffer, const GLfloat *value)>(ResolveAngleGl("glClearBufferfv"));
    if (fn) fn(buffer, drawbuffer, value); else NoteGlUnsupported();
}
void GL_APIENTRY G_glClearBufferfi(GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil)>(ResolveAngleGl("glClearBufferfi"));
    if (fn) fn(buffer, drawbuffer, depth, stencil); else NoteGlUnsupported();
}
void GL_APIENTRY G_glCopyBufferSubData(GLenum readTarget, GLenum writeTarget, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum readTarget, GLenum writeTarget, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size)>(ResolveAngleGl("glCopyBufferSubData"));
    if (fn) fn(readTarget, writeTarget, readOffset, writeOffset, size); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetUniformIndices(GLuint program, GLsizei uniformCount, const GLchar *const*uniformNames, GLuint *uniformIndices) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint program, GLsizei uniformCount, const GLchar *const*uniformNames, GLuint *uniformIndices)>(ResolveAngleGl("glGetUniformIndices"));
    if (fn) fn(program, uniformCount, uniformNames, uniformIndices); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetActiveUniformsiv(GLuint program, GLsizei uniformCount, const GLuint *uniformIndices, GLenum pname, GLint *params) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint program, GLsizei uniformCount, const GLuint *uniformIndices, GLenum pname, GLint *params)>(ResolveAngleGl("glGetActiveUniformsiv"));
    if (fn) fn(program, uniformCount, uniformIndices, pname, params); else NoteGlUnsupported();
}
GLuint GL_APIENTRY G_glGetUniformBlockIndex(GLuint program, const GLchar *uniformBlockName) {
    if (!GuestGlReady()) return static_cast<GLuint>(0);
    static auto fn = reinterpret_cast<GLuint (GL_APIENTRY *)(GLuint program, const GLchar *uniformBlockName)>(ResolveAngleGl("glGetUniformBlockIndex"));
    if (fn) return fn(program, uniformBlockName);
    NoteGlUnsupported();
    return static_cast<GLuint>(0);
}
void GL_APIENTRY G_glGetActiveUniformBlockiv(GLuint program, GLuint uniformBlockIndex, GLenum pname, GLint *params) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint program, GLuint uniformBlockIndex, GLenum pname, GLint *params)>(ResolveAngleGl("glGetActiveUniformBlockiv"));
    if (fn) fn(program, uniformBlockIndex, pname, params); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetActiveUniformBlockName(GLuint program, GLuint uniformBlockIndex, GLsizei bufSize, GLsizei *length, GLchar *uniformBlockName) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint program, GLuint uniformBlockIndex, GLsizei bufSize, GLsizei *length, GLchar *uniformBlockName)>(ResolveAngleGl("glGetActiveUniformBlockName"));
    if (fn) fn(program, uniformBlockIndex, bufSize, length, uniformBlockName); else NoteGlUnsupported();
}
void GL_APIENTRY G_glUniformBlockBinding(GLuint program, GLuint uniformBlockIndex, GLuint uniformBlockBinding) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint program, GLuint uniformBlockIndex, GLuint uniformBlockBinding)>(ResolveAngleGl("glUniformBlockBinding"));
    if (fn) fn(program, uniformBlockIndex, uniformBlockBinding); else NoteGlUnsupported();
}
void GL_APIENTRY G_glDrawArraysInstanced(GLenum mode, GLint first, GLsizei count, GLsizei instancecount) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum mode, GLint first, GLsizei count, GLsizei instancecount)>(ResolveAngleGl("glDrawArraysInstanced"));
    if (fn) fn(mode, first, count, instancecount); else NoteGlUnsupported();
}
void GL_APIENTRY G_glDrawElementsInstanced(GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount)>(ResolveAngleGl("glDrawElementsInstanced"));
    if (fn) fn(mode, count, type, indices, instancecount); else NoteGlUnsupported();
}
GLsync GL_APIENTRY G_glFenceSync(GLenum condition, GLbitfield flags) {
    if (!GuestGlReady()) return static_cast<GLsync>(0);
    static auto fn = reinterpret_cast<GLsync (GL_APIENTRY *)(GLenum condition, GLbitfield flags)>(ResolveAngleGl("glFenceSync"));
    if (fn) return fn(condition, flags);
    NoteGlUnsupported();
    return static_cast<GLsync>(0);
}
GLboolean GL_APIENTRY G_glIsSync(GLsync sync) {
    if (!GuestGlReady()) return static_cast<GLboolean>(0);
    static auto fn = reinterpret_cast<GLboolean (GL_APIENTRY *)(GLsync sync)>(ResolveAngleGl("glIsSync"));
    if (fn) return fn(sync);
    NoteGlUnsupported();
    return static_cast<GLboolean>(0);
}
void GL_APIENTRY G_glDeleteSync(GLsync sync) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLsync sync)>(ResolveAngleGl("glDeleteSync"));
    if (fn) fn(sync); else NoteGlUnsupported();
}
GLenum GL_APIENTRY G_glClientWaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout) {
    if (!GuestGlReady()) return static_cast<GLenum>(0);
    static auto fn = reinterpret_cast<GLenum (GL_APIENTRY *)(GLsync sync, GLbitfield flags, GLuint64 timeout)>(ResolveAngleGl("glClientWaitSync"));
    if (fn) return fn(sync, flags, timeout);
    NoteGlUnsupported();
    return static_cast<GLenum>(0);
}
void GL_APIENTRY G_glWaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLsync sync, GLbitfield flags, GLuint64 timeout)>(ResolveAngleGl("glWaitSync"));
    if (fn) fn(sync, flags, timeout); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetInteger64v(GLenum pname, GLint64 *data) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum pname, GLint64 *data)>(ResolveAngleGl("glGetInteger64v"));
    if (fn) fn(pname, data); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetSynciv(GLsync sync, GLenum pname, GLsizei count, GLsizei *length, GLint *values) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLsync sync, GLenum pname, GLsizei count, GLsizei *length, GLint *values)>(ResolveAngleGl("glGetSynciv"));
    if (fn) fn(sync, pname, count, length, values); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetInteger64i_v(GLenum target, GLuint index, GLint64 *data) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLuint index, GLint64 *data)>(ResolveAngleGl("glGetInteger64i_v"));
    if (fn) fn(target, index, data); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetBufferParameteri64v(GLenum target, GLenum pname, GLint64 *params) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLenum pname, GLint64 *params)>(ResolveAngleGl("glGetBufferParameteri64v"));
    if (fn) fn(target, pname, params); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGenSamplers(GLsizei count, GLuint *samplers) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLsizei count, GLuint *samplers)>(ResolveAngleGl("glGenSamplers"));
    if (fn) fn(count, samplers); else NoteGlUnsupported();
}
void GL_APIENTRY G_glDeleteSamplers(GLsizei count, const GLuint *samplers) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLsizei count, const GLuint *samplers)>(ResolveAngleGl("glDeleteSamplers"));
    if (fn) fn(count, samplers); else NoteGlUnsupported();
}
GLboolean GL_APIENTRY G_glIsSampler(GLuint sampler) {
    if (!GuestGlReady()) return static_cast<GLboolean>(0);
    static auto fn = reinterpret_cast<GLboolean (GL_APIENTRY *)(GLuint sampler)>(ResolveAngleGl("glIsSampler"));
    if (fn) return fn(sampler);
    NoteGlUnsupported();
    return static_cast<GLboolean>(0);
}
void GL_APIENTRY G_glBindSampler(GLuint unit, GLuint sampler) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint unit, GLuint sampler)>(ResolveAngleGl("glBindSampler"));
    if (fn) fn(unit, sampler); else NoteGlUnsupported();
}
void GL_APIENTRY G_glSamplerParameteri(GLuint sampler, GLenum pname, GLint param) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint sampler, GLenum pname, GLint param)>(ResolveAngleGl("glSamplerParameteri"));
    if (fn) fn(sampler, pname, param); else NoteGlUnsupported();
}
void GL_APIENTRY G_glSamplerParameteriv(GLuint sampler, GLenum pname, const GLint *param) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint sampler, GLenum pname, const GLint *param)>(ResolveAngleGl("glSamplerParameteriv"));
    if (fn) fn(sampler, pname, param); else NoteGlUnsupported();
}
void GL_APIENTRY G_glSamplerParameterf(GLuint sampler, GLenum pname, GLfloat param) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint sampler, GLenum pname, GLfloat param)>(ResolveAngleGl("glSamplerParameterf"));
    if (fn) fn(sampler, pname, param); else NoteGlUnsupported();
}
void GL_APIENTRY G_glSamplerParameterfv(GLuint sampler, GLenum pname, const GLfloat *param) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint sampler, GLenum pname, const GLfloat *param)>(ResolveAngleGl("glSamplerParameterfv"));
    if (fn) fn(sampler, pname, param); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetSamplerParameteriv(GLuint sampler, GLenum pname, GLint *params) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint sampler, GLenum pname, GLint *params)>(ResolveAngleGl("glGetSamplerParameteriv"));
    if (fn) fn(sampler, pname, params); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetSamplerParameterfv(GLuint sampler, GLenum pname, GLfloat *params) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint sampler, GLenum pname, GLfloat *params)>(ResolveAngleGl("glGetSamplerParameterfv"));
    if (fn) fn(sampler, pname, params); else NoteGlUnsupported();
}
void GL_APIENTRY G_glVertexAttribDivisor(GLuint index, GLuint divisor) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint index, GLuint divisor)>(ResolveAngleGl("glVertexAttribDivisor"));
    if (fn) fn(index, divisor); else NoteGlUnsupported();
}
void GL_APIENTRY G_glBindTransformFeedback(GLenum target, GLuint id) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLuint id)>(ResolveAngleGl("glBindTransformFeedback"));
    if (fn) fn(target, id); else NoteGlUnsupported();
}
void GL_APIENTRY G_glDeleteTransformFeedbacks(GLsizei n, const GLuint *ids) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLsizei n, const GLuint *ids)>(ResolveAngleGl("glDeleteTransformFeedbacks"));
    if (fn) fn(n, ids); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGenTransformFeedbacks(GLsizei n, GLuint *ids) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLsizei n, GLuint *ids)>(ResolveAngleGl("glGenTransformFeedbacks"));
    if (fn) fn(n, ids); else NoteGlUnsupported();
}
GLboolean GL_APIENTRY G_glIsTransformFeedback(GLuint id) {
    if (!GuestGlReady()) return static_cast<GLboolean>(0);
    static auto fn = reinterpret_cast<GLboolean (GL_APIENTRY *)(GLuint id)>(ResolveAngleGl("glIsTransformFeedback"));
    if (fn) return fn(id);
    NoteGlUnsupported();
    return static_cast<GLboolean>(0);
}
void GL_APIENTRY G_glPauseTransformFeedback() {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)()>(ResolveAngleGl("glPauseTransformFeedback"));
    if (fn) fn(); else NoteGlUnsupported();
}
void GL_APIENTRY G_glResumeTransformFeedback() {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)()>(ResolveAngleGl("glResumeTransformFeedback"));
    if (fn) fn(); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetProgramBinary(GLuint program, GLsizei bufSize, GLsizei *length, GLenum *binaryFormat, void *binary) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint program, GLsizei bufSize, GLsizei *length, GLenum *binaryFormat, void *binary)>(ResolveAngleGl("glGetProgramBinary"));
    if (fn) fn(program, bufSize, length, binaryFormat, binary); else NoteGlUnsupported();
}
void GL_APIENTRY G_glProgramBinary(GLuint program, GLenum binaryFormat, const void *binary, GLsizei length) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint program, GLenum binaryFormat, const void *binary, GLsizei length)>(ResolveAngleGl("glProgramBinary"));
    if (fn) fn(program, binaryFormat, binary, length); else NoteGlUnsupported();
}
void GL_APIENTRY G_glProgramParameteri(GLuint program, GLenum pname, GLint value) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint program, GLenum pname, GLint value)>(ResolveAngleGl("glProgramParameteri"));
    if (fn) fn(program, pname, value); else NoteGlUnsupported();
}
void GL_APIENTRY G_glInvalidateFramebuffer(GLenum target, GLsizei numAttachments, const GLenum *attachments) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLsizei numAttachments, const GLenum *attachments)>(ResolveAngleGl("glInvalidateFramebuffer"));
    if (fn) fn(target, numAttachments, attachments); else NoteGlUnsupported();
}
void GL_APIENTRY G_glInvalidateSubFramebuffer(GLenum target, GLsizei numAttachments, const GLenum *attachments, GLint x, GLint y, GLsizei width, GLsizei height) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLsizei numAttachments, const GLenum *attachments, GLint x, GLint y, GLsizei width, GLsizei height)>(ResolveAngleGl("glInvalidateSubFramebuffer"));
    if (fn) fn(target, numAttachments, attachments, x, y, width, height); else NoteGlUnsupported();
}
void GL_APIENTRY G_glTexStorage2D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height)>(ResolveAngleGl("glTexStorage2D"));
    if (fn) fn(target, levels, internalformat, width, height); else NoteGlUnsupported();
}
void GL_APIENTRY G_glTexStorage3D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth)>(ResolveAngleGl("glTexStorage3D"));
    if (fn) fn(target, levels, internalformat, width, height, depth); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetInternalformativ(GLenum target, GLenum internalformat, GLenum pname, GLsizei count, GLint *params) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLenum internalformat, GLenum pname, GLsizei count, GLint *params)>(ResolveAngleGl("glGetInternalformativ"));
    if (fn) fn(target, internalformat, pname, count, params); else NoteGlUnsupported();
}
void GL_APIENTRY G_glDispatchCompute(GLuint num_groups_x, GLuint num_groups_y, GLuint num_groups_z) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint num_groups_x, GLuint num_groups_y, GLuint num_groups_z)>(ResolveAngleGl("glDispatchCompute"));
    if (fn) fn(num_groups_x, num_groups_y, num_groups_z); else NoteGlUnsupported();
}
void GL_APIENTRY G_glDispatchComputeIndirect(GLintptr indirect) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLintptr indirect)>(ResolveAngleGl("glDispatchComputeIndirect"));
    if (fn) fn(indirect); else NoteGlUnsupported();
}
void GL_APIENTRY G_glDrawArraysIndirect(GLenum mode, const void *indirect) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum mode, const void *indirect)>(ResolveAngleGl("glDrawArraysIndirect"));
    if (fn) fn(mode, indirect); else NoteGlUnsupported();
}
void GL_APIENTRY G_glDrawElementsIndirect(GLenum mode, GLenum type, const void *indirect) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum mode, GLenum type, const void *indirect)>(ResolveAngleGl("glDrawElementsIndirect"));
    if (fn) fn(mode, type, indirect); else NoteGlUnsupported();
}
void GL_APIENTRY G_glFramebufferParameteri(GLenum target, GLenum pname, GLint param) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLenum pname, GLint param)>(ResolveAngleGl("glFramebufferParameteri"));
    if (fn) fn(target, pname, param); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetFramebufferParameteriv(GLenum target, GLenum pname, GLint *params) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLenum pname, GLint *params)>(ResolveAngleGl("glGetFramebufferParameteriv"));
    if (fn) fn(target, pname, params); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetProgramInterfaceiv(GLuint program, GLenum programInterface, GLenum pname, GLint *params) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint program, GLenum programInterface, GLenum pname, GLint *params)>(ResolveAngleGl("glGetProgramInterfaceiv"));
    if (fn) fn(program, programInterface, pname, params); else NoteGlUnsupported();
}
GLuint GL_APIENTRY G_glGetProgramResourceIndex(GLuint program, GLenum programInterface, const GLchar *name) {
    if (!GuestGlReady()) return static_cast<GLuint>(0);
    static auto fn = reinterpret_cast<GLuint (GL_APIENTRY *)(GLuint program, GLenum programInterface, const GLchar *name)>(ResolveAngleGl("glGetProgramResourceIndex"));
    if (fn) return fn(program, programInterface, name);
    NoteGlUnsupported();
    return static_cast<GLuint>(0);
}
void GL_APIENTRY G_glGetProgramResourceName(GLuint program, GLenum programInterface, GLuint index, GLsizei bufSize, GLsizei *length, GLchar *name) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint program, GLenum programInterface, GLuint index, GLsizei bufSize, GLsizei *length, GLchar *name)>(ResolveAngleGl("glGetProgramResourceName"));
    if (fn) fn(program, programInterface, index, bufSize, length, name); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetProgramResourceiv(GLuint program, GLenum programInterface, GLuint index, GLsizei propCount, const GLenum *props, GLsizei count, GLsizei *length, GLint *params) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint program, GLenum programInterface, GLuint index, GLsizei propCount, const GLenum *props, GLsizei count, GLsizei *length, GLint *params)>(ResolveAngleGl("glGetProgramResourceiv"));
    if (fn) fn(program, programInterface, index, propCount, props, count, length, params); else NoteGlUnsupported();
}
GLint GL_APIENTRY G_glGetProgramResourceLocation(GLuint program, GLenum programInterface, const GLchar *name) {
    if (!GuestGlReady()) return static_cast<GLint>(0);
    static auto fn = reinterpret_cast<GLint (GL_APIENTRY *)(GLuint program, GLenum programInterface, const GLchar *name)>(ResolveAngleGl("glGetProgramResourceLocation"));
    if (fn) return fn(program, programInterface, name);
    NoteGlUnsupported();
    return static_cast<GLint>(0);
}
void GL_APIENTRY G_glUseProgramStages(GLuint pipeline, GLbitfield stages, GLuint program) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint pipeline, GLbitfield stages, GLuint program)>(ResolveAngleGl("glUseProgramStages"));
    if (fn) fn(pipeline, stages, program); else NoteGlUnsupported();
}
void GL_APIENTRY G_glActiveShaderProgram(GLuint pipeline, GLuint program) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint pipeline, GLuint program)>(ResolveAngleGl("glActiveShaderProgram"));
    if (fn) fn(pipeline, program); else NoteGlUnsupported();
}
GLuint GL_APIENTRY G_glCreateShaderProgramv(GLenum type, GLsizei count, const GLchar *const*strings) {
    if (!GuestGlReady()) return static_cast<GLuint>(0);
    static auto fn = reinterpret_cast<GLuint (GL_APIENTRY *)(GLenum type, GLsizei count, const GLchar *const*strings)>(ResolveAngleGl("glCreateShaderProgramv"));
    if (fn) return fn(type, count, strings);
    NoteGlUnsupported();
    return static_cast<GLuint>(0);
}
void GL_APIENTRY G_glBindProgramPipeline(GLuint pipeline) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint pipeline)>(ResolveAngleGl("glBindProgramPipeline"));
    if (fn) fn(pipeline); else NoteGlUnsupported();
}
void GL_APIENTRY G_glDeleteProgramPipelines(GLsizei n, const GLuint *pipelines) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLsizei n, const GLuint *pipelines)>(ResolveAngleGl("glDeleteProgramPipelines"));
    if (fn) fn(n, pipelines); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGenProgramPipelines(GLsizei n, GLuint *pipelines) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLsizei n, GLuint *pipelines)>(ResolveAngleGl("glGenProgramPipelines"));
    if (fn) fn(n, pipelines); else NoteGlUnsupported();
}
GLboolean GL_APIENTRY G_glIsProgramPipeline(GLuint pipeline) {
    if (!GuestGlReady()) return static_cast<GLboolean>(0);
    static auto fn = reinterpret_cast<GLboolean (GL_APIENTRY *)(GLuint pipeline)>(ResolveAngleGl("glIsProgramPipeline"));
    if (fn) return fn(pipeline);
    NoteGlUnsupported();
    return static_cast<GLboolean>(0);
}
void GL_APIENTRY G_glGetProgramPipelineiv(GLuint pipeline, GLenum pname, GLint *params) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint pipeline, GLenum pname, GLint *params)>(ResolveAngleGl("glGetProgramPipelineiv"));
    if (fn) fn(pipeline, pname, params); else NoteGlUnsupported();
}
void GL_APIENTRY G_glProgramUniform1i(GLuint program, GLint location, GLint v0) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint program, GLint location, GLint v0)>(ResolveAngleGl("glProgramUniform1i"));
    if (fn) fn(program, location, v0); else NoteGlUnsupported();
}
void GL_APIENTRY G_glProgramUniform2i(GLuint program, GLint location, GLint v0, GLint v1) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint program, GLint location, GLint v0, GLint v1)>(ResolveAngleGl("glProgramUniform2i"));
    if (fn) fn(program, location, v0, v1); else NoteGlUnsupported();
}
void GL_APIENTRY G_glProgramUniform3i(GLuint program, GLint location, GLint v0, GLint v1, GLint v2) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint program, GLint location, GLint v0, GLint v1, GLint v2)>(ResolveAngleGl("glProgramUniform3i"));
    if (fn) fn(program, location, v0, v1, v2); else NoteGlUnsupported();
}
void GL_APIENTRY G_glProgramUniform4i(GLuint program, GLint location, GLint v0, GLint v1, GLint v2, GLint v3) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint program, GLint location, GLint v0, GLint v1, GLint v2, GLint v3)>(ResolveAngleGl("glProgramUniform4i"));
    if (fn) fn(program, location, v0, v1, v2, v3); else NoteGlUnsupported();
}
void GL_APIENTRY G_glProgramUniform1ui(GLuint program, GLint location, GLuint v0) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint program, GLint location, GLuint v0)>(ResolveAngleGl("glProgramUniform1ui"));
    if (fn) fn(program, location, v0); else NoteGlUnsupported();
}
void GL_APIENTRY G_glProgramUniform2ui(GLuint program, GLint location, GLuint v0, GLuint v1) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint program, GLint location, GLuint v0, GLuint v1)>(ResolveAngleGl("glProgramUniform2ui"));
    if (fn) fn(program, location, v0, v1); else NoteGlUnsupported();
}
void GL_APIENTRY G_glProgramUniform3ui(GLuint program, GLint location, GLuint v0, GLuint v1, GLuint v2) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint program, GLint location, GLuint v0, GLuint v1, GLuint v2)>(ResolveAngleGl("glProgramUniform3ui"));
    if (fn) fn(program, location, v0, v1, v2); else NoteGlUnsupported();
}
void GL_APIENTRY G_glProgramUniform4ui(GLuint program, GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint program, GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3)>(ResolveAngleGl("glProgramUniform4ui"));
    if (fn) fn(program, location, v0, v1, v2, v3); else NoteGlUnsupported();
}
void GL_APIENTRY G_glProgramUniform1f(GLuint program, GLint location, GLfloat v0) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint program, GLint location, GLfloat v0)>(ResolveAngleGl("glProgramUniform1f"));
    if (fn) fn(program, location, v0); else NoteGlUnsupported();
}
void GL_APIENTRY G_glProgramUniform2f(GLuint program, GLint location, GLfloat v0, GLfloat v1) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint program, GLint location, GLfloat v0, GLfloat v1)>(ResolveAngleGl("glProgramUniform2f"));
    if (fn) fn(program, location, v0, v1); else NoteGlUnsupported();
}
void GL_APIENTRY G_glProgramUniform3f(GLuint program, GLint location, GLfloat v0, GLfloat v1, GLfloat v2) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint program, GLint location, GLfloat v0, GLfloat v1, GLfloat v2)>(ResolveAngleGl("glProgramUniform3f"));
    if (fn) fn(program, location, v0, v1, v2); else NoteGlUnsupported();
}
void GL_APIENTRY G_glProgramUniform4f(GLuint program, GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint program, GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3)>(ResolveAngleGl("glProgramUniform4f"));
    if (fn) fn(program, location, v0, v1, v2, v3); else NoteGlUnsupported();
}
void GL_APIENTRY G_glProgramUniform1iv(GLuint program, GLint location, GLsizei count, const GLint *value) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint program, GLint location, GLsizei count, const GLint *value)>(ResolveAngleGl("glProgramUniform1iv"));
    if (fn) fn(program, location, count, value); else NoteGlUnsupported();
}
void GL_APIENTRY G_glProgramUniform2iv(GLuint program, GLint location, GLsizei count, const GLint *value) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint program, GLint location, GLsizei count, const GLint *value)>(ResolveAngleGl("glProgramUniform2iv"));
    if (fn) fn(program, location, count, value); else NoteGlUnsupported();
}
void GL_APIENTRY G_glProgramUniform3iv(GLuint program, GLint location, GLsizei count, const GLint *value) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint program, GLint location, GLsizei count, const GLint *value)>(ResolveAngleGl("glProgramUniform3iv"));
    if (fn) fn(program, location, count, value); else NoteGlUnsupported();
}
void GL_APIENTRY G_glProgramUniform4iv(GLuint program, GLint location, GLsizei count, const GLint *value) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint program, GLint location, GLsizei count, const GLint *value)>(ResolveAngleGl("glProgramUniform4iv"));
    if (fn) fn(program, location, count, value); else NoteGlUnsupported();
}
void GL_APIENTRY G_glProgramUniform1uiv(GLuint program, GLint location, GLsizei count, const GLuint *value) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint program, GLint location, GLsizei count, const GLuint *value)>(ResolveAngleGl("glProgramUniform1uiv"));
    if (fn) fn(program, location, count, value); else NoteGlUnsupported();
}
void GL_APIENTRY G_glProgramUniform2uiv(GLuint program, GLint location, GLsizei count, const GLuint *value) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint program, GLint location, GLsizei count, const GLuint *value)>(ResolveAngleGl("glProgramUniform2uiv"));
    if (fn) fn(program, location, count, value); else NoteGlUnsupported();
}
void GL_APIENTRY G_glProgramUniform3uiv(GLuint program, GLint location, GLsizei count, const GLuint *value) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint program, GLint location, GLsizei count, const GLuint *value)>(ResolveAngleGl("glProgramUniform3uiv"));
    if (fn) fn(program, location, count, value); else NoteGlUnsupported();
}
void GL_APIENTRY G_glProgramUniform4uiv(GLuint program, GLint location, GLsizei count, const GLuint *value) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint program, GLint location, GLsizei count, const GLuint *value)>(ResolveAngleGl("glProgramUniform4uiv"));
    if (fn) fn(program, location, count, value); else NoteGlUnsupported();
}
void GL_APIENTRY G_glProgramUniform1fv(GLuint program, GLint location, GLsizei count, const GLfloat *value) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint program, GLint location, GLsizei count, const GLfloat *value)>(ResolveAngleGl("glProgramUniform1fv"));
    if (fn) fn(program, location, count, value); else NoteGlUnsupported();
}
void GL_APIENTRY G_glProgramUniform2fv(GLuint program, GLint location, GLsizei count, const GLfloat *value) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint program, GLint location, GLsizei count, const GLfloat *value)>(ResolveAngleGl("glProgramUniform2fv"));
    if (fn) fn(program, location, count, value); else NoteGlUnsupported();
}
void GL_APIENTRY G_glProgramUniform3fv(GLuint program, GLint location, GLsizei count, const GLfloat *value) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint program, GLint location, GLsizei count, const GLfloat *value)>(ResolveAngleGl("glProgramUniform3fv"));
    if (fn) fn(program, location, count, value); else NoteGlUnsupported();
}
void GL_APIENTRY G_glProgramUniform4fv(GLuint program, GLint location, GLsizei count, const GLfloat *value) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint program, GLint location, GLsizei count, const GLfloat *value)>(ResolveAngleGl("glProgramUniform4fv"));
    if (fn) fn(program, location, count, value); else NoteGlUnsupported();
}
void GL_APIENTRY G_glProgramUniformMatrix2fv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)>(ResolveAngleGl("glProgramUniformMatrix2fv"));
    if (fn) fn(program, location, count, transpose, value); else NoteGlUnsupported();
}
void GL_APIENTRY G_glProgramUniformMatrix3fv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)>(ResolveAngleGl("glProgramUniformMatrix3fv"));
    if (fn) fn(program, location, count, transpose, value); else NoteGlUnsupported();
}
void GL_APIENTRY G_glProgramUniformMatrix4fv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)>(ResolveAngleGl("glProgramUniformMatrix4fv"));
    if (fn) fn(program, location, count, transpose, value); else NoteGlUnsupported();
}
void GL_APIENTRY G_glProgramUniformMatrix2x3fv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)>(ResolveAngleGl("glProgramUniformMatrix2x3fv"));
    if (fn) fn(program, location, count, transpose, value); else NoteGlUnsupported();
}
void GL_APIENTRY G_glProgramUniformMatrix3x2fv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)>(ResolveAngleGl("glProgramUniformMatrix3x2fv"));
    if (fn) fn(program, location, count, transpose, value); else NoteGlUnsupported();
}
void GL_APIENTRY G_glProgramUniformMatrix2x4fv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)>(ResolveAngleGl("glProgramUniformMatrix2x4fv"));
    if (fn) fn(program, location, count, transpose, value); else NoteGlUnsupported();
}
void GL_APIENTRY G_glProgramUniformMatrix4x2fv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)>(ResolveAngleGl("glProgramUniformMatrix4x2fv"));
    if (fn) fn(program, location, count, transpose, value); else NoteGlUnsupported();
}
void GL_APIENTRY G_glProgramUniformMatrix3x4fv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)>(ResolveAngleGl("glProgramUniformMatrix3x4fv"));
    if (fn) fn(program, location, count, transpose, value); else NoteGlUnsupported();
}
void GL_APIENTRY G_glProgramUniformMatrix4x3fv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)>(ResolveAngleGl("glProgramUniformMatrix4x3fv"));
    if (fn) fn(program, location, count, transpose, value); else NoteGlUnsupported();
}
void GL_APIENTRY G_glValidateProgramPipeline(GLuint pipeline) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint pipeline)>(ResolveAngleGl("glValidateProgramPipeline"));
    if (fn) fn(pipeline); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetProgramPipelineInfoLog(GLuint pipeline, GLsizei bufSize, GLsizei *length, GLchar *infoLog) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint pipeline, GLsizei bufSize, GLsizei *length, GLchar *infoLog)>(ResolveAngleGl("glGetProgramPipelineInfoLog"));
    if (fn) fn(pipeline, bufSize, length, infoLog); else NoteGlUnsupported();
}
void GL_APIENTRY G_glBindImageTexture(GLuint unit, GLuint texture, GLint level, GLboolean layered, GLint layer, GLenum access, GLenum format) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint unit, GLuint texture, GLint level, GLboolean layered, GLint layer, GLenum access, GLenum format)>(ResolveAngleGl("glBindImageTexture"));
    if (fn) fn(unit, texture, level, layered, layer, access, format); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetBooleani_v(GLenum target, GLuint index, GLboolean *data) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLuint index, GLboolean *data)>(ResolveAngleGl("glGetBooleani_v"));
    if (fn) fn(target, index, data); else NoteGlUnsupported();
}
void GL_APIENTRY G_glMemoryBarrier(GLbitfield barriers) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLbitfield barriers)>(ResolveAngleGl("glMemoryBarrier"));
    if (fn) fn(barriers); else NoteGlUnsupported();
}
void GL_APIENTRY G_glMemoryBarrierByRegion(GLbitfield barriers) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLbitfield barriers)>(ResolveAngleGl("glMemoryBarrierByRegion"));
    if (fn) fn(barriers); else NoteGlUnsupported();
}
void GL_APIENTRY G_glTexStorage2DMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations)>(ResolveAngleGl("glTexStorage2DMultisample"));
    if (fn) fn(target, samples, internalformat, width, height, fixedsamplelocations); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetMultisamplefv(GLenum pname, GLuint index, GLfloat *val) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum pname, GLuint index, GLfloat *val)>(ResolveAngleGl("glGetMultisamplefv"));
    if (fn) fn(pname, index, val); else NoteGlUnsupported();
}
void GL_APIENTRY G_glSampleMaski(GLuint maskNumber, GLbitfield mask) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint maskNumber, GLbitfield mask)>(ResolveAngleGl("glSampleMaski"));
    if (fn) fn(maskNumber, mask); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetTexLevelParameteriv(GLenum target, GLint level, GLenum pname, GLint *params) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLint level, GLenum pname, GLint *params)>(ResolveAngleGl("glGetTexLevelParameteriv"));
    if (fn) fn(target, level, pname, params); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetTexLevelParameterfv(GLenum target, GLint level, GLenum pname, GLfloat *params) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLint level, GLenum pname, GLfloat *params)>(ResolveAngleGl("glGetTexLevelParameterfv"));
    if (fn) fn(target, level, pname, params); else NoteGlUnsupported();
}
void GL_APIENTRY G_glBindVertexBuffer(GLuint bindingindex, GLuint buffer, GLintptr offset, GLsizei stride) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint bindingindex, GLuint buffer, GLintptr offset, GLsizei stride)>(ResolveAngleGl("glBindVertexBuffer"));
    if (fn) fn(bindingindex, buffer, offset, stride); else NoteGlUnsupported();
}
void GL_APIENTRY G_glVertexAttribFormat(GLuint attribindex, GLint size, GLenum type, GLboolean normalized, GLuint relativeoffset) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint attribindex, GLint size, GLenum type, GLboolean normalized, GLuint relativeoffset)>(ResolveAngleGl("glVertexAttribFormat"));
    if (fn) fn(attribindex, size, type, normalized, relativeoffset); else NoteGlUnsupported();
}
void GL_APIENTRY G_glVertexAttribIFormat(GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset)>(ResolveAngleGl("glVertexAttribIFormat"));
    if (fn) fn(attribindex, size, type, relativeoffset); else NoteGlUnsupported();
}
void GL_APIENTRY G_glVertexAttribBinding(GLuint attribindex, GLuint bindingindex) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint attribindex, GLuint bindingindex)>(ResolveAngleGl("glVertexAttribBinding"));
    if (fn) fn(attribindex, bindingindex); else NoteGlUnsupported();
}
void GL_APIENTRY G_glVertexBindingDivisor(GLuint bindingindex, GLuint divisor) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint bindingindex, GLuint divisor)>(ResolveAngleGl("glVertexBindingDivisor"));
    if (fn) fn(bindingindex, divisor); else NoteGlUnsupported();
}
void GL_APIENTRY G_glBlendBarrier() {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)()>(ResolveAngleGl("glBlendBarrier"));
    if (fn) fn(); else NoteGlUnsupported();
}
void GL_APIENTRY G_glCopyImageSubData(GLuint srcName, GLenum srcTarget, GLint srcLevel, GLint srcX, GLint srcY, GLint srcZ, GLuint dstName, GLenum dstTarget, GLint dstLevel, GLint dstX, GLint dstY, GLint dstZ, GLsizei srcWidth, GLsizei srcHeight, GLsizei srcDepth) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint srcName, GLenum srcTarget, GLint srcLevel, GLint srcX, GLint srcY, GLint srcZ, GLuint dstName, GLenum dstTarget, GLint dstLevel, GLint dstX, GLint dstY, GLint dstZ, GLsizei srcWidth, GLsizei srcHeight, GLsizei srcDepth)>(ResolveAngleGl("glCopyImageSubData"));
    if (fn) fn(srcName, srcTarget, srcLevel, srcX, srcY, srcZ, dstName, dstTarget, dstLevel, dstX, dstY, dstZ, srcWidth, srcHeight, srcDepth); else NoteGlUnsupported();
}
void GL_APIENTRY G_glDebugMessageControl(GLenum source, GLenum type, GLenum severity, GLsizei count, const GLuint *ids, GLboolean enabled) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum source, GLenum type, GLenum severity, GLsizei count, const GLuint *ids, GLboolean enabled)>(ResolveAngleGl("glDebugMessageControl"));
    if (fn) fn(source, type, severity, count, ids, enabled); else NoteGlUnsupported();
}
void GL_APIENTRY G_glDebugMessageInsert(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *buf) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *buf)>(ResolveAngleGl("glDebugMessageInsert"));
    if (fn) fn(source, type, id, severity, length, buf); else NoteGlUnsupported();
}
GLuint GL_APIENTRY G_glGetDebugMessageLog(GLuint count, GLsizei bufSize, GLenum *sources, GLenum *types, GLuint *ids, GLenum *severities, GLsizei *lengths, GLchar *messageLog) {
    if (!GuestGlReady()) return static_cast<GLuint>(0);
    static auto fn = reinterpret_cast<GLuint (GL_APIENTRY *)(GLuint count, GLsizei bufSize, GLenum *sources, GLenum *types, GLuint *ids, GLenum *severities, GLsizei *lengths, GLchar *messageLog)>(ResolveAngleGl("glGetDebugMessageLog"));
    if (fn) return fn(count, bufSize, sources, types, ids, severities, lengths, messageLog);
    NoteGlUnsupported();
    return static_cast<GLuint>(0);
}
void GL_APIENTRY G_glPushDebugGroup(GLenum source, GLuint id, GLsizei length, const GLchar *message) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum source, GLuint id, GLsizei length, const GLchar *message)>(ResolveAngleGl("glPushDebugGroup"));
    if (fn) fn(source, id, length, message); else NoteGlUnsupported();
}
void GL_APIENTRY G_glPopDebugGroup() {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)()>(ResolveAngleGl("glPopDebugGroup"));
    if (fn) fn(); else NoteGlUnsupported();
}
void GL_APIENTRY G_glObjectLabel(GLenum identifier, GLuint name, GLsizei length, const GLchar *label) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum identifier, GLuint name, GLsizei length, const GLchar *label)>(ResolveAngleGl("glObjectLabel"));
    if (fn) fn(identifier, name, length, label); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetObjectLabel(GLenum identifier, GLuint name, GLsizei bufSize, GLsizei *length, GLchar *label) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum identifier, GLuint name, GLsizei bufSize, GLsizei *length, GLchar *label)>(ResolveAngleGl("glGetObjectLabel"));
    if (fn) fn(identifier, name, bufSize, length, label); else NoteGlUnsupported();
}
void GL_APIENTRY G_glObjectPtrLabel(const void *ptr, GLsizei length, const GLchar *label) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(const void *ptr, GLsizei length, const GLchar *label)>(ResolveAngleGl("glObjectPtrLabel"));
    if (fn) fn(ptr, length, label); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetObjectPtrLabel(const void *ptr, GLsizei bufSize, GLsizei *length, GLchar *label) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(const void *ptr, GLsizei bufSize, GLsizei *length, GLchar *label)>(ResolveAngleGl("glGetObjectPtrLabel"));
    if (fn) fn(ptr, bufSize, length, label); else NoteGlUnsupported();
}
void GL_APIENTRY G_glEnablei(GLenum target, GLuint index) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLuint index)>(ResolveAngleGl("glEnablei"));
    if (fn) fn(target, index); else NoteGlUnsupported();
}
void GL_APIENTRY G_glDisablei(GLenum target, GLuint index) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLuint index)>(ResolveAngleGl("glDisablei"));
    if (fn) fn(target, index); else NoteGlUnsupported();
}
void GL_APIENTRY G_glBlendEquationi(GLuint buf, GLenum mode) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint buf, GLenum mode)>(ResolveAngleGl("glBlendEquationi"));
    if (fn) fn(buf, mode); else NoteGlUnsupported();
}
void GL_APIENTRY G_glBlendEquationSeparatei(GLuint buf, GLenum modeRGB, GLenum modeAlpha) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint buf, GLenum modeRGB, GLenum modeAlpha)>(ResolveAngleGl("glBlendEquationSeparatei"));
    if (fn) fn(buf, modeRGB, modeAlpha); else NoteGlUnsupported();
}
void GL_APIENTRY G_glBlendFunci(GLuint buf, GLenum src, GLenum dst) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint buf, GLenum src, GLenum dst)>(ResolveAngleGl("glBlendFunci"));
    if (fn) fn(buf, src, dst); else NoteGlUnsupported();
}
void GL_APIENTRY G_glBlendFuncSeparatei(GLuint buf, GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint buf, GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha)>(ResolveAngleGl("glBlendFuncSeparatei"));
    if (fn) fn(buf, srcRGB, dstRGB, srcAlpha, dstAlpha); else NoteGlUnsupported();
}
void GL_APIENTRY G_glColorMaski(GLuint index, GLboolean r, GLboolean g, GLboolean b, GLboolean a) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint index, GLboolean r, GLboolean g, GLboolean b, GLboolean a)>(ResolveAngleGl("glColorMaski"));
    if (fn) fn(index, r, g, b, a); else NoteGlUnsupported();
}
GLboolean GL_APIENTRY G_glIsEnabledi(GLenum target, GLuint index) {
    if (!GuestGlReady()) return static_cast<GLboolean>(0);
    static auto fn = reinterpret_cast<GLboolean (GL_APIENTRY *)(GLenum target, GLuint index)>(ResolveAngleGl("glIsEnabledi"));
    if (fn) return fn(target, index);
    NoteGlUnsupported();
    return static_cast<GLboolean>(0);
}
void GL_APIENTRY G_glDrawElementsBaseVertex(GLenum mode, GLsizei count, GLenum type, const void *indices, GLint basevertex) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum mode, GLsizei count, GLenum type, const void *indices, GLint basevertex)>(ResolveAngleGl("glDrawElementsBaseVertex"));
    if (fn) fn(mode, count, type, indices, basevertex); else NoteGlUnsupported();
}
void GL_APIENTRY G_glDrawRangeElementsBaseVertex(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const void *indices, GLint basevertex) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const void *indices, GLint basevertex)>(ResolveAngleGl("glDrawRangeElementsBaseVertex"));
    if (fn) fn(mode, start, end, count, type, indices, basevertex); else NoteGlUnsupported();
}
void GL_APIENTRY G_glDrawElementsInstancedBaseVertex(GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount, GLint basevertex) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount, GLint basevertex)>(ResolveAngleGl("glDrawElementsInstancedBaseVertex"));
    if (fn) fn(mode, count, type, indices, instancecount, basevertex); else NoteGlUnsupported();
}
void GL_APIENTRY G_glFramebufferTexture(GLenum target, GLenum attachment, GLuint texture, GLint level) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLenum attachment, GLuint texture, GLint level)>(ResolveAngleGl("glFramebufferTexture"));
    if (fn) fn(target, attachment, texture, level); else NoteGlUnsupported();
}
void GL_APIENTRY G_glPrimitiveBoundingBox(GLfloat minX, GLfloat minY, GLfloat minZ, GLfloat minW, GLfloat maxX, GLfloat maxY, GLfloat maxZ, GLfloat maxW) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLfloat minX, GLfloat minY, GLfloat minZ, GLfloat minW, GLfloat maxX, GLfloat maxY, GLfloat maxZ, GLfloat maxW)>(ResolveAngleGl("glPrimitiveBoundingBox"));
    if (fn) fn(minX, minY, minZ, minW, maxX, maxY, maxZ, maxW); else NoteGlUnsupported();
}
GLenum GL_APIENTRY G_glGetGraphicsResetStatus() {
    if (!GuestGlReady()) return static_cast<GLenum>(0);
    static auto fn = reinterpret_cast<GLenum (GL_APIENTRY *)()>(ResolveAngleGl("glGetGraphicsResetStatus"));
    if (fn) return fn();
    NoteGlUnsupported();
    return static_cast<GLenum>(0);
}
void GL_APIENTRY G_glReadnPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLsizei bufSize, void *data) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLsizei bufSize, void *data)>(ResolveAngleGl("glReadnPixels"));
    if (fn) fn(x, y, width, height, format, type, bufSize, data); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetnUniformfv(GLuint program, GLint location, GLsizei bufSize, GLfloat *params) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint program, GLint location, GLsizei bufSize, GLfloat *params)>(ResolveAngleGl("glGetnUniformfv"));
    if (fn) fn(program, location, bufSize, params); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetnUniformiv(GLuint program, GLint location, GLsizei bufSize, GLint *params) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint program, GLint location, GLsizei bufSize, GLint *params)>(ResolveAngleGl("glGetnUniformiv"));
    if (fn) fn(program, location, bufSize, params); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetnUniformuiv(GLuint program, GLint location, GLsizei bufSize, GLuint *params) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint program, GLint location, GLsizei bufSize, GLuint *params)>(ResolveAngleGl("glGetnUniformuiv"));
    if (fn) fn(program, location, bufSize, params); else NoteGlUnsupported();
}
void GL_APIENTRY G_glMinSampleShading(GLfloat value) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLfloat value)>(ResolveAngleGl("glMinSampleShading"));
    if (fn) fn(value); else NoteGlUnsupported();
}
void GL_APIENTRY G_glPatchParameteri(GLenum pname, GLint value) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum pname, GLint value)>(ResolveAngleGl("glPatchParameteri"));
    if (fn) fn(pname, value); else NoteGlUnsupported();
}
void GL_APIENTRY G_glTexParameterIiv(GLenum target, GLenum pname, const GLint *params) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLenum pname, const GLint *params)>(ResolveAngleGl("glTexParameterIiv"));
    if (fn) fn(target, pname, params); else NoteGlUnsupported();
}
void GL_APIENTRY G_glTexParameterIuiv(GLenum target, GLenum pname, const GLuint *params) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLenum pname, const GLuint *params)>(ResolveAngleGl("glTexParameterIuiv"));
    if (fn) fn(target, pname, params); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetTexParameterIiv(GLenum target, GLenum pname, GLint *params) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLenum pname, GLint *params)>(ResolveAngleGl("glGetTexParameterIiv"));
    if (fn) fn(target, pname, params); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetTexParameterIuiv(GLenum target, GLenum pname, GLuint *params) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLenum pname, GLuint *params)>(ResolveAngleGl("glGetTexParameterIuiv"));
    if (fn) fn(target, pname, params); else NoteGlUnsupported();
}
void GL_APIENTRY G_glSamplerParameterIiv(GLuint sampler, GLenum pname, const GLint *param) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint sampler, GLenum pname, const GLint *param)>(ResolveAngleGl("glSamplerParameterIiv"));
    if (fn) fn(sampler, pname, param); else NoteGlUnsupported();
}
void GL_APIENTRY G_glSamplerParameterIuiv(GLuint sampler, GLenum pname, const GLuint *param) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint sampler, GLenum pname, const GLuint *param)>(ResolveAngleGl("glSamplerParameterIuiv"));
    if (fn) fn(sampler, pname, param); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetSamplerParameterIiv(GLuint sampler, GLenum pname, GLint *params) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint sampler, GLenum pname, GLint *params)>(ResolveAngleGl("glGetSamplerParameterIiv"));
    if (fn) fn(sampler, pname, params); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetSamplerParameterIuiv(GLuint sampler, GLenum pname, GLuint *params) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint sampler, GLenum pname, GLuint *params)>(ResolveAngleGl("glGetSamplerParameterIuiv"));
    if (fn) fn(sampler, pname, params); else NoteGlUnsupported();
}
void GL_APIENTRY G_glTexBuffer(GLenum target, GLenum internalformat, GLuint buffer) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLenum internalformat, GLuint buffer)>(ResolveAngleGl("glTexBuffer"));
    if (fn) fn(target, internalformat, buffer); else NoteGlUnsupported();
}
void GL_APIENTRY G_glTexBufferRange(GLenum target, GLenum internalformat, GLuint buffer, GLintptr offset, GLsizeiptr size) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLenum internalformat, GLuint buffer, GLintptr offset, GLsizeiptr size)>(ResolveAngleGl("glTexBufferRange"));
    if (fn) fn(target, internalformat, buffer, offset, size); else NoteGlUnsupported();
}
void GL_APIENTRY G_glTexStorage3DMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLboolean fixedsamplelocations) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLboolean fixedsamplelocations)>(ResolveAngleGl("glTexStorage3DMultisample"));
    if (fn) fn(target, samples, internalformat, width, height, depth, fixedsamplelocations); else NoteGlUnsupported();
}
void GL_APIENTRY G_glBlendBarrierKHR() {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)()>(ResolveAngleGl("glBlendBarrierKHR"));
    if (fn) fn(); else NoteGlUnsupported();
}
void GL_APIENTRY G_glPushDebugGroupKHR(GLenum source, GLuint id, GLsizei length, const GLchar *message) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum source, GLuint id, GLsizei length, const GLchar *message)>(ResolveAngleGl("glPushDebugGroupKHR"));
    if (fn) fn(source, id, length, message); else NoteGlUnsupported();
}
void GL_APIENTRY G_glPopDebugGroupKHR() {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)()>(ResolveAngleGl("glPopDebugGroupKHR"));
    if (fn) fn(); else NoteGlUnsupported();
}
void GL_APIENTRY G_glObjectLabelKHR(GLenum identifier, GLuint name, GLsizei length, const GLchar *label) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum identifier, GLuint name, GLsizei length, const GLchar *label)>(ResolveAngleGl("glObjectLabelKHR"));
    if (fn) fn(identifier, name, length, label); else NoteGlUnsupported();
}
void GL_APIENTRY G_glObjectPtrLabelKHR(const void *ptr, GLsizei length, const GLchar *label) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(const void *ptr, GLsizei length, const GLchar *label)>(ResolveAngleGl("glObjectPtrLabelKHR"));
    if (fn) fn(ptr, length, label); else NoteGlUnsupported();
}
GLenum GL_APIENTRY G_glGetGraphicsResetStatusKHR() {
    if (!GuestGlReady()) return static_cast<GLenum>(0);
    static auto fn = reinterpret_cast<GLenum (GL_APIENTRY *)()>(ResolveAngleGl("glGetGraphicsResetStatusKHR"));
    if (fn) return fn();
    NoteGlUnsupported();
    return static_cast<GLenum>(0);
}
void GL_APIENTRY G_glReadnPixelsKHR(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLsizei bufSize, void *data) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLsizei bufSize, void *data)>(ResolveAngleGl("glReadnPixelsKHR"));
    if (fn) fn(x, y, width, height, format, type, bufSize, data); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetnUniformfvKHR(GLuint program, GLint location, GLsizei bufSize, GLfloat *params) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint program, GLint location, GLsizei bufSize, GLfloat *params)>(ResolveAngleGl("glGetnUniformfvKHR"));
    if (fn) fn(program, location, bufSize, params); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetnUniformivKHR(GLuint program, GLint location, GLsizei bufSize, GLint *params) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint program, GLint location, GLsizei bufSize, GLint *params)>(ResolveAngleGl("glGetnUniformivKHR"));
    if (fn) fn(program, location, bufSize, params); else NoteGlUnsupported();
}
void GL_APIENTRY G_glCopyImageSubDataOES(GLuint srcName, GLenum srcTarget, GLint srcLevel, GLint srcX, GLint srcY, GLint srcZ, GLuint dstName, GLenum dstTarget, GLint dstLevel, GLint dstX, GLint dstY, GLint dstZ, GLsizei srcWidth, GLsizei srcHeight, GLsizei srcDepth) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint srcName, GLenum srcTarget, GLint srcLevel, GLint srcX, GLint srcY, GLint srcZ, GLuint dstName, GLenum dstTarget, GLint dstLevel, GLint dstX, GLint dstY, GLint dstZ, GLsizei srcWidth, GLsizei srcHeight, GLsizei srcDepth)>(ResolveAngleGl("glCopyImageSubDataOES"));
    if (fn) fn(srcName, srcTarget, srcLevel, srcX, srcY, srcZ, dstName, dstTarget, dstLevel, dstX, dstY, dstZ, srcWidth, srcHeight, srcDepth); else NoteGlUnsupported();
}
void * GL_APIENTRY G_glMapBufferOES(GLenum target, GLenum access) {
    if (!GuestGlReady()) return static_cast<void *>(0);
    static auto fn = reinterpret_cast<void * (GL_APIENTRY *)(GLenum target, GLenum access)>(ResolveAngleGl("glMapBufferOES"));
    if (fn) return fn(target, access);
    NoteGlUnsupported();
    return static_cast<void *>(0);
}
GLboolean GL_APIENTRY G_glUnmapBufferOES(GLenum target) {
    if (!GuestGlReady()) return static_cast<GLboolean>(0);
    static auto fn = reinterpret_cast<GLboolean (GL_APIENTRY *)(GLenum target)>(ResolveAngleGl("glUnmapBufferOES"));
    if (fn) return fn(target);
    NoteGlUnsupported();
    return static_cast<GLboolean>(0);
}
void GL_APIENTRY G_glGetBufferPointervOES(GLenum target, GLenum pname, void **params) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLenum pname, void **params)>(ResolveAngleGl("glGetBufferPointervOES"));
    if (fn) fn(target, pname, params); else NoteGlUnsupported();
}
void GL_APIENTRY G_glBindVertexArrayOES(GLuint array) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint array)>(ResolveAngleGl("glBindVertexArrayOES"));
    if (fn) fn(array); else NoteGlUnsupported();
}
void GL_APIENTRY G_glDeleteVertexArraysOES(GLsizei n, const GLuint *arrays) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLsizei n, const GLuint *arrays)>(ResolveAngleGl("glDeleteVertexArraysOES"));
    if (fn) fn(n, arrays); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGenVertexArraysOES(GLsizei n, GLuint *arrays) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLsizei n, GLuint *arrays)>(ResolveAngleGl("glGenVertexArraysOES"));
    if (fn) fn(n, arrays); else NoteGlUnsupported();
}
GLboolean GL_APIENTRY G_glIsVertexArrayOES(GLuint array) {
    if (!GuestGlReady()) return static_cast<GLboolean>(0);
    static auto fn = reinterpret_cast<GLboolean (GL_APIENTRY *)(GLuint array)>(ResolveAngleGl("glIsVertexArrayOES"));
    if (fn) return fn(array);
    NoteGlUnsupported();
    return static_cast<GLboolean>(0);
}
void GL_APIENTRY G_glBlitFramebufferANGLE(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter)>(ResolveAngleGl("glBlitFramebufferANGLE"));
    if (fn) fn(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter); else NoteGlUnsupported();
}
void GL_APIENTRY G_glRenderbufferStorageMultisampleANGLE(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height)>(ResolveAngleGl("glRenderbufferStorageMultisampleANGLE"));
    if (fn) fn(target, samples, internalformat, width, height); else NoteGlUnsupported();
}
void GL_APIENTRY G_glDrawArraysInstancedANGLE(GLenum mode, GLint first, GLsizei count, GLsizei primcount) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum mode, GLint first, GLsizei count, GLsizei primcount)>(ResolveAngleGl("glDrawArraysInstancedANGLE"));
    if (fn) fn(mode, first, count, primcount); else NoteGlUnsupported();
}
void GL_APIENTRY G_glDrawElementsInstancedANGLE(GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei primcount) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei primcount)>(ResolveAngleGl("glDrawElementsInstancedANGLE"));
    if (fn) fn(mode, count, type, indices, primcount); else NoteGlUnsupported();
}
void GL_APIENTRY G_glVertexAttribDivisorANGLE(GLuint index, GLuint divisor) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint index, GLuint divisor)>(ResolveAngleGl("glVertexAttribDivisorANGLE"));
    if (fn) fn(index, divisor); else NoteGlUnsupported();
}
void GL_APIENTRY G_glCopyImageSubDataEXT(GLuint srcName, GLenum srcTarget, GLint srcLevel, GLint srcX, GLint srcY, GLint srcZ, GLuint dstName, GLenum dstTarget, GLint dstLevel, GLint dstX, GLint dstY, GLint dstZ, GLsizei srcWidth, GLsizei srcHeight, GLsizei srcDepth) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint srcName, GLenum srcTarget, GLint srcLevel, GLint srcX, GLint srcY, GLint srcZ, GLuint dstName, GLenum dstTarget, GLint dstLevel, GLint dstX, GLint dstY, GLint dstZ, GLsizei srcWidth, GLsizei srcHeight, GLsizei srcDepth)>(ResolveAngleGl("glCopyImageSubDataEXT"));
    if (fn) fn(srcName, srcTarget, srcLevel, srcX, srcY, srcZ, dstName, dstTarget, dstLevel, dstX, dstY, dstZ, srcWidth, srcHeight, srcDepth); else NoteGlUnsupported();
}
void GL_APIENTRY G_glInsertEventMarkerEXT(GLsizei length, const GLchar *marker) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLsizei length, const GLchar *marker)>(ResolveAngleGl("glInsertEventMarkerEXT"));
    if (fn) fn(length, marker); else NoteGlUnsupported();
}
void GL_APIENTRY G_glPushGroupMarkerEXT(GLsizei length, const GLchar *marker) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLsizei length, const GLchar *marker)>(ResolveAngleGl("glPushGroupMarkerEXT"));
    if (fn) fn(length, marker); else NoteGlUnsupported();
}
void GL_APIENTRY G_glPopGroupMarkerEXT() {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)()>(ResolveAngleGl("glPopGroupMarkerEXT"));
    if (fn) fn(); else NoteGlUnsupported();
}
void GL_APIENTRY G_glDiscardFramebufferEXT(GLenum target, GLsizei numAttachments, const GLenum *attachments) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLsizei numAttachments, const GLenum *attachments)>(ResolveAngleGl("glDiscardFramebufferEXT"));
    if (fn) fn(target, numAttachments, attachments); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGenQueriesEXT(GLsizei n, GLuint *ids) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLsizei n, GLuint *ids)>(ResolveAngleGl("glGenQueriesEXT"));
    if (fn) fn(n, ids); else NoteGlUnsupported();
}
void GL_APIENTRY G_glDeleteQueriesEXT(GLsizei n, const GLuint *ids) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLsizei n, const GLuint *ids)>(ResolveAngleGl("glDeleteQueriesEXT"));
    if (fn) fn(n, ids); else NoteGlUnsupported();
}
GLboolean GL_APIENTRY G_glIsQueryEXT(GLuint id) {
    if (!GuestGlReady()) return static_cast<GLboolean>(0);
    static auto fn = reinterpret_cast<GLboolean (GL_APIENTRY *)(GLuint id)>(ResolveAngleGl("glIsQueryEXT"));
    if (fn) return fn(id);
    NoteGlUnsupported();
    return static_cast<GLboolean>(0);
}
void GL_APIENTRY G_glBeginQueryEXT(GLenum target, GLuint id) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLuint id)>(ResolveAngleGl("glBeginQueryEXT"));
    if (fn) fn(target, id); else NoteGlUnsupported();
}
void GL_APIENTRY G_glEndQueryEXT(GLenum target) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target)>(ResolveAngleGl("glEndQueryEXT"));
    if (fn) fn(target); else NoteGlUnsupported();
}
void GL_APIENTRY G_glQueryCounterEXT(GLuint id, GLenum target) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint id, GLenum target)>(ResolveAngleGl("glQueryCounterEXT"));
    if (fn) fn(id, target); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetQueryivEXT(GLenum target, GLenum pname, GLint *params) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLenum pname, GLint *params)>(ResolveAngleGl("glGetQueryivEXT"));
    if (fn) fn(target, pname, params); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetQueryObjectivEXT(GLuint id, GLenum pname, GLint *params) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint id, GLenum pname, GLint *params)>(ResolveAngleGl("glGetQueryObjectivEXT"));
    if (fn) fn(id, pname, params); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetQueryObjectuivEXT(GLuint id, GLenum pname, GLuint *params) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint id, GLenum pname, GLuint *params)>(ResolveAngleGl("glGetQueryObjectuivEXT"));
    if (fn) fn(id, pname, params); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetQueryObjecti64vEXT(GLuint id, GLenum pname, GLint64 *params) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint id, GLenum pname, GLint64 *params)>(ResolveAngleGl("glGetQueryObjecti64vEXT"));
    if (fn) fn(id, pname, params); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetQueryObjectui64vEXT(GLuint id, GLenum pname, GLuint64 *params) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint id, GLenum pname, GLuint64 *params)>(ResolveAngleGl("glGetQueryObjectui64vEXT"));
    if (fn) fn(id, pname, params); else NoteGlUnsupported();
}
void GL_APIENTRY G_glDrawBuffersEXT(GLsizei n, const GLenum *bufs) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLsizei n, const GLenum *bufs)>(ResolveAngleGl("glDrawBuffersEXT"));
    if (fn) fn(n, bufs); else NoteGlUnsupported();
}
void GL_APIENTRY G_glEnableiEXT(GLenum target, GLuint index) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLuint index)>(ResolveAngleGl("glEnableiEXT"));
    if (fn) fn(target, index); else NoteGlUnsupported();
}
void GL_APIENTRY G_glDisableiEXT(GLenum target, GLuint index) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLuint index)>(ResolveAngleGl("glDisableiEXT"));
    if (fn) fn(target, index); else NoteGlUnsupported();
}
void GL_APIENTRY G_glBlendEquationiEXT(GLuint buf, GLenum mode) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint buf, GLenum mode)>(ResolveAngleGl("glBlendEquationiEXT"));
    if (fn) fn(buf, mode); else NoteGlUnsupported();
}
void GL_APIENTRY G_glBlendEquationSeparateiEXT(GLuint buf, GLenum modeRGB, GLenum modeAlpha) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint buf, GLenum modeRGB, GLenum modeAlpha)>(ResolveAngleGl("glBlendEquationSeparateiEXT"));
    if (fn) fn(buf, modeRGB, modeAlpha); else NoteGlUnsupported();
}
void GL_APIENTRY G_glBlendFunciEXT(GLuint buf, GLenum src, GLenum dst) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint buf, GLenum src, GLenum dst)>(ResolveAngleGl("glBlendFunciEXT"));
    if (fn) fn(buf, src, dst); else NoteGlUnsupported();
}
void GL_APIENTRY G_glBlendFuncSeparateiEXT(GLuint buf, GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint buf, GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha)>(ResolveAngleGl("glBlendFuncSeparateiEXT"));
    if (fn) fn(buf, srcRGB, dstRGB, srcAlpha, dstAlpha); else NoteGlUnsupported();
}
void GL_APIENTRY G_glColorMaskiEXT(GLuint index, GLboolean r, GLboolean g, GLboolean b, GLboolean a) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint index, GLboolean r, GLboolean g, GLboolean b, GLboolean a)>(ResolveAngleGl("glColorMaskiEXT"));
    if (fn) fn(index, r, g, b, a); else NoteGlUnsupported();
}
GLboolean GL_APIENTRY G_glIsEnablediEXT(GLenum target, GLuint index) {
    if (!GuestGlReady()) return static_cast<GLboolean>(0);
    static auto fn = reinterpret_cast<GLboolean (GL_APIENTRY *)(GLenum target, GLuint index)>(ResolveAngleGl("glIsEnablediEXT"));
    if (fn) return fn(target, index);
    NoteGlUnsupported();
    return static_cast<GLboolean>(0);
}
void GL_APIENTRY G_glDrawArraysInstancedEXT(GLenum mode, GLint start, GLsizei count, GLsizei primcount) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum mode, GLint start, GLsizei count, GLsizei primcount)>(ResolveAngleGl("glDrawArraysInstancedEXT"));
    if (fn) fn(mode, start, count, primcount); else NoteGlUnsupported();
}
void GL_APIENTRY G_glDrawElementsInstancedEXT(GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei primcount) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei primcount)>(ResolveAngleGl("glDrawElementsInstancedEXT"));
    if (fn) fn(mode, count, type, indices, primcount); else NoteGlUnsupported();
}
void GL_APIENTRY G_glVertexAttribDivisorEXT(GLuint index, GLuint divisor) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint index, GLuint divisor)>(ResolveAngleGl("glVertexAttribDivisorEXT"));
    if (fn) fn(index, divisor); else NoteGlUnsupported();
}
void * GL_APIENTRY G_glMapBufferRangeEXT(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access) {
    if (!GuestGlReady()) return static_cast<void *>(0);
    static auto fn = reinterpret_cast<void * (GL_APIENTRY *)(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access)>(ResolveAngleGl("glMapBufferRangeEXT"));
    if (fn) return fn(target, offset, length, access);
    NoteGlUnsupported();
    return static_cast<void *>(0);
}
void GL_APIENTRY G_glFlushMappedBufferRangeEXT(GLenum target, GLintptr offset, GLsizeiptr length) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLintptr offset, GLsizeiptr length)>(ResolveAngleGl("glFlushMappedBufferRangeEXT"));
    if (fn) fn(target, offset, length); else NoteGlUnsupported();
}
void GL_APIENTRY G_glRenderbufferStorageMultisampleEXT(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height)>(ResolveAngleGl("glRenderbufferStorageMultisampleEXT"));
    if (fn) fn(target, samples, internalformat, width, height); else NoteGlUnsupported();
}
void GL_APIENTRY G_glFramebufferTexture2DMultisampleEXT(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLsizei samples) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLsizei samples)>(ResolveAngleGl("glFramebufferTexture2DMultisampleEXT"));
    if (fn) fn(target, attachment, textarget, texture, level, samples); else NoteGlUnsupported();
}
GLenum GL_APIENTRY G_glGetGraphicsResetStatusEXT() {
    if (!GuestGlReady()) return static_cast<GLenum>(0);
    static auto fn = reinterpret_cast<GLenum (GL_APIENTRY *)()>(ResolveAngleGl("glGetGraphicsResetStatusEXT"));
    if (fn) return fn();
    NoteGlUnsupported();
    return static_cast<GLenum>(0);
}
void GL_APIENTRY G_glReadnPixelsEXT(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLsizei bufSize, void *data) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLsizei bufSize, void *data)>(ResolveAngleGl("glReadnPixelsEXT"));
    if (fn) fn(x, y, width, height, format, type, bufSize, data); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetnUniformfvEXT(GLuint program, GLint location, GLsizei bufSize, GLfloat *params) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint program, GLint location, GLsizei bufSize, GLfloat *params)>(ResolveAngleGl("glGetnUniformfvEXT"));
    if (fn) fn(program, location, bufSize, params); else NoteGlUnsupported();
}
void GL_APIENTRY G_glGetnUniformivEXT(GLuint program, GLint location, GLsizei bufSize, GLint *params) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLuint program, GLint location, GLsizei bufSize, GLint *params)>(ResolveAngleGl("glGetnUniformivEXT"));
    if (fn) fn(program, location, bufSize, params); else NoteGlUnsupported();
}
void GL_APIENTRY G_glTexStorage1DEXT(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width)>(ResolveAngleGl("glTexStorage1DEXT"));
    if (fn) fn(target, levels, internalformat, width); else NoteGlUnsupported();
}
void GL_APIENTRY G_glTexStorage2DEXT(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height)>(ResolveAngleGl("glTexStorage2DEXT"));
    if (fn) fn(target, levels, internalformat, width, height); else NoteGlUnsupported();
}
void GL_APIENTRY G_glTexStorage3DEXT(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth) {
    if (!GuestGlReady()) return;
    static auto fn = reinterpret_cast<void (GL_APIENTRY *)(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth)>(ResolveAngleGl("glTexStorage3DEXT"));
    if (fn) fn(target, levels, internalformat, width, height, depth); else NoteGlUnsupported();
}

const guest::HostSymbol kAutoGles1Symbols[] = {
    {"glAlphaFunc", reinterpret_cast<void*>(&G_glAlphaFunc)},
    {"glClearColor", reinterpret_cast<void*>(&G_glClearColor)},
    {"glClearDepthf", reinterpret_cast<void*>(&G_glClearDepthf)},
    {"glClipPlanef", reinterpret_cast<void*>(&G_glClipPlanef)},
    {"glColor4f", reinterpret_cast<void*>(&G_glColor4f)},
    {"glDepthRangef", reinterpret_cast<void*>(&G_glDepthRangef)},
    {"glFogf", reinterpret_cast<void*>(&G_glFogf)},
    {"glFogfv", reinterpret_cast<void*>(&G_glFogfv)},
    {"glFrustumf", reinterpret_cast<void*>(&G_glFrustumf)},
    {"glGetClipPlanef", reinterpret_cast<void*>(&G_glGetClipPlanef)},
    {"glGetFloatv", reinterpret_cast<void*>(&G_glGetFloatv)},
    {"glGetLightfv", reinterpret_cast<void*>(&G_glGetLightfv)},
    {"glGetMaterialfv", reinterpret_cast<void*>(&G_glGetMaterialfv)},
    {"glGetTexEnvfv", reinterpret_cast<void*>(&G_glGetTexEnvfv)},
    {"glGetTexParameterfv", reinterpret_cast<void*>(&G_glGetTexParameterfv)},
    {"glLightModelf", reinterpret_cast<void*>(&G_glLightModelf)},
    {"glLightModelfv", reinterpret_cast<void*>(&G_glLightModelfv)},
    {"glLightf", reinterpret_cast<void*>(&G_glLightf)},
    {"glLightfv", reinterpret_cast<void*>(&G_glLightfv)},
    {"glLineWidth", reinterpret_cast<void*>(&G_glLineWidth)},
    {"glLoadMatrixf", reinterpret_cast<void*>(&G_glLoadMatrixf)},
    {"glMaterialf", reinterpret_cast<void*>(&G_glMaterialf)},
    {"glMaterialfv", reinterpret_cast<void*>(&G_glMaterialfv)},
    {"glMultMatrixf", reinterpret_cast<void*>(&G_glMultMatrixf)},
    {"glMultiTexCoord4f", reinterpret_cast<void*>(&G_glMultiTexCoord4f)},
    {"glNormal3f", reinterpret_cast<void*>(&G_glNormal3f)},
    {"glOrthof", reinterpret_cast<void*>(&G_glOrthof)},
    {"glPointParameterf", reinterpret_cast<void*>(&G_glPointParameterf)},
    {"glPointParameterfv", reinterpret_cast<void*>(&G_glPointParameterfv)},
    {"glPointSize", reinterpret_cast<void*>(&G_glPointSize)},
    {"glPolygonOffset", reinterpret_cast<void*>(&G_glPolygonOffset)},
    {"glRotatef", reinterpret_cast<void*>(&G_glRotatef)},
    {"glScalef", reinterpret_cast<void*>(&G_glScalef)},
    {"glTexEnvf", reinterpret_cast<void*>(&G_glTexEnvf)},
    {"glTexEnvfv", reinterpret_cast<void*>(&G_glTexEnvfv)},
    {"glTexParameterf", reinterpret_cast<void*>(&G_glTexParameterf)},
    {"glTexParameterfv", reinterpret_cast<void*>(&G_glTexParameterfv)},
    {"glTranslatef", reinterpret_cast<void*>(&G_glTranslatef)},
    {"glActiveTexture", reinterpret_cast<void*>(&G_glActiveTexture)},
    {"glAlphaFuncx", reinterpret_cast<void*>(&G_glAlphaFuncx)},
    {"glBindBuffer", reinterpret_cast<void*>(&G_glBindBuffer)},
    {"glBindTexture", reinterpret_cast<void*>(&G_glBindTexture)},
    {"glBlendFunc", reinterpret_cast<void*>(&G_glBlendFunc)},
    {"glBufferData", reinterpret_cast<void*>(&G_glBufferData)},
    {"glBufferSubData", reinterpret_cast<void*>(&G_glBufferSubData)},
    {"glClear", reinterpret_cast<void*>(&G_glClear)},
    {"glClearColorx", reinterpret_cast<void*>(&G_glClearColorx)},
    {"glClearDepthx", reinterpret_cast<void*>(&G_glClearDepthx)},
    {"glClearStencil", reinterpret_cast<void*>(&G_glClearStencil)},
    {"glClientActiveTexture", reinterpret_cast<void*>(&G_glClientActiveTexture)},
    {"glClipPlanex", reinterpret_cast<void*>(&G_glClipPlanex)},
    {"glColor4ub", reinterpret_cast<void*>(&G_glColor4ub)},
    {"glColor4x", reinterpret_cast<void*>(&G_glColor4x)},
    {"glColorMask", reinterpret_cast<void*>(&G_glColorMask)},
    {"glColorPointer", reinterpret_cast<void*>(&G_glColorPointer)},
    {"glCompressedTexImage2D", reinterpret_cast<void*>(&G_glCompressedTexImage2D)},
    {"glCompressedTexSubImage2D", reinterpret_cast<void*>(&G_glCompressedTexSubImage2D)},
    {"glCopyTexImage2D", reinterpret_cast<void*>(&G_glCopyTexImage2D)},
    {"glCopyTexSubImage2D", reinterpret_cast<void*>(&G_glCopyTexSubImage2D)},
    {"glCullFace", reinterpret_cast<void*>(&G_glCullFace)},
    {"glDeleteBuffers", reinterpret_cast<void*>(&G_glDeleteBuffers)},
    {"glDeleteTextures", reinterpret_cast<void*>(&G_glDeleteTextures)},
    {"glDepthFunc", reinterpret_cast<void*>(&G_glDepthFunc)},
    {"glDepthMask", reinterpret_cast<void*>(&G_glDepthMask)},
    {"glDepthRangex", reinterpret_cast<void*>(&G_glDepthRangex)},
    {"glDisable", reinterpret_cast<void*>(&G_glDisable)},
    {"glDisableClientState", reinterpret_cast<void*>(&G_glDisableClientState)},
    {"glDrawArrays", reinterpret_cast<void*>(&G_glDrawArrays)},
    {"glDrawElements", reinterpret_cast<void*>(&G_glDrawElements)},
    {"glEnable", reinterpret_cast<void*>(&G_glEnable)},
    {"glEnableClientState", reinterpret_cast<void*>(&G_glEnableClientState)},
    {"glFinish", reinterpret_cast<void*>(&G_glFinish)},
    {"glFlush", reinterpret_cast<void*>(&G_glFlush)},
    {"glFogx", reinterpret_cast<void*>(&G_glFogx)},
    {"glFogxv", reinterpret_cast<void*>(&G_glFogxv)},
    {"glFrontFace", reinterpret_cast<void*>(&G_glFrontFace)},
    {"glFrustumx", reinterpret_cast<void*>(&G_glFrustumx)},
    {"glGetBooleanv", reinterpret_cast<void*>(&G_glGetBooleanv)},
    {"glGetBufferParameteriv", reinterpret_cast<void*>(&G_glGetBufferParameteriv)},
    {"glGetClipPlanex", reinterpret_cast<void*>(&G_glGetClipPlanex)},
    {"glGenBuffers", reinterpret_cast<void*>(&G_glGenBuffers)},
    {"glGenTextures", reinterpret_cast<void*>(&G_glGenTextures)},
    {"glGetFixedv", reinterpret_cast<void*>(&G_glGetFixedv)},
    {"glGetIntegerv", reinterpret_cast<void*>(&G_glGetIntegerv)},
    {"glGetLightxv", reinterpret_cast<void*>(&G_glGetLightxv)},
    {"glGetMaterialxv", reinterpret_cast<void*>(&G_glGetMaterialxv)},
    {"glGetPointerv", reinterpret_cast<void*>(&G_glGetPointerv)},
    {"glGetTexEnviv", reinterpret_cast<void*>(&G_glGetTexEnviv)},
    {"glGetTexEnvxv", reinterpret_cast<void*>(&G_glGetTexEnvxv)},
    {"glGetTexParameteriv", reinterpret_cast<void*>(&G_glGetTexParameteriv)},
    {"glGetTexParameterxv", reinterpret_cast<void*>(&G_glGetTexParameterxv)},
    {"glHint", reinterpret_cast<void*>(&G_glHint)},
    {"glIsBuffer", reinterpret_cast<void*>(&G_glIsBuffer)},
    {"glIsEnabled", reinterpret_cast<void*>(&G_glIsEnabled)},
    {"glIsTexture", reinterpret_cast<void*>(&G_glIsTexture)},
    {"glLightModelx", reinterpret_cast<void*>(&G_glLightModelx)},
    {"glLightModelxv", reinterpret_cast<void*>(&G_glLightModelxv)},
    {"glLightx", reinterpret_cast<void*>(&G_glLightx)},
    {"glLightxv", reinterpret_cast<void*>(&G_glLightxv)},
    {"glLineWidthx", reinterpret_cast<void*>(&G_glLineWidthx)},
    {"glLoadIdentity", reinterpret_cast<void*>(&G_glLoadIdentity)},
    {"glLoadMatrixx", reinterpret_cast<void*>(&G_glLoadMatrixx)},
    {"glLogicOp", reinterpret_cast<void*>(&G_glLogicOp)},
    {"glMaterialx", reinterpret_cast<void*>(&G_glMaterialx)},
    {"glMaterialxv", reinterpret_cast<void*>(&G_glMaterialxv)},
    {"glMatrixMode", reinterpret_cast<void*>(&G_glMatrixMode)},
    {"glMultMatrixx", reinterpret_cast<void*>(&G_glMultMatrixx)},
    {"glMultiTexCoord4x", reinterpret_cast<void*>(&G_glMultiTexCoord4x)},
    {"glNormal3x", reinterpret_cast<void*>(&G_glNormal3x)},
    {"glNormalPointer", reinterpret_cast<void*>(&G_glNormalPointer)},
    {"glOrthox", reinterpret_cast<void*>(&G_glOrthox)},
    {"glPixelStorei", reinterpret_cast<void*>(&G_glPixelStorei)},
    {"glPointParameterx", reinterpret_cast<void*>(&G_glPointParameterx)},
    {"glPointParameterxv", reinterpret_cast<void*>(&G_glPointParameterxv)},
    {"glPointSizex", reinterpret_cast<void*>(&G_glPointSizex)},
    {"glPolygonOffsetx", reinterpret_cast<void*>(&G_glPolygonOffsetx)},
    {"glPopMatrix", reinterpret_cast<void*>(&G_glPopMatrix)},
    {"glPushMatrix", reinterpret_cast<void*>(&G_glPushMatrix)},
    {"glReadPixels", reinterpret_cast<void*>(&G_glReadPixels)},
    {"glRotatex", reinterpret_cast<void*>(&G_glRotatex)},
    {"glSampleCoverage", reinterpret_cast<void*>(&G_glSampleCoverage)},
    {"glSampleCoveragex", reinterpret_cast<void*>(&G_glSampleCoveragex)},
    {"glScalex", reinterpret_cast<void*>(&G_glScalex)},
    {"glScissor", reinterpret_cast<void*>(&G_glScissor)},
    {"glShadeModel", reinterpret_cast<void*>(&G_glShadeModel)},
    {"glStencilFunc", reinterpret_cast<void*>(&G_glStencilFunc)},
    {"glStencilMask", reinterpret_cast<void*>(&G_glStencilMask)},
    {"glStencilOp", reinterpret_cast<void*>(&G_glStencilOp)},
    {"glTexCoordPointer", reinterpret_cast<void*>(&G_glTexCoordPointer)},
    {"glTexEnvi", reinterpret_cast<void*>(&G_glTexEnvi)},
    {"glTexEnvx", reinterpret_cast<void*>(&G_glTexEnvx)},
    {"glTexEnviv", reinterpret_cast<void*>(&G_glTexEnviv)},
    {"glTexEnvxv", reinterpret_cast<void*>(&G_glTexEnvxv)},
    {"glTexImage2D", reinterpret_cast<void*>(&G_glTexImage2D)},
    {"glTexParameteri", reinterpret_cast<void*>(&G_glTexParameteri)},
    {"glTexParameterx", reinterpret_cast<void*>(&G_glTexParameterx)},
    {"glTexParameteriv", reinterpret_cast<void*>(&G_glTexParameteriv)},
    {"glTexParameterxv", reinterpret_cast<void*>(&G_glTexParameterxv)},
    {"glTexSubImage2D", reinterpret_cast<void*>(&G_glTexSubImage2D)},
    {"glTranslatex", reinterpret_cast<void*>(&G_glTranslatex)},
    {"glVertexPointer", reinterpret_cast<void*>(&G_glVertexPointer)},
    {"glViewport", reinterpret_cast<void*>(&G_glViewport)},
    {"glPointSizePointerOES", reinterpret_cast<void*>(&G_glPointSizePointerOES)},
};

const guest::HostSymbol kAutoGles2And3Symbols[] = {
    {"glClearColor", reinterpret_cast<void*>(&G_glClearColor)},
    {"glClearDepthf", reinterpret_cast<void*>(&G_glClearDepthf)},
    {"glDepthRangef", reinterpret_cast<void*>(&G_glDepthRangef)},
    {"glGetFloatv", reinterpret_cast<void*>(&G_glGetFloatv)},
    {"glGetTexParameterfv", reinterpret_cast<void*>(&G_glGetTexParameterfv)},
    {"glLineWidth", reinterpret_cast<void*>(&G_glLineWidth)},
    {"glPolygonOffset", reinterpret_cast<void*>(&G_glPolygonOffset)},
    {"glTexParameterf", reinterpret_cast<void*>(&G_glTexParameterf)},
    {"glTexParameterfv", reinterpret_cast<void*>(&G_glTexParameterfv)},
    {"glActiveTexture", reinterpret_cast<void*>(&G_glActiveTexture)},
    {"glBindBuffer", reinterpret_cast<void*>(&G_glBindBuffer)},
    {"glBindTexture", reinterpret_cast<void*>(&G_glBindTexture)},
    {"glBlendFunc", reinterpret_cast<void*>(&G_glBlendFunc)},
    {"glBufferData", reinterpret_cast<void*>(&G_glBufferData)},
    {"glBufferSubData", reinterpret_cast<void*>(&G_glBufferSubData)},
    {"glClear", reinterpret_cast<void*>(&G_glClear)},
    {"glClearStencil", reinterpret_cast<void*>(&G_glClearStencil)},
    {"glColorMask", reinterpret_cast<void*>(&G_glColorMask)},
    {"glCompressedTexImage2D", reinterpret_cast<void*>(&G_glCompressedTexImage2D)},
    {"glCompressedTexSubImage2D", reinterpret_cast<void*>(&G_glCompressedTexSubImage2D)},
    {"glCopyTexImage2D", reinterpret_cast<void*>(&G_glCopyTexImage2D)},
    {"glCopyTexSubImage2D", reinterpret_cast<void*>(&G_glCopyTexSubImage2D)},
    {"glCullFace", reinterpret_cast<void*>(&G_glCullFace)},
    {"glDeleteBuffers", reinterpret_cast<void*>(&G_glDeleteBuffers)},
    {"glDeleteTextures", reinterpret_cast<void*>(&G_glDeleteTextures)},
    {"glDepthFunc", reinterpret_cast<void*>(&G_glDepthFunc)},
    {"glDepthMask", reinterpret_cast<void*>(&G_glDepthMask)},
    {"glDisable", reinterpret_cast<void*>(&G_glDisable)},
    {"glDrawArrays", reinterpret_cast<void*>(&G_glDrawArrays)},
    {"glDrawElements", reinterpret_cast<void*>(&G_glDrawElements)},
    {"glEnable", reinterpret_cast<void*>(&G_glEnable)},
    {"glFinish", reinterpret_cast<void*>(&G_glFinish)},
    {"glFlush", reinterpret_cast<void*>(&G_glFlush)},
    {"glFrontFace", reinterpret_cast<void*>(&G_glFrontFace)},
    {"glGetBooleanv", reinterpret_cast<void*>(&G_glGetBooleanv)},
    {"glGetBufferParameteriv", reinterpret_cast<void*>(&G_glGetBufferParameteriv)},
    {"glGenBuffers", reinterpret_cast<void*>(&G_glGenBuffers)},
    {"glGenTextures", reinterpret_cast<void*>(&G_glGenTextures)},
    {"glGetIntegerv", reinterpret_cast<void*>(&G_glGetIntegerv)},
    {"glGetPointerv", reinterpret_cast<void*>(&G_glGetPointerv)},
    {"glGetTexParameteriv", reinterpret_cast<void*>(&G_glGetTexParameteriv)},
    {"glHint", reinterpret_cast<void*>(&G_glHint)},
    {"glIsBuffer", reinterpret_cast<void*>(&G_glIsBuffer)},
    {"glIsEnabled", reinterpret_cast<void*>(&G_glIsEnabled)},
    {"glIsTexture", reinterpret_cast<void*>(&G_glIsTexture)},
    {"glPixelStorei", reinterpret_cast<void*>(&G_glPixelStorei)},
    {"glReadPixels", reinterpret_cast<void*>(&G_glReadPixels)},
    {"glSampleCoverage", reinterpret_cast<void*>(&G_glSampleCoverage)},
    {"glScissor", reinterpret_cast<void*>(&G_glScissor)},
    {"glStencilFunc", reinterpret_cast<void*>(&G_glStencilFunc)},
    {"glStencilMask", reinterpret_cast<void*>(&G_glStencilMask)},
    {"glStencilOp", reinterpret_cast<void*>(&G_glStencilOp)},
    {"glTexImage2D", reinterpret_cast<void*>(&G_glTexImage2D)},
    {"glTexParameteri", reinterpret_cast<void*>(&G_glTexParameteri)},
    {"glTexParameteriv", reinterpret_cast<void*>(&G_glTexParameteriv)},
    {"glTexSubImage2D", reinterpret_cast<void*>(&G_glTexSubImage2D)},
    {"glViewport", reinterpret_cast<void*>(&G_glViewport)},
    {"glAttachShader", reinterpret_cast<void*>(&G_glAttachShader)},
    {"glBindAttribLocation", reinterpret_cast<void*>(&G_glBindAttribLocation)},
    {"glBindFramebuffer", reinterpret_cast<void*>(&G_glBindFramebuffer)},
    {"glBindRenderbuffer", reinterpret_cast<void*>(&G_glBindRenderbuffer)},
    {"glBlendColor", reinterpret_cast<void*>(&G_glBlendColor)},
    {"glBlendEquation", reinterpret_cast<void*>(&G_glBlendEquation)},
    {"glBlendEquationSeparate", reinterpret_cast<void*>(&G_glBlendEquationSeparate)},
    {"glBlendFuncSeparate", reinterpret_cast<void*>(&G_glBlendFuncSeparate)},
    {"glCheckFramebufferStatus", reinterpret_cast<void*>(&G_glCheckFramebufferStatus)},
    {"glCompileShader", reinterpret_cast<void*>(&G_glCompileShader)},
    {"glCreateProgram", reinterpret_cast<void*>(&G_glCreateProgram)},
    {"glCreateShader", reinterpret_cast<void*>(&G_glCreateShader)},
    {"glDeleteFramebuffers", reinterpret_cast<void*>(&G_glDeleteFramebuffers)},
    {"glDeleteProgram", reinterpret_cast<void*>(&G_glDeleteProgram)},
    {"glDeleteRenderbuffers", reinterpret_cast<void*>(&G_glDeleteRenderbuffers)},
    {"glDeleteShader", reinterpret_cast<void*>(&G_glDeleteShader)},
    {"glDetachShader", reinterpret_cast<void*>(&G_glDetachShader)},
    {"glDisableVertexAttribArray", reinterpret_cast<void*>(&G_glDisableVertexAttribArray)},
    {"glEnableVertexAttribArray", reinterpret_cast<void*>(&G_glEnableVertexAttribArray)},
    {"glFramebufferRenderbuffer", reinterpret_cast<void*>(&G_glFramebufferRenderbuffer)},
    {"glFramebufferTexture2D", reinterpret_cast<void*>(&G_glFramebufferTexture2D)},
    {"glGenerateMipmap", reinterpret_cast<void*>(&G_glGenerateMipmap)},
    {"glGenFramebuffers", reinterpret_cast<void*>(&G_glGenFramebuffers)},
    {"glGenRenderbuffers", reinterpret_cast<void*>(&G_glGenRenderbuffers)},
    {"glGetActiveAttrib", reinterpret_cast<void*>(&G_glGetActiveAttrib)},
    {"glGetActiveUniform", reinterpret_cast<void*>(&G_glGetActiveUniform)},
    {"glGetAttachedShaders", reinterpret_cast<void*>(&G_glGetAttachedShaders)},
    {"glGetAttribLocation", reinterpret_cast<void*>(&G_glGetAttribLocation)},
    {"glGetFramebufferAttachmentParameteriv", reinterpret_cast<void*>(&G_glGetFramebufferAttachmentParameteriv)},
    {"glGetProgramiv", reinterpret_cast<void*>(&G_glGetProgramiv)},
    {"glGetProgramInfoLog", reinterpret_cast<void*>(&G_glGetProgramInfoLog)},
    {"glGetRenderbufferParameteriv", reinterpret_cast<void*>(&G_glGetRenderbufferParameteriv)},
    {"glGetShaderiv", reinterpret_cast<void*>(&G_glGetShaderiv)},
    {"glGetShaderInfoLog", reinterpret_cast<void*>(&G_glGetShaderInfoLog)},
    {"glGetShaderPrecisionFormat", reinterpret_cast<void*>(&G_glGetShaderPrecisionFormat)},
    {"glGetShaderSource", reinterpret_cast<void*>(&G_glGetShaderSource)},
    {"glGetUniformfv", reinterpret_cast<void*>(&G_glGetUniformfv)},
    {"glGetUniformiv", reinterpret_cast<void*>(&G_glGetUniformiv)},
    {"glGetUniformLocation", reinterpret_cast<void*>(&G_glGetUniformLocation)},
    {"glGetVertexAttribfv", reinterpret_cast<void*>(&G_glGetVertexAttribfv)},
    {"glGetVertexAttribiv", reinterpret_cast<void*>(&G_glGetVertexAttribiv)},
    {"glGetVertexAttribPointerv", reinterpret_cast<void*>(&G_glGetVertexAttribPointerv)},
    {"glIsFramebuffer", reinterpret_cast<void*>(&G_glIsFramebuffer)},
    {"glIsProgram", reinterpret_cast<void*>(&G_glIsProgram)},
    {"glIsRenderbuffer", reinterpret_cast<void*>(&G_glIsRenderbuffer)},
    {"glIsShader", reinterpret_cast<void*>(&G_glIsShader)},
    {"glLinkProgram", reinterpret_cast<void*>(&G_glLinkProgram)},
    {"glReleaseShaderCompiler", reinterpret_cast<void*>(&G_glReleaseShaderCompiler)},
    {"glRenderbufferStorage", reinterpret_cast<void*>(&G_glRenderbufferStorage)},
    {"glShaderBinary", reinterpret_cast<void*>(&G_glShaderBinary)},
    {"glShaderSource", reinterpret_cast<void*>(&G_glShaderSource)},
    {"glStencilFuncSeparate", reinterpret_cast<void*>(&G_glStencilFuncSeparate)},
    {"glStencilMaskSeparate", reinterpret_cast<void*>(&G_glStencilMaskSeparate)},
    {"glStencilOpSeparate", reinterpret_cast<void*>(&G_glStencilOpSeparate)},
    {"glUniform1f", reinterpret_cast<void*>(&G_glUniform1f)},
    {"glUniform1fv", reinterpret_cast<void*>(&G_glUniform1fv)},
    {"glUniform1i", reinterpret_cast<void*>(&G_glUniform1i)},
    {"glUniform1iv", reinterpret_cast<void*>(&G_glUniform1iv)},
    {"glUniform2f", reinterpret_cast<void*>(&G_glUniform2f)},
    {"glUniform2fv", reinterpret_cast<void*>(&G_glUniform2fv)},
    {"glUniform2i", reinterpret_cast<void*>(&G_glUniform2i)},
    {"glUniform2iv", reinterpret_cast<void*>(&G_glUniform2iv)},
    {"glUniform3f", reinterpret_cast<void*>(&G_glUniform3f)},
    {"glUniform3fv", reinterpret_cast<void*>(&G_glUniform3fv)},
    {"glUniform3i", reinterpret_cast<void*>(&G_glUniform3i)},
    {"glUniform3iv", reinterpret_cast<void*>(&G_glUniform3iv)},
    {"glUniform4f", reinterpret_cast<void*>(&G_glUniform4f)},
    {"glUniform4fv", reinterpret_cast<void*>(&G_glUniform4fv)},
    {"glUniform4i", reinterpret_cast<void*>(&G_glUniform4i)},
    {"glUniform4iv", reinterpret_cast<void*>(&G_glUniform4iv)},
    {"glUniformMatrix2fv", reinterpret_cast<void*>(&G_glUniformMatrix2fv)},
    {"glUniformMatrix3fv", reinterpret_cast<void*>(&G_glUniformMatrix3fv)},
    {"glUniformMatrix4fv", reinterpret_cast<void*>(&G_glUniformMatrix4fv)},
    {"glUseProgram", reinterpret_cast<void*>(&G_glUseProgram)},
    {"glValidateProgram", reinterpret_cast<void*>(&G_glValidateProgram)},
    {"glVertexAttrib1f", reinterpret_cast<void*>(&G_glVertexAttrib1f)},
    {"glVertexAttrib1fv", reinterpret_cast<void*>(&G_glVertexAttrib1fv)},
    {"glVertexAttrib2f", reinterpret_cast<void*>(&G_glVertexAttrib2f)},
    {"glVertexAttrib2fv", reinterpret_cast<void*>(&G_glVertexAttrib2fv)},
    {"glVertexAttrib3f", reinterpret_cast<void*>(&G_glVertexAttrib3f)},
    {"glVertexAttrib3fv", reinterpret_cast<void*>(&G_glVertexAttrib3fv)},
    {"glVertexAttrib4f", reinterpret_cast<void*>(&G_glVertexAttrib4f)},
    {"glVertexAttrib4fv", reinterpret_cast<void*>(&G_glVertexAttrib4fv)},
    {"glVertexAttribPointer", reinterpret_cast<void*>(&G_glVertexAttribPointer)},
    {"glReadBuffer", reinterpret_cast<void*>(&G_glReadBuffer)},
    {"glDrawRangeElements", reinterpret_cast<void*>(&G_glDrawRangeElements)},
    {"glTexImage3D", reinterpret_cast<void*>(&G_glTexImage3D)},
    {"glTexSubImage3D", reinterpret_cast<void*>(&G_glTexSubImage3D)},
    {"glCopyTexSubImage3D", reinterpret_cast<void*>(&G_glCopyTexSubImage3D)},
    {"glCompressedTexImage3D", reinterpret_cast<void*>(&G_glCompressedTexImage3D)},
    {"glCompressedTexSubImage3D", reinterpret_cast<void*>(&G_glCompressedTexSubImage3D)},
    {"glGenQueries", reinterpret_cast<void*>(&G_glGenQueries)},
    {"glDeleteQueries", reinterpret_cast<void*>(&G_glDeleteQueries)},
    {"glIsQuery", reinterpret_cast<void*>(&G_glIsQuery)},
    {"glBeginQuery", reinterpret_cast<void*>(&G_glBeginQuery)},
    {"glEndQuery", reinterpret_cast<void*>(&G_glEndQuery)},
    {"glGetQueryiv", reinterpret_cast<void*>(&G_glGetQueryiv)},
    {"glGetQueryObjectuiv", reinterpret_cast<void*>(&G_glGetQueryObjectuiv)},
    {"glUnmapBuffer", reinterpret_cast<void*>(&G_glUnmapBuffer)},
    {"glGetBufferPointerv", reinterpret_cast<void*>(&G_glGetBufferPointerv)},
    {"glDrawBuffers", reinterpret_cast<void*>(&G_glDrawBuffers)},
    {"glUniformMatrix2x3fv", reinterpret_cast<void*>(&G_glUniformMatrix2x3fv)},
    {"glUniformMatrix3x2fv", reinterpret_cast<void*>(&G_glUniformMatrix3x2fv)},
    {"glUniformMatrix2x4fv", reinterpret_cast<void*>(&G_glUniformMatrix2x4fv)},
    {"glUniformMatrix4x2fv", reinterpret_cast<void*>(&G_glUniformMatrix4x2fv)},
    {"glUniformMatrix3x4fv", reinterpret_cast<void*>(&G_glUniformMatrix3x4fv)},
    {"glUniformMatrix4x3fv", reinterpret_cast<void*>(&G_glUniformMatrix4x3fv)},
    {"glBlitFramebuffer", reinterpret_cast<void*>(&G_glBlitFramebuffer)},
    {"glRenderbufferStorageMultisample", reinterpret_cast<void*>(&G_glRenderbufferStorageMultisample)},
    {"glFramebufferTextureLayer", reinterpret_cast<void*>(&G_glFramebufferTextureLayer)},
    {"glMapBufferRange", reinterpret_cast<void*>(&G_glMapBufferRange)},
    {"glFlushMappedBufferRange", reinterpret_cast<void*>(&G_glFlushMappedBufferRange)},
    {"glBindVertexArray", reinterpret_cast<void*>(&G_glBindVertexArray)},
    {"glDeleteVertexArrays", reinterpret_cast<void*>(&G_glDeleteVertexArrays)},
    {"glGenVertexArrays", reinterpret_cast<void*>(&G_glGenVertexArrays)},
    {"glIsVertexArray", reinterpret_cast<void*>(&G_glIsVertexArray)},
    {"glGetIntegeri_v", reinterpret_cast<void*>(&G_glGetIntegeri_v)},
    {"glBeginTransformFeedback", reinterpret_cast<void*>(&G_glBeginTransformFeedback)},
    {"glEndTransformFeedback", reinterpret_cast<void*>(&G_glEndTransformFeedback)},
    {"glBindBufferRange", reinterpret_cast<void*>(&G_glBindBufferRange)},
    {"glBindBufferBase", reinterpret_cast<void*>(&G_glBindBufferBase)},
    {"glTransformFeedbackVaryings", reinterpret_cast<void*>(&G_glTransformFeedbackVaryings)},
    {"glGetTransformFeedbackVarying", reinterpret_cast<void*>(&G_glGetTransformFeedbackVarying)},
    {"glVertexAttribIPointer", reinterpret_cast<void*>(&G_glVertexAttribIPointer)},
    {"glGetVertexAttribIiv", reinterpret_cast<void*>(&G_glGetVertexAttribIiv)},
    {"glGetVertexAttribIuiv", reinterpret_cast<void*>(&G_glGetVertexAttribIuiv)},
    {"glVertexAttribI4i", reinterpret_cast<void*>(&G_glVertexAttribI4i)},
    {"glVertexAttribI4ui", reinterpret_cast<void*>(&G_glVertexAttribI4ui)},
    {"glVertexAttribI4iv", reinterpret_cast<void*>(&G_glVertexAttribI4iv)},
    {"glVertexAttribI4uiv", reinterpret_cast<void*>(&G_glVertexAttribI4uiv)},
    {"glGetUniformuiv", reinterpret_cast<void*>(&G_glGetUniformuiv)},
    {"glGetFragDataLocation", reinterpret_cast<void*>(&G_glGetFragDataLocation)},
    {"glUniform1ui", reinterpret_cast<void*>(&G_glUniform1ui)},
    {"glUniform2ui", reinterpret_cast<void*>(&G_glUniform2ui)},
    {"glUniform3ui", reinterpret_cast<void*>(&G_glUniform3ui)},
    {"glUniform4ui", reinterpret_cast<void*>(&G_glUniform4ui)},
    {"glUniform1uiv", reinterpret_cast<void*>(&G_glUniform1uiv)},
    {"glUniform2uiv", reinterpret_cast<void*>(&G_glUniform2uiv)},
    {"glUniform3uiv", reinterpret_cast<void*>(&G_glUniform3uiv)},
    {"glUniform4uiv", reinterpret_cast<void*>(&G_glUniform4uiv)},
    {"glClearBufferiv", reinterpret_cast<void*>(&G_glClearBufferiv)},
    {"glClearBufferuiv", reinterpret_cast<void*>(&G_glClearBufferuiv)},
    {"glClearBufferfv", reinterpret_cast<void*>(&G_glClearBufferfv)},
    {"glClearBufferfi", reinterpret_cast<void*>(&G_glClearBufferfi)},
    {"glCopyBufferSubData", reinterpret_cast<void*>(&G_glCopyBufferSubData)},
    {"glGetUniformIndices", reinterpret_cast<void*>(&G_glGetUniformIndices)},
    {"glGetActiveUniformsiv", reinterpret_cast<void*>(&G_glGetActiveUniformsiv)},
    {"glGetUniformBlockIndex", reinterpret_cast<void*>(&G_glGetUniformBlockIndex)},
    {"glGetActiveUniformBlockiv", reinterpret_cast<void*>(&G_glGetActiveUniformBlockiv)},
    {"glGetActiveUniformBlockName", reinterpret_cast<void*>(&G_glGetActiveUniformBlockName)},
    {"glUniformBlockBinding", reinterpret_cast<void*>(&G_glUniformBlockBinding)},
    {"glDrawArraysInstanced", reinterpret_cast<void*>(&G_glDrawArraysInstanced)},
    {"glDrawElementsInstanced", reinterpret_cast<void*>(&G_glDrawElementsInstanced)},
    {"glFenceSync", reinterpret_cast<void*>(&G_glFenceSync)},
    {"glIsSync", reinterpret_cast<void*>(&G_glIsSync)},
    {"glDeleteSync", reinterpret_cast<void*>(&G_glDeleteSync)},
    {"glClientWaitSync", reinterpret_cast<void*>(&G_glClientWaitSync)},
    {"glWaitSync", reinterpret_cast<void*>(&G_glWaitSync)},
    {"glGetInteger64v", reinterpret_cast<void*>(&G_glGetInteger64v)},
    {"glGetSynciv", reinterpret_cast<void*>(&G_glGetSynciv)},
    {"glGetInteger64i_v", reinterpret_cast<void*>(&G_glGetInteger64i_v)},
    {"glGetBufferParameteri64v", reinterpret_cast<void*>(&G_glGetBufferParameteri64v)},
    {"glGenSamplers", reinterpret_cast<void*>(&G_glGenSamplers)},
    {"glDeleteSamplers", reinterpret_cast<void*>(&G_glDeleteSamplers)},
    {"glIsSampler", reinterpret_cast<void*>(&G_glIsSampler)},
    {"glBindSampler", reinterpret_cast<void*>(&G_glBindSampler)},
    {"glSamplerParameteri", reinterpret_cast<void*>(&G_glSamplerParameteri)},
    {"glSamplerParameteriv", reinterpret_cast<void*>(&G_glSamplerParameteriv)},
    {"glSamplerParameterf", reinterpret_cast<void*>(&G_glSamplerParameterf)},
    {"glSamplerParameterfv", reinterpret_cast<void*>(&G_glSamplerParameterfv)},
    {"glGetSamplerParameteriv", reinterpret_cast<void*>(&G_glGetSamplerParameteriv)},
    {"glGetSamplerParameterfv", reinterpret_cast<void*>(&G_glGetSamplerParameterfv)},
    {"glVertexAttribDivisor", reinterpret_cast<void*>(&G_glVertexAttribDivisor)},
    {"glBindTransformFeedback", reinterpret_cast<void*>(&G_glBindTransformFeedback)},
    {"glDeleteTransformFeedbacks", reinterpret_cast<void*>(&G_glDeleteTransformFeedbacks)},
    {"glGenTransformFeedbacks", reinterpret_cast<void*>(&G_glGenTransformFeedbacks)},
    {"glIsTransformFeedback", reinterpret_cast<void*>(&G_glIsTransformFeedback)},
    {"glPauseTransformFeedback", reinterpret_cast<void*>(&G_glPauseTransformFeedback)},
    {"glResumeTransformFeedback", reinterpret_cast<void*>(&G_glResumeTransformFeedback)},
    {"glGetProgramBinary", reinterpret_cast<void*>(&G_glGetProgramBinary)},
    {"glProgramBinary", reinterpret_cast<void*>(&G_glProgramBinary)},
    {"glProgramParameteri", reinterpret_cast<void*>(&G_glProgramParameteri)},
    {"glInvalidateFramebuffer", reinterpret_cast<void*>(&G_glInvalidateFramebuffer)},
    {"glInvalidateSubFramebuffer", reinterpret_cast<void*>(&G_glInvalidateSubFramebuffer)},
    {"glTexStorage2D", reinterpret_cast<void*>(&G_glTexStorage2D)},
    {"glTexStorage3D", reinterpret_cast<void*>(&G_glTexStorage3D)},
    {"glGetInternalformativ", reinterpret_cast<void*>(&G_glGetInternalformativ)},
    {"glDispatchCompute", reinterpret_cast<void*>(&G_glDispatchCompute)},
    {"glDispatchComputeIndirect", reinterpret_cast<void*>(&G_glDispatchComputeIndirect)},
    {"glDrawArraysIndirect", reinterpret_cast<void*>(&G_glDrawArraysIndirect)},
    {"glDrawElementsIndirect", reinterpret_cast<void*>(&G_glDrawElementsIndirect)},
    {"glFramebufferParameteri", reinterpret_cast<void*>(&G_glFramebufferParameteri)},
    {"glGetFramebufferParameteriv", reinterpret_cast<void*>(&G_glGetFramebufferParameteriv)},
    {"glGetProgramInterfaceiv", reinterpret_cast<void*>(&G_glGetProgramInterfaceiv)},
    {"glGetProgramResourceIndex", reinterpret_cast<void*>(&G_glGetProgramResourceIndex)},
    {"glGetProgramResourceName", reinterpret_cast<void*>(&G_glGetProgramResourceName)},
    {"glGetProgramResourceiv", reinterpret_cast<void*>(&G_glGetProgramResourceiv)},
    {"glGetProgramResourceLocation", reinterpret_cast<void*>(&G_glGetProgramResourceLocation)},
    {"glUseProgramStages", reinterpret_cast<void*>(&G_glUseProgramStages)},
    {"glActiveShaderProgram", reinterpret_cast<void*>(&G_glActiveShaderProgram)},
    {"glCreateShaderProgramv", reinterpret_cast<void*>(&G_glCreateShaderProgramv)},
    {"glBindProgramPipeline", reinterpret_cast<void*>(&G_glBindProgramPipeline)},
    {"glDeleteProgramPipelines", reinterpret_cast<void*>(&G_glDeleteProgramPipelines)},
    {"glGenProgramPipelines", reinterpret_cast<void*>(&G_glGenProgramPipelines)},
    {"glIsProgramPipeline", reinterpret_cast<void*>(&G_glIsProgramPipeline)},
    {"glGetProgramPipelineiv", reinterpret_cast<void*>(&G_glGetProgramPipelineiv)},
    {"glProgramUniform1i", reinterpret_cast<void*>(&G_glProgramUniform1i)},
    {"glProgramUniform2i", reinterpret_cast<void*>(&G_glProgramUniform2i)},
    {"glProgramUniform3i", reinterpret_cast<void*>(&G_glProgramUniform3i)},
    {"glProgramUniform4i", reinterpret_cast<void*>(&G_glProgramUniform4i)},
    {"glProgramUniform1ui", reinterpret_cast<void*>(&G_glProgramUniform1ui)},
    {"glProgramUniform2ui", reinterpret_cast<void*>(&G_glProgramUniform2ui)},
    {"glProgramUniform3ui", reinterpret_cast<void*>(&G_glProgramUniform3ui)},
    {"glProgramUniform4ui", reinterpret_cast<void*>(&G_glProgramUniform4ui)},
    {"glProgramUniform1f", reinterpret_cast<void*>(&G_glProgramUniform1f)},
    {"glProgramUniform2f", reinterpret_cast<void*>(&G_glProgramUniform2f)},
    {"glProgramUniform3f", reinterpret_cast<void*>(&G_glProgramUniform3f)},
    {"glProgramUniform4f", reinterpret_cast<void*>(&G_glProgramUniform4f)},
    {"glProgramUniform1iv", reinterpret_cast<void*>(&G_glProgramUniform1iv)},
    {"glProgramUniform2iv", reinterpret_cast<void*>(&G_glProgramUniform2iv)},
    {"glProgramUniform3iv", reinterpret_cast<void*>(&G_glProgramUniform3iv)},
    {"glProgramUniform4iv", reinterpret_cast<void*>(&G_glProgramUniform4iv)},
    {"glProgramUniform1uiv", reinterpret_cast<void*>(&G_glProgramUniform1uiv)},
    {"glProgramUniform2uiv", reinterpret_cast<void*>(&G_glProgramUniform2uiv)},
    {"glProgramUniform3uiv", reinterpret_cast<void*>(&G_glProgramUniform3uiv)},
    {"glProgramUniform4uiv", reinterpret_cast<void*>(&G_glProgramUniform4uiv)},
    {"glProgramUniform1fv", reinterpret_cast<void*>(&G_glProgramUniform1fv)},
    {"glProgramUniform2fv", reinterpret_cast<void*>(&G_glProgramUniform2fv)},
    {"glProgramUniform3fv", reinterpret_cast<void*>(&G_glProgramUniform3fv)},
    {"glProgramUniform4fv", reinterpret_cast<void*>(&G_glProgramUniform4fv)},
    {"glProgramUniformMatrix2fv", reinterpret_cast<void*>(&G_glProgramUniformMatrix2fv)},
    {"glProgramUniformMatrix3fv", reinterpret_cast<void*>(&G_glProgramUniformMatrix3fv)},
    {"glProgramUniformMatrix4fv", reinterpret_cast<void*>(&G_glProgramUniformMatrix4fv)},
    {"glProgramUniformMatrix2x3fv", reinterpret_cast<void*>(&G_glProgramUniformMatrix2x3fv)},
    {"glProgramUniformMatrix3x2fv", reinterpret_cast<void*>(&G_glProgramUniformMatrix3x2fv)},
    {"glProgramUniformMatrix2x4fv", reinterpret_cast<void*>(&G_glProgramUniformMatrix2x4fv)},
    {"glProgramUniformMatrix4x2fv", reinterpret_cast<void*>(&G_glProgramUniformMatrix4x2fv)},
    {"glProgramUniformMatrix3x4fv", reinterpret_cast<void*>(&G_glProgramUniformMatrix3x4fv)},
    {"glProgramUniformMatrix4x3fv", reinterpret_cast<void*>(&G_glProgramUniformMatrix4x3fv)},
    {"glValidateProgramPipeline", reinterpret_cast<void*>(&G_glValidateProgramPipeline)},
    {"glGetProgramPipelineInfoLog", reinterpret_cast<void*>(&G_glGetProgramPipelineInfoLog)},
    {"glBindImageTexture", reinterpret_cast<void*>(&G_glBindImageTexture)},
    {"glGetBooleani_v", reinterpret_cast<void*>(&G_glGetBooleani_v)},
    {"glMemoryBarrier", reinterpret_cast<void*>(&G_glMemoryBarrier)},
    {"glMemoryBarrierByRegion", reinterpret_cast<void*>(&G_glMemoryBarrierByRegion)},
    {"glTexStorage2DMultisample", reinterpret_cast<void*>(&G_glTexStorage2DMultisample)},
    {"glGetMultisamplefv", reinterpret_cast<void*>(&G_glGetMultisamplefv)},
    {"glSampleMaski", reinterpret_cast<void*>(&G_glSampleMaski)},
    {"glGetTexLevelParameteriv", reinterpret_cast<void*>(&G_glGetTexLevelParameteriv)},
    {"glGetTexLevelParameterfv", reinterpret_cast<void*>(&G_glGetTexLevelParameterfv)},
    {"glBindVertexBuffer", reinterpret_cast<void*>(&G_glBindVertexBuffer)},
    {"glVertexAttribFormat", reinterpret_cast<void*>(&G_glVertexAttribFormat)},
    {"glVertexAttribIFormat", reinterpret_cast<void*>(&G_glVertexAttribIFormat)},
    {"glVertexAttribBinding", reinterpret_cast<void*>(&G_glVertexAttribBinding)},
    {"glVertexBindingDivisor", reinterpret_cast<void*>(&G_glVertexBindingDivisor)},
    {"glBlendBarrier", reinterpret_cast<void*>(&G_glBlendBarrier)},
    {"glCopyImageSubData", reinterpret_cast<void*>(&G_glCopyImageSubData)},
    {"glDebugMessageControl", reinterpret_cast<void*>(&G_glDebugMessageControl)},
    {"glDebugMessageInsert", reinterpret_cast<void*>(&G_glDebugMessageInsert)},
    {"glGetDebugMessageLog", reinterpret_cast<void*>(&G_glGetDebugMessageLog)},
    {"glPushDebugGroup", reinterpret_cast<void*>(&G_glPushDebugGroup)},
    {"glPopDebugGroup", reinterpret_cast<void*>(&G_glPopDebugGroup)},
    {"glObjectLabel", reinterpret_cast<void*>(&G_glObjectLabel)},
    {"glGetObjectLabel", reinterpret_cast<void*>(&G_glGetObjectLabel)},
    {"glObjectPtrLabel", reinterpret_cast<void*>(&G_glObjectPtrLabel)},
    {"glGetObjectPtrLabel", reinterpret_cast<void*>(&G_glGetObjectPtrLabel)},
    {"glEnablei", reinterpret_cast<void*>(&G_glEnablei)},
    {"glDisablei", reinterpret_cast<void*>(&G_glDisablei)},
    {"glBlendEquationi", reinterpret_cast<void*>(&G_glBlendEquationi)},
    {"glBlendEquationSeparatei", reinterpret_cast<void*>(&G_glBlendEquationSeparatei)},
    {"glBlendFunci", reinterpret_cast<void*>(&G_glBlendFunci)},
    {"glBlendFuncSeparatei", reinterpret_cast<void*>(&G_glBlendFuncSeparatei)},
    {"glColorMaski", reinterpret_cast<void*>(&G_glColorMaski)},
    {"glIsEnabledi", reinterpret_cast<void*>(&G_glIsEnabledi)},
    {"glDrawElementsBaseVertex", reinterpret_cast<void*>(&G_glDrawElementsBaseVertex)},
    {"glDrawRangeElementsBaseVertex", reinterpret_cast<void*>(&G_glDrawRangeElementsBaseVertex)},
    {"glDrawElementsInstancedBaseVertex", reinterpret_cast<void*>(&G_glDrawElementsInstancedBaseVertex)},
    {"glFramebufferTexture", reinterpret_cast<void*>(&G_glFramebufferTexture)},
    {"glPrimitiveBoundingBox", reinterpret_cast<void*>(&G_glPrimitiveBoundingBox)},
    {"glGetGraphicsResetStatus", reinterpret_cast<void*>(&G_glGetGraphicsResetStatus)},
    {"glReadnPixels", reinterpret_cast<void*>(&G_glReadnPixels)},
    {"glGetnUniformfv", reinterpret_cast<void*>(&G_glGetnUniformfv)},
    {"glGetnUniformiv", reinterpret_cast<void*>(&G_glGetnUniformiv)},
    {"glGetnUniformuiv", reinterpret_cast<void*>(&G_glGetnUniformuiv)},
    {"glMinSampleShading", reinterpret_cast<void*>(&G_glMinSampleShading)},
    {"glPatchParameteri", reinterpret_cast<void*>(&G_glPatchParameteri)},
    {"glTexParameterIiv", reinterpret_cast<void*>(&G_glTexParameterIiv)},
    {"glTexParameterIuiv", reinterpret_cast<void*>(&G_glTexParameterIuiv)},
    {"glGetTexParameterIiv", reinterpret_cast<void*>(&G_glGetTexParameterIiv)},
    {"glGetTexParameterIuiv", reinterpret_cast<void*>(&G_glGetTexParameterIuiv)},
    {"glSamplerParameterIiv", reinterpret_cast<void*>(&G_glSamplerParameterIiv)},
    {"glSamplerParameterIuiv", reinterpret_cast<void*>(&G_glSamplerParameterIuiv)},
    {"glGetSamplerParameterIiv", reinterpret_cast<void*>(&G_glGetSamplerParameterIiv)},
    {"glGetSamplerParameterIuiv", reinterpret_cast<void*>(&G_glGetSamplerParameterIuiv)},
    {"glTexBuffer", reinterpret_cast<void*>(&G_glTexBuffer)},
    {"glTexBufferRange", reinterpret_cast<void*>(&G_glTexBufferRange)},
    {"glTexStorage3DMultisample", reinterpret_cast<void*>(&G_glTexStorage3DMultisample)},
    {"glBlendBarrierKHR", reinterpret_cast<void*>(&G_glBlendBarrierKHR)},
    {"glPushDebugGroupKHR", reinterpret_cast<void*>(&G_glPushDebugGroupKHR)},
    {"glPopDebugGroupKHR", reinterpret_cast<void*>(&G_glPopDebugGroupKHR)},
    {"glObjectLabelKHR", reinterpret_cast<void*>(&G_glObjectLabelKHR)},
    {"glObjectPtrLabelKHR", reinterpret_cast<void*>(&G_glObjectPtrLabelKHR)},
    {"glGetGraphicsResetStatusKHR", reinterpret_cast<void*>(&G_glGetGraphicsResetStatusKHR)},
    {"glReadnPixelsKHR", reinterpret_cast<void*>(&G_glReadnPixelsKHR)},
    {"glGetnUniformfvKHR", reinterpret_cast<void*>(&G_glGetnUniformfvKHR)},
    {"glGetnUniformivKHR", reinterpret_cast<void*>(&G_glGetnUniformivKHR)},
    {"glCopyImageSubDataOES", reinterpret_cast<void*>(&G_glCopyImageSubDataOES)},
    {"glMapBufferOES", reinterpret_cast<void*>(&G_glMapBufferOES)},
    {"glUnmapBufferOES", reinterpret_cast<void*>(&G_glUnmapBufferOES)},
    {"glGetBufferPointervOES", reinterpret_cast<void*>(&G_glGetBufferPointervOES)},
    {"glBindVertexArrayOES", reinterpret_cast<void*>(&G_glBindVertexArrayOES)},
    {"glDeleteVertexArraysOES", reinterpret_cast<void*>(&G_glDeleteVertexArraysOES)},
    {"glGenVertexArraysOES", reinterpret_cast<void*>(&G_glGenVertexArraysOES)},
    {"glIsVertexArrayOES", reinterpret_cast<void*>(&G_glIsVertexArrayOES)},
    {"glBlitFramebufferANGLE", reinterpret_cast<void*>(&G_glBlitFramebufferANGLE)},
    {"glRenderbufferStorageMultisampleANGLE", reinterpret_cast<void*>(&G_glRenderbufferStorageMultisampleANGLE)},
    {"glDrawArraysInstancedANGLE", reinterpret_cast<void*>(&G_glDrawArraysInstancedANGLE)},
    {"glDrawElementsInstancedANGLE", reinterpret_cast<void*>(&G_glDrawElementsInstancedANGLE)},
    {"glVertexAttribDivisorANGLE", reinterpret_cast<void*>(&G_glVertexAttribDivisorANGLE)},
    {"glCopyImageSubDataEXT", reinterpret_cast<void*>(&G_glCopyImageSubDataEXT)},
    {"glInsertEventMarkerEXT", reinterpret_cast<void*>(&G_glInsertEventMarkerEXT)},
    {"glPushGroupMarkerEXT", reinterpret_cast<void*>(&G_glPushGroupMarkerEXT)},
    {"glPopGroupMarkerEXT", reinterpret_cast<void*>(&G_glPopGroupMarkerEXT)},
    {"glDiscardFramebufferEXT", reinterpret_cast<void*>(&G_glDiscardFramebufferEXT)},
    {"glGenQueriesEXT", reinterpret_cast<void*>(&G_glGenQueriesEXT)},
    {"glDeleteQueriesEXT", reinterpret_cast<void*>(&G_glDeleteQueriesEXT)},
    {"glIsQueryEXT", reinterpret_cast<void*>(&G_glIsQueryEXT)},
    {"glBeginQueryEXT", reinterpret_cast<void*>(&G_glBeginQueryEXT)},
    {"glEndQueryEXT", reinterpret_cast<void*>(&G_glEndQueryEXT)},
    {"glQueryCounterEXT", reinterpret_cast<void*>(&G_glQueryCounterEXT)},
    {"glGetQueryivEXT", reinterpret_cast<void*>(&G_glGetQueryivEXT)},
    {"glGetQueryObjectivEXT", reinterpret_cast<void*>(&G_glGetQueryObjectivEXT)},
    {"glGetQueryObjectuivEXT", reinterpret_cast<void*>(&G_glGetQueryObjectuivEXT)},
    {"glGetQueryObjecti64vEXT", reinterpret_cast<void*>(&G_glGetQueryObjecti64vEXT)},
    {"glGetQueryObjectui64vEXT", reinterpret_cast<void*>(&G_glGetQueryObjectui64vEXT)},
    {"glDrawBuffersEXT", reinterpret_cast<void*>(&G_glDrawBuffersEXT)},
    {"glEnableiEXT", reinterpret_cast<void*>(&G_glEnableiEXT)},
    {"glDisableiEXT", reinterpret_cast<void*>(&G_glDisableiEXT)},
    {"glBlendEquationiEXT", reinterpret_cast<void*>(&G_glBlendEquationiEXT)},
    {"glBlendEquationSeparateiEXT", reinterpret_cast<void*>(&G_glBlendEquationSeparateiEXT)},
    {"glBlendFunciEXT", reinterpret_cast<void*>(&G_glBlendFunciEXT)},
    {"glBlendFuncSeparateiEXT", reinterpret_cast<void*>(&G_glBlendFuncSeparateiEXT)},
    {"glColorMaskiEXT", reinterpret_cast<void*>(&G_glColorMaskiEXT)},
    {"glIsEnablediEXT", reinterpret_cast<void*>(&G_glIsEnablediEXT)},
    {"glDrawArraysInstancedEXT", reinterpret_cast<void*>(&G_glDrawArraysInstancedEXT)},
    {"glDrawElementsInstancedEXT", reinterpret_cast<void*>(&G_glDrawElementsInstancedEXT)},
    {"glVertexAttribDivisorEXT", reinterpret_cast<void*>(&G_glVertexAttribDivisorEXT)},
    {"glMapBufferRangeEXT", reinterpret_cast<void*>(&G_glMapBufferRangeEXT)},
    {"glFlushMappedBufferRangeEXT", reinterpret_cast<void*>(&G_glFlushMappedBufferRangeEXT)},
    {"glRenderbufferStorageMultisampleEXT", reinterpret_cast<void*>(&G_glRenderbufferStorageMultisampleEXT)},
    {"glFramebufferTexture2DMultisampleEXT", reinterpret_cast<void*>(&G_glFramebufferTexture2DMultisampleEXT)},
    {"glGetGraphicsResetStatusEXT", reinterpret_cast<void*>(&G_glGetGraphicsResetStatusEXT)},
    {"glReadnPixelsEXT", reinterpret_cast<void*>(&G_glReadnPixelsEXT)},
    {"glGetnUniformfvEXT", reinterpret_cast<void*>(&G_glGetnUniformfvEXT)},
    {"glGetnUniformivEXT", reinterpret_cast<void*>(&G_glGetnUniformivEXT)},
    {"glTexStorage1DEXT", reinterpret_cast<void*>(&G_glTexStorage1DEXT)},
    {"glTexStorage2DEXT", reinterpret_cast<void*>(&G_glTexStorage2DEXT)},
    {"glTexStorage3DEXT", reinterpret_cast<void*>(&G_glTexStorage3DEXT)},
};
