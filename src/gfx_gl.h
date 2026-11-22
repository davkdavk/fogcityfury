// gfx_gl.h - gfx.h on OpenGL 3.3 core and OpenGL ES 3.0. See gfx.h for what each call promises.
#pragma once
#ifndef GL_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
#endif

namespace gfx {
namespace gl {
struct TexObj { GLenum kind = 0; GLuint name = 0; };   // kind: GL_TEXTURE_2D, GL_TEXTURE_CUBE_MAP or GL_RENDERBUFFER
struct BufObj { GLuint vao = 0, vbo = 0, ebo = 0; };
static std::vector<TexObj> tex(1);
static std::vector<BufObj> buf(1);
static std::vector<GLuint> fbo(1, 0);                   // fbo[0]: the screen
static GLuint emptyVAO = 0;
static const GLenum SCRATCH = GL_TEXTURE7;              // textures are made here, so the units the game uses keep what is bound to them
struct FmtGL { GLint internal; GLenum format, type; };
static FmtGL fmtGL(Fmt f) {
    switch (f) {
    case R8: return { GL_R8, GL_RED, GL_UNSIGNED_BYTE };
    case R16F: return { GL_R16F, GL_RED, GL_FLOAT };
    case RGB8: return { GL_RGB8, GL_RGB, GL_UNSIGNED_BYTE };
    case RGBA8: return { GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE };
    case RGBA16F: return { GL_RGBA16F, GL_RGBA, GL_FLOAT };
    default: return { GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT };
    }
}
static Tex newTex(GLenum kind) { TexObj o; o.kind = kind; if (kind == GL_RENDERBUFFER) glGenRenderbuffers(1, &o.name); else glGenTextures(1, &o.name); tex.push_back(o); return (Tex)(tex.size() - 1); }
static void sampler(GLenum kind, Sampler s) {
    glTexParameteri(kind, GL_TEXTURE_MIN_FILTER, s.mips ? GL_LINEAR_MIPMAP_LINEAR : s.linear ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(kind, GL_TEXTURE_MAG_FILTER, s.linear ? GL_LINEAR : GL_NEAREST);
    GLint wrap = s.repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE;
    glTexParameteri(kind, GL_TEXTURE_WRAP_S, wrap); glTexParameteri(kind, GL_TEXTURE_WRAP_T, wrap); if (kind == GL_TEXTURE_CUBE_MAP) glTexParameteri(kind, GL_TEXTURE_WRAP_R, wrap);
    if (s.compare) { glTexParameteri(kind, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE); glTexParameteri(kind, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL); }
}
static void attribs(Layout l) {
    static const int MESH[] = { 3, 3, 3, 2, 1, 0 }, PART[] = { 3, 2, 4, 2, 0 }, UI[] = { 2, 2, 4, 0 };
    const int* sz = l == L_MESH ? MESH : l == L_PART ? PART : UI; int stride = l == L_MESH ? 48 : l == L_PART ? 44 : 32; size_t off = 0;
    for (int i = 0; sz[i]; i++) { glEnableVertexAttribArray(i); glVertexAttribPointer(i, sz[i], GL_FLOAT, GL_FALSE, stride, (void*)off); off += sz[i] * 4; }
}
}   // namespace gl

static void init() {
    glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LEQUAL); glDisable(GL_CULL_FACE); glPixelStorei(GL_UNPACK_ALIGNMENT, 1); glPixelStorei(GL_PACK_ALIGNMENT, 1);
#ifndef FCF_GLES
    glEnable(GL_MULTISAMPLE); glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);   // always on in GLES 3
#endif
    glGenVertexArrays(1, &gl::emptyVAO);
}
static int maxSamples() { GLint ms = 0; glGetIntegerv(0x8D57 /* GL_MAX_SAMPLES */, &ms); return ms; }
static int maxAniso() {
    float fa = 1; if (SDL_GL_ExtensionSupported("GL_EXT_texture_filter_anisotropic") || SDL_GL_ExtensionSupported("GL_ARB_texture_filter_anisotropic")) glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &fa);
    return (int)fa;
}

// ---- textures ----
static void tex2D(Tex& t, Fmt f, int w, int h, const void* px, Sampler s) {
    if (!t) t = gl::newTex(GL_TEXTURE_2D);
    gl::FmtGL g = gl::fmtGL(f); glActiveTexture(gl::SCRATCH); glBindTexture(GL_TEXTURE_2D, gl::tex[t].name);
    glTexImage2D(GL_TEXTURE_2D, 0, g.internal, w, h, 0, g.format, g.type, px);
    if (s.mips && px) glGenerateMipmap(GL_TEXTURE_2D);
    gl::sampler(GL_TEXTURE_2D, s); glBindTexture(GL_TEXTURE_2D, 0);
}
static void texCube(Tex& t, Fmt f, int size, Sampler s) {
    if (!t) t = gl::newTex(GL_TEXTURE_CUBE_MAP);
    gl::FmtGL g = gl::fmtGL(f); glActiveTexture(gl::SCRATCH); glBindTexture(GL_TEXTURE_CUBE_MAP, gl::tex[t].name);
    for (int i = 0; i < 6; i++) glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, g.internal, size, size, 0, g.format, g.type, nullptr);
    gl::sampler(GL_TEXTURE_CUBE_MAP, s); glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}
