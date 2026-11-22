# The track editor and track files

Custom tracks are made in the game (PC: mouse and keyboard) and saved as small text files (`.fcft`).
The code is `src/editor.h` (the editor) and `src/trackfile.h` (the file format).

## Using it

On the track screen: **F4** edits the track shown (a built-in track is copied first; the original is
never changed) and **F6** starts a new one. Custom tracks appear in the track list after the six
built-in ones. They are offline only. Their keys are the ones you hide with the KEYS tool (which keys you have found
on each is kept with your settings). **Del** on the track screen deletes one of your own tracks (its file goes).

The editor shows the island from straight above. Every change rebuilds the world a moment later
(about a third of a second) and saves the file.

| Tool (Tab, or 1-6) | Mouse | Keys |
|---|---|---|
| 1 Road | Drag a point. Click the yellow line to add a point. Right-click a point to remove it. | Del removes the chosen point |
| 2 Hills | Click to add a hill, drag to move it. Right-click removes it. | + and - height (below 0 digs water), 9 and 0 width |
| 3 Tunnel | Click the road where it goes down, then where it comes up. Drag the ends. Click a shortcut to put the whole shortcut in a tunnel; right click it to bring it back up. | + and - depth, Del removes it |
| 4 Shortcuts | Click the road where the shortcut leaves, then where it rejoins. Drag the ends. Right-click an end to remove it. | Del removes the chosen one |
| 5 Jumps | Click on or beside the road to put a jump there (it faces the way you drive), drag it. Right-click removes it. | + and - height, 9 and 0 length, R turns it round, Del removes it |
| 6 Keys | Click to hide a key (up to five), drag it. Right-click removes it. | + and - raise it off the ground (into the air past a jump, or onto a roof) |

Everywhere: wheel (or PgUp/PgDn) zooms, right-drag or the arrow keys move the map, **Z** undoes,
**F2** renames, **K** sky, **X** weather, **M** song, **C** how much city, **G** a new city layout,
**Enter** drives the track (free roam), **T** races it, **F4** comes back from the drive, **Esc** is done.

### On a touch screen (Android)

On the track screen tap **EDIT** or **NEW** (and **DELETE** for one of your own). In the editor the tools are the
row of buttons along the bottom; the rest are down the sides: **DRIVE**, **UNDO**, **DEL**, **+**, **-**,
**WIDE**, **SLIM**, **TURN** and **DONE** on the right, **NAME**, **SKY**, **WEATHER**, **SONG**, **CITY** and
**LAYOUT** on the left. One finger does what the left mouse button does: drag a point, tap the road to add one.
Dragging on empty map moves the map; two fingers move it and pinch to zoom. Tap a tunnelled shortcut again
with the TUNNEL tool to bring it back up. While test-driving, pause and tap **EDITOR** to come back.

### With a gamepad

On the track screen **X** edits and **Back** starts a new track. In the editor the left stick moves a pointer
and **A** is the mouse button (hold it to drag). The right stick, or pushing the pointer against an edge,
moves the map; the triggers zoom. **LB/RB** change tool, the d-pad is + and - (up/down) and narrower/wider
(left/right), **X** deletes, **Back** undoes, **Y** or **Start** drives the track, **B** is done. Click the
sticks for sky (left) and weather (right). While test-driving, pause and press **Y** to come back.

The editor is not in the Xbox 360 version: tracks cannot be saved there.

### Ground

The editor decides the ground from the finished shape, every time it rebuilds: road, kerb and shoulder along
the road, paving and buildings in town, grass in the fields, rock on steep slopes, and sand for about 13 m
back from any water (the sea or a lagoon you dug). Town paving and buildings keep off the beach. Moving a road
or digging a lagoon therefore never leaves the old ground behind.

The panel on the right lists what is wrong with the track as built (tight corners, very steep
slopes, a tunnel that never goes under, a shortcut that could not be built); red rings mark the
places. A track with warnings can still be driven.

Limits: 4 to 60 road points, 24 hills, 3 shortcuts, 2 tunnels (click the road twice more for a second one). A tunnel or a shortcut cannot cross
the start line; a shortcut skips less than 45% of a lap; a tunnel needs 260 m to get down and up.
A tunnel gets its glass roof wherever it runs under water, so dig a lagoon (a hill below 0) on its way.
A shortcut in a tunnel goes down about 60 m before it reaches the sea and comes back up just past the far shore
(all on land, it stays under the whole way). It needs 260 m too. A shortcut cannot leave or rejoin the road inside a
tunnel: its end is moved to the tunnel's mouth.

