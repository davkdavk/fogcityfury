# Credits

Fog City Fury is made by Dave (davkdavk) with contributors. The game, its server, its editor and its tools are free
software under the GNU General Public License, version 3 or (at your option) any later version: see [LICENSE](LICENSE).

## What the game is built on

| What | Used for | Licence |
| --- | --- | --- |
| [SDL 2](https://www.libsdl.org) | windows, input and sound on PC and Android | zlib |
| [xsdk](https://github.com/xenosworks/xsdk) by Mena / xenosworks and contributors | the Xbox 360 build (no Microsoft tooling, no leaked SDK material) | MIT |
| "Dolphin" by Tarnyloo (Blend Swap #83681) | the dolphins: see [models/dolphin/README.md](models/dolphin/README.md) | CC-BY 3.0 |

Everything else (the cars, the racer, the Wardens, the towns, the sound and the music) is made by the game's own code.

## Code and ideas from other projects

Nothing from another project is in the game yet. When code is brought in, it is listed here with its authors, where
it came from, its licence and what was changed. Projects we mean to draw on:

| Project | For | Licence |
| --- | --- | --- |
| [Recast & Detour](https://github.com/recastnavigation/recastnavigation) | finding a way round buildings on foot | zlib |
| [BehaviorTree.CPP](https://github.com/BehaviorTree/BehaviorTree.CPP) | how enemies decide what to do | MIT |
| [TORCS](https://sourceforge.net/projects/torcs/) and [Speed Dreams](https://www.speed-dreams.net) | racing lines, braking and overtaking for the computer drivers | GPL v2 or later |
| [SuperTuxKart](https://github.com/supertuxkart/stk-code) | drifting and weapon use for the computer drivers | GPL v3 or later |

## Adding someone else's code

- Its licence must allow use in a GPL v3 program (zlib, MIT, BSD, Apache 2.0, GPL v2-or-later and GPL v3 do;
  GPL v2-only does not).
- Keep its copyright and licence notices in the files, and add a row to the table above.
- Models, textures and sounds: your own work, CC0, CC-BY or CC-BY-SA, with a credit line. No ripped game assets.
- Never anything from Microsoft's leaked XDK.
