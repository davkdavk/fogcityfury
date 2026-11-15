# Working on Fog City Fury

This file is for AI models (and people) picking the project up cold. Read it first; it tells you how
the code is laid out, how to check your work without a screen or speakers, and what will bite you.

Fog City Fury is an arcade street racer for Linux: C++17, SDL2, OpenGL 3.3 core, GLSL 3.30, with
UDP multiplayer through a dedicated server. About 2,250 lines in nine source files. There are no asset files: terrain, tracks, cars, textures, sound
effects and the soundtrack are all generated in code.

## Start here

    sudo apt install g++ make pkg-config libsdl2-dev libgl1-mesa-dev   # build
    sudo apt install xvfb libgl1-mesa-dri imagemagick                  # only for screenshots
    make            # builds ./fogcityfury and ./fogcityfury-server (about 20 s)
    make test       # 26 headless tests, about 40 s, no display or sound card needed
    make test-render  # the same plus rendering smoke tests under xvfb (about 2 min)
    make windows    # cross-compile fogcityfury.exe and fogcityfury-server.exe (see "Windows build")

Run `make test` before and after every change. It must stay green. If you change behaviour on
purpose (handling numbers, a track layout), update the affected thresholds in the same change and
say why.

Further reading, in order of usefulness:

- `docs/TESTING.md` - every test hook, the JSON report, how to look at the game without a display.
- `docs/ARCHITECTURE.md` - how a frame is simulated and drawn, how a world is generated.
- `docs/HANDLING.md` - the tyre model, what each number does, measured behaviour per car.
- `docs/NETWORK.md` - multiplayer: who decides what, the wire format, server options.
- `README.md` - the player-facing description and controls.

## Nobody has played this build

Everything so far was written and checked by a model with no screen, no speakers and no hands on a
controller. Verification has been: the test suite, screenshots from Mesa's software renderer, and
numeric analysis of rendered audio. That means:

- **Feel is unverified.** Handling, camera motion, difficulty and opponent pace are tuned to numbers
  and to a description of what should happen, not to someone driving. Treat player feedback as
  ground truth over anything in these docs.
- **Sound is unverified by ear.** The soundtrack's levels, rhythm and tuning were measured; whether
  the guitars sound good was not.
- **Multiplayer has only run on loopback.** No real latency, loss or more than two game clients.
  (The Windows build has been run on a real Windows PC by the user and works.)
- **It has never run on real GPU hardware.** Only Mesa llvmpipe. Driver-specific GLSL strictness
  is a likely first-run failure; shader compile errors print to stderr and exit.

When the user reports something, reproduce it with a scripted run first (see TESTING.md), fix it,
then add a test that would have caught it.

## Source map

| File | Lines | What is in it |
|---|---|---|
| `src/glload.h` | 35 | OpenGL headers; on Windows, the list of post-1.1 functions and their loader |
| `src/base.h` | 235 | `V3`/`M4` maths, noise, `Prog` (shader + uniform cache), `MB` mesh builder, `Mesh`, pixel font, `UI` batch, BMP writer |
| `src/shaders.h` | 250 | All GLSL as string constants: shared fragment prelude (sky, fog, shadows, tonemap), world, car, water, particles, skid marks, post chain, UI |
| `src/world.h` | 447 | `TrackDef` data for the four tracks, `World`: terrain generation, road stamping, colliders (`ground`, `collide`), buildings, ramps, trees, the secret builders (`rooftop`, `courtyard`, `island`, `waterJump`, `pier`, `summit`, `garage`), `bridge` and `decorate()` which places them |
| `src/cars.h` | 130 | `buildCar` (lofted body, glasshouse, details) and `buildWheel` |
| `src/audio.h` | 130 | Engine/tyre/wind synthesis, one-shot `tone()`, the `mus` namespace (soundtrack sequencer and instruments), offline WAV render |
| `src/net.h` | 65 | Multiplayer wire format and UDP helpers, shared with the server |
| `src/netclient.h` | 80 | Multiplayer client: connect, send our car, receive snapshots, dead-reckon remote cars |
| `src/server.cpp` | 140 | The dedicated server (`fogcityfury-server`); builds without SDL/OpenGL |
| `src/game.cpp` | 840 | Car specs, `stepPlayer` (physics), `stepAI`, autopilot, `update` (race rules, effects, sound), `drawScene`/`render` (passes, camera, post), HUD, menus, `main` (flags, sim/shot/live loops) |
| `tests/run_tests.py` | 580 | The test suite |
| `tools/shot.sh` | - | Headless screenshot helper |

