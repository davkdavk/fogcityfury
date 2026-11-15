# Testing and looking at the game without a screen

There are three ways to exercise the game without a person at the keyboard. Use the cheapest one
that answers your question.

| Mode | Needs | Speed | Use it for |
|---|---|---|---|
| `--sim` | nothing (no display, GL or audio) | ~100x real time | physics, race rules, keys, anything with a number |
| `--shot` | `xvfb-run` + Mesa software GL | 0.3-1 s per drawn frame | how something looks |
| `--wav` | nothing | fast | the soundtrack |

## The test suite

    make test                                  # all headless tests, ~40 s (the network tests run in real time)
    python3 tests/run_tests.py handling -v     # tests whose name contains "handling", with numbers
    make test-render                           # adds rendering smoke tests (needs xvfb)
    python3 tests/run_tests.py --windows       # adds the Windows build under Wine (needs `make windows`, wine)

Exit status is non-zero on any failure. `-v` prints every measured value next to its threshold;
use that output when you retune something and need new numbers.

What is covered:

| Group | Tests | What they assert |
|---|---|---|
| boot | 3 | all four tracks build at 3.1-3.9 km; 5 keys (one inside a building), 8 checkpoints, a ramp each; key routes start on land; identical runs give identical reports |
| race | 5 | the autopilot finishes 3 laps on every track in all three cars, no sinking, at most one wreck, camera never off-axis, lap times in range; a parked car gets TIME UP |
| keys | 2 | each of the 20 keys can be collected by the route its secret registered; the water jump works in every car |
| bridges | 1 | Hilltop has one bridge and Midnight Mile two, the deck clears the road below by about 7.5 m, and the autopilot laps both over and under without hitting anything |
| rockets | 2 | no ammo, no shot; two rockets wreck two opponents, leave two shells and cost them places; with nobody to lock on to the rocket flies off; a crate gives two rockets, never more, and returns after 12 s |
| wreck / reset | 2 | a crash leaves one shell and 7-11 loose parts where the car died; `--reset` puts a car that is far off the road back on the centre line, stopped |
| options | 2 | the video page changes resolution, AA, AF, vsync and display and they survive a restart; F1 from pause returns to pause; a rebound key drives the car, the old key stops, a key belongs to one action, Esc cancels a capture, F5 restores defaults |
| hazard | 2 | a wall at speed wrecks then respawns; deep water sinks then respawns |
| nitro | 2 | the boost lifts top speed and standing-start pace; the tank is 2.5 s, refills in 10 s, taps cost only what they burn, and a dry tank will not re-fire while the button is held |
| weather | 3 | all four weathers keep the understeer-first balance; rain and storm cost grip and braking distance, fog does not; races finish on wet tracks and storm laps are slower; the choice is saved per track |
| handling | 9 | straight-line stability and top speed; understeer at the limit; not on rails; handbrake drift and recovery; brake steer and per-car balance; progressive brakes and stopping distance; low-gear power slide; left/right symmetry; parking speed and reverse |
| net | 5 | rocket hits are relayed (`hitId`, `hitSeq`) and a real client's rocket blows up the other real client; the server's whole lobby/countdown/race/results cycle, places and finish checks, rejections, garbage packets, timeouts (with a Python client speaking the wire format); two real game clients racing; no-server handling. See `docs/NETWORK.md` |
| audio | 1 | soundtrack renders stereo at a sane level without clipping and with a rhythm |
| render (optional) | 2 | nine screens (including the night track and both options pages) render without shader errors and are not blank; MSAA, anisotropic filtering and the render size each change the picture |
| windows (optional) | 1 | the cross-compiled .exe under Wine simulates identically to Linux and races on a Linux server |

Not covered, and worth adding when you touch them: HUD text, menus and key handling, the save file,
gamepad input, opponent behaviour beyond "the race finishes", anything about how it feels.

### Adding a test

Tests are plain functions decorated with `@test` in `tests/run_tests.py`. Helpers:

- `sim(*flags, seconds=N)` runs `./fogcityfury --sim ...` and returns the JSON report as a dict.
- `flat(car, speed, script, seconds=N)` is a handling run on the flat plane.
- `dump(track)` returns the built world; `keyrun(track, key, ...)` drives a key's registered route;
  `find_key(kind)` finds a secret by kind (`pier`, `courtyard`, ...) so tests do not hard-code where it is.
- `Server(...)` (context manager) and `FakeClient` for multiplayer tests.
- `check(condition, "what", value)` and `between(value, lo, hi, "what")` record a pass/fail.

When you fix a bug, first write the scripted run that shows it, then the fix, then keep the run as
a test. Check that the test fails without the fix (the suite was validated this way: giving a car
far too much grip fails the handling group; disabling checkpoints fails the race group).

## Flags

