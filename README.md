# Fog City Fury

An arcade hill-jumping street racer for Linux in the spirit of the late-90s
San Francisco arcade racers: steep streets, big air, exploding wrecks,
shortcuts and hidden keys. Original code, names, tracks and cars; no assets
(everything is generated at start-up).

**New in 1.5: build your own tracks.** A track editor is built into the game: draw the road, shape
hills and lagoons, lay undersea tunnels and shortcuts, then drive it straight away. Plus **Twin Isles**,
an endurance track across two islands. Runs on Linux, Windows, Android and the Xbox 360.

C++17, SDL2, OpenGL 3.3 core, GLSL 3.30.

**Just want to play?** Read the **[Player's Guide](docs/PLAYING.md)**: downloads, controls, modes, online play, the track editor and troubleshooting.

**Want to script a track?** Read the **[event editor guide](docs/FCFEDIT.md)**: Warden misfires, falling buildings, explosions and messages, in a few clicks.

## The A12 Engine

Fog City Fury runs on the **A12 Engine**, which is in this repository too: generated island worlds with roads,
towns and weather, a track editor, vehicle and on-foot physics, synthesised sound and music, online play, and
a headless test harness. No dependencies beyond SDL2 and OpenGL, and it runs on Linux, Windows, Android and the
Xbox 360. To make a different game from it, start with `docs/ENGINE.md`.

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

## What's new in 2.7

Gunplay, along Fortnite lines but in the game's own spirit: quick, clean and satisfying.

- **Weapon handling**
  - Every gun has a magazine. It clicks and reloads itself when empty; R (pad X, touch RELOAD) reloads early. Pistol
    0.85 s, rifle 1.45 s, SMG 1.3 s, launcher 1.55 s; the shotgun loads shell by shell and firing stops it.
  - Quick swaps: 1-5, the mouse wheel and Q (last gun); pad LB/RB and Y; tap or swipe GUN on touch. A swap takes
    0.25 s and cancels a reload.
  - Rarity on gun pick-ups: grey, green, blue, purple, gold. Better guns hit a little harder, spread less and reload
    faster; each glows its colour and its name shows in that colour.
- **Movement**
  - Jump (Space, pad A, touch JUMP), crouch (C or Ctrl, pad B, touch DUCK), slide (crouch while sprinting), and
    climbing onto low walls, crates and cars (jump at them). Roll moves to V / Left Alt / the right stick.
  - Crouched you're slower, steadier and harder to hit; you can stand on bonnets and roofs.
- **The racer, reworked:** broader build, collar, race number on his back, visor, gloves, knee pads, heavier boots.
  Guns held in both hands: low and ready, up at the target, shouldered down the sights. Knees bend and arms swing as
  he runs; poses for jumps, crouches, slides and climbs.
- **Enemies that fight back**
  - Rooftop snipers show a red laser on you for about a second before they fire. Break line of sight and the shot is
    called off.
  - Mini Wardens that land take cover, step out to fire short bursts (their muzzle glows first), and one of a group
    goes round your flank.
  - Mini Wardens are plated: pistol and SMG rounds lose damage at range; shotgun, rifle, grenades, fists and headshots
    go through. A vest no longer covers the head in a showdown.
  - Showdown bots use cover and go for a medkit when hurt.

## What's new in 2.6.3

- **Rampage bots reach the ring.** They drive the shortest way there from wherever they are: down alleys, round water,
  straight across open ground when it's clear. All five get in on every track, and nobody is dropped at the ring's edge.
- **The Wardens and UFOs keep moving** while everyone drives to the ring (they used to freeze).
- **Car damage in races.** Walls, other cars, rocket bursts and cars blowing up beside you wear your car down (the CAR
  meter). Hurt, it smokes and loses top speed and pick-up; badly hurt, it burns; at nothing it's wrecked and comes
  back whole. It mends slowly as you drive, fully at the start/finish line or with a shield. Not in MANIC or online.
- **Every weapon hurts mini Wardens:** fists, every gun, grenades and the car's rockets (rifle rounds and grenades used
  to glance off).
- **INCOMING!** flashes when a Warden fires a revenge rocket at you, with a warning tone and an arrow pointing at it
  (and its distance) until it lands.
- **Tidier event messages:** short, upper case, one colour for warnings, one for good news, one for information,
  and no repeats piling up.
- **Your car stays drawn** in the in-car camera when you're out of it.
- **Dolphins leap higher:** about 5.5 m up over 16 m, each leap a little higher or lower, bigger dolphins higher.

## What's new in 2.6.2

