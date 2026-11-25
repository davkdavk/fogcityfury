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

Code brought in, with its authors, where it came from, its licence and what was changed:

| Project | Authors | Where it is | Licence | What was changed |
| --- | --- | --- | --- | --- |
| [Recast & Detour](https://github.com/recastnavigation/recastnavigation) (commit 9f4ce64, 2026-02-27) | Mikko Mononen and contributors | `third_party/recastnavigation/` (used by `src/nav.h` through `src/nav_lib.h`) | zlib ([License.txt](third_party/recastnavigation/License.txt)) | The files are unchanged; `Recast/` and `Detour/` Include and Source are laid flat in one folder. `nav_lib.h` renames a few file-local helpers while it reads them into the game's one translation unit. |
| [Quake III Arena](https://github.com/id-Software/Quake-III-Arena) bot AI (`code/game/ai_dmq3.c`, `ai_dmnet.c`) | Id Software, Inc. (1999-2005) | `src/showdown.h`: `q3Aggression`, `q3AttackMove`, `q3CheckAttack` and the fight nodes in `botThink` | GPL v2 or later | Adapted, not copied whole: `BotAggression`, `BotAttackMove`, `BotCheckAttack` and the Battle_Fight / Chase / Retreat / NBG nodes rewritten for this game's fighters, guns, metres and step rate; Quake's area system is replaced by Detour paths. |

The small behaviour tree in `src/bt.h` is our own; its selector and sequence nodes follow the shape of
[BehaviorTree.CPP](https://github.com/BehaviorTree/BehaviorTree.CPP) (MIT), none of whose code is used.

Projects we still mean to draw on:

| Project | For | Licence |
| --- | --- | --- |
| [TORCS](https://sourceforge.net/projects/torcs/) and [Speed Dreams](https://www.speed-dreams.net) | racing lines, braking and overtaking for the computer drivers | GPL v2 or later |
| [SuperTuxKart](https://github.com/supertuxkart/stk-code) | drifting and weapon use for the computer drivers | GPL v3 or later |

## Adding someone else's code

- Its licence must allow use in a GPL v3 program (zlib, MIT, BSD, Apache 2.0, GPL v2-or-later and GPL v3 do;
  GPL v2-only does not).
- Keep its copyright and licence notices in the files, and add a row to the table above.
- Models, textures and sounds: your own work, CC0, CC-BY or CC-BY-SA, with a credit line. No ripped game assets.
- Never anything from Microsoft's leaked XDK.
