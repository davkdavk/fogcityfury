# The A12 Engine: making another game from it

Fog City Fury is a racing game built on the **A12 Engine**, a small, dependency-free 3D game engine that lives
in this repository alongside the game. This guide says what is reusable, what is Fog City Fury's own, and how to turn the one into something else: a horse-riding
game, a delivery game, a boat game, an exploring game.

Read `AGENTS.md` first (axes, headings, units and the conventions every sign depends on), then
`docs/ARCHITECTURE.md` (how a frame runs). This document assumes both.

## What the A12 Engine gives you

- **Runs on** Linux, Windows, Android, and the Xbox 360 (homebrew, xsdk toolchain). The only dependencies
  are SDL2 and OpenGL / OpenGL ES; the 360 uses neither.
- **A generated world.** An island from a short description (road points, hills, islands, colours, sky):
  terrain, a road with kerbs, a town on a street grid, trees, sea, bridges, undersea tunnels, shortcuts,
  jumps, weather, day and night.
- **A track editor and a file format** for that description (`.fcft`, plain text). Players can make worlds
  and share them online.
- **Vehicle physics** with a tyre model, airtime, crashes and respawning. **People on foot** with their own
  movement, animation, fighting and AI. **A procedurally animated walker** (the Warden's legs).
- **Synthesised sound and music.** No sound files: every effect and the soundtrack are generated in code.
- **UI:** a pixel font, panels, menus, gamepad, keyboard and touch, rebindable controls.
- **Online play:** a headless dedicated server, a UDP protocol, a server list, host-run bots, and track
  downloads.
- **A test harness:** the whole game runs headless and deterministic, prints a JSON report, and can take
  screenshots. Most of what you change can be checked by a script.

## The layers

| Layer | Files | Part of the A12 Engine? |
|---|---|---|
| Platform | `platform.h` (window, input events, timing, threads, audio device, files) | Yes: keep as is |
| Graphics | `gfx.h` (the interface), `gfx_gl.h` (OpenGL 3.3 / ES 3.0), `gfx_xsdk.h` (360), `glload.h` | Yes: keep as is |
| Basics | `base.h` (maths, mesh builder `MB`, `Mesh`, pixel font and `UI` batch) | Yes: keep as is |
| Shaders | `shaders.h` (all GLSL), `xenon/lean_shaders.py` (360 versions) | Yes: change the looks |
| Sound | `audio.h` (synth voices, `tone()`, the `mus` music engine) | Yes: write your own sounds and tunes |
| World | `world.h` (generator, colliders, `ground()`, `collide()`), `trackfile.h` | Yes: reskin |
| Editor | `editor.h` | Yes: rename the tools you repurpose |
| Network | `net.h`, `netclient.h`, `server.cpp` | Yes, optional: keep it for online play |
| Game | `game.cpp` (state machine, `update`, `render`, HUD, menus, main), `cars.h`, `carspec.h` | Mixed: the frame loop and harness are engine; the rest is the game |
| Fog City Fury content | `mech.h`, `wardenfx.h`, `minis.h`, `showdown.h`, `netshd.h`, `champ.h`, `fighter.h`, `foot.h` | No: delete, or keep what fits |

The game is one translation unit: `game.cpp` includes the headers in a fixed order and they share globals.
There is no A12 library to link. You fork the repository and edit it.

## The rules that keep it working

- **Determinism.** Simulation code uses `grng` and fixed 1/60 s steps, never `rand()` or the clock. That is
  what makes two `--sim` runs identical, and the tests depend on it.
- **Everything goes through `gfx.h`.** Never call OpenGL from game code, or the 360 build breaks.
- **Materials are a per-vertex number** (`mat`) picked in the fragment shader (see `AGENTS.md`). A new kind of
  surface is a new number and a new branch in the shader, not a new texture.
- **No asset files.** Meshes are built in code from boxes, quads and grids (`MB::box`, `quad`, `tri`, and
  `emitGrid` in `cars.h` for smooth shapes); sounds are synthesised. A fork can add files, but the existing
  code never loads any, which is part of why it ports so easily.
- **The 360 has its own shaders.** If you change a scene shader in `shaders.h`, make the same change to its
  lean version in `xenon/lean_shaders.py`, or the 360 build keeps the old look. Build the 360 with xsdk only.
- **Tests stay green.** `make all && python3 tests/run_tests.py`. Delete tests for features you remove; add a
  test, and a report field, for each one you add.

## Step by step

### 1. Fork and rename

Copy the repository. Rename the title (`drawLogo` in `game.cpp`, and the window title passed to `plat::openDisplay` in `main()`), the
executable in the `Makefile`, the Android package in `android/`, the save files and track folder (`savePath()`, `cfgPath()` and the tracks folder beside them, all in `game.cpp`), and the
network magic number in `net.h` (`MAGIC`). The new magic number keeps your game and Fog City Fury from
joining each other's servers. Build and run the tests before changing anything else.

### 2. Strip what you do not want

Remove a feature's `#include` from `game.cpp`, then follow the compiler errors to its calls. The big
optional pieces:

- **Wardens** (`mech.h`, `wardenfx.h`): the giant walkers. Delete them, but read `mech.h` first if you want
  animated legs (see step 4).
- **Fighting on foot** (`fighter.h`, `foot.h`, `showdown.h`, `netshd.h`, `minis.h`): guns, the end-of-race
  fight, the mini Wardens.
- **Championship** (`champ.h`), **rockets, wrecks and explosions** (in `game.cpp`, search `rocket`, `wreck`,
  `explode`).
- **Hidden keys and the places they hide in** (`world.h` `decorate()` and `addKey()`, `keysGot` in `game.cpp`).

Run the tests after each removal and delete the ones that tested it.

### 3. The player: a new body and new handling

The player is a `Car` (`game.cpp`) driven by an `Input` (steer, gas, brake, handbrake, boost, fire) through
`stepPlayer()`. The bots are `Car`s too, moved along the road by `stepAI()`. To make something else:

- **Its look.** `cars.h` builds each car from a `CarSpec` (`carspec.h`): body sections, cabin, wheels, all from
  boxes and smooth grids. Write a builder for your body the same way and draw it where `drawCar()` is called.
- **Its handling.** `docs/HANDLING.md` explains the tyre model. Keep `stepPlayer`'s outer part (ground
  contact through `W.ground`, wall pushes through `W.collide`, airtime, respawning) and replace the force
  model in the middle. A walking animal or a person wants no slip angles: steer the heading directly, pick
  the speed from the input, and keep the ground following.