static void renderbuffer(Tex& t, Fmt f, int w, int h, int samples) {
    if (!t) t = gl::newTex(GL_RENDERBUFFER);
    glBindRenderbuffer(GL_RENDERBUFFER, gl::tex[t].name); glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, gl::fmtGL(f).internal, w, h);
}
static void aniso(Tex t, int n) { if (!t) return; glActiveTexture(gl::SCRATCH); glBindTexture(gl::tex[t].kind, gl::tex[t].name); glTexParameterf(gl::tex[t].kind, GL_TEXTURE_MAX_ANISOTROPY_EXT, (float)n); glBindTexture(gl::tex[t].kind, 0); }
static void mips(Tex t) { glActiveTexture(gl::SCRATCH); glBindTexture(gl::tex[t].kind, gl::tex[t].name); glGenerateMipmap(gl::tex[t].kind); glBindTexture(gl::tex[t].kind, 0); }
static void bind(int unit, Tex t) { glActiveTexture(GL_TEXTURE0 + unit); glBindTexture(t ? gl::tex[t].kind : GL_TEXTURE_2D, t ? gl::tex[t].name : 0); }

// ---- targets ----
static void target(Target& t, Tex color, int face, Tex depth) {
    if (!t) { GLuint f; glGenFramebuffers(1, &f); gl::fbo.push_back(f); t = (Target)(gl::fbo.size() - 1); }
    glBindFramebuffer(GL_FRAMEBUFFER, gl::fbo[t]);
    auto attach = [](GLenum at, Tex x, int face) {
        if (!x) return; const gl::TexObj& o = gl::tex[x];
        if (o.kind == GL_RENDERBUFFER) glFramebufferRenderbuffer(GL_FRAMEBUFFER, at, GL_RENDERBUFFER, o.name);
        else glFramebufferTexture2D(GL_FRAMEBUFFER, at, o.kind == GL_TEXTURE_CUBE_MAP ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + face : GL_TEXTURE_2D, o.name, 0);
    };
    attach(GL_COLOR_ATTACHMENT0, color, face < 0 ? 0 : face); attach(GL_DEPTH_ATTACHMENT, depth, 0);
    if (!color) { GLenum none = GL_NONE; glDrawBuffers(1, &none); glReadBuffer(GL_NONE); }
}
static void use(Target t, int w, int h) { glBindFramebuffer(GL_FRAMEBUFFER, gl::fbo[t]); glViewport(0, 0, w, h); }
static void clear(bool color, bool depth) { glClear((color ? GL_COLOR_BUFFER_BIT : 0) | (depth ? GL_DEPTH_BUFFER_BIT : 0)); }
static void resolve(Target from, Target to, int w, int h) {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, gl::fbo[from]); glBindFramebuffer(GL_DRAW_FRAMEBUFFER, gl::fbo[to]);
    glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
}

// ---- geometry ----
static void buffer(Buf& b, Layout l) {
    gl::BufObj o; glGenVertexArrays(1, &o.vao); glGenBuffers(1, &o.vbo); glBindVertexArray(o.vao); glBindBuffer(GL_ARRAY_BUFFER, o.vbo); gl::attribs(l);
    gl::buf.push_back(o); b = (Buf)(gl::buf.size() - 1);
}
static void data(Buf b, const void* p, size_t bytes, bool stream) { glBindVertexArray(gl::buf[b].vao); glBindBuffer(GL_ARRAY_BUFFER, gl::buf[b].vbo); glBufferData(GL_ARRAY_BUFFER, bytes, p, stream ? GL_STREAM_DRAW : GL_DYNAMIC_DRAW); }
static void draw(Buf b, int first, int n) { if (n <= 0) return; glBindVertexArray(gl::buf[b].vao); glDrawArrays(GL_TRIANGLES, first, n); }
static void mesh(Buf& b, const void* v, int nv, const uint32_t* idx, int ni) {
    if (!b) { gl::BufObj o; glGenVertexArrays(1, &o.vao); glGenBuffers(1, &o.vbo); glGenBuffers(1, &o.ebo); gl::buf.push_back(o); b = (Buf)(gl::buf.size() - 1); }
    const gl::BufObj& o = gl::buf[b]; glBindVertexArray(o.vao); glBindBuffer(GL_ARRAY_BUFFER, o.vbo);
    glBufferData(GL_ARRAY_BUFFER, (size_t)nv * 48, v, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, o.ebo); glBufferData(GL_ELEMENT_ARRAY_BUFFER, (size_t)ni * 4, idx, GL_STATIC_DRAW);
    gl::attribs(L_MESH); glBindVertexArray(0);
}
static void drawMeshRange(Buf b, int first, int ni) { if (ni <= 0) return; glBindVertexArray(gl::buf[b].vao); glDrawElements(GL_TRIANGLES, ni, GL_UNSIGNED_INT, (void*)((size_t)first * sizeof(uint32_t))); }
static void drawMesh(Buf b, int ni) { drawMeshRange(b, 0, ni); }
static void drawFull() { glBindVertexArray(gl::emptyVAO); glDrawArrays(GL_TRIANGLES, 0, 3); }

