# Fog City Fury

An arcade hill-jumping street racer for Linux in the spirit of the late-90s
San Francisco arcade racers: steep streets, big air, exploding wrecks,
shortcuts and hidden keys. Original code, names, tracks and cars; no assets
(everything is generated at start-up).

C++17, SDL2, OpenGL 3.3 core, GLSL 3.30.

## Build and run

    # Debian / Ubuntu / Mint
    sudo apt install g++ make pkg-config libsdl2-dev libgl1-mesa-dev
    # Fedora
    sudo dnf install gcc-c++ make pkgconf-pkg-config SDL2-devel mesa-libGL-devel
    # Arch
    sudo pacman -S gcc make pkgconf sdl2 mesa

    make
    ./fogcityfury              # add --fullscreen, or --size 1920 1080

Download prebuilt binaries from the [Actions tab](https://github.com/davkdavk/fogcityfury/actions) —
every push builds them automatically (or click **"Run workflow"** to rebuild). Choose the artifact
that matches your system:

- **Linux client** (`FogCityFury-LinuxClient`): needs a distro with glibc 2.31+ and libstdc++ 7+.
  Run `./fogcityfury --fullscreen`, or add `--size WIDTH HEIGHT`. If it does not start on your
  distro, run `make` (see below). Progress is saved in `~/.fogcityfury.sav`.
- **Linux server** (`FogCityFuryServer`): static x86-64 binary. Start with `./fogcityfury-server`; it
  listens on UDP port 27015. Needs no graphics. For Raspberry Pi / SBCs, download the ARM64 build.
- **Windows client** + **server** (`.exe`): single files, nothing to install. Needs 64-bit Windows
  and a graphics driver with OpenGL 3.3. Progress is saved in `%APPDATA%\fogcityfury.sav`.
- **ARM64 server** (`FogCityFury-ARM64Server`): statically linked for Raspberry Pi and other ARM
  boards running Linux.

### Build from source

    sudo apt install g++ make pkg-config libsdl2-dev libgl1-mesa-dev   # Debian / Ubuntu / Mint
    make                                                               # builds fogcityfury + fogcityfury-server
    ./fogcityfury                                                      # add --fullscreen, or --size 1920 1080

Fedora: `sudo dnf install gcc-c++ make pkgconf-pkg-config SDL2-devel mesa-libGL-devel`.
Arch: `sudo pacman -S gcc make pkgconf sdl2 mesa`.

### Windows

Cross-compiled from Linux with MinGW-w64 (see `AGENTS.md`). Windows and Linux players can race each
other on a server running on either platform.

## Android

The source builds for Android too (OpenGL ES 3.0, on-screen buttons): see `docs/ANDROID.md`. There
is no prebuilt APK yet - it has to be built with GitHub Actions (nothing to install) or Android
Studio, and it has not been run on a device.

Touchscreens on Windows and Linux get the same on-screen buttons the moment the screen is touched
(or start with `--touch` to try them with the mouse).

## Controls

| Action            | Keyboard            | Gamepad            |
|-------------------|---------------------|--------------------|
| Steer             | Left / Right, A / D | Left stick         |
| Accelerate        | Up, W               | Right trigger or A |
| Brake / reverse   | Down, S             | Left trigger or B  |
| Nitro boost       | Shift               | Right shoulder     |
| Fire a rocket     | F (or E)            | Left shoulder      |
| Handbrake (drift) | Space               | X                  |
| Back on track     | R (or Backspace)    | Back               |
| Change camera     | C                   | Y                  |
| Music on / off    | M                   |                    |
| Online race       | O (title screen)    |                    |
| Pause             | Esc                 | Start              |
| Fullscreen        | F11                 |                    |
| Options           | F1 (title or pause) | X (in the menus)   |

The keyboard keys above are the defaults; every driving key can be changed
in the options.

## What's new in 1.39

- **Faster on Android**: phones and tablets draw the world in chunks, as the Xbox 360 does: nothing out of view, simpler ground in the distance, small props only up close. It looks the same.
- **Shadows** draw each piece of ground once (the 360 drew it four times over).

## What's new in 1.38

- **Sunset Causeway shortcut**: a backstreet route through town gives racers and bots another way to the finish.
- **Shields**: a shield pick-up protects a car from one hit.
- **Bot rockets and car bumps**: opponents can fire rockets and cars now jostle instead of passing through each other.
- **Xbox 360 port**: the full game runs on RGH/JTAG consoles, with cross-play against PC and Android.
- **Controller button prompts**, a faster track select, and a new H.J.B Enterprizez start-up logo.
- **Network hardening and protocol 7**: malformed packets are ignored; all clients and servers must use the matching protocol.

## What's new in 1.37

- **Xbox 360 port**: the full game now runs on RGH/JTAG consoles, with cross-play against PC and Android.
- **Controller button prompts**: menus show the matching gamepad buttons.
- **Faster track selection**: previews build only for the track you stop on.
- **New start-up logo**: H.J.B Enterprizez in spinning gold blocks.
- **Network hardening**: malformed or impossible car-state packets are ignored.
- **Online compatibility**: protocol is unchanged, so version 1.36 players can still join.

## What's new in 1.31

- **West-coast funk soundtrack**: a new swung score with drums, a gliding bass, electric piano and a
  high sliding lead, replacing the metal soundtrack.
- **Shorter grab phase**: the showdown countdown to grab guns and take position is 15 seconds, down from 30.
- **Ready-up screen restored online**: the lobby lists every player with READY or WAITING, shows
  how many are ready, and shows the start countdown.

## What's new in 1.3

- **Showdown at the finish line**: the top three finishers fight on foot at the finish, not at a
  separate site. The three cars are parked round the line and you get out beside them.
- **Podium drivers fight**: the other two drivers are on foot too, as fighters with their own guns.
- **Shrinking ring**: the ring starts wide, tightens after about 50 seconds of fighting, and closes
  hard once two fighters are left. Outside it, your health drains in steps.
- **Pick-ups**: guns, medkits and body armour lie around the ground during the 15 second grab phase.
  Race places matter: the winner starts with 50 armour, second with 25.
- **Better character and gun models**, with recoil, flinch and a fall animation.

## What's new in 1.2

- **Showdown camera**: on foot, the view follows the person from behind, so you can see who you are
  fighting. The race clock now stops at the finish, and the rampage race shows its time.
- **Showdown site**: a beacon marks the site on the road, with its distance and direction on the HUD.
- **Gun and health panel**: your current gun and health sit at the bottom of the screen during the showdown.

## What's new in 1.1

- **Rampage mode** (third option on the mode menu): a race, then a showdown. The winner drives to a site halfway
  round the track, gets out, has 30 seconds to grab guns, then fights the others on foot. A ring
  closes in once two fighters are left; stand outside it and your health drains. Last one standing wins.
- **On foot**: press G to get out of a car that has almost stopped, and G again beside it to get
  back in. Walk with the steering and throttle keys; walls and cars stop you.
- **Guns**: pistol, SMG (hold to fire), shotgun and rocket launcher. Change weapon with Q. Guns unlock
  as you collect hidden keys: 5, 10 and 20 keys in total.
- **Cleaner touch controls** on Android: buttons are frameless and see-through, and light up while held.
- **Build and release fixes**: the Windows and ARM64 server builds are now verified on every release,
  and the Android APK ships with the release when it builds.

## What's in it

- **Nitro**: a 2.5 second boost on a meter that refills in 10 seconds.
- **Rockets**: drive through a crate for two homing rockets; they lock on to
  the closest car and blow it up.
- **Wreckage**: every car that blows up leaves its burnt-out shell and loose
  parts on the road for ten seconds, then they fade out.
- **Weather** per track: clear, rain, fog or storm.
- **4 tracks**, 3.2-3.8 km: Hilltop Heights (daytime figure 8 with a bridge
  and steep crests), Bayside Breaker (sunset, causeway across the bay),
  Summit Switchbacks (misty mountain pass, long climb) and Midnight Mile
  (night city, a double figure 8 with two bridges).
- **Backstreets**: every block in town is ringed by narrow streets - worn
  tarmac, kerbs, lamp posts, dumpsters against the walls - so you can cut
  through town. One of them on each track ends at an open warehouse door.
- **4 cars**: RipDog (wide, quick off the line, at home in the dirt, its name on the bonnet), Stallion GT (all-rounder), Katana RS (fastest, poor off-road),
  Bruiser 4x4 (slower, goes anywhere). 7 paints.
- **Race** (1-9 laps, 9 opponents lined up in painted grid boxes; the camera starts
  above the grid and comes down behind your car before the countdown - Enter
  skips it; checkpoint clock), **Free Roam** (no clock,
  no traffic - for key hunting) or **Online** (up to 10 players, see below).
- **5 hidden keys per track**, saved between sessions in `~/.fogcityfury.sav`.
  Find all 20 to unlock the gold chrome paint.
- The whole map is drivable: leave the road, cut corners, drive on roofs.
  Deep water sinks you; hitting a wall above ~110 km/h blows the car up.

## Multiplayer

    make all
    ./fogcityfury-server                 # on the host: UDP port 27015
    ./fogcityfury                        # each player: press O on the title screen

Pressing O shows the **server list**: each server's name, track, number of
players and ping, refreshed every few seconds. Up/Down and Enter join one; A
adds an address (name or IP, `:port` optional), Del removes one, N changes
your player name. The list starts with `thadav.com:27015` and this computer,
and is saved with your settings.

Type the server's address (`host` or `host:port`) and a name, pick a car, and
press Enter in the lobby when ready. The race starts when everyone is ready;
after the results the server moves to the next track. Up to 10 players, no AI
cars online. `./fogcityfury --connect HOST --name YOU` skips the address
screen. Server options (`--laps`, `--port`, `--track`, `--weather`, `--lobby`, ...)
are listed by `./fogcityfury-server --help` and in `docs/NETWORK.md`. The server
needs no graphics. It trusts its clients, so use it with people you know.

## Nitro

Hold Shift (or the right shoulder button) for a hard shove in the back: about 25% more
top speed, and it pulls even with the throttle shut. A full tank is 2.5 seconds
of boost and empties in one long burn or several taps; the meter above the
speedometer refills from empty in 10 seconds. Run it dry and it locks out -
let go, let it build back up, and press again. It only fires with the wheels on
the ground, so it is wasted in mid-air.

## Options (F1)

Press F1 on the title screen or in the pause menu. Tab switches between two
pages; Esc saves and goes back. Settings are kept in `~/.fogcityfury.cfg`
(`%APPDATA%\fogcityfury.cfg` on Windows).

**Controls.** Each action has two key slots. Up/Down picks the action,
Left/Right the slot, Enter then waits for the key to use (Esc cancels), Del
empties a slot, F5 restores the defaults. A key can do one thing only, so
giving it to an action takes it from the one that had it. Esc, M, F1 and F11
are fixed. The gamepad layout is shown but cannot be changed.

**Video.** Changes apply as you make them.

| Setting | Choices |
|---|---|
| Display | Window or full screen (F11 does the same) |
| Resolution | 1024x576 up to 3840x2160, limited to what your desktop can show |
| Anti-aliasing | Off, 2x, 4x, 8x MSAA (as far as the graphics driver goes) |
| Anisotropic filtering | Off, 2x, 4x, 8x, 16x: keeps road and ground detail sharp in the distance |
| VSync | On or off |

In a window, the resolution is the window's size. Full screen always uses
the desktop's own mode: the game is drawn at the chosen resolution and scaled
to fill the screen (the HUD stays sharp), so a lower resolution is a real
speed-up without the monitor changing mode.

## Rockets

Six pairs of green crates sit round every lap. Driving through one gives you
two rockets, and that is your set for the lap: the crates disappear until you
cross the start line again. With
rockets on board a red LOCK bracket marks the closest car - one ahead of you
is preferred to one behind. Press F: the rocket homes in, the car explodes and
leaves its wreck on the road. A wrecked opponent is out for three seconds; a
wrecked player respawns as after any crash. With nobody in range the rocket
flies straight on. Opponents do not shoot back. Online, your rockets hit the
other players (their rockets are not drawn on your screen, only the
explosion).

## Wrecks and getting back on track

Any car that explodes - you into a wall, anyone hit by a rocket - leaves a
burning, then smouldering, shell and scattered panels and wheels. The shell is
solid: drive round it. Loose parts get kicked along when you hit them.
Opponents pull out to pass a wreck. After ten seconds the shell and its
parts fade away.

Press R (or Backspace, or Back on a gamepad) at any time to be put back on the
road, stopped and pointing the right way. It also works from the pause menu,
and the game reminds you when you look stuck, lost or the wrong way round.

## Weather

Pick the weather for each track on the track-select screen (up/down to choose
the row, left/right to change it). Your choice is remembered per track.

| | What changes |
|---|---|
| Clear | Dry road, full grip |
| Rain | Wet road with puddles that mirror the surroundings, about 18% less grip, longer braking, spray |
| Fog | You cannot see the corners coming; grip is normal |
| Storm | Heavy rain, lightning bolts and thunder, about 26% less grip |

Opponents slow down in the wet too. Online, the server decides the weather.

## Handling

Each car has a front and a rear axle with their own grip. Ask too much and the front pushes wide
(slight understeer). The rear comes round if you pull the handbrake, lift or brake mid-corner, or
floor a rear-drive car in a low gear; opposite lock catches it.

Brakes build up over a third of a second rather than slamming on, and braking
into a corner turns the car in harder (brake steer).

- Stallion GT: rear-drive, mild; power-slides out of slow corners.
- Katana RS: most grip, but loose when you lift or trail-brake.
- Bruiser 4x4: four-wheel drive, understeers, hard to unsettle.

## Feel

- Chase camera on springs: pulls back under power, closes in on the brakes,
  rolls into corners, lags behind jumps and dips on landing; FOV and a
  radial speed blur widen with speed.
- Suspension: squat, dive, body roll, a heave spring that compresses on
  landings, wheels that droop in the air.
- Gear shifts cut power for a moment with a blow-off hiss and exhaust flame;
  lifting off at high revs backfires.
- Skid marks, tyre smoke, landing sparks, "BIG AIR" hang-time call-outs,
  rocket starts (hit the throttle just before GO), slow motion on wrecks,
  pass-by whooshes, gamepad rumble.
- A synthesised west-coast funk soundtrack (swung drums, gliding bass, electric
  piano, a high sliding lead) generated in real time - no audio files.

## Rendering

- Post-processing: HDR with 4x MSAA, bloom, speed blur, chromatic
  aberration on impacts, vignette.

- Car paint reflects a dynamic HDR cubemap rendered around the car every
  frame (clear-coat Fresnel over a tinted metallic base).
- Shadow-mapped sun with PCF, procedural sky and clouds, distance fog.
- Wet roads: a bump-mapped surface (asphalt grain, raindrop rings on the
  puddles) reflecting the same cubemap, so buildings and lit windows show
  in the road; overcast cloud decks, drifting fog banks, thin wind-blown
  rain, splashes, spray behind every car and drops on the lens.
- Water: animated normals, Fresnel sky reflection, sun glitter, depth-based
  colour and shoreline foam (reads the terrain height map).
- Particles: tyre smoke, dust, exhaust, water spray, sparks, explosions; in the
  wet, water off the wheels on the road and mud, torn grass and water off it.
- Each wheel follows the ground under it and every car has a soft contact
  shadow, so cars sit on rough ground instead of seeming to hover.
- Engine, tyre, wind and effect sounds are synthesised.
- `./fogcityfury --wav out.wav 60` writes a minute of the soundtrack to a file.

## Key hints (spoilers)

Every track also has a warehouse with its roller door open (yellow door
frame): the fifth key is inside. It stands across the end of a backstreet two
blocks in from the main road; that street is the one lined with orange lamps
on both sides.

Hilltop Heights: a walled courtyard north of the crossing, between the two
loops; a striped ramp onto a roof on the far west side; a lighthouse island
off the south-east shore; the top of the big hill inside the east loop.

Bayside Breaker: the end of the pier; a warehouse yard; in mid-air off the
shoreline ramp (land on the islet, drive back over the sandbar); the second
roof of the rooftop run.

Summit Switchbacks: the very top of the mountain, inside the boulders; a
lighthouse island off the north-west shore; a shoreline ramp in the south; a
courtyard on the north-west hillside.

Midnight Mile: a two-roof rooftop run at the far west end; the end of the
north-east pier; a warehouse yard inside the middle loop; a shoreline ramp
in the north.

## For developers and AI models

`AGENTS.md` is the starting point: conventions, source map, pitfalls, known gaps. `docs/` has the
architecture, the handling model and the testing guide.

    make test          # 39 headless tests in ~90 s: races, all 20 keys, bridges,
                       # rockets, wrecks, reset, options, hazards, handling,
                       # nitro, weather, multiplayer, audio
    make test-render   # plus rendering smoke tests (needs xvfb-run)

`./fogcityfury --sim ...` runs the simulation with no window and prints a JSON report.

## Source layout

    src/base.h      maths, GL helpers, mesh builder, pixel font
    src/shaders.h   all GLSL
    src/world.h     track definitions, terrain, colliders, secrets
    src/cars.h      car and wheel meshes
    src/audio.h     engine/effects synthesis and the soundtrack
    src/net.h       multiplayer wire format (shared with the server)
    src/netclient.h multiplayer client
    src/server.cpp  dedicated server
    src/game.cpp    physics, AI, camera, rendering, menus
    tests/          test suite
    tools/shot.sh   headless screenshots
    docs/           architecture, handling, testing
