// glload.h - OpenGL headers for both platforms.
// Linux: libGL exports every function, so prototypes are enough.
// Windows: opengl32.dll only exports OpenGL 1.1; everything newer is fetched from the driver after the
// context exists (loadGL). Add any new post-1.1 GL call you use to FCF_GL_FUNCS or the Windows build will not link.
#pragma once
// FCF_GLES: build against OpenGL ES 3.0 instead of desktop OpenGL 3.3 (Android; also `make gles` on Linux, to test
// the same code path with Mesa). The renderer uses only what both have; the few differences are marked FCF_GLES.
#if defined(__ANDROID__) && !defined(FCF_GLES)
#define FCF_GLES 1
#endif
#ifdef FCF_GLES
#include <GLES3/gl3.h>
#include <GLES2/gl2ext.h>
static bool loadGL() { return true; }
#elif defined(_WIN32)
#include <GL/gl.h>
#include <GL/glext.h>
#define FCF_GL_FUNCS(X) \
    X(ActiveTexture, ACTIVETEXTURE) X(AttachShader, ATTACHSHADER) X(BindBuffer, BINDBUFFER) X(BindFramebuffer, BINDFRAMEBUFFER) \
    X(BindRenderbuffer, BINDRENDERBUFFER) X(BindVertexArray, BINDVERTEXARRAY) X(BlitFramebuffer, BLITFRAMEBUFFER) X(BufferData, BUFFERDATA) \
    X(CompileShader, COMPILESHADER) X(CreateProgram, CREATEPROGRAM) X(CreateShader, CREATESHADER) X(EnableVertexAttribArray, ENABLEVERTEXATTRIBARRAY) \
    X(FramebufferRenderbuffer, FRAMEBUFFERRENDERBUFFER) X(FramebufferTexture2D, FRAMEBUFFERTEXTURE2D) X(GenBuffers, GENBUFFERS) X(GenFramebuffers, GENFRAMEBUFFERS) \
    X(GenRenderbuffers, GENRENDERBUFFERS) X(GenVertexArrays, GENVERTEXARRAYS) X(GenerateMipmap, GENERATEMIPMAP) X(GetProgramInfoLog, GETPROGRAMINFOLOG) \
    X(GetProgramiv, GETPROGRAMIV) X(GetShaderInfoLog, GETSHADERINFOLOG) X(GetShaderiv, GETSHADERIV) X(GetUniformLocation, GETUNIFORMLOCATION) \
    X(LinkProgram, LINKPROGRAM) X(RenderbufferStorage, RENDERBUFFERSTORAGE) X(RenderbufferStorageMultisample, RENDERBUFFERSTORAGEMULTISAMPLE) X(ShaderSource, SHADERSOURCE) \
    X(Uniform1f, UNIFORM1F) X(Uniform1i, UNIFORM1I) X(Uniform2f, UNIFORM2F) X(Uniform3f, UNIFORM3F) X(Uniform4f, UNIFORM4F) X(DrawBuffers, DRAWBUFFERS) X(UniformMatrix4fv, UNIFORMMATRIX4FV) X(UseProgram, USEPROGRAM) \
    X(VertexAttribPointer, VERTEXATTRIBPOINTER)
#define X(n, U) static PFNGL##U##PROC gl##n = nullptr;
FCF_GL_FUNCS(X)
#undef X
static bool loadGL() {
    bool ok = true;
#define X(n, U) gl##n = (PFNGL##U##PROC)SDL_GL_GetProcAddress("gl" #n); if (!gl##n) { fprintf(stderr, "OpenGL function missing: gl" #n "\n"); ok = false; }
    FCF_GL_FUNCS(X)
#undef X
    return ok;
}
#else
#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>
#include <GL/glext.h>
static bool loadGL() { return true; }
#endif