Normal use: `--fullscreen`, `--size W H`.

Setup (work in every mode):

| Flag | Meaning |
|---|---|
| `--rockets N` | start with N rockets on board |
| `--fire T` | fire a rocket at race time T seconds (repeatable) |
| `--reset T` | press the reset key at race time T (repeatable) |
| `--press T:KEY` | press a key T seconds after the start (repeatable); goes through the same code as a real key press, so menus can be driven. KEY is an SDL key name: `F1`, `RETURN`, `ESCAPE`, `TAB`, `UP`, `X`... |
| `--hold KEY` | keep a key held for the whole run; the car is then driven through the key bindings instead of standing still |
| `--aa N`, `--af N`, `--renderh H` | anti-aliasing samples, anisotropic filtering level, height the scene is drawn at (not saved) |
| `--state 8`, `--optpage N` | start in the options (page 0 controls, 1 video) |
| `--touch`, `--tap T:LABEL`, `--touchhold LABEL` | show the on-screen buttons; tap one at time T; keep a finger on one for the whole run |
| `--server HOST` | use exactly these servers in the online list (repeatable) |
| `--countdown` | keep the fly-in and the real countdown with `--state 3` |
| `--ldr` | force 8-bit render targets (the fallback for phones without half-float targets) |
| `--track N` | 0 Hilltop Heights, 1 Bayside Breaker, 2 Summit Switchbacks, 3 Midnight Mile (night) |
| `--car N` `--paint N` | 0 Stallion GT, 1 Katana RS, 2 Bruiser 4x4; paint 0-7 |
| `--state N` | start in: 0 title (autopilot attract), 1 car select, 2 track select, 3 driving |
| `--mode N` | 0 race (opponents, clock), 1 free roam (neither) |
| `--weather N` | 0 clear, 1 rain, 2 fog, 3 storm. Also written to the save file for that track |
| `--save` | re-enable the save file in `--sim` (which otherwise never writes one) |
| `--pos X Z YAW` | put the player there (y is taken from the ground) |
| `--speed V` | initial forward speed in m/s |
| `--nosave` | do not read or write `~/.fogcityfury.sav` (`--sim` implies it) |
| `--flat` | replace the world with an endless level tarmac plane at y = 5: no walls, water or slopes |

Drivers (pick one; without any, `--sim` applies no input):

| Flag | Meaning |
|---|---|
| `--auto` | autopilot follows the centre line and brakes for corners |
| `--gas` | full throttle, no steering |
| `--drift` | autopilot plus handbrake in corners (for smoke and skid-mark pictures) |
| `--script "t:steer,gas,brake,hb[,nitro];..."` | timed inputs; each entry applies from race time `t` seconds. steer -1..1 (+ = left), gas/brake 0..1, hb and nitro 0/1 |
| `--waypoints "x,z;x,z;..."` `--wpspeed V` | steer at each point in turn holding V m/s, then brake to a stop |
| `--keyrun K` | start where hidden key K's recipe says and drive its waypoints (add `--gas` to go flat out with no steering instead) |
| `--connect HOST[:PORT]` `--name N` `--ready` | join a multiplayer server; with `--sim` the client steps in real time. `--ready --auto` makes a bot player |

Headless simulation:

| Flag | Meaning |
|---|---|
| `--sim` | no window; step at 60 Hz, print one JSON line, exit |
| `--seconds S` | how long to simulate (also sets `--skip` for `--shot`) |
| `--dump` | with `--sim`: print the built world (track length, start, checkpoints, keys with their kind and route start, ramps) instead of running |

Screenshots:

| Flag | Meaning |
|---|---|
| `--shot FILE.bmp` | simulate, draw, write a BMP, print the JSON report, exit |
| `--skip N` | frames to simulate without drawing first |
| `--frames N` | frames to draw before the capture (default 3; the reflection cubemap needs 2-3) |
| `--noui` | hide the HUD |
| `--top H X Z` | camera H metres above (X, Z) looking straight down, fog off |
| `--cam EX EY EZ TX TY TZ` | fixed camera position and target |

Audio: `--wav FILE SECONDS` renders the soundtrack. Environment `FCF_SOLO` is a bitmask: 1 guitars,
2 bass, 4 drums, 8 lead.

Environment `FCF_TRACE=1` prints a line to stderr every 0.1 s of simulated time: position, speed,
sideslip, front/rear slip ratios, yaw rate, grounded, steering, air time. This is the tool for
"where on the lap does it go wrong".

## The JSON report

One line on stdout from `--sim` and `--shot`.