// ---- state ----
static void depthTest(bool on) { if (on) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST); }
static void depthWrite(bool on) { glDepthMask(on ? GL_TRUE : GL_FALSE); }
static void blend(Blend m) {
    if (m == B_OFF) { glDisable(GL_BLEND); return; }
    glEnable(GL_BLEND); glBlendFunc(m == B_ALPHA ? GL_SRC_ALPHA : GL_ONE, m == B_ADD ? GL_ONE : GL_ONE_MINUS_SRC_ALPHA);
}
static void offset(float factor, float units) { if (factor == 0 && units == 0) { glDisable(GL_POLYGON_OFFSET_FILL); return; } glEnable(GL_POLYGON_OFFSET_FILL); glPolygonOffset(factor, units); }
static void cull(bool on) { if (on) { glEnable(GL_CULL_FACE); glCullFace(GL_BACK); glFrontFace(GL_CCW); } else glDisable(GL_CULL_FACE); }
static void readRGBA(int w, int h, unsigned char* out) { glPixelStorei(GL_PACK_ALIGNMENT, 1); glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, out); }
static void finish() { glFinish(); }
}   // namespace gfx

// ---- shaders ----
struct Prog {
    GLuint id = 0; std::unordered_map<std::string, GLint> loc;
    GLint L(const char* n) { auto it = loc.find(n); if (it != loc.end()) return it->second; GLint l = glGetUniformLocation(id, n); loc[n] = l; return l; }
    void use() { glUseProgram(id); }
    bool has(const char* n) { return L(n) >= 0; }
    void f1(const char* n, float v) { glUniform1f(L(n), v); }
    void f2(const char* n, float a, float b) { glUniform2f(L(n), a, b); }
    void i1(const char* n, int v) { glUniform1i(L(n), v); }
    void v3(const char* n, V3 v) { glUniform3f(L(n), v.x, v.y, v.z); }
    void v4(const char* n, float a, float b, float c, float d) { glUniform4f(L(n), a, b, c, d); }
    void m4(const char* n, const M4& m) { glUniformMatrix4fv(L(n), 1, GL_FALSE, m.m); }
};
// Shaders are written as GLSL 3.30. On OpenGL ES the version line is swapped for the ES 3.00 one plus default precisions;
// the bodies are kept to the subset both accept (no implicit int-to-float conversions, for one).
static GLuint compile(GLenum type, const char* a, const char* b) {
    GLuint s = glCreateShader(type); const char* head = "";
#ifdef FCF_GLES
    static const char V330[] = "#version 330 core";
    head = "#version 300 es\nprecision highp float;\nprecision highp int;\nprecision highp sampler2D;\nprecision highp samplerCube;\nprecision highp sampler2DShadow;\n";
    if (!strncmp(a, V330, sizeof V330 - 1)) a += sizeof V330 - 1; else if (!strncmp(b, V330, sizeof V330 - 1)) b += sizeof V330 - 1;
#endif
    const char* src[3] = { head, a, b };
    glShaderSource(s, 3, src, nullptr); glCompileShader(s);
    if (const char* dir = getenv("FCF_DUMP_SHADERS")) {   // write every shader exactly as it is handed to the driver (to check them with another compiler)
        static int n = 0; char path[512]; snprintf(path, sizeof path, "%s/%02d.%s", dir, n++, type == GL_VERTEX_SHADER ? "vert" : "frag");
        if (FILE* f = fopen(path, "w")) { fputs(head, f); fputs(a, f); fputs(b, f); fclose(f); }
    }
    GLint ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) { char log[4096]; glGetShaderInfoLog(s, 4096, nullptr, log); fprintf(stderr, "SHADER ERROR:\n%s\n", log); fatal("This graphics driver rejected one of the game's shaders:", log); }
    return s;
}
static Prog makeProg(const char* name, const char* vs, const char* fsCommon, const char* fs) {
    (void)name; Prog p; p.id = glCreateProgram();
    GLuint v = compile(GL_VERTEX_SHADER, vs, ""), f = compile(GL_FRAGMENT_SHADER, fsCommon, fs);
    glAttachShader(p.id, v); glAttachShader(p.id, f); glLinkProgram(p.id);
    GLint ok; glGetProgramiv(p.id, GL_LINK_STATUS, &ok);
    if (!ok) { char log[4096]; glGetProgramInfoLog(p.id, 4096, nullptr, log); fprintf(stderr, "LINK ERROR:\n%s\n", log); fatal("This graphics driver could not link one of the game's shaders:", log); }
    return p;
}