- **Its numbers.** `CarSpec` holds the tuning (top speed, acceleration, turning, grip). Make your own spec
  table for your vehicles or animals.
- **The camera** follows `player` in `render()`. It works for anything that moves along the ground.

### 4. Things with legs

Two working examples:

- **`foot.h`**: people. One simulation for the player and the bots (a `FootIn` per step), a skinned-box body,
  walk, run and jump animation, and a bot brain. Start here for people.
- **`mech.h`**: the Warden, a walker whose feet are planted on the ground and whose legs are solved to reach
  them, with a gait that picks the next footfall. That is the hard part of any four-legged animal. Copy the
  gait and leg solver, change the leg count and proportions, and build the body from boxes.

### 5. The world

`TrackDef` (`world.h`) describes a world; `World::build()` turns it into meshes and colliders. The rest of
the code asks the world questions: `H(x, z)` terrain height, `ground(x, z, y)` what a thing at that height
stands on, `collide()` walls, `RD(x, z)` distance to the road, `nearest()` the closest road sample.

To change what grows on the island, edit the generator:

- **Town or not:** `city()` (custom tracks: whole blocks, tidied by `tidyTown()`). Make it return false
  everywhere for countryside.
- **Buildings:** the loop under `// --- random buildings ---` in `build()`, using `building()`.
- **Trees and scenery:** `tree()`, and `decorate()` for the rest (lamps, benches, signs).
- **Special places:** `decorate()` and the helpers it calls (`courtyard()`, `warehouse()`, `waterJump()`, the
  lighthouse) show how to add a landmark with its own colliders.
- **Colours and surfaces:** the terrain colours are set per vertex in `build()` (grass, sand, rock, paving);
  the patterns are in the world shader. A new surface (mud, sand, snow) is a new `mat` value or colour rule,
  and the same in the 360's lean shaders.
- **Sky, sun, fog, weather:** `TrackDef` fields and the weather table (`wx()`).

The road does not have to be a racetrack. It is a path through the world: a trail, a river, a rail line. If
your game has no laps, set `exploreMode` (free roam already works that way) and skip the checkpoints.

### 6. Things to collect and places to reach

Copy the nearest existing feature:

