# Scripted events and the event editor (fcfedit)

Any track can have a script of events: trigger lines drawn across the road, and things that happen when a car crosses
one, when a lap starts, at a race time, or after another event. The race director (`src/events.h`) plays them; the event
editor `fcfedit` (`src/evedit.h`) makes them.

## Making them

Use **fcfedit**. [FCFEDIT.md](FCFEDIT.md) is the guide: Warden misfires in one click, new moments click by click,
chains, preview and save. This page is the reference for what the editor writes.

## What every event has

| Setting | Values | Default |
| --- | --- | --- |
| set off by | a line, `lap` (a lap starting), `time` (race time), `after` another event | lap |
| laps | `all`, or any set of laps | all |
| delay | seconds from the trigger to it happening | 0 |
| how often | once a race, once a lap, every time (a second apart) | once a race |
| chance | 0-100%; the roll comes from the track and the event, so every game agrees | 100% |
| whose car | the player, any car, the leader, a computer car | the player |

Lines before the lines and events:

    pod p1  210 -380  r 60  n 3  k 1        dolphins roaming this ring of sea all race: middle, radius, how many, size
    drift 90                                the weather turns by itself about every 90 s (clear, rain, storm; clear, fog)
    roamminis off                           2.9.7: no groups of mini Wardens drop in while roaming this track (placed ones still come)
    pickup rocket  -480 9.7 -175            a rocket crate here (2.9.6); with any pickup lines, they replace the track's own
    pickup shield  -470 9.8 -170            a shield here (up to 8 count)
    pickup ghost   -460 9.8 -165            a ghost here (2.9.8.3: 7 s passing through other cars)
    race "STORM WARNING"  mode race  weather storm  laps 3   2.9.8.3: this file's career race (mode race, rampage or manic)

Kinds now (the editor's name in brackets where it differs): **rocket** (flies to its target and bursts there, or on the first wall it meets), **blast** (EXPLOSION), **splash**,
**shake**, **say** (MESSAGE: words on screen), **collapse** (BUILDING FALLS: the building there comes down),
**dolphin** (DOLPHIN LEAP: at `at` towards `to`; the nearest pod dolphin takes it, or one swims in; `k` its size),
**weather** (`wx clear|rain|fog|storm  over SECONDS`; offline only), **minis** (`k` mini Wardens, 1-4, on the roof at `at`
jetpack down to `to`; with `stay` they keep the roof's edge and snipe at you when you are on foot; with `gun mg` they fire machine-gun bursts at your car instead, 2.9.6). A rocket also has:

| Setting | Values |
| --- | --- |
| fired from | its own spot, Warden 1-4, or the nearest Warden (out of its cannon, wherever it has walked to) |
| fells (COLLAPSE) | yes: the building it is aimed at falls when the rocket hits. A Warden's misfire sets it by itself: on a building yes, on the ground no |

A building that falls drops into its own dust, throws out slabs and leaves rubble; at most four a race. The dolphin
and mini Wardens jetpacking down from the rooftops come next.

## The file

Plain text, one thing per line, so it can be read, diffed and edited by hand:

    warden 1  -200 -118
    line warden1  -267.0 -276.7  -238.0 -317.3
    event misfire1 rocket  to -147 21 -93  fells  from warden1  laps 1  on warden1
    event say0 say  text "WARDEN MISFIRE!"  laps 1  delay 0.4  on after misfire1

After saving a built-in track's events, run `python3 tools/embed_events.py`: it builds `events/` into the game
(`src/events_builtin.h`), which is what the Xbox 360 and Android read. The tests check it is up to date.

Online races don't play events yet: in 2.3 the server sends its track's event file to the players.