`game.cpp` is a single translation unit that includes the headers in order; `cars.h`, `audio.h` and
`netclient.h` are included part-way down because they use things defined above them. `server.cpp`
is a second, separate program that includes only `net.h`. There is no separate
compilation and no build system beyond the Makefile.

Style: dense, long lines, several statements per line. Keep to it within a file rather than
reformatting; diffs stay readable that way. Comment the why, not the what.

## Conventions you must not get wrong

**Axes.** Y is up. The world is a 1536 m square, X and Z from -768 to 768 (`HALF`), on a 513 x 513
height grid with 3 m cells (`CELL`). Sea level is y = 0. Each track is an island about 600 m in radius.

**Heading.** `yaw = 0` faces +Z. Positive yaw turns towards +X. Forward is `(sin yaw, 0, cos yaw)`.
The car's *left* is `(cos yaw, 0, -sin yaw)`, which is +X at yaw 0. So:

- steering input +1 = left, yaw rate positive = turning left;
- lateral velocity `vs` is positive when the car moves to its left;
- car model space is +Z forward, +X left, origin on the ground under the centre;
- track cross-section: lateral offset positive = left of the direction of travel.

The handedness looks odd if you expect +X to be right. Every sign in the physics and camera depends
on it; `handling_left_right_symmetric` in the tests asserts that positive steer turns towards +X.

**Angles wrap.** `yaw` is kept in (-pi, pi]. Never blend or subtract headings directly; use
`wrapPi(a - b)`. The first camera bug in this project was exactly that (a `lerp` between a heading
and an `atan2`), and it only showed on parts of the lap.

**Units.** Metres, seconds, m/s, radians. Gravity is 18 m/s^2 on purpose (arcade). Speeds on the HUD
are `vf * 3.6`.

**Top-down debug view** (`--top`): image right is +X, image down is +Z.

**Two-sided shading.** Face culling is off everywhere. Fragment shaders flip the normal when
`!gl_FrontFacing`. Flat-shaded geometry from `MB::quad`/`tri` therefore needs no winding care, but
anything with *smooth* normals must have its triangle winding agree with those normals, or it will
be lit inside-out. `emitGrid` in `cars.h` handles this; copy it if you add smooth meshes.

**Materials** are a per-vertex float `mat`, switched on in the fragment shader:

- world shader: 0 terrain, 1 road (uv = lateral metres, distance metres), 2 building wall with
  windows (uv = metres along wall, metres above street), 3 flat colour, 4 checker, 5 hazard stripes;
- car shader: 0 plain, 1 paint (colour comes from the `uPaint` uniform), 2 glass, 3 headlight,
  4 tail light, 5 rubber.

**All lit shaders output linear HDR** (`uHDR = 1`); the post pass tonemaps. Do not tonemap in a
scene shader.

**GLSL portability.** `smoothstep(a, b, x)` with `a > b` is undefined; write `1.0 - smoothstep(...)`.
Every sampler uniform must be set to its own texture unit (0 shadow map, 1 reflection cubemap,
2 terrain height texture, 3 font, 4/5 post) or draw calls fail on strict drivers.

**Determinism.** The simulation uses its own RNG (`grng`) and fixed 1/60 s steps in `--sim` and
`--shot` modes, so two runs give identical reports. Do not call `rand()` or read the clock in
simulation code. Particles consume `grng`, so adding a spawn changes later random numbers - expect
small shifts in autopilot lap times, not in the handling tests (those spawn nothing that matters).

## Windows build

The Windows client and server are cross-compiled from Linux with MinGW-w64; nobody builds on Windows.

    sudo apt install g++-mingw-w64-x86-64 wine64
    # unpack SDL2-devel-2.x.y-mingw.tar.gz from github.com/libsdl-org/SDL/releases, then:
    make windows SDL2_MINGW=/path/to/SDL2-2.x.y/x86_64-w64-mingw32
    WINEDEBUG=-all wine ./fogcityfury.exe --sim --state 3 --auto --seconds 60     # should print (almost exactly) the same JSON as ./fogcityfury
    python3 tests/run_tests.py --windows                                           # the parity + cross-platform network test

