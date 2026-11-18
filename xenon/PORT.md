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
- Pass-0 XEX reaches `boot: frame 3 complete` in Xenia without a crash. The user
  reports the screen is cyan/black; title text is absent, cars are thin slivers,
  city fronts collapse into a diagonal strip, and ground is a flat red plane.
- XSDK source inspection: `gpu_draw.c:595` encodes `size_bytes / 4` (dwords)
  shifted by 2. Xenos' fetch constant size is 24 bits (`xe_gpu_vertex_fetch_t`),
  so its field limit is 16,777,215 dwords, not 65,535. The game nevertheless
  chunks at 5,461 48-byte vertices (262,128 bytes) as a conservative test.
- Allocation instrumentation in `gfx_xsdk.h` checks ranges, failures, and
  running bytes. The 30-second object-test run made 222 app GPU allocations,
  totaling 53,936,128 live bytes including two 8 MiB stream arenas; no null
  allocations, overlaps, or 512 MiB-budget warnings appeared. XSDK itself has
  no fixed GPU sub-pool: `xsdk_gpu_alloc` page-rounds and calls
  `MmAllocatePhysicalMemoryEx` over the physical address range.
- `xenon/build/game/fogcityfury_objects.xex` cycles terrain, props, water, and
  cars every three seconds; its log says which is active. It is the next visual
  diagnostic. Pass 0 draws UI after the direct scene path, and sampler units
  are rebound immediately before each draw, but title text still needs visual
  confirmation.
- `xenon/build/game/xenia-objects.log` and `xenia-objects-console.log` contain
  the Xenia run. Xenia reports unknown writes to registers 0x183B and 0x0A10;
  startup reaches frame 3. Pixel screenshots must come from the user's Xenia
  window (this environment cannot capture its Vulkan surface).
- The LUT workaround is reproducible: `build_game.sh` applies the patch under
  `xenon/patches/` to `~/xsdk`; Xenia does not emulate display register 0x1928.
- XEX path: `xenon/build/game/fogcityfury.xex`.
- Sound, saves, and network remain disabled/not implemented on the 360.
- Real RGH 360 testing is now in progress; wait for the user's visual result
  before changing graphics code.
- Startup screen diagnostics (XSDK only): red after `openDisplay`, orange after
  `gfx::init`, yellow after all `makeProg` calls, green after `ui.init`, cyan
  after texture setup, blue after car meshes, purple after `startGame`, then
  white when startup is complete. Each checkpoint is held for 0.5 s; the last
  colour shown identifies the last completed step. The main loop then alternates
  white and black once per second for 5 s before normal rendering.


## Later (don't start unless asked)
- Big-endian data: save files, network packets, anything read from disk.
- Sound, saves, network on the 360.
- Splitting the full world shader so the 360 town matches the PC.


## Rules
- Desktop tests must stay green: `make all && python3 tests/run_tests.py --render`.
- Commit small, push xenon-port often.