- **Flowers** (`minis.h`, the blooms): items that appear in the world, are picked up by walking or driving over
  them, and add to a saved total with levels. The closest match for apples, coins and rosettes.
- **Hidden keys** (`world.h` `keys`, pickup in `update()`): fixed places to find, remembered per track.
- **Checkpoints and laps** (`update()`): gates in order with a timer.
- **Jumps** (`TrackDef::jumps`, built by `buildCustom()` in `world.h`; the editor's JUMPS tool): ramps placed
  along the road. They become fences, logs or hoops by changing what `buildCustom()` builds.

### 7. Game modes, menus and HUD

- **States:** `enum State` in `game.cpp`. Each screen is a state; `onKey()` moves between them (gamepad and
  touch are translated into the same keys). Add a state, handle it in `onKey()`, `update()` and `drawHUD()`.
- **Modes within a race:** `selMode` (race, rampage, free roam). Free roam (`exploreMode`) is the easiest base
  for a game without a finish line.
- **HUD and menus:** `drawHUD()`, using `ui.text`, `ui.rect`, `ui.disc` and `panel()`. Write button prompts
  with `P("KEYBOARD TEXT", "{A} PAD TEXT")` so pads show their own buttons.
- **Settings:** saved by `saveCfg()` / `loadCfg()` as one line per setting. Append new lines at the end, so
  older save files still load.

### 8. Sound and music

`tone(freq, duration, volume, type, slide, delay)` plays a one-shot synthesised sound; the continuous ones
(engine, tyres, wind) are voices in `audio.h` fed from the `snd` parameters each frame. The soundtrack is the
`mus` engine: patterns and instruments in code, one song per track, chosen by `mus::want`. Hooves, birdsong
and a gentle tune are new voices and new patterns in the same style. `--wav FILE` renders the music to a file
for listening without running the game.

### 9. Online play

If you keep it: the server only relays and keeps time, and each game simulates its own player. Change the
state you send (`CarState` in `net.h`) to fit your player. Bump `PROTO` whenever a struct changes, and read
`docs/NETWORK.md` before changing anything else there. If you drop it: remove `netclient.h`, `netroam.h`,
`netshd.h` and the `net*` calls in `update()`, and stop building `server.cpp`.

### 10. Test as you go

- `./game --sim --track 0 --state 3 --seconds 30` runs 30 seconds headless and prints a JSON report.
  `printJSON()` writes the report; add a field for anything you want a test to check.
- `--shot file.bmp --size W H --frames 2` (under `xvfb-run` on a server) saves a screenshot.
  `--top H X Z` looks straight down; `--cam` places the camera.
- `tests/run_tests.py` is plain Python with no libraries. Copy a test near your feature and change it.
- The autopilot (`--auto`) drives the player for tests; it follows the road. Keep a version of it for your
  player so long runs can be tested without a person.

## Worked example: a horse-riding game

| Fog City Fury | Becomes |
|---|---|
| The car, `cars.h`, `CarSpec` | A horse built from boxes; specs for each horse (speed, stamina, jumping) |
| Tyre physics in `stepPlayer` | Walk, trot, canter and gallop picked by the gas input; heading steered directly; ground following kept |
| Wheels turning | Legs: the `mech.h` gait and leg solver with four legs, or a simpler swinging-leg cycle |
| The road and kerbs | A dirt trail (a new road `mat`, no kerbs) |
| Town | Mostly countryside (`city()` false), a few farm buildings built like `courtyard()` |
| Jumps | Show-jumping fences: a new look in `buildCustom()`, a clean jump scored when the horse clears it |
| Flowers | Apples and carrots to collect, and rosettes as levels |
| Bots | Other riders racing the trail |
| Rockets, Wardens, the showdown | Removed |
| Engine sound, funk soundtrack | Hoofbeats from the speed, birdsong, a gentle tune |
| Track editor | Trail editor: draw the trail, place fences and apples |

## Checklist for a new A12 game

1. Fork, rename, change `MAGIC`, build, tests green.
2. Remove what you will not use, a feature at a time, tests green each time.
3. Change the player's body and handling; keep a test that drives it with the autopilot.
4. Change the world's look: generator rules, colours, surfaces, the same in the lean shaders.
5. Add your goals: collectibles, places, modes, HUD.
6. Sound and music.
7. Menus, title, settings.
8. Online, if you keep it: new `CarState`, bump `PROTO`.
9. Build every platform you want (`docs/ANDROID.md`, `xenon/PORT.md`).
