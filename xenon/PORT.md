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
- Vertex layout fields in xsdk are ordered `usage, usage_index, format,
  stream, offset_bytes, components, per_instance`; the port now uses designated
  initializers and logs the `world` layout. Xenia reports stride 48 and
  POSITION/FLOAT3/0, COLOR/FLOAT3/12, TEXCOORD0/FLOAT3/24,
  TEXCOORD1/FLOAT2/36, TEXCOORD2/FLOAT1/44. This matches the struct and fcfcar.
- The XEX rebuilt with the current layout and sampler mapping at
  2026-11-19 08:26:31 and reached frames 1-3 in Xenia. Sampler log entries show
  units mapped to xsc slots, e.g. world `uEnv` 1->0, `uShadow` 0->1,
  `uDetail` 6->2; UI `uFont` 3->0; post `uTex` 4->0, `uBloom` 5->1.
- The latest build's `xenia_scan` did not list `lookAt`, `perspective`,
  `drawCar`, `setCommon`, or a named matrix-builder function. It did flag
  `UI::text` (`rldic r10,r10,3,57`), `World::decorate` (`rldic r4,r4,47,1`),
  and `update` (three `rldic` instructions); Xenia silently skips these, so
  the UI result in Xenia is not conclusive.
- The c85f2bd RGH log stopped after repeated `persistent-stream-buffer`
  free/alloc pairs as skid marks grew. `data(..., false)` now starts at 256 KiB,
  doubles capacity, and retires old blocks only after their last-use fence.
- `xenia-hwdiag.log` shows one persistent marks allocation of 262,144 bytes and
  no repeated reallocation during its 30-second run. It also shows ten
  `hardware-frame begin/end` pairs: each begin status is 0, and each 2-second
  fence wait returns status 0 / complete=1. The log calls are `xsdk_log_write`
  to xsdk's synchronous file handle.
- The user reports that 10668cf renders correctly in Xenia, but hangs on the
  RGH console in its first real frame: `present_begin=0`, then fence 548 returns
  `0xC00000B5` / incomplete, and subsequent frames time out. The preceding
  colour-only startup frames work. `xenon/fcfcar` draws and animates its car on
  the same console. Whole-group constant uploads are ruled out by the user.
- The previous full-game XEX was
  `xenon/build/game/fogcityfury.xex`; it reached frame 3 in Xenia, where
  unsupported instructions make visual results unreliable.
- Six draw-mask diagnostics are built by
  `XSDK_DRAW_MASKS=ON ./build_game.sh`. Files are named for their numeric mask:
  `draw_mask_0.xex` clear only; `draw_mask_1.xex` sky; `draw_mask_64.xex` UI;
  `draw_mask_144.xex` one player car; `draw_mask_6.xex` terrain and props;
  `draw_mask_95.xex` everything except marks, shadow patches, particles and
  the ring. Outputs are in `xenon/build/game/`.
- Each diagnostic logs `draw: <group> queued` immediately before its first-frame
  draw group. The existing `hardware-frame begin/end` fence diagnostics remain
  enabled for the first ten frames. The draw-group markers are limited to the
  first rendered frame to keep the synchronous log concise.
- The six mask builds change draw selection only; no framebuffer, texture,
  depth, stream, shader or draw-submission behavior was changed for this test.

## RGH First-Frame Investigation
- Frame order matches the xsdk samples: `plat::beginFrame` obtains a present
  address and `gfx::beginFrame` sets the screen target; `render` selects the
  screen target, configures clear-on-resolve and queues draws; `presentFrame`
  resolves, records a fence, then `plat::endFrame` calls `xsdk_present_end` and
  waits on that fence. `samples/triangle/main.c` and `samples/cube/main.c` both
  set a target, draw, resolve, then end presentation.
- `docs/GUIDE.md` says `xsdk_gpu_set_target` sets the colour target and its
  depth allocation, but does not enable the depth test. `samples/cube/main.c`
  explicitly calls `xsdk_gpu_set_depth_stencil` after `xsdk_gpu_set_target`.
  The game's normal sky path toggles depth off and back on; the isolated masks
  do not add a separate depth-state fix, per the test constraint. In particular,
  car-only and terrain/props-only may not have the same depth state as the full
  scene; report that alongside each console result.
