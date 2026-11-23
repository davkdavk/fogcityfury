# Architecture

Two executables. The game is one translation unit (`src/game.cpp` including its headers in order; since 2.26 its own parts are
`serverlist.h`, `settings.h`, `combat.h`, `driving.h`, `update.h`, `scene.h`, `hud.h` and `input.h`) with
global state and no dependencies beyond SDL2 and OpenGL. The dedicated server is `src/server.cpp`
plus `src/net.h` and depends on nothing; `docs/NETWORK.md` covers it. Read `AGENTS.md` first for conventions.

## Program modes

`main()` parses flags, then takes one of four paths:

1. `--wav`: render the soundtrack to a file and exit. No SDL at all.
2. `--sim`: set `g_noGL`, build the world, call `update(1/60)` N times, print JSON, exit. No SDL.
3. `--shot`: create a window and GL context (under xvfb), simulate, draw, write a BMP, exit.
4. Otherwise the live loop: poll events, `update(dt)`, `render()`, swap.

All four share `startGame` (a lambda in `main`): `initTracks`, load save, `W.build(track)`,
`setupCars`, apply `--state`/`--pos`/`--speed`.

## Game state

`enum State { TITLE, CARSEL, TRACKSEL, RACE, PAUSED, RESULTS, NETJOIN, NETLOBBY }` in one global `state`. `onKey()`
handles every menu transition and the in-race keys (pause, camera, reset, music). Gamepad buttons
are translated into the same key codes.

The title screen is the real game with the autopilot driving. Car and track select show the real
world with an orbiting camera. There is no separate menu scene.

Globals worth knowing: `W` (the world), `player`, `ai[5]`, `parts` (particles), `marks` (skid-mark
vertices), `snd` (parameters read by the audio thread), `tm` (test metrics), and a block of feel
state (`shake`, `flashT`, `caT`, `slowT`, `fovKick`, `shiftCut`, camera springs).

## update(dt)

Called once per frame (or per 1/60 s in sim/shot modes). In order:

0. `netPump` (send our car, read snapshots, move remote cars) and `netReact` (follow the server's phase).
1. Choose the input: keyboard/gamepad, or the autopilot, `--script`, `--waypoints`.
2. Physics in 120 Hz substeps: `stepPlayer`, then each opponent's `stepAI` and a circle-vs-circle
   push between it and the player.
3. Race rules: checkpoint and lap detection (eight gates, passing within `ROADW + 16` m), the
   countdown clock (45 s, +11 s per checkpoint; off online), progress and place, wrong-way detection, finish.
4. Key pickup (3.4 m sphere), unlock, save.
5. Effects that depend on the car's state: smoke, dust, spray, skid marks, exhaust flames, gear and
   rpm, sound parameters.
6. Test metrics.
7. Particle integration; decay of the feel state.

Race progress for ranking is in metres along the centre line: opponents use their rail distance,
the player uses the nearest centre-line sample plus laps, corrected around the start line.

### stepPlayer

See `docs/HANDLING.md` for the tyre model. Around it:

- Ground contact comes from `W.ground(x, z, y)`: terrain height, or a roof/ramp if the car is
  already above it. Vertical velocity follows the ground while in contact (`rise / dt`), which is
  what launches the car off crests and ramps; a step taller than the slope limit lifts the car
  without adding velocity.
- `W.collide()` pushes a 1.15 m circle out of building and ramp walls and returns the closing speed.
  Above 30 m/s the car explodes (`explode`), waits, and `respawn`s on the nearest road sample.
- Terrain below -1.1 m with the car under water sinks it; same respawn.
- Body attitude is from four ground samples at the wheel positions; the visual-only suspension
  (heave spring, squat, lean, wheel droop) is layered on top and never feeds back into the physics.

### stepAI

Opponents advance a distance `stot` along the centre line at a target speed limited by the
curvature ahead and a rubber band on the player's progress, with a slowly wandering lateral offset.
Position, pitch and roll come from the terrain. They cannot crash, jump or leave the road.

## World generation (world.h)

`World::build(id)` runs once per track load, about 0.3 s:

1. **Height field** `h`: 513 x 513 samples at 3 m. `baseHeight()` = base level + noise + gaussian
   hills, minus a falloff beyond `landR` so the map is an island.
2. **Centre line**: closed Catmull-Rom through the control points, resampled to one sample per
   metre (`ts`: position, tangent, curvature). Road height = terrain height under it, floored at
   2.2 m (so it becomes a causeway over water), smoothed over 11 m.
3. **Stamp**: every grid point within 42 m of the road is blended towards the road height; `rd`
   stores distance to the road. `hr` is `h` lowered a little under the ribbon, for drawing only.
4. **Road ribbon**, kerb skirts, checkpoint gates into the prop mesh builder.
5. **decorate()**: five secrets per track (the fifth, `garage`, places itself), each a call to a builder (`rooftop`, `courtyard`,
   `island`, `waterJump`, `pier`, `summit`). Builders reshape terrain with `flatten` (which never
   touches the road corridor), find the coast with `shore`, add buildings and ramps, reserve their
   approach with exclusion circles, and register the key plus the scripted route to it (`Recipe`).
6. **Random buildings** on a 27 m grid where the city noise mask or road proximity allows, never on
   the road, on steep ground, in water or inside an exclusion circle. Each is a collider `Box` in a
   48 m spatial grid.
7. **Trees** (decoration only, no collision).
8. Upload: props mesh, terrain mesh with per-vertex colour, a water quad, and `h` as a float texture
   (the water shader reads depth from it).

Colliders: `Box` (oriented, with a top height you can land on) and `Ramp` (oriented wedge). Both are
used by `ground()` and `collide()`. `World::flat` short-circuits all of it for handling tests.

## Rendering (render() and drawScene())

Per frame:

1. **Camera.** Menus orbit the car. In a race the chase camera is a set of first-order springs: yaw
   towards a blend of heading and travel direction, distance pushed by acceleration, height lagging
   on jumps, roll into corners, a dip on landing, jitter from `shake`, speed and off-road.
2. **Shadow map**: 2048^2 depth, orthographic, 105 m around a point ahead of the car. Terrain,
   props and cars.
3. **Reflection cubemap**: 256^2 RGBA16F around the player's car, two faces per frame (all six on
   the first frames and in shot mode), then mipmaps. Sky, terrain, props and water only.