- **Every track's script is reworked so each lap plays differently.** Everything is set off by lines across the
  road just ahead of you, so it happens where you can see it.
  - **Lap 1:** a Warden misfires and a tower comes down. On Midnight Mile, where no Warden stands by the road, the
    first tower simply gives way.
  - **Lap 2:** another building collapses on its own, with a shake and "THE CITY IS COMING DOWN!".
  - **Lap 3:** a third comes down, with "IT'S ALL COMING DOWN!".
  - Each track keeps its one Warden where it was, because moving Wardens changes how they fight the race.
  - **Every lap:** dolphins leap beside the road just as you pass.
  - **Weather:**
    - Bayside Breaker and Lagoon Loop: a storm breaks on lap 3.
    - Summit Switchbacks: fog rolls in on lap 2.
    - Harbour Deep and Midnight Mile: rain on lap 2.
  - Midnight Mile and Hilltop Heights keep their rooftop snipers. Twin Isles, the endurance track, gets a lap 1
    misfire, its mini Warden drop and dolphins.
- `--survey` also gives the road's direction at each sea spot (for scripting leaps alongside the road). A
  scripted rocket's burst no longer draws on the race's random numbers, so scripted moments don't change how the
  rest of the race plays out.

## What's new in 2.6.1

- **MANIC is you against your own ghost.**
  - You're alone on the track, starting from the middle of the start line.
  - Your best lap on that track drives alongside as a see-through ghost, in step with your current lap.
  - Under the clock, GHOST shows how far ahead (green) or behind you are, live; checkpoint splits compare against
    the ghost too.
  - Beat your best lap and that lap becomes the new ghost. It's kept per track in `<save>.ghostN`.

## What's new in 2.6

Gunplay, part 1 of 4: how a hit feels.
- **Headshots:** a small head hitbox takes double damage, with a bright tink and a yellow marker. A rifle round to
  the head drops a rooftop sniper in one.
- **Damage numbers** rise off whatever you hit: white for the body, yellow for the head, blue for armour. A
  shotgun's pellets add up into one number.
- **Crosshair bloom:** firing fast opens it. Settled, still and down the sights, your first shot goes exactly
  where you aim.
- **Recoil:** each gun throws the view up its own amount, with a little sideways wander, and it drifts back.
- **Kills** get a bigger red marker and a two-note sting.
- **Damage falls off with range** for the pistol, SMG and shotgun; the rifle holds its damage to its full range.

## What's new in 2.5

- **MANIC**, a new mode: a time trial over the laps you set.
  - The field still drives, but as see-through ghosts with no contact.
  - There are no rocket crates or shields on the track, nobody fires (Wardens and mini Wardens included), and
    there's no countdown clock.
  - Lap times show as you cross the line (green when it is your best), and splits at each checkpoint compare
    against your best lap.
  - The results list every lap, the total and the record. The best total for each track and lap count is kept
    (`<save>.trial`).

## What's new in 2.4

- **Aim down the sights on foot:** hold the right mouse button (on a pad, the left trigger).
  - The camera comes in over your shoulder and zooms, with a crosshair that turns red over a target and shows the
    range.
  - You face where you look, and left and right **strafe**.
  - Shots go exactly where the crosshair is, high or low, so rooftops are in reach.
- **Rooftop snipers:** mini Wardens can now stay up at the edge of a roof. Once you're on foot and in their sight,
  they shoot at you. Any gun brings them down, and they come apart into Warden pieces. Hilltop Heights and Midnight
  Mile have them. In fcfedit, a MINI WARDENS moment's THEN THEY setting is now JETPACK DOWN or STAY UP AND SNIPE.
- **Pick-ups in town:** boxes of rounds and medkits on the paths between the buildings, away from the road. There
  are about 30 a track, and each comes back 40 seconds after it is taken.
- **Dolphins laugh:** shoot one (or lob a grenade at it) and it chatters and whistles at you. It can't be hurt.
- Fixed: the golden UFO, cars and their shadows no longer warp over a building that has come down. Only the
  town's own shapes are flattened where it stood.
- Test flags: `--onfoot`, `--ads`, `--aimat X Y Z`, `--aimmini`, `--aimdolphin`.

## What's new in 2.3

- **Dolphins** (Tarnyloo's *Dolphin*, CC-BY 3.0): pods live in the sea all race. They wander, keep to deep water,
  and surface to breathe with their back and fin cutting the water and a wake behind. At the spots a track sets, the
  nearest one races over, comes out nose first with a small ring, arches over and dives back in with a big splash.
- **Swells and a tide:** the sea's surface moves now. Long rolling swells (gentle on a clear day, bigger in a
  storm, never Wave Race big) fade out in the shallows, and the whole sea rises and falls slowly. The rougher the
  weather, the faster the chop and the more whitecaps.
