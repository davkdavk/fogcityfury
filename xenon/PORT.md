# Xbox 360 port (branch xenon-port)


Toolchain: xsdk (not libXenon). Test in Xenia. Only the xenon-port branch is
pushed to; master is the desktop game, and patches from Claude come in as
files applied with `git am -3 <file>`.


## Done
- src/platform.h: timing, threads, audio device, gamepad and error dialogs
  are behind plat:: (step 1).
- The game reads its own input event list (step 2a).
- xenon/fcfcar: an xsdk test title drawing the real Stallion with the game's
  car shader compiled for Xenos. Works in Xenia.
- xenon/transform.py + xenon/build_shaders.sh: GLSL to Xenos for all ten
  programs (20 stages). One command; all compile. The world shader is a
  lighter version on the 360 (the full one is too big for Xenos).
- src/carspec.h: car specs shared by the game and xenon/extract_car.cpp.
- Step 2b: every draw call goes through src/gfx.h (the interface, with a
  spec of every function at the top); src/gfx_gl.h is the OpenGL backend.
  Desktop renders byte-identical to before. 61 tests pass, plus the 4
  rendering tests (`python3 tests/run_tests.py --render`).


## Next: the real game's title screen drawing in Xenia
1. Window and frame swap: main() still creates the window and GL context
   with SDL and calls SDL_GL_SwapWindow. Move that behind plat:: (open the
   display, begin a frame, end a frame). Desktop keeps SDL; the 360 uses
   xsdk_present_open / xsdk_present_begin / xsdk_present_end, as in
   xenon/fcfcar/main.c. Desktop tests must still pass.
2. Write src/gfx_xsdk.h, the 360 backend for gfx.h (built with
   -DFCF_GFX_XSDK). Copy the shape of src/gfx_gl.h.
   - makeProg(name, ...) gets the program name ("world", "car", "water",
     "sky", "part", "depth", "ui", "mark", "ring", "post"): load the pair
     build_shaders.sh compiled.
   - Uniforms are set by name: make transform.py also output each program's
     uniform names and byte offsets in its Globals block; Prog writes there
     and uploads the block at draw time.
   - Sampler units are fixed (0 shadow, 1 env cube, 2 height, 3 font,
     4 post input, 5 bloom, 6 detail): make transform.py give each sampler
     its unit's binding, not binding i+1.
   - Render targets: Xenos draws into EDRAM and resolves to a texture. Build
     use() / clear() / resolve() around that. The shadow map, the
     reflection-cube faces and the bloom passes each need a resolve.
   - Depth range is 0..1 (the same fix as in fcfcar).
3. Goal: the title screen draws in Xenia. No sound, saves or network yet.
   Push xenon-port when it works, or when stuck.


## Later (don't start unless asked)
- Big-endian data: save files, network packets, anything read from disk.
- Sound, saves, network on the 360.
- Splitting the full world shader so the 360 town matches the PC.


## Rules
- Desktop tests must stay green: `make all && python3 tests/run_tests.py --render`.
- Commit small, push xenon-port often.