- `gfx::drawFull` issues `xsdk_gpu_draw(XSDK_PRIMITIVE_TRIANGLE_LIST, 3)` without
  a vertex stream. xsdk implements that as an auto-indexed draw (`AUTO_INDEX` in
  `src/gpu_draw.c`); `samples/triangle` and `samples/cube` both use vertex
  streams, so neither is a hardware sample of the no-stream case. No stream
  reset was added, keeping the sky diagnostic focused on the existing path.
- `docs/GUIDE.md`, “Memory the GPU reads,” requires aligned GPU memory, volatile
  32-bit writes and invalidation after CPU writes. The game's stream arena copy
  uses volatile word stores and calls `xsdk_gpu_invalidate` before draws; this
  path is retained in the UI diagnostic.
- Pass 0 still binds the existing shadow D24S8 texture and environment cube,
  although that pass does not render either texture first. No fallback texture
  was introduced, so the masks also preserve this suspected game-vs-`fcfcar`
  difference for the requested console comparison.
- Clear-only issues no draw calls. As the guide notes, the first frame after
  `xsdk_gpu_open` resolves whatever was already in eDRAM; a resolve then clears
  eDRAM for the next frame. Startup's colour-only diagnostic frames run before
  the first game frame.
- RGH logs in `xenon/build/game/` show clean first-frame fences for masks 0
  (clear only, fence 551), 1 (sky, fence 550), 6 (terrain+props, fence 551),
  and 144 (single car, fence 550). Their runtime status lines continue to show
  `present_begin=0` and `fence_complete=1`; mask 1 exits with status 0.
- Mask 95 queues sky, terrain+props, cars, wreckage+pickups, water and UI, then
  its first fence 551 times out with `0xC00000B5`; subsequent begin/fence calls
  also time out. The queued markers are CPU-side and do not identify which GPU
  draw stalled. There is no `draw_mask_64.log` in the workspace, so the UI-only
  test is not yet accounted for. The logs alone cannot establish whether a
  passing frame displayed the expected picture.
- These results rule out the clear, sky, terrain+props and single-car groups as
  standalone causes of the fence hang. Mask 95 additionally includes UI,
  water, wreckage/pickups and interactions between groups; those remain
  unresolved. Do not alter rendering behavior beyond draw-mask gating until
  the missing UI result and visual reports arrive.
- Unfixed xost GLSL math sites: `FS_COMMON::hash` uses `sin(dot(p, ...))` on
  world/cloud coordinates (line 37); `FS_WATER` uses `sin(depth*9-uTime*2.5)`
  (line 290); `FS_RING` uses `sin(uTime*10)` (line 339); `FS_POST` hashes cell
  coordinates through `sin(dot(id, ...))` (line 366). `mod` is used with
  world coordinates in `FS_WORLD` (lines 123, 133), block parity (150), and
  checker UVs (211), where exact multiples may be wrong. No shader `cos` or
  `atan` calls were found. These are recorded only; not fixed yet.
- The conservative 262,140-byte fetch cap has been built and run in Xenia; it
  reaches frame 3. The user reports it did not change the corruption.
- `xenon/build/game/fogcityfury_objects.xex` cycles terrain, props, water, and
  cars every three seconds; its log says which is active. Pass 0 draws UI after
  the direct scene path, and sampler units are rebound before each draw, but
  title text still needs visual confirmation.
- `xenon/build/game/xenia-objects-final.log` and
  `xenia-objects-final-console.log` contain the latest object-test run. Modes
  0 terrain, 1 props, 2 water, and 3 cars all logged repeatedly. Startup reaches
  frame 3. Pixel screenshots must come from the user's Xenia window (this
  environment cannot capture its Vulkan surface).
- The LUT workaround is reproducible: `build_game.sh` applies the patch under
  `xenon/patches/` to `~/xsdk`; Xenia does not emulate display register 0x1928.
- XEX path: `xenon/build/game/fogcityfury.xex`.
- Sound, saves, and network remain disabled/not implemented on the 360.
- Real RGH 360 testing is in progress; wait for the user's visual result before
  changing rendering behavior beyond diagnostic draw masks.
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