- **Weather that changes mid-race:** a WEATHER event turns it (the sky, fog, rain, grip and sea all blend over a few
  seconds), and a track can let its weather drift by itself, telling you what's coming.
- **Mini Wardens off the rooftops:** they jetpack down to the roadside and take a shot at you as you pass.
- **Every track has a scripted moment:** towers come down on Hilltop Heights, Summit Switchbacks, Harbour Deep and
  Lagoon Loop. Dolphins leap on six tracks, minis drop on Midnight Mile and Twin Isles, the rain rolls in on
  Midnight Mile's second lap, and the weather drifts on Bayside Breaker and Twin Isles.
- **Online:** a server sends its event files (`--events DIR`, or `events/` beside it), so online races play the
  same moments.
- **fcfedit:**
  - **DOLPHINS** (`D`): click the sea for the pod, then each spot where they leap.
  - **MINI WARDENS:** where cars set it off, the roof, then where they land.
  - **WEATHER CHANGE.**
  - **The event list and timeline** (`E`): every moment, and the laps it happens on. Click a lap to turn it on or
    off, and turn drifting weather on or off from there.
  - Paths are drawn for everything that moves: a rocket's line, a leap's arc and a jetpack's drop.

## What's new in 2.29.2

- **fcfedit is much easier to use** (new guide: [docs/FCFEDIT.md](docs/FCFEDIT.md)):
  - **Warden misfires in one click.** Click a Warden and choose **+ MISFIRE**. It gets its own trigger line, a rocket
    and a message. Drag the red ring onto a building and that building comes down when the rocket hits; on the ground
    it just explodes.
  - **New moments, step by step** (`N`): choose what happens, then click what the bar asks for (where cars set it off,
    the Warden, the building...). **+ THEN…** chains the next step after any step.
  - **A chain box** spells out the selected moment in plain words ("WHEN YOU CROSS WARDEN 1 TRIGGER → WARDEN 1 FIRES
    AT BUILDING 528, IT FALLS → MESSAGE").
  - **Changing track:** **< PREV / NEXT >** buttons, or the `<` `>` keys.
  - Plain names: EXPLOSION, MESSAGE, BUILDING FALLS, TRIGGER lines. A Warden's rocket is drawn from that Warden.
- Hilltop Heights' script is just the Warden misfire now (the confusing rooftop rocket from 2.28 is gone)

## What's new in 2.29.1

- The water sliders do three times what they did: WATER RINGS raises the rings' waves 3x (and still sets their size),
  SPLASH MIST throws 3x the spray (1.5x on the Xbox 360, where overlapping spray is what its GPU likes least)

## What's new in 2.29

- **Buildings come down.** A rocket aimed at a building with "brings it down" set, or a collapse event, drops it into
  its own dust: concrete slabs thrown out, rubble left on the lot (and cars meet the rubble, not the old walls). Up to
  four a race
- **Warden misfires:** a rocket can be fired from a Warden's cannon (Warden 1-4, or the nearest), wherever it has walked
  to, with a flash at the muzzle
- In fcfedit: FIRED FROM and BRINGS IT DOWN on rockets, key 7 for a collapse
- Hilltop Heights: on the first lap a Warden's cannon misfires and the tower ahead on the left comes down

## What's new in 2.28.1

- fcfedit saves again: CTRL+S (and CTRL+Z, SHIFT for fine steps, ALT to drop a target off a building) were read after
  the keys were let go, and W A S D were taken as arrow keys; there is a SAVE button at the top right as well

## What's new in 2.28

- **Scripted events on any track**, and **fcfedit**, the event editor to make them (see `docs/EVENTS.md`): draw
  trigger lines across the road, place rockets, blasts, splashes, shakes and messages, and set for each what sets it
  off (a line, a lap starting, a race time, another event), on which laps, after what delay, how often, by what chance
  and by whose car. Aim a rocket at a building by dropping its target on it; set where each Warden starts walking.
  Preview a race with the events in the editor, then save
- Hilltop Heights has the first script: a rocket streaks across the road ahead as you pass the gate, every lap
- `make fcfedit` (Linux) and `make windows` (fcfedit.exe); the same editor is in the game with `--evedit`

## What's new in 2.27

- A new car: the **Ronin 120**, an early-70s Japanese sedan, light, rear-driven and eager to turn. Round headlights,
  chrome bumpers and door mirrors, steel wheels; it takes any paint like the others
- It is the first car made in Blender: the Blender conduit (`tools/blender/fcf_model.py`) brings a model into the game
  for every platform, trims it to the triangle budget, and bakes its soft shading (in the wheel arches, under the
  bumpers, between the panels) into a texture the car shaders use, on the Xbox 360 too
- Online, an older version of the game shows the Ronin as a Stallion

## What's new in 2.26

- Under the bonnet: game.cpp (3,150 lines) is split into eight files by job: serverlist, settings, combat, driving,
  update, scene, hud and input (game.cpp keeps the start-up and the main loop, 740 lines). The 95 lines over 300
  characters are broken up. The compiler sees exactly the same code as 2.25 in every build: nothing in the game changes

## What's new in 2.25

- Under the bonnet: the Xbox 360 build is one title now, "Fog City Fury" (the bring-up's test builds and their
  switches are gone, about 300 lines), and every 360 shader gets its vertex layout from the shader build itself, so
  a new effect can't go missing the way the space track's planet did
- Nothing in the game changes: same look, same handling, same tracks

## What's new in 2.24

- A town that looks like one: stepped towers, office towers on shop podiums, blue glass office blocks, round glass
  towers, brick flats with balconies, and warehouses with sawtooth roofs and roller doors by the water
- On the roofs: parapets, air-conditioning plant, stair houses, water tanks, masts with red aircraft lights, helipads
- At street level: shop signs that glow at night and canvas awnings over the shop windows
- Every building stands where it did and is the same size and height (only its look changed), so the routes,
  shortcuts and keys are all where they were
- Xbox 360: the space track's planet is back and its road is see-through again (their shaders never got the mesh
  vertex layout, so the planet drew nothing and the road came out solid)

