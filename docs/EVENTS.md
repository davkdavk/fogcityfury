# Scripted events and the event editor (fcfedit)

Any track can have a script of events: trigger lines drawn across the road, and things that happen when a car crosses
one, when a lap starts, at a race time, or after another event. The race director (`src/events.h`) plays them; the event
editor `fcfedit` (`src/evedit.h`) makes them.

## Running the editor

    make fcfedit && ./fcfedit --track 0          # Linux (fcfedit.exe on Windows: make windows)
    ./fogcityfury --evedit --track 0             # the same editor inside the game

It opens on the track's event file. Built-in tracks keep theirs in `events/trackN.fcfe`; a custom track's sits beside
its `.fcft` file.

| Key / mouse | Does |
| --- | --- |
| Left click / drag | pick and move a line's end, an event, a rocket's target or a Warden's start |
| Right drag, wheel | move and zoom the map (fly view: right drag looks round) |
| `L` | draw a trigger line: click its two ends |
| `1`-`5` | place an event: rocket, blast, splash, shake, say |
| `6` | place where a Warden starts walking (Wardens 1-4, beside the road) |
| `7` | a building that comes down: click it |
| Panel on the right | click a setting to step it on, right-click to step back, SHIFT for finer steps |
| `DEL` | remove the selection |
| `CTRL+Z`, `CTRL+S` | undo, save (or the SAVE button, top right) |
| `P` | preview a race with the events as they are (saved or not); `F4` comes back |
| `TAB` | map view / fly view (`W A S D` move, `Q E` down and up, SHIFT fast) |
| `PGUP`, `PGDN` | the previous / next track (saves first) |

A rocket's target dropped on or near a building snaps to it, and the building is outlined (hold ALT to drop it on open
ground). With BRINGS IT DOWN on, that building falls when the rocket hits.

## What every event has

| Setting | Values | Default |
| --- | --- | --- |
| set off by | a line, `lap` (a lap starting), `time` (race time), `after` another event | lap |
| laps | `all`, or any set of laps | all |
| delay | seconds from the trigger to it happening | 0 |
| how often | once a race, once a lap, every time (a second apart) | once a race |
| chance | 0-100%; the roll comes from the track and the event, so every game agrees | 100% |
| whose car | the player, any car, the leader, a computer car | the player |

Kinds now: **rocket** (flies to its target and bursts there, or on the first wall it meets), **blast**, **splash**,
**shake**, **say** (a message on screen), **collapse** (the building there comes down). A rocket also has:

| Setting | Values |
| --- | --- |
| fired from | its own spot, Warden 1-4, or the nearest Warden (out of its cannon, wherever it has walked to) |
| brings it down | yes: the building it is aimed at falls when the rocket hits |

A building that falls drops into its own dust, throws out slabs and leaves rubble; at most four a race. The dolphin
and mini Wardens jetpacking down from the rooftops come next.

## The file

Plain text, one thing per line, so it can be read, diffed and edited by hand:

    warden 1  -170 -120
    line gate1  -143.7 -108.2  -125.5 -165.4
    event rocket1 rocket  at 8 62 -199.5  to -25 22 -95  laps all  delay 0  times lap  chance 100  by player  on gate1
    event shake1 shake  k 0.5  delay 1.6  times lap  on after rocket1
    event say1 say  text "INCOMING FROM THE ROOFTOPS!"  laps 2  on lap
    event misfire1 rocket  to -147 21 -93  fells  from nearest  laps 1  on gate0

After saving a built-in track's events, run `python3 tools/embed_events.py`: it builds `events/` into the game
(`src/events_builtin.h`), which is what the Xbox 360 and Android read. The tests check it is up to date.

Online races don't play events yet: in 2.3 the server sends its track's event file to the players.