## Where the files are

| System | Folder |
|---|---|
| Linux | `~/.fogcityfury/tracks/` |
| Windows | `%APPDATA%\fogcityfury\tracks\` |
| Android | the app's own storage, `tracks/` |

Every `.fcft` file in the folder is loaded when the game starts (up to 40). `--tracksdir DIR` uses
another folder; `--trackfile FILE` loads one more file and picks it; `--exporttracks DIR` writes the
seven built-in tracks as files (good examples to copy).

## The file

One setting per line. Lines the game does not know are skipped. `#` starts a comment.

    fcft 1
    name    HARBOUR RUN
    sub     A FAST LOOP ROUND THE DOCKS
    seed    7
    land    base 10  noise 3  radius 562  city 0.40
    sky     top 0.07 0.22 0.62  horizon 0.40 0.56 0.80  fog 0.0008
    sun     dir 0.45 0.62 0.38  colour 2.5 2.3 1.95
    night   0
    song    3
    weather rain
    road    -460 -240  -290 -325  -120 -120  120 120  300 315  480 215
    hill    300 318 16 45
    branch  0.75 0.96
    branch  0.46 0.61  210 -40  260 10
    tunnel  0.30 0.42  depth 19  glass 0.33 0.39

| Line | Meaning |
|---|---|
| `fcft 1` | Must be there: the format's version. |
| `name`, `sub` | The track's name (26 letters) and the line under it (52). Capitals, digits, space and `- ' . ! ,`. |
| `seed` | The city's layout: another number, other buildings. |
| `land` | `base` ground height (4-18 m), `noise` roughness (0-5), `radius` of the island (380-640 m), `city` 0.2 (all city) to 0.8 (hardly any). |
| `sky`, `sun` | Colours of the sky's top and horizon, fog density, the sun's direction and colour. |
| `night` | 1: lit windows, stars, headlights. |
| `song` | 1-4, 6 or 7. Left out: the built-in one for its place in the list. |
| `weather` | `clear`, `rain`, `fog` or `storm`: what the track starts with. |
| `road` | The loop's control points, `x z` in metres from the middle of the map (each within 640). Several `road` lines add up. The first point is the start line. |
| `hill` | `x z height radius`. A height below 0 digs; deep enough and it fills with water. |
| `branch` | A shortcut: the fractions of a lap where it leaves and rejoins, then (optional) points it passes through. |
| `tunnel` | The fractions of a lap where the road starts down and is back up, `depth` below the sea (12-26 m), `glass` the fractions between which the roof is glass. Up to two `tunnel` lines. `tunnel shortcut K depth D` puts shortcut K (counting the `branch` lines from 0) in a tunnel instead. |
| `island` | `x z radius`: one island (up to four). Leave them all out for one round island of `land radius`. |
| `wardens` | How many Wardens walk the track, 0 to 4 (2 if left out). |
| `laps` | `x2` or `x3`: an endurance track, raced at that many times the laps picked. |
| `jump` | `u lat  length L  height H` (and `reverse` to face against the way you drive): a jump at that fraction of the lap, that far off the middle of the road (m). Up to twelve. The bots steer round them. |
| `key` | `x z  height H`: a hidden key, H metres above the ground. Up to five. A key just past a jump, in line with it, is reached off the jump. |

Every number is clamped to what the track builder can take (`trackSanitize`), so a hand-made file
cannot crash the game; a file with fewer than four usable road points is not loaded.

## For developers

- A custom track is a `TrackDef` appended to `TRACKS` (index `NTRACKS` and up, at most `MAXT`). `TRACKS`
  is reserved to `MAXT`, so a `TrackDef` never moves and `World::def` stays valid.
- `TrackDef::deco` says which built-in set of hidden keys a track has; -1 (every custom track) is none.
- `World::gen` counts builds. Anything cached per track must compare `W.gen`, not `W.id`: the editor
  rebuilds the same id (the Warden routes and UFO landings do this).
- Saves, the online protocol and the championship know only the built-in tracks.
- Tests drive the editor without a window: `--ednew` / `--edit` open it, `--edscript "tool 1;down X Z;move X Z;up;key plus;wait"`
  works the mouse in map metres (see `ed::script`). The tests are `track_files_round_trip`,
  `track_files_cannot_crash_the_game` and `the_editor_makes_a_track_you_can_race`.
- Not on the Xbox 360 build (`FCF_GFX_XSDK`): no folder scan and no editor there.