The executables are fully static: they import only DLLs that ship with Windows. Three things differ
by platform, each in one place:

- **OpenGL functions** (`src/glload.h`). `opengl32.dll` exports only OpenGL 1.1, so on Windows every
  newer function is a pointer fetched after the context is created. **If you call a GL function
  that is not already used, add it to `FCF_GL_FUNCS`** or Linux will build and Windows will not.
- **Sockets** (`net.h`: `init`, `closeSock`, `sleepMs`, `prepare`). Use those, not `close`/`usleep`/`fcntl`.
- **Save file** location (`savePath` in `game.cpp`): `%APPDATA%\fogcityfury.sav`.

`windows.h` is included first on Windows (`base.h`) with `NOMINMAX`, and its `near`/`far` macros are
undefined. The Windows client is a GUI-subsystem program: stdout only appears when redirected.

Verified under Wine 9 only: the simulation matches Linux to within rounding (the C maths libraries
differ in the last bit, so do not expect byte-identical reports), rendering works, and Windows
and Linux clients and servers interoperate. It has not run on real Windows or a real Windows GPU driver.

## How to do common jobs

**Tune how a car drives.** `SPECS` at the top of `game.cpp`, then the lateral block in `stepPlayer`.
Read `docs/HANDLING.md` first. Measure with the flat-plane scripts in TESTING.md; lock the result in
`tests/run_tests.py`.

**Add a car.** Add a `CarSpec` row, a paint is not needed. Add a branch with body/cabin key sections
in `buildCar`. Bump the `3`s: `SPECS[3]`, `carMesh[3]`, the `% 3` in menus and AI setup. Add it to
the per-car loops in the tests.

**Add or move a hidden key.** `World::decorate()` in `world.h` is four short lists (one per track) of calls to the
secret builders. Each builder makes its scenery, finds the shoreline itself where it needs one,
reserves its approach (`lane`, `addExcl`) and registers the key *with a recipe* (`addKey`): the start
pose, waypoints and speed a scripted driver needs to collect it. The tests and `--keyrun K` use
those recipes, so moving a secret needs no test change - run `python3 tests/run_tests.py keys`.
Exactly `NKEYS` (5) keys per track (the last is always `garage`, a building with an open door that finds its own plot beside the road), in a stable order: key k is bit k in the save file. To invent a
new kind of secret, write a builder next to the others and end it with `addKey`. Check placement
from above with `tools/shot.sh map --track N --state 3 --mode 1 --top 1330 0 0 --noui`, and
`--sim --track N --dump` prints where everything ended up.

**Add a track.** Push another `TrackDef` in `initTracks()`: control points of a closed loop (x, z
pairs), gaussian hills, palette, `night` (1 turns on lit windows, stars and the headlight cone).
Road height follows the terrain, so a hill on the centre line becomes a jump (amplitude 12-17,
sigma about 40) and a negative hill becomes a bay the road crosses on a causeway. Bump `NTRACKS`
in `world.h` and `net::TRACKS` in `net.h` (a `static_assert` ties them), add a `decorate()` branch
with five secrets, a race test, and check `boot_reports_valid_state` (tracks are 3.1-3.9 km). Keep
control points within about `landR`: beyond it the terrain falls into the sea. The save file is
versioned (`v2`) and sized by `NTRACKS`; an old-format save is ignored.

