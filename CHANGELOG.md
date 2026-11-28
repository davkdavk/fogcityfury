# Fog City Fury changelog

Fog City Fury is two things growing together: **the game** (an arcade street racer where the top five fight it out on
foot) and **the A12 Engine** under it (PC, Android and a homebrew Xbox 360 build, editors, AI and effects).
Every change below is marked **Game** (what you play) or **Engine** (what it is built on). Newest first; the GitHub
releases keep these milestones only. Full notes for each version are in the README's "What's new" sections.

| Version | Game | Engine |
| --- | --- | --- |
| 2.9.9.7 | https stations through curl (hidden on Windows); music/radio.pls or .m3u (or the playlist pasted in radio.txt), each entry tried in turn; the built-in G-funk songs off (cfg `gfunk 1`) | Web: the playlist's first entry |
| 2.9.9.6 | Internet radio in the races (music/radio.txt or --radio URL; ICY/HTTP MP3 streams, .pls/.m3u, redirects; M: RADIO / OWN / OFF) | The menus keep main.mp3; the web game plays https streams in the browser |
| 2.9.9.5 | Your own music: music/<track>.mp3 per track (main for the menus, showdown, custom tracks by name), or a `music NAME` line in the event file; looped, cross-faded; the web game fetches them beside its page | The lobby's event files pick each track's song for everyone (minimp3, CC0) |
| 2.9.9.4 | A race starts in the weather its event file starts it in (no snap at the green light) | The lobby waits in that sky; the server sets it before the countdown |
| 2.9.9.3 | Bots get one rocket a lap and pick up nothing; wrecks are ghosts you drive through (flicker, gone in 5 s); no more fling at the road's edge; AI drivers Easy / Medium / Hard (Options, sticks page; Hard = as before); the web game is online only | Lobbies play the events' weather online (server-driven, blended on clients); `deploy/lobbies.conf` per-lobby flags, tracks rotate, `--events`; lobbies.sh restart loop |
| 2.9.9.2 | | Browser players in the lobbies: `fogcityfury-wsbridge` (WebSocket to the lobbies' UDP), the web game online over WebSockets, Apache `ProxyPass /fcf/lobby/`; lobbies.sh starts the bridge |
| 2.9.9.1 | | `deploy/fcf-lobbies.sh`: eight 10-player lobbies as systemd services (UDP 27015-27022); the game lists all eight |
| 2.9.9 | | The game in a web page (3.0, step 1): `make web`, WebAssembly + WebGL2, saves in the browser; docs/WEB.md |
| 2.9.8.6 | | `tools/blender/fcf_car.py`: any car model (any format, a zip) into the game in one command; added cars can be hidden behind a code typed on the main menu |
| 2.9.8.5 | Wardens stand facing their first misfire, cannon on it, then walk | fcfedit: the track's own crates, shields and ghosts can be moved and deleted; each can come out from a set lap |
| 2.9.8.4 | | The track editor edits a built-in track itself (saved as `events/trackN.fcft`, read on PC and 360), not a copy; buildings coming down no longer choke the 360 |
| **2.9.8.3** | **The career: five races per track, you start where you last finished; ghosts; held shields and ghosts; rockets held by level** | fcfedit edits each track's five race files |
| 2.9.8.2 | | The 360 finds the events folder beside the XEX on the console |
| 2.9.8.1 | The car drives itself at the results until you roam; RACE OVER | |
| 2.9.8 | | The Xbox 360 reads an events folder beside the XEX |
| 2.9.7 | | fcfedit: ROAM MINIS on/off per track |
| **2.9.6** | Machine-gun mini Wardens | fcfedit places rocket crates, shields and single mini Wardens |
| **2.9.5** | Android steering stick; steering and aim sensitivity for pads and touch | Options get a STICKS page |
| **2.9.4** | | fcfedit's preview tape: pause, rewind, step and 0.1x–4x playback, events marked on the race clock; races replay exactly |
| **2.9.3** | Roam on after the race: the race's world carries on as free roam, after a rampage's showdown too | |
| **2.9.2** | Rival cars take damage, show their health, smoke, burn and get wrecked; hard knocks put them off the road | |
| **2.9.1** | Rival drivers back to the 2.8 feel, now steering through lane changes and overtaking cleanly; stock tracks left unscripted for your own events; guns only in the rampage ring; Android on-foot controls (weapon menu, finger to aim and fire, CROUCH) | Showdown cars sit on the road and pitch with the ground |
| 2.9 | *Withdrawn: racing-line drivers, replaced by 2.9.1* | |
| **2.8** | Showdown bots fight like Quake III's; circle-strafing | Recast & Detour pathfinding on foot; a small behaviour tree; GPL v3 licence and credits |
| 2.7.2 | Rampage bots take the quickest way to the ring | Time-costed route map for cars |
| 2.7.1 | Triggers as analog pedals; LB aims down the sights | 360 triggers read over their whole pull |
| **2.7** | Magazines, reloads, quick swaps, gun rarity; jump, crouch, slide, climb; the racer reworked; sniper lasers; mini Wardens in cover; armour plating | |
| **2.6.3** | Car damage; every weapon hurts mini Wardens; INCOMING! warnings; higher dolphin leaps | Car route map so rampage bots reach the ring |
| 2.6 – 2.6.2 | Gunplay hit feel (headshots, damage numbers, bloom, recoil); MANIC against your own ghost | |
| **2.5** | MANIC: a time trial with ghosts, splits and records | |
| **2.4** | Aim down the sights, strafing, rooftop snipers, ammo and medkits in town, dolphins that laugh | |
| **2.3** | Dolphins, mini Wardens jetting off roofs, storms rolling in | Swell and tide on the sea, weather that blends, the event editor complete, events shared online |
| **2.28 – 2.29.2** | Warden misfires that bring buildings down | The race director (scripted events) and fcfedit, the event editor |
| **2.27** | The Ronin 120 | The Blender conduit: textured models on every platform |
| **2.25 – 2.26** | | One Xbox 360 build, shader layouts from a table; the code split by job |
| 2.11 – 2.24 | A town that looks like one; a livelier Warden with a tracking minigun | Anti-aliasing, grass in rings, the 360 space track |
| **2.0 – 2.2** | The golden UFO and the secret space track | Touch and gamepad track editor, swaying grass, splash rings, beaches |
| **1.74** | | The Xbox 360 build plays online |
| **1.36** | Championship: four rounds on four tracks | |
| 1.31 – 1.35 | The showdown: top five fight on foot anywhere on the map; ten cars; keys unlock guns; online play | Multiplayer protocol |
| **1.0** | The first release | Release pipeline for every platform |

Versions in **bold** keep a GitHub release with downloads.