4. **Main pass** into a 4x MSAA RGBA16F target: sky (full-screen triangle), terrain, props, skid
   marks, cars, keys, water (blended), particles (premultiplied, no depth write).
5. **Post**: resolve, bright pass to quarter size, two blur iterations, then the composite - radial
   speed blur, chromatic aberration, bloom, tonemap (ACES fit), vignette, desaturation while
   wrecked, flash.
6. **HUD** straight to the back buffer with the UI batch.

Night tracks (`TrackDef::night`) set `uNight`: building windows become emissive, the sky gains
stars, and `headlights()` in `FS_COMMON` adds a cone of light from the player's car to world
geometry.

Every lit fragment shader is `FS_COMMON` + its own body. `FS_COMMON` has the procedural sky
(`skyColor`), `ambient`, PCF `shadow`, `applyFog` and `finish`. The sky function is also what the
water and windows reflect.

Vertex format for all meshes is `Vtx`: position, normal, colour, uv, material (12 floats).
Particles and skid marks use a second format: position, uv, rgba, two extras (11 floats).

## Audio (audio.h)

One SDL callback, stereo float at 44.1 kHz. It mixes:

- engine: two saws and a sine through a low-pass that opens with throttle, pitch from `snd.engF`;
- tyres and wind: filtered noise scaled by `snd.skid` and `snd.wind`;
- up to 12 one-shot `Tone`s (sine, square or filtered noise with a pitch slide), queued by `tone()`
  under the audio lock;
- the soundtrack, `mus::render`: a swung sixteenth-note sequencer at 94 BPM driving a gliding synth
  bass, a four-voice electric piano (ninth chords, panned slowly from side to side), synthesised
  kick, snare with a clap, closed and open hats, and a high sliding sine lead with vibrato and an echo.

The game thread only writes the handful of floats in `snd`; the sequencer lives entirely in the
audio thread.

## Persistence

`~/.fogcityfury.sav`: one text line - `v2`, then key bitmask and best lap for each track.

## Later additions

Bridges, alleys, wreckage, rockets, the reset key and the wet-road shading are summarised in the
last section of `AGENTS.md`; the rocket wire format is in `docs/NETWORK.md`. In the frame order
above, `update()` now also runs crate pick-ups, `stepRockets` and `stepWreckage` after the race
bookkeeping, and `drawScene` draws `drawWreckage` (shells, parts, rockets, crates) with the car
shader right after the cars, in the main pass and the shadow pass.
