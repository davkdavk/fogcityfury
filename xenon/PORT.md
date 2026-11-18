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
- `src/gfx_xsdk.h` and the `plat::Display` backend build the real game XEX.
  `xenon/build_game.sh` uses `~/xsdk` and `~/xsdk-gcc`, applies
  `xenon/patches/xsdk-display-lut.patch`, and copies `xenon/dbgprint.txt`
  beside the XEX so startup breadcrumbs appear in Xenia's log.
- The C++ runtime probe passes global constructors, `new[]`, a 64 MiB vector,
  `std::string`, throw/catch, and `fopen` (null without a test file).
- Xenia-specific fixes: bypass XSDK's unsupported timezone import; package
  shader containers in big-endian word order; store each program's own literal
  counts; restore EDRAM targets after resolves; use LDR at 720p to fit EDRAM.
- XSDK sampler rebinding is done immediately before draws. `data(..., true)`
  streams dynamic UI/particle vertices through two fenced 8 MiB arenas;
  `data(..., false)` uses persistent GPU memory for buffers such as skid marks.
- `--xsdk-passes 0..3` selects direct, post, shadow, and reflection passes;
  the XSDK build defaults to 0, so Xenia needs no launch arguments.


## Current Xenia Verification
- Xenia runs the no-argument, pass-0 XEX for 30 seconds without returning to
  dash. `xenia-run.log`
  has `boot: frame 1 complete`, `boot: frame 2 complete`, and
  `boot: frame 3 complete`; no shader or GPU assertion follows.
- Pass 0 draws the title's sky/world/props/cars and HUD directly, skipping post,
  shadows, and reflections. The expected title UI is FOG CITY FURY with RACE,
  ONLINE, and OPTIONS.
- X11 creates the `Fog City Fury` Xenia window, but reports it as `IsUnMapped`.
  XWD gets `BadMatch` on its Vulkan surface and root captures show the desktop,
  so the pixels are not yet visually confirmed. This is not a locked desktop.
- The LUT workaround is reproducible: `build_game.sh` applies the patch under
  `xenon/patches/` to `~/xsdk`; Xenia does not emulate display register 0x1928.
- XEX path: `xenon/build/game/fogcityfury.xex`.
- Sound, saves, and network remain disabled/not implemented on the 360.


## Later (don't start unless asked)
- Big-endian data: save files, network packets, anything read from disk.
- Sound, saves, network on the 360.
- Splitting the full world shader so the 360 town matches the PC.


## Rules
- Desktop tests must stay green: `make all && python3 tests/run_tests.py --render`.
- Commit small, push xenon-port often.