| Field | Meaning |
|---|---|
| `track`, `car` | ids |
| `x`, `y`, `z`, `yaw` | player position (m) and heading (rad) |
| `vf`, `yawRate` | forward speed (m/s), yaw rate (rad/s) |
| `lap`, `cp` | laps started (1 = first lap), checkpoints passed in total |
| `trackLen` | centre-line length in metres |
| `keys` | bitmask of keys collected on this track (bit k = key k in `decorate()` order) |
| `dead` | 1 while wrecked or sunk and waiting to respawn |
| `place`, `timeLeft` | race position 1-6, seconds on the clock |
| `state` | 0 title, 1 car select, 2 track select, 3 race, 4 paused, 5 results, 6 online join screen, 7 online lobby |
| `finished`, `timeUp` | race ended by completing the laps / by the clock |
| `raceT`, `bestLap` | race time, best lap this race (s) |
| `maxAir`, `bigAirs` | longest single jump (s), number of jumps over 0.9 s |
| `marks` | skid-mark quads on the road |
| `wrecks`, `sinks` | explosions, drownings |
| `camOffAxis` | frames the chase camera's heading was more than 0.9 rad off the car's while the car was not sliding. Must be 0 |
| `maxBeta` | largest sideslip angle (rad) above 10 m/s on the ground: 0 = pointing where it is going, 1.57 = fully sideways |
| `maxSlipF`, `maxSlipR` | largest front / rear slip ratio: slip angle divided by the angle of peak grip. Above 1 the tyre is past its peak and sliding |
| `maxSpeed`, `maxYawRate` | maxima (m/s, rad/s) |
| `weather` | 0 clear, 1 rain, 2 fog, 3 storm |
| `nitro` | tank level now, 0..1 |
| `minNitro`, `boostT`, `boosts` | lowest the tank got, seconds spent boosting, times it fired |
| `dist`, `heading` | distance travelled (m), net heading change (rad, + = left) |
| `simT` | simulated seconds |
| `rockets`, `rocketsFired`, `rocketHits`, `pickups` | rockets on board, fired, that hit a car; crates collected |
| `wrecksLeft`, `wreckX/Y/Z`, `debris` | burnt shells and loose parts lying in the world; where the newest shell is |
| `resets`, `roadDist` | times the car was reset; distance (m) from the car to the nearest road centre line |
| `maxDeck`, `bridges` | greatest height of the grounded car above the terrain (a bridge deck is about 7.5); number of bridges |
| `aa`, `af`, `full`, `resW`, `resH`, `vsync`, `maxAF` | the video settings in force; highest anisotropy the driver offers |
| `renderW`, `renderH` | size the scene was last drawn at (0 in `--sim`) |
| `optPage`, `binds` | options page; key bindings as `key,second;...` (SDL scancodes) in the order of the controls screen |
| `net`, `netId`, `netPhase`, `netPlayers`, `netPlace`, `netRace`, `netRemoteMoved`, `netError` | multiplayer: connected, our id, server phase (0 lobby, 1 countdown, 2 race, 3 results), players now (`netMaxPlayers`: most seen at once), our place, in an online race, furthest any remote car was seen to travel (m), last error |

## Recipes

Measure cornering at the limit in the Katana (160 km/h, full left lock from 0.5 s):

    ./fogcityfury --sim --flat --state 3 --mode 1 --car 1 --pos 0 0 0 --speed 45 \
        --seconds 5 --script "0:0,1,0,0;0.5:1,1,0,0"

Find where on Hilltop the autopilot slides:

    FCF_TRACE=1 ./fogcityfury --sim --track 0 --car 0 --state 3 --auto --seconds 70 2>&1 >/dev/null \
        | awk '{b=$12; if (b<0) b=-b; if (b>0.4) print}'

Check a level edit from above (whole island, then close in on one spot), then drive a key's route:

    tools/shot.sh map --track 1 --state 3 --mode 1 --top 1330 0 0 --noui --size 900 900
    tools/shot.sh spot --track 1 --state 3 --mode 1 --top 250 -60 -640 --noui --size 900 700
    ./fogcityfury --sim --track 1 --state 3 --mode 1 --keyrun 2 --seconds 12
    tools/shot.sh look --track 1 --state 3 --mode 1 --keyrun 2 --skip 230

Picture of a handbrake slide on the road:

    tools/shot.sh slide --state 3 --mode 1 --pos 270 -445 1.72 --speed 32 --skip 100 \
        --script "0:0,1,0,0;0.3:1,0.5,0,0;0.7:1,0,0,1;1.3:-0.4,1,0,0"

Screenshots land in `shots/` as PNG. Look at them: a model can read the image file directly. The
software renderer matches a real GPU closely enough for layout, colour and geometry; it says nothing
about frame rate.

## What a passing suite does not tell you

That the game is fun, that the camera is comfortable, that the music is good, or that it runs on a
given GPU. Say so when you report results.

`--dump` also lists `bridges` (middle of each deck: `x`, `z`, `deck` height, ground height `under`) and `crates` (rocket pick-ups).
