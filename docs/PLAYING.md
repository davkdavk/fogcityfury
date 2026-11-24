# Fog City Fury: Player's Guide

Fog City Fury is an arcade street racer: steep hills and big air, rockets and nitro, exploding wrecks,
undersea tunnels, shortcuts and hidden keys. Race nine opponents, roam freely, fight it out on foot after
a rampage race, race friends online, and build and share your own tracks.

This guide is for players. If you want to build the game yourself or work on its code, see `README.md`.

## Contents

1. [Getting the game](#getting-the-game)
2. [Controls](#controls)
3. [Game modes](#game-modes)
4. [Racing tips: nitro, rockets, wrecks, weather](#racing-tips)
5. [On foot](#on-foot)
6. [Free roam: mini Wardens, flowers and levels](#free-roam)
7. [Hidden keys](#hidden-keys)
8. [Playing online](#playing-online)
9. [Making your own tracks](#making-your-own-tracks)
10. [Options](#options)
11. [Where your progress is saved](#where-your-progress-is-saved)
12. [Running your own server](#running-your-own-server)
13. [Troubleshooting](#troubleshooting)

## Getting the game

Download the version for your system from the **Releases** page on GitHub
(<https://github.com/davkdavk/fogcityfury/releases>). Nothing needs installing.

| System | What to download | How to start it |
|---|---|---|
| Windows (64-bit) | `fogcityfury.exe` | Double-click it. Needs a graphics driver with OpenGL 3.3 (any PC from the last ten years) |
| Linux (64-bit) | the Linux client | `./fogcityfury` in a terminal (add `--fullscreen`). Needs glibc 2.31 or newer |
| Android | the `.apk` | Open it on your phone and allow installing from this source. Android 5.0 or later |
| Xbox 360 | the 360 release (`.xex`) | Homebrew: a modded (RGH/JTAG) console only. Copy it over and launch it from your dashboard (Aurora, FreeStyle, XeXMenu) |

Windows may warn that the program is from an unknown publisher: choose **More info**, then **Run anyway**.

## Controls

The game works with a keyboard, a gamepad (Xbox-style) or a touch screen. Every keyboard key for driving
can be changed in **Options** (F1).

### Driving

| Action | Keyboard | Gamepad |
|---|---|---|
| Steer | Left / Right, or A / D | Left stick |
| Accelerate | Up, or W | Right trigger, or A |
| Brake / reverse | Down, or S | Left trigger, or B |
| Nitro | Shift | Right shoulder |
| Fire a rocket | F, or E | Left shoulder |
| Handbrake (drift) | Space | X |
| Back on the road | R, or Backspace | Back |
| Change camera | C | Y |
| Get out of the car / back in | G | |
| Change weapon (on foot) | Q | |
| Aim down the sights (on foot) | Right mouse button (hold) | Left trigger (hold) |
| Music on / off | M | |
| Pause | Esc | Start |
| Full screen | F11 | |
| Options | F1 (title screen or pause menu) | X (in the menus) |

### Menus

Up / Down / Left / Right move around, **Enter** (pad A) chooses, **Esc** (pad B) goes back.

### Touch screens

On Android the buttons are always on screen; on a Windows or Linux touch screen they appear the moment
you touch it. Steer with the stick on the left; the buttons on the right accelerate, brake, fire and so on.

## Game modes

Choose the mode on the track screen.

- **Race.** You against nine opponents over 1 to 9 laps. Each checkpoint adds time to the clock: run out
  and the race is over. Press Enter during the fly-over at the start to skip it.
- **Roam.** No clock and no traffic: explore, hunt for keys, and take on the mini Wardens (see
  [Free roam](#free-roam)).
- **Rampage.** A race, and then the top five finishers fight it out on foot somewhere on the island. The
  last one standing wins the podium.
- **Champ (championship).** Four rampage races in a row on different tracks, with points for each.
- **Manic.** A time trial: your laps against the clock over the laps you set.
  - The other cars still drive round, but as see-through ghosts you pass straight through.
  - There are no rockets or shields on the track, nobody fires, and there's no countdown clock.
  - Each lap's time shows as you cross the line (green if it beats your best), and each checkpoint shows a
    split against your best lap so far.
  - The results list every lap and your total. The best total for each track and lap count is kept as the
    record to beat.
- **Online.** Race other people (see [Playing online](#playing-online)).

There are seven tracks: Hilltop Heights, Bayside Breaker, Summit Switchbacks, Midnight Mile, Harbour Deep,
Lagoon Loop and Twin Isles (an endurance track: races are twice the laps you pick). Four cars, each with its
own character: RipDog (quick off the line, good in the dirt), Stallion GT (all-rounder), Katana RS (fastest,
poor off-road) and Bruiser 4x4 (slower, goes anywhere).

The whole island is drivable: leave the road, cut corners, drive along the backstreets or over roofs.
Deep water sinks you, and hitting a wall at over about 110 km/h blows the car up.

## Racing tips

**Nitro.** Hold Shift (right shoulder) for a hard push: about 25% more top speed. A full tank lasts
2.5 seconds and refills in 10. Run it dry and it locks until you let go and it builds back up. It only works
with the wheels on the ground, and not before the green light.

**Rockets.** Drive through a green crate for two rockets (one set per lap). A red LOCK bracket marks the
car they will chase; press F to fire. A car that is hit explodes and is out for a few seconds.

**Rocket start.** Press accelerate in the last moment before the lights go green (less than half a second before) for a burst off the line. Holding it down any earlier doesn't count.

**Wrecks.** Every car that blows up leaves a burning shell for about ten seconds. The shell is solid, so
drive round it.

**Stuck or facing the wrong way?** Press R (pad Back) to be put back on the road, pointing the right way.

**Shortcuts and tunnels.** Many tracks have shortcuts off the main road, and some have glass-roofed
tunnels under the sea. The opponents use them too.

**Weather.** Pick it per track on the track screen. Rain (about 18% less grip, puddles, spray), fog (you
can't see the corners coming) and storm (heavy rain, lightning, about 26% less grip) all change how you
have to drive. Online, the server picks the weather.

## On foot

Press **G** to get out of the car and walk, run and fight; press it again near your car to get back in. In
a rampage showdown everyone is on foot. **F** fires, **Q** changes weapon, and **Shift** sprints (it uses
stamina). Weapons are fists, pistol, rifle, SMG, shotgun and grenade launcher; you start with your fists
and pick the others up.

**Aiming down the sights:** hold the **right mouse button** (on a pad, the **left trigger**). The camera
comes in over your shoulder and zooms, a crosshair appears (it turns red over something you would hit),
and you face where you look. Left and right now **strafe** instead of turning, and your shots go exactly
where the crosshair is, high or low, so you can pick off mini Wardens sniping from the rooftops. Let go
and the usual lock-on aiming is back.

**Pick-ups in town:** boxes of rounds (olive, with a brass band) and medkits lie on the paths between the
buildings, away from the road, about 30 on each track. Each box of rounds tops up every gun you have, and
a medkit gives back 50 health. Each comes back 40 seconds after it is taken.

**Hits:** a shot to the head does double damage (a yellow marker and a bright tink), and numbers rise off
whatever you hit (white body, yellow head, blue armour). The crosshair opens as you fire fast, so short
bursts land better than holding the trigger down. Each gun kicks the view up a little; it settles back on its
own. The pistol, SMG and shotgun lose damage at range, but the rifle doesn't.

**Rooftop snipers:** on some tracks, mini Wardens wait at the edge of a roof. They leave drivers alone, but
once you're on foot they shoot at you when they can see you. Aim down the sights and any gun brings them
down.

**Dolphins** can't be hurt. Shoot one and it laughs at you.

## Free roam

In Roam mode, **mini Wardens** (person-sized versions of the giant Wardens) drop in now and then, alone or
in groups of three. There is no warning: each one shows as a **red dot** on the minimap. They jetpack towards
you, fight hand to hand when you are on foot, and shoot at your car when you are driving.

- **Fists, pistol, SMG and shotgun** hurt them. Rifle rounds and grenades bounce off their armour.
- Each one you beat drops **flowers**: walk or drive over them to collect them.
- **Don't run them over.** You may, but you lose the flowers you were collecting towards your next level,
  and a big Warden fires a rocket at you.
- Your car has **health** in Roam: it smokes when badly hurt and blows up at zero.

Flowers add up to **levels**, and each level unlocks a lasting bonus:

| Level | Flowers | Bonus |
|---|---|---|
| 1 | 10 | Tough skin: more health on foot |
| 2 | 25 | Iron lungs: sprint for longer |
| 3 | 45 | Steady aim: your shots spread less |
| 4 | 70 | Armour plating: start on foot with armour |
| 5 | 100 | Hard hitter: your shots do more damage |
| 6 | 140 | Reinforced car: gunfire and rockets hurt your car less |
| 7 | 190 | Nitro tank: nitro refills faster |
| 8 | 250 | Green thumb: double flowers |

The bonuses count offline. Online, everyone is equal.

### The golden UFO and the space race

Somewhere off the road on every track, a **golden UFO** hovers over a clearing, with a beam of light going
up into the sky so you can find it from far away. It is only there in Roam (offline, or on a roam server).

Drive (or walk) under it and stay still for two seconds. Ten seconds later you are racing on the secret
**Space Ring**: a road of see-through green slime with gold and silver stars inside, wrapped round a planet
covered in the game's islands, with nothing but stars underneath. The strip of rock each side is narrow:
go over the edge and you fall into space (you are put back on the road a moment later).

Offline it is you against the bots, and afterwards you go back to the title screen. On a roam server
everyone goes to space together, and after the race the server goes back to roaming.

## Hidden keys

Every built-in track (except Twin Isles) hides **five keys**: on roofs, down backstreets, at the end of
a jump. Roam mode is the easiest way to look for them. Listen for a faint metal jingle: you hear it when a key you haven't found is close. Collect all of them to unlock the **gold chrome**
paint. One key on every track is inside a warehouse with its roller door open, at the end of a backstreet
lined with orange lamps.

Tracks you make yourself can have their own hidden keys too (see below).

## Playing online

1. On the title screen press **O** (pad Y). The server list shows each server's name, track, players and
   ping. `thadav.com` is the public server.
2. Choose a server with Up / Down and press **Enter**. **A** adds a server by address, **Del** removes one,
   and **N** sets your player name.
3. Pick your car, then press **Enter** in the lobby when you are ready. The race starts when everyone is
   ready. Up to 10 players.

After each race the server moves on to the next track.

### Sharing your own track

In the lobby press **T** (pad Y, or SHARE TRACK on a touch screen), pick one of your tracks and press
Enter. Everyone else sees a **map of your track** and votes: **Y / Enter** (pad A) to play it, **N / Esc**
(pad B) for no. If more say yes than no, everyone races it and **keeps a copy** in their own tracks.
Afterwards the server goes back to its own tracks.

You can share one track every two minutes.

## Making your own tracks

On a PC, go to the track screen and press **F4** to edit the track shown (a built-in track is copied, never
changed) or **F6** to start a new one. Number keys pick the tool:

| Tool | What it does |
|---|---|
| 1 Road | Drag the road's points; click the road to add a point, right-click a point to remove it |
| 2 Hills | Add hills, and lagoons (hills below sea level) |
| 3 Tunnel | Click where the road goes down and where it comes back up, for an undersea tunnel. Click a shortcut to put the whole shortcut in a tunnel |
| 4 Cuts | Click where a shortcut leaves the road and where it rejoins |
| 5 Jumps | Place ramps by the road |
| 6 Keys | Hide up to five keys for others to find |

On a phone, tap **EDIT** or **NEW** on the track screen: the tools are buttons along the bottom, one finger
drags and taps, two fingers move the map and pinch to zoom. With a gamepad, **X** edits and **Back** starts a
new track; the left stick moves a pointer and **A** clicks. The full list is in
[EDITOR.md](EDITOR.md). (The Xbox 360 version has no editor.)

Press **Enter** to drive your track at once and **F4** to come back to the editor. You can also pick the sky,
the weather and the song. Your tracks save automatically and appear in the track list. **Del** on the track
screen deletes one of your own tracks.

Each track is a small `.fcft` text file in your tracks folder (see below). Send the file to a friend and
they can drop it into their tracks folder, or share it online from the lobby.

## Options

Press **F1** on the title screen or in the pause menu. **Tab** switches between the two pages.

- **Controls.** Pick an action, press Enter, then press the key you want. Each action has two keys. Del
  clears one, F5 puts the defaults back.
- **Video.** Window or full screen, resolution, anti-aliasing, anisotropic filtering (keeps the road sharp
  into the distance), vsync, and **brightness** (lower it if the picture looks washed out; 100% is the
  original look). On a slow computer, a lower resolution in full screen is a real speed-up.
- **Effects** (on the Video page). **Water rings**: the size of the rings cars and splashes leave on the water
  (off up to 300%). **Splash mist**: how much spray goes up when something lands in the water (off up to 200%).
  **Grass**: how thick the grass grows (off up to 150%; turn it down on a slow machine). The Xbox 360 keeps its
  own fixed grass and has no water rings.

## Where your progress is saved

| | Progress | Settings | Your tracks |
|---|---|---|---|
| Windows | `%APPDATA%\fogcityfury.sav` | `%APPDATA%\fogcityfury.cfg` | `%APPDATA%\fogcityfury\tracks\` |
| Linux | `~/.fogcityfury.sav` | `~/.fogcityfury.cfg` | `~/.fogcityfury/tracks/` |
| Android | inside the app | inside the app | inside the app |
| Xbox 360 | not saved yet | not saved yet | not saved yet |

To type `%APPDATA%` paths, paste them into the address bar of a File Explorer window.

## Running your own server

Download the server for your system from the Releases page (Linux, Windows, and ARM for a Raspberry Pi) and
run it:

    ./fogcityfury-server

It needs no graphics and listens on **UDP port 27015**. Players on your network join it by your computer's
address. For players over the internet, forward UDP port 27015 on your router to that computer.

Useful options (`./fogcityfury-server --help` lists them all):

| Option | What it does |
|---|---|
| `--name NAME` | the name shown in the server list |
| `--laps N` | laps per race |
| `--track N` | the first track (0 to 6) |
| `--weather 0-3` or `random` | clear, rain, fog, storm |
| `--tracks FOLDER` | also run the custom tracks in that folder; players download them automatically |
| `--roam` | a free-roam server: everyone drives and walks the same island |
| `--no-uploads` | players can't share tracks |

## Troubleshooting

**The game won't start on Windows / Linux.** Update your graphics driver: the game needs OpenGL 3.3. On
Linux, if the downloaded build won't run on your distribution, build it from source (see `README.md`).

**It runs slowly.** In Options > Video, lower the resolution, turn anti-aliasing off, or turn vsync off.

**The picture looks too bright or washed out.** Options > Video > Brightness.

**"SERVER VERSION MISMATCH" online.** You and the server have different versions. Download the latest
release.

**"SERVER IS FULL".** All 10 places are taken, or that address already has the most players allowed from one
connection (4).

**"NO RESPONSE FROM SERVER" or "CONNECTION LOST".** The server is down, the address is wrong, or a firewall
is blocking UDP port 27015.

**A shared track didn't start.** The lobby voted no, or you shared another one less than two minutes ago.

**Xbox 360: progress is lost.** The 360 version does not save yet.