**Change the weather or nitro.** `WX[]` and the `NITRO_*` constants at the top of `game.cpp`, the
nitro block in `stepPlayer`, the effects in `update()`. `docs/HANDLING.md` explains the one
non-obvious part (why the traction circle divides out the weather's grip). The weather a player
picks for each track is saved (`v3` save file) and, online, comes from the server.

**Change the music.** `mus` namespace in `audio.h`. Riffs are strings, one character per sixteenth
note (see the comment on `Bar`). `--wav out.wav 60` renders it; `FCF_SOLO=1|2|4|8` solos guitars,
bass, drums, lead. `audio_soundtrack_renders_sane` checks level, stereo and that a rhythm exists.

**Add a screen effect.** `FS_POST` in `shaders.h`, uniforms set at the end of `render()`.

**Add a HUD element.** `drawHUD()`. Coordinates are a 720-high virtual screen, width `ui.W`.
The font only has ASCII 32..90 (upper case, digits, basic punctuation).

## Pitfalls that have already cost time

- A trailing `// comment` on one of the long multi-statement lines comments out the rest of the
  line, including closing braces. Put comments on their own line or at the true end of the line.
- The Makefile depends on `src/*.h` by wildcard. If you add a source directory, update it, or edits
  will silently not rebuild.
- Online, `raceLaps` (from the server) replaces the `LAPS` constant and `netRace` switches off the
  clock, AI and pausing. If you add a race rule, decide what it does when `netRace` is set.
- Changing any struct in `net.h` changes the wire format: bump `net::PROTO`, update the Python
  mirror at the top of the multiplayer tests and the tables in `docs/NETWORK.md`.
- `Mesh::upload` and the height texture are skipped when `g_noGL` is set (`--sim`). Any new GL call
  reachable from `World::build` or `update()` must be guarded the same way or `--sim` will crash.
- Render terrain (`hr`) is lowered slightly under the road ribbon so the two do not z-fight; physics
  uses `h`. Do not "fix" the difference.
- A car on a kerb-like step must not be launched: see the `rise > hs * dt * 0.75f` test in
  `stepPlayer`. Roofs and ramps count as ground only if the car is already above them.
- The autopilot (`autoInput`) is a test and attract-mode driver, not an opponent. It cannot reverse;
  a rescue in `update()` respawns it if it is stuck for 2 s.
- Opponents (`stepAI`) run on rails along the centre line and never leave the ground.
- `shake`, `flashT`, `caT`, `slowT` and friends are global feel state decayed at the end of
  `update()`. If you add one, decay it there.
- The software renderer is slow (0.3-1 s per frame). Use `--skip N` to simulate frames without
  drawing them, and `--sim` whenever you do not need a picture.

## Known gaps and good next tasks

- Opponent pace versus a human is a guess (`21.0f` lateral limit and `aiTop` in `setupCars`). The
  autopilot always finishes 6th, which says the bot is slow, not that the AI is right.
- Opponents do not collide with each other, do not jump, and all share the player's reflection map.
- Multiplayer trusts clients (see `docs/NETWORK.md`); remote cars are extrapolated, not
  interpolated; no chat, voting or server browser.
- Opponents (`stepAI`) do not use nitro; the player's is the only one, apart from other humans online.
- Rain is a particle effect around the player, so it does not fall behind buildings properly and
  there are no puddles or windscreen drops.
- No difficulty setting, no split-screen, no replays, no options menu (volume, key rebinding).
- The two-roof rooftop keys need a slow, precise run (20 m/s); faster cars overshoot the first roof.
- Summit Switchbacks is 3.27 km, a little shorter than the others, and mostly open hillside.
- On the night track only the player's own headlights light the road.
- Keyboard throttle is on/off, which makes low-gear power slides in the Stallion easy to provoke.
  A throttle ramp for digital input would help.
- Brakes peak at 26 m/s^2 after a 0.3 s build-up; whether that is "not acute" enough is the
  player's call.
- No level-of-detail: the terrain is one 526k-triangle mesh drawn in the shadow, cubemap and main
  passes. Fine on a discrete GPU; unknown on integrated graphics.
- Front-tyre skid marks, brake-light glow on opponents, wind-screen reflections in bonnet view.

## Added later: bridges, alleys, wrecks, rockets (read before touching them)

- **The road can cross itself.** `World::build` finds every self-crossing of the centre line and
  lifts the *later* pass over the earlier one (`BR_LIFT` 7.5 m, climb `BR_LW` samples each side,
  deck `BR_SPAN` samples each side; `W.cross` holds the middle sample of each bridge, `TS::br` marks
  deck samples). A track with a figure-8 control polygon gets its bridge automatically. Deck samples
  are not stamped into the height field; the deck is a chain of `Ramp`s with `thick` set (open
  underneath) plus railing and pier `Box`es. Consequences, all already handled - keep them that way:
  `W.nearest(x, z, y)` must be given the height wherever a car is looked up (otherwise a car under
  the bridge is matched to the deck); road height is `ts[i].y` / `W.at()`, never `W.H()`;
  `collide()` skips boxes whose base and decks whose underside are above the car; checkpoints
  compare height too. `rd` (distance to road) includes deck samples, the internal `rdh` does not.
- **Alleys.** Random buildings sit on a 27 m grid and are clamped so an 8 m lane is always free
  between neighbours. The terrain shader paves city ground (vertex `u` = 1) with the same grid
  (`mod(x + 660, 27)`); if you change the grid in `world.h`, change the shader too.
- **Wrecks** (`Wreck`, `Debris`, `leaveWreck`, `stepWreckage`, `drawWreckage` in `game.cpp`): any
  `explode()` and any rocket hit leaves a shell (solid, radius `WRECK_R`, max 14) and parts (max
  ~120, kicked by the player). `dodge()` lets the autopilot and the rail opponents pass a wreck;
  `respawn()` moves across the road if a wreck is on the spawn point. The car shader's `uBurnt`
  (1 burnt, 2 burnt with embers) must be reset to 0 after drawing them.
- **Rockets** (`Rocket`, `Crate`, `pickTarget`, `fireRocket`, `stepRockets`, `rocketHit`): target
  ids are 0..NAI-1 for opponents and 16+id for online players. Opponents hit are `dead` for 3 s
  (`stepAI` is skipped). Online hits travel as `hitId`/`hitSeq` in `CarState` (docs/NETWORK.md).
- **Reset** is `doReset()` (R, Backspace, gamepad Back, also from pause). `resetHint` drives the
  on-screen reminder.
- **Wet road.** `FS_COMMON` now declares `uEnv` (cubemap, unit 1) for every lit shader and
  `setCommon` sets it for all of them - an unset sampler defaults to unit 0, collides with the
  shadow sampler and silently stops the draw call. `uUseEnv` is 0 while the cubemap itself is being
  rendered (`envPass`). The road branch builds its bump normal from `roadGrain` and `ripple`.
- Test flags: `--rockets N`, `--fire T`, `--reset T`; report fields in docs/TESTING.md.

## Added later: options, key bindings, video settings

- State `OPTS` (8) is the options screen: page 0 controls, page 1 video. `viewState()` gives the
  state behind it (title or pause). Opened with F1 in `handleKey`, which is the single entry point
  for every key press (window events and `--press`); it also handles F11 and the "press a key"
  capture before the menu code sees anything.
- Bindings: `binds[NBIND][2]` holds SDL scancodes (0 = empty), defaults in `BIND_DEF`. `readInput`
  and the in-race keys go through `binds`/`bindHit`; never test `SDLK_r` and friends directly.
  Gamepad buttons call the actions directly and are not rebindable. To add an action: extend
  `enum Bind`, `BIND_NAME`, `BIND_DEF`, and use it.
- Video: `vidFull`, `vidW/H`, `vidAA`, `vidAF`, `vidVsync`; `applyVideo()` pushes window mode,
  size, swap interval and anisotropy; `render()` rebuilds its targets when the sample count or
  render size changes. The scene is drawn at `rw x rh` (window size, or the chosen resolution
  scaled to the screen's shape when full screen) and the last post pass scales it to the back
  buffer; the HUD is drawn at native size afterwards. There is no display-mode switching.
- Anisotropic filtering acts on `detailTex` (unit 6, `uDetail` in the world shader): a generated,
  tileable, mipmapped grain texture used by the road and the ground. Purely procedural shader
  detail cannot be anisotropically filtered, so keep fine ground detail in that texture.
- Settings live in `fogcityfury.cfg` beside the save file (`loadCfg`/`saveCfg`, format `v1`); not
  read or written with `--nosave`. Command-line `--aa/--af/--renderh/--size/--fullscreen` override
  for one run without saving.

## Added later: backstreets, wreck lifetime, rockets per lap

- The backstreet grid is `World::GRID`, `GRID0`, `alleyLine(k)`, `alleyDist(v)`; the terrain shader
  repeats it (`mod(vP.xz + 660, 27) - 13.5` is the offset from the nearest centre line) and paints
  tarmac, gutter, kerb and paving from it. `cityGround()` says where that applies. Street lamps
  stand at every other crossing and the shader lights the ground under them at night; dumpsters
  (solid) hug the walls. Furniture uses its own `Rng` so adding some never moves buildings or trees.
- `garage()` first looks for a backstreet that leaves the main road and puts the `warehouse()`
  across its end, two to four blocks in (`W.keyStreet`, also the shader's `uKeySt`, and
  `lampTrail()` marks it with orange lamps). Only if a track has no suitable town does it fall back
  to a plot beside the road.
- Wrecks and their parts live `WRECK_LIFE` (10 s) and dissolve over the last `WRECK_FADE` seconds
  (`uGone` in the car shader, a dither discard so it needs no blending or sorting).
- Rockets: one set per lap. `rocketLap` is the lap on which a crate was last taken; `cratesOn()`
  hides every crate until `player.lap` changes.
- Every random building is centred on its lot with half-size `World::LOT` (or 1.2 m less), so walls are
  parallel to the kerb the shader draws at 9.6 m from the lot centre. Do not reintroduce position
  jitter: it makes the street surface look misaligned. The warehouse plot (`W.keyLot`, shader
  `uKeyLot`) is painted as one yard with no street through it. `--dump` lists `onStreet`: buildings
  that reach past a kerb; the boot test requires it to be empty.
- Cars on the ground: `World::H` interpolates each grid square as the same two triangles the terrain
  mesh draws (`sampleTri`), `Car::wy[4]`/`ride` let each wheel reach the ground under it (drawing
  only), and `drawScene` lays a soft dark patch under every car (`pMark` with `misc.y = 1`). Report
  fields `maxGap` (driven ground vs drawn ground, should stay ~0) and `offroadT`.
- Wet off-road wheel effects (water mist, mud clods, grass) are in the effects block of `update()`
  next to the wet-road spray; dry dust only when it is not wet.
- Starting grid: `gridS(k)`/`gridLat(k)` in `world.h` give slot k (0 = pole; the player takes slot
  `NAI`, online players their join order). `World::build` paints a box per slot (`GRID_SLOTS`).
  During the countdown `stepAI` holds each opponent's lateral position; `gridDrift` in the report
  must stay ~0. `--countdown` keeps the real countdown with `--state 3`.
- `World::ground()` uses the drawn terrain height (`drawnH`) off the road ribbon, because the mesh
  dips under and just beside the ribbon; do not change that dip without looking at the road edges
  from above (the terrain pokes through the kerbs).
- Race intro: `introT` (`INTRO_LEN` s) is set by `startRace`; `render()` blends from a view straight
  above the grid to the normal camera, and the offline countdown does not tick until it is over
  (online it overlaps the server's countdown). Enter, Space or the accelerate key skip it. `--state 3`
  skips it unless `--countdown` is given. Report field `intro`.

- Server list (`ServerEntry`, `srvOpen/srvPump/srvAsk/srvLookup` in `game.cpp`): the `NETJOIN`
  screen. `srvMode` 0 picks, 1 types a new address, 2 types the player name. Defaults in
  `DEFAULT_SERVERS`; saved as `servers N` / `server host` / `name` lines in the cfg. `--server H`
  (repeatable) replaces the list for one run; report field `servers` is `host|status|players|track|name;...`
  with status 0 looking up, 1 asked, 2 answered, 3 no reply, 4 address not found. Headless runs
  sleep 16 ms a step while on the online screens so real servers have time to answer.
- OpenGL ES / Android: `docs/ANDROID.md`. `make gles` builds `fogcityfury-gles` (GLES 3.0 + phone
  settings) on Linux; keep it rendering - it is the only test of the Android renderer here. Shader
  rules that follow: no implicit int-to-float conversions, no GLSL ES reserved words as names (`patch`
  bit once), nothing that is not in both GLSL 3.30 and ES 3.00. `FCF_DUMP_SHADERS=dir` writes the
  shaders as compiled, for `glslangValidator`. Render targets fall back to 8-bit when half-float ones
  are not renderable (`hdrOK`, `--ldr` to force it).
- Touch: `touchLayout()` lists the buttons for each state (hold buttons feed `readInput` through
  `touchHolds`, tap buttons go through `handleKey` or call the action). Menus hide their keyboard
  hints when `touchOn`. Test flags `--touch`, `--tap T:LABEL`, `--touchhold LABEL`; report fields
  `touch`, `touchBtns`, `hdr`.