## What's new in 2.23

- The Wardens walk more like something alive and heavy: they turn onto their path gradually, shift their weight
  over the foot that carries it, rise and settle with each step, roll heel to toe, and rock when a foot comes down
- They track the UFOs: the head follows them in, the cannon comes half up while one circles, then fully on it, led
  ahead so the rounds meet it; a UFO is always brought round in front of the Warden before it is shot
- The cannon is a proper rotary gun now: it winds up, fires a burst of about 35 rounds with muzzle flashes and
  tracers walking onto the target, kicks with every round, throws out brass cases that tumble, bounce and ring on
  the ground, then winds down smoking
- Xbox 360: the space track's shaders rewritten without the features the console's compiler may refuse

## What's new in 2.22

- Xbox 360: the space track's road is see-through slime with gold and silver sparkles inside it, the sky is full of
  stars, and the planet has its own shading (clouds, the sea catching the sun, the glow of its air, town lights at night)

## What's new in 2.21

- Sliders in Options > Video for the size of the water rings, the amount of splash mist and how thick the grass is

## What's new in 2.2

- A secret track: find the golden UFO in Roam (follow its beam of light), park under it, and race the Space Ring,
  a road of see-through green slime with stars inside, round a planet made of the game's islands
- On a roam server the golden UFO takes everyone to space together, then back to roaming (servers need updating:
  protocol 11)

## What's new in 2.11

- Anti-aliasing now also smooths edges inside surfaces (lit windows, road lines) and very bright edges, which
  multisampling alone left jagged
- Grass reaches further (about 190 m) in three steps of detail and takes the ground's colour at its far edge, so you
  no longer see it arrive

## What's new in 2.1

- Grass no longer pops in: it fades in with distance, with a second, lighter ring of grass out to about 125 m
- Grass on the verges along the road in town
- Landing in the water throws up a column of mist
- A third UFO for every Warden, and some come down in the sea with a splash

## What's new in 2.0

- Track editor on phones (touch) and with a gamepad
- Grass in the fields that sways in the wind (PC and Android); roadside grass on the Xbox 360
- Splashes: cars, bullets and blasts leave rings on the water (PC and Android)
- Custom tracks get a sandy beach along every shore, and town keeps off it
- New touch controls: round buttons with icons, tappable server and track lists
- Keys jingle faintly when you are near one
- Wardens lob rockets over buildings; mini Wardens come in groups of three
- Xbox 360: headlights on the night circuit

## What's new in 1.74

- Xbox 360: no longer dropped from online lobbies while it builds the track
- Cleaner waterline where undersea tunnels meet the sea

## What's new in 1.73

