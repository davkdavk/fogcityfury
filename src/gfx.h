// gfx.h - everything the game asks of the graphics hardware, in one place.
//
// The game never calls OpenGL (or anything else) directly: it calls these. A backend implements them:
//   gfx_gl.h    OpenGL 3.3 core / OpenGL ES 3.0 (desktop, Android, ARM boards). The default.
//   gfx_xsdk.h  the Xbox 360 (xsdk: Xenos GPU, shaders compiled ahead of time by xsc). Built with -DFCF_GFX_XSDK.
//
// What a backend must provide, and what the game relies on:
//
// Handles. Tex, Buf and Target are small integers; 0 means none (Target 0 is the screen). A function taking a
// handle by reference creates the object the first time (handle 0) and replaces its contents after that.
//
// Textures (Tex)
//   tex2D(t, fmt, w, h, px, sampler)   2D texture, px may be null (contents undefined: a render target). With
//                                      sampler.mips and px given, the mip chain is built from px.
//                                      px layout per format: R8 1 byte, RGB8 3, RGBA8 4 (bytes per pixel, rows packed);
//                                      R16F and RGBA16F take 32-bit floats (1 or 4) and store them as halves; DEPTH24 takes
//                                      no data. Rows go bottom to top (OpenGL order).
//   texCube(t, fmt, size, sampler)     cube map with undefined contents (only ever drawn into)
//   renderbuffer(t, fmt, w, h, n)      colour or depth storage for a Target that is never sampled; n samples (0: none)
//   aniso(t, n)                        anisotropic filtering, n = 1..maxAniso()
//   mips(t)                            rebuild the mip chain from level 0 (the cube map, after it is drawn into)
//   bind(unit, t)                      units the game uses: 0 shadow map, 1 reflection cube, 2 height map, 3 font,
//                                      4 post input, 5 bloom, 6 ground detail, 7 Xenos perf-test noise. A shader's sampler uniform names its unit
//                                      with Prog::i1 (e.g. i1("uShadow", 0)); a backend with fixed sampler slots maps
//                                      units to its slots there.
//   A DEPTH24 texture with sampler.compare is a shadow map: sampled with a depth comparison (less-or-equal), linear.
//
// Targets (Target)
//   target(t, color, face, depth)      what is drawn into: color is a Tex or renderbuffer (0: no colour, depth only),
//                                      face is the cube face (0..5, order +X -X +Y -Y +Z -Z) if color is a cube, else -1;
//                                      depth is a DEPTH24 texture or renderbuffer, or 0
//   use(t, w, h)                       draw into t from now on; the viewport is (0, 0, w, h)
//   clear(color, depth)                clear the current target: colour to transparent black, depth to 1
//   resolve(from, to, w, h)            copy the colour of from (multisampled) into to (single sample), same size
//
// Geometry (Buf). Three vertex layouts, all 32-bit floats, interleaved:
//   L_MESH  48 bytes: position 3, normal 3, colour 3, uv 2, material 1   (Vtx, base.h)  attribute locations 0-4
//   L_PART  44 bytes: position 3, uv 2, colour 4, misc 2                 (particles, marks, ring)   locations 0-3
//   L_UI    32 bytes: position 2, uv 2, colour 4                         (the HUD and menus)       locations 0-2
//   buffer(b, layout)                  a vertex buffer for one layout
//   data(b, p, bytes, stream)          its contents; stream: rewritten every frame (else now and then)
//   draw(b, first, n)                  triangles from vertices first .. first+n-1
//   mesh(b, v, nv, idx, ni)            an L_MESH buffer with 32-bit indices, uploaded once
//   drawMesh(b, ni)                    its first ni triangles
//   drawMeshRange(b, first, n)         indexed triangles in [first, first+n)
//   drawFull()                         one triangle covering the target; the vertex shader makes it from the vertex
//                                      index (no vertices are read)
//
// State. Whatever was set last stays set.
//   depthTest(on), depthWrite(on)      the depth test is less-or-equal
//   blend(mode)                        B_OFF, B_ALPHA (src*a + dst*(1-a)), B_PREMUL (src + dst*(1-a))
//   offset(factor, units)              polygon depth offset; offset(0, 0) turns it off
//   No face culling, ever.
//
// Shaders (Prog). makeProg(name, vs, fsCommon, fs) builds one of the game's ten programs: world, car, water, sky, part,
// depth, ui, mark, ring, post. The sources are the GLSL in shaders.h; a backend that cannot compile at run time
// uses name to find its precompiled copy. Uniforms are set by name, and a name the program does not use is ignored:
//   use()  f1 f2 v3 v4 i1 m4 (matrices column-major, as M4 stores them)  has(name)
//
// Odds and ends
//   init()                             once, after the context exists
//   maxSamples(), maxAniso()           what the hardware can do (0 = no multisampling; 1 = no anisotropic filtering)
//   readRGBA(w, h, out)                the screen, bottom row first, 4 bytes a pixel (screenshots, tests)
//   finish()                           wait for the GPU
#pragma once

namespace gfx {
enum Fmt : uint8_t { R8, R16F, RGB8, RGBA8, RGBA16F, DEPTH24 };
enum Layout : uint8_t { L_MESH, L_PART, L_UI };
enum Blend : uint8_t { B_OFF, B_ALPHA, B_PREMUL };
struct Sampler { bool linear = true, mips = false, repeat = false, compare = false; };
typedef uint32_t Tex, Buf, Target;
}

#ifdef FCF_GFX_XSDK
#include "gfx_xsdk.h"
#else
#include "gfx_gl.h"
#endif