- Shared tracks go to a vote: the lobby sees a map of the track and says yes or no
- Everyone who plays a shared track keeps it; the server then goes back to its own track
- Tidier towns on custom tracks; nitro pickups on the starting grid

## What's new in 1.72

- Shortcuts can be tunnels: in the editor's Tunnel tool, click a shortcut (right click takes it back up)
- A shortcut can no longer start or end inside a tunnel
- Brightness setting in Options > Video (starts at 85%; 100% is the old look)

## What's new in 1.71

- Share your own tracks online: SHARE TRACK in the lobby (T / Y) sends one to the server and everyone gets it
- Server: --no-uploads turns sharing off

## What's new in 1.7

- Online roam: fogcityfury-server --roam (everyone shares the same mini Wardens)
- Servers can host custom tracks: fogcityfury-server --tracks DIR (players download them on joining)
- Mini Wardens can be hurt with fists, pistol, SMG and shotgun; fists are always selectable

## What's new in 1.6

- Free roam: mini Wardens drop in now and then (red dots on the map). Beat them with your fists for flowers that level you up and unlock attributes. Run one over and the Wardens fire back.
- Track editor: place jumps and hide up to five keys on your own tracks. Del on the track screen deletes one of your tracks.
- Smoother shortcut joins; bots brake for them.

## What's new in 1.5

- **Track editor (PC).** Build your own tracks in the game: on the track screen press **F4** to edit the
  track shown (built-in tracks are copied, never changed) or **F6** to start from scratch. Drag the road,
  raise hills and dig lagoons, lay undersea tunnels and shortcuts, pick the sky, weather and song, then
  press **Enter** to drive it and **F4** to jump back. Tracks save as small text files (`.fcft`) you can
  share. See `docs/EDITOR.md`.
- **Twin Isles**, a new endurance track: two islands joined by two glass-roofed undersea tunnels and a
  causeway, two hill shortcuts, four Wardens, and its own G-funk tune. Races run twice the laps you pick.
- The undersea tunnels are dry inside, the camera stays inside the tube against the wall, and the
  glass-section road edges match the tunnel's tint. Fish keep to deep water; UFO wrecks never land in a tunnel.
- Bots and the autopilot slow down and steer round Warden feet and falling UFOs; reset puts you on a
  clear lane past anything blocking the road.
- Online play needs everyone on 1.5 (new track list).

## What's new in 1.47

- Harbour Deep and Lagoon Loop bring undersea glass-roof tunnels, dry roads below sea level, surface causeways and new music.

## What's new in 1.46

- Two shortcuts on every track; Wardens shoot down UFOs that land as solid wrecks (road wrecks obstruct traffic) and plant giant flowers; Warden feet are solid; showdown bots no longer run at unreachable rings; the showdown has its own song. Xbox 360 now ships the verified lean XEX.

## What's new in 1.45

- A G-funk song for each track and the showdown; the Xbox 360 release now uses the verified lean XEX (previous downloads were plain builds).

## What's new in 1.44

- Town streets now meet the main road with a grass verge, kerbstone and brick promenade, on PC, Android and Xbox 360.

## What's new in 1.43

- Nitro jets you can see; rain shows on the 360; the shortcut is one smooth curve with kerbs; windows sit properly on the buildings; town paving meets the road cleanly.

## What's new in 1.42

- Nitro jets you can see; rain shows on the 360; the shortcut is one smooth curve with kerbs; windows sit properly on the buildings.

## What's new in 1.41

- Two Wardens on every track, walking round the buildings with their feet on the ground.

## What's new in 1.40

- **The Warden**: a giant harbour mech idles by Sunset Causeway, then patrols beside the road on every track.
- **Shadows draw each piece of ground once**, reducing duplicate work on Android and Xbox 360.

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

## Track editor

Make your own tracks (PC, mouse and keyboard). On the track screen press **F4** to edit the track
shown (built-in tracks are copied, never changed) or **F6** to start a new one. Drag the road's
points, add hills and lagoons, put in an undersea tunnel and shortcuts, pick the sky, weather and
song, then press **Enter** to drive it and **F4** to come back. Tracks are saved as small text files
(`.fcft`) in `~/.fogcityfury/tracks/` (Windows: `%APPDATA%\fogcityfury\tracks\`) and show up in the
track list. Custom tracks are offline only and have no hidden keys. Details and the file format:
`docs/EDITOR.md`.

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

## Credits

- "Dolphin" by **Tarnyloo** (Blend Swap #83681), CC-BY 3.0: the dolphins. See [models/dolphin/README.md](models/dolphin/README.md).
