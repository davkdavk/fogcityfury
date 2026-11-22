# Multiplayer

Up to 10 players race over UDP through a dedicated server. The code is in three files:

| File | Role |
|---|---|
| `src/net.h` | The wire format and socket helpers. Shared by both sides. No SDL or OpenGL. |
| `src/server.cpp` | `fogcityfury-server`: a standalone headless program (about 140 lines). |
| `src/netclient.h` | The game's client: connect, send our car, receive snapshots, move remote cars. |

`netStartRace`, `netReact` and `netLeave` in `game.cpp` connect the client to the game's state
machine; the lobby/join screens are in `drawHUD` and `onKey`.

## Running it

    make all                                   # builds fogcityfury and fogcityfury-server
    ./fogcityfury-server                       # UDP port 27015, 3 laps, rotates through all tracks
    ./fogcityfury                              # press O on the title screen, type the address, Enter
    ./fogcityfury --connect HOST[:PORT] --name YOU     # or skip the menu

Server options:

| Flag | Default | Meaning |
|---|---|---|
| `--port N` | 27015 | UDP port to listen on |
| `--laps N` | 3 | laps per race (1-20) |
| `--track N` | 0 | first track; `--fixed-track` stops it rotating after each race |
| `--weather N` | 0 | 0 clear, 1 rain, 2 fog, 3 storm, or `random` for a new one each race. The server's choice overrides each client's |
| `--countdown S` | 5 | seconds from "everyone ready" to the green light |
| `--lobby S` | 0 | 0: start when every player is ready. Otherwise start S seconds after the first player is ready |
| `--finish-timeout S` | 45 | the race ends this long after the winner finishes; the rest are DNF |
| `--results S` | 12 | how long the results stay up |
| `--no-showdown` | | end at the race results. By default the top five finishers then fight on foot (see "The showdown") |
| `--bots N` | 4 | computer-driven cars added to each race (0-4, fewer if the server is nearly full). See "Bots" |
| `--showdown-site U` | | always put the showdown's ring that far round the lap (0-1) instead of somewhere new each race |
| `--timeout S` | 5 | drop a player after this much silence |
| `--tracks DIR` | | also offer every `.fcft` track file in DIR (made in the track editor). Players who have not got one are sent it |
| `--custom-only` | | only the tracks from `--tracks` |
| `--roam` | | a roam server: no race; everyone drives and walks the same track together, mini Wardens included |
| `--roam-minutes M` | stay | roam: move to the next track every M minutes |
| `--quiet` | | no log output |

The server needs one UDP port reachable by the players. It has no dependencies beyond libstdc++, so
it runs on a machine with no graphics. It prints joins, leaves, phase changes and each result table.

Client flags: `--connect HOST[:PORT]`, `--name NAME` (16 characters), `--ready` (mark ready as soon
as the lobby appears; with `--auto` this makes a bot player for testing).

## Who decides what

The design is **client-authoritative cars, server-authoritative session**:

- Each client runs the full physics for its own car only, exactly as in single player, and reports
  position, velocity, attitude, lap, checkpoint count and progress 30 times a second.
- The server never simulates a car. It keeps the roster, runs the phase cycle, relays everyone's
  latest state 20 times a second, ranks players, and stamps finish times and order with its own
  clock when a client first reports finishing.
- Contact between players is resolved by each client for its own car only (the same circle push
  used against AI cars). Both players see a bump; neither can shove the other off the road with
  authority. There are no AI opponents online.

This keeps the driving feel identical to single player at any ping and the server tiny. The cost is
trust: a modified client could report any position or a false finish. The server only checks that a
finish claim has the laps done, that numbers are finite, and that packets come from the address that
joined. That is fine among friends and on a LAN, and not enough for public servers. Making it
cheat-resistant means running `stepPlayer` on the server (it already builds without graphics:
`--sim`) and sending inputs instead of states; see "Next steps".

## Session cycle

    LOBBY --(all ready, or --lobby timer)--> COUNTDOWN --(timer)--> RACE --(five finished, or all,
      or finish-timeout after the winner, or 20 min)--> SHOWDOWN --(one fighter left)--> RESULTS
      --(timer)--> LOBBY on the next track

The showdown is skipped (straight to RESULTS) with `--no-showdown`, or when fewer than two players finished.

- Players who are connected when the countdown starts, or who join during it, are *racing*.
  Anyone joining mid-race waits in the lobby screen and joins the next race.
- If every racing player leaves, the server returns to the lobby.
- Grid slots are by player id, lowest at the front.
- Places: finishers in finishing order, then everyone else by `progress` (metres along the track,
  laps included), then non-racers.

Client side, `netReact()` runs every frame and compares the game state with the server's phase:
COUNTDOWN or RACE while flagged racing starts the race locally (loading the server's track if
needed and syncing the countdown); RESULTS shows the standings; LOBBY returns to the lobby screen
on the server's next track. A local finish sets `F_FINISHED` and shows live standings until the
server moves on. Six seconds without a packet disconnects with a message on the title screen.

An online race cannot be paused: Esc opens a menu while the car coasts.

## Wire format

UDP datagrams. Integers little-endian, floats IEEE-754 single, structs packed with no padding.
Both ends are assumed little-endian (x86-64, ARM64). `PROTO` (currently 10) in `net.h` must be bumped
whenever a struct changes or a flag bit gains a meaning; a client with a different version gets `S_REJECT` reason 2.

Every packet starts with an 8-byte header:

| Offset | Type | Field |
|---|---|---|
| 0 | u32 | magic `0x59464346` (bytes `FCFY`) |
| 4 | u16 | protocol version (2) |
| 6 | u8 | packet type |
| 7 | u8 | sender's player id, `0xFF` if none yet or from the server |

Packet types:

| Type | Name | Direction | Body after the header |
|---|---|---|---|
| 1 | `C_JOIN` | client to server | `char name[16]`, `u8 car`, `u8 paint`. Sent every 0.4 s until welcomed |
| 2 | `C_STATE` | client to server | `u32 seq`, `CarState` |
| 3 | `C_LEAVE` | client to server | nothing |
| 16 | `S_WELCOME` | server to client | `u8 yourId`, `u8 track`, `u8 laps`, `u8 phase`, `u8 weather` |
| 17 | `S_SNAPSHOT` | server to client | `u32 tick`, `u8 phase`, `u8 track`, `u8 laps`, `u8 count`, `u8 weather`, `f32 phaseT`, then `count` x `PlayerSnap` |
| 18 | `S_REJECT` | server to client | `u8 reason`: 1 full, 2 version mismatch |
| 7 | `C_BOTS` | host to server | `u32 seq`, `u8 n`, then 4 x (`u8 id`, `CarState`): the bots' cars. Taken only from the host |
| 8 | `C_BOTFOOT` | host to server | `u32 seq`, `u8 n`, then 5 x (`u8 slot`, `FootState`): the bots' fighters in a showdown |
| 4 | `C_QUERY` | anyone to server | `u32 nonce`, 36 bytes of padding (48 bytes in all). Not a player: no join needed |
| 19 | `S_INFO` | server to asker | `u32 nonce` (echoed), `u8 track, laps, phase, weather, count, maxp`, `char name[24]` (42 bytes) |
| 5 | `C_FOOT` | fighter to server | `u32 seq`, `FootState`. 30 times a second for the whole showdown |
| 6 | `C_SDSETUP` | slot 0's client to server | `u8 n`, `f32 site[3]`, 12 x `SdDrop`: where the ring goes and where the pick-ups lie. Repeated until the server has them |
| 20 | `S_SHOWDOWN` | server to everyone | the showdown's state (439 bytes), 20 times a second while the phase is SHOWDOWN |

`CarState` (56 bytes):

| Type | Fields |
|---|---|
| 10 x f32 | `x y z`, `vx vy vz`, `yaw pitch roll`, `steer` |
| 4 x u8 | `car` (0-2), `paint` (0-7), `flags`, `lap` |
| u16 | `cp` checkpoints passed |
| 2 x f32 | `progress` (m), `finishT` (s; filled in by the server in snapshots) |
| 2 x u8 | `hitId`, `hitSeq`: rockets. `hitSeq` goes up by one each time this player's rocket hits player `hitId` |

`PlayerSnap` (74 bytes): `u8 id`, `u8 place` (1-based), `char name[16]`, `CarState`.
A snapshot is 21 + 74 x count bytes (761 at most).

**Rockets.** Cars are client-authoritative, so a hit is too: the shooter's game decides that its
rocket reached player N, sets `hitId = N` and increments `hitSeq`. The server relays both bytes
untouched in every snapshot (so a lost datagram does not lose the hit). The victim's game sees
`hitSeq` change with its own id in `hitId` and blows its own car up; everyone else sees the
victim's wrecked flag rise and leaves a wreck there. Rockets in flight are not sent: other players
see the explosion, not the missile. Crates are local to each client.

Flags: 1 ready (client, lobby only), 2 braking, 4 wrecked or sunk, 8 finished, 16 nitro burning,
32 racing (set by the server only; stripped from client packets).

`phaseT`: lobby = seconds until auto-start or -1; countdown = seconds to green; race = race time;
results = seconds until the next lobby.

Loss and ordering: nothing is retransmitted. States and snapshots are complete, so a lost one is
replaced by the next; out-of-order ones are dropped by `seq` / `tick`. Join is repeated until
answered. Leave is sent twice and backed by the timeout.

## Custom tracks and roam (protocol 10)

**Custom tracks.** Track ids from 7 up are the server's `--tracks` files, in name order. Every `C_STATE` ends with
`u8 have` (the highest-numbered server track the client holds; 0 if none) and 3 bytes of padding. While a player's
`have` is not the current custom track, the server sends the file as `S_TRACK` (21) pieces: track id, crc32 of the
whole file, total length, offset, piece length (up to 1024), then the bytes. A file is at most 8192 bytes. The
client puts the pieces together, checks the crc, builds the track and only then may mark itself ready. In the
server list a custom track shows as "CUSTOM TRACK".

**Roam.** A `--roam` server skips the lobby and sits in phase `ROAM` (5). Cars travel as in a race. On top:

| Packet | Type | From | Carries |
|---|---|---|---|
| `C_FOOT` | 5 | any player | their state on foot (as in the showdown) |
| `C_MINIS` | 9 | the host | up to 4 mini Wardens (40 bytes each: id, position, velocity, yaw, health, target, fire count, jet) |
| `C_MINIHIT` | 10 | any player | a hit on a mini Warden: its id, kind (0 fists or gun, 1 run over), damage |
| `S_ROAM` | 22 | server, 15 Hz | everyone on foot, the host's mini Wardens, and the last 8 hits |

The host (the lowest-numbered player) runs the mini Wardens: where they land, who they chase, when they die. Other
games draw what the host reports and send their hits to it through the server. The host gives the flowers to
whoever landed the last blow, or the penalty to whoever ran one over. When a mini Warden shoots at you, your own
game decides whether it hit.

## The showdown

After the race the first five finishers (fewer, if fewer finished; at least two) fight on foot.
Up to ten cars fit on a server, bots included. A bot that finishes in the top five fights like
anyone else (its host's game runs it, see "Bots"); everyone who is not in it watches, and the
camera key steps the view from one fighter to the next.

Phase 4, `SHOWDOWN`, has four stages, sent in `S_SHOWDOWN`:

| Stage | Ends when |
|---|---|
| 0 drive | every fighter reports `FF_ARRIVED` (their car is inside the ring), or after 90 s |
| 1 countdown | 20 s |
| 2 fight | one fighter is left, or after 180 s (then the healthiest wins) |
| 3 over | 6.5 s, then RESULTS |

**Who decides what.** The server picks the fighters (slots 0-4, in finishing order), the stage, the
ring's radius, who owns each pick-up, and the order fighters went down in. It has no map, so the
place is chosen by slot 0's game (the race winner's), which sends it with the pick-ups in
`C_SDSETUP`: usually open ground off the road. `siteKind` in `S_SHOWDOWN` says what `site` holds:
0 not chosen yet, 1 a stretch of road (`site[0]` of the way round the lap; used with
`--showdown-site`, or if the winner's game has not answered within 4 s), 2 a point `x y z`. Each
fighter's own game simulates their car, then their body, and reports it in `FootState`.

`FootState` (44 bytes): 6 x f32 `x y z vx vz yaw`; 8 x u8 `hp armour gun flags fireSeq target
killedBy pad`; 5 x u16 `dealt` (by slot); u16 `claim`. Flags: 1 out of the car, 2 alive, 4 aiming,
8 rolling, 16 sprinting, 32 car inside the ring, 64 blown apart by a grenade.

- **Damage** follows the rocket rule: the shooter decides it hit. It reports the total damage it has
  dealt to each slot (`dealt`, tenths of a health point). Each victim takes the increase off its own
  health and armour, and reports `hp`. A lost datagram loses nothing, because the totals are
  repeated. An increase of 500 points or more is a grenade at close range: the victim is gone.
- **Going down.** A fighter that reports itself not alive during the fight is placed behind everyone
  still standing; `killedBy` (a slot) gets the credit. Leaving the server counts as going down.
- **Pick-ups.** Slot 0's game knows the ground, so it lays them out and sends `C_SDSETUP` until the
  server echoes them. `claim` has a bit for each pick-up the fighter is standing on. The server
  gives a pick-up to the first claim it sees (`SdDrop.owner`), and puts it back 15 s later (45 s
  for the grenade launcher). A game only claims guns its player has found the keys for.
- **Shots** are not sent. `fireSeq` goes up with each one, and the other games draw the flash and
  tracer (or a harmless grenade) from the fighter's position and `target`.

`S_SHOWDOWN` (439 bytes): header, `u32 tick`, `u8 stage`, `f32 stageT, ring, site[3]`, `u8 siteKind`,
`u8 fighter[5]` (player ids, 255 = empty), `u8 place[5]` (0 while standing), `u8 kills[5]`,
`u8 winner`, `u8 ndrops`, 5 x `FootState`, 12 x `SdDrop` (`f32 x y z`, `u8 kind`, `u8 owner`).

Client side this is `src/netshd.h`. The same trust caveat applies as for cars: a modified client
could claim hits or refuse damage.

## Bots

The server has no map, so it cannot drive anything. Instead, when the countdown starts it adds up
to `--bots` players of its own (VEGA, MOSS, KATO, REYES; flag `F_BOT`, 64, in their `CarState`),
and names one person their host: the lowest-numbered player who is racing (flag `F_HOST`, 128, on
that player's own entry). The bots are removed again when the session returns to the lobby.

The host's game drives them with the offline opponent code and reports them: `C_BOTS` 20 times a
second for the cars (same `CarState` as a player's, `progress` in metres, `F_FINISHED` with
`lap > laps` when one crosses the line), and in a showdown `C_BOTFOOT` 30 times a second for any
bot that is fighting. The server treats those reports exactly like a player's own; it ignores
them from anyone but the host. To every other game a bot is one more remote player: it is drawn,
collided with, shot at and hit in the same way, and damage to a bot fighter is reported in
`dealt[]` and applied by the host's game.

If the host leaves, the next person becomes host and their game takes the bots over from where
they were last reported. In a showdown that is not possible (a fighter's state is more than its
position), so the bots still standing are marked down. If a bot wins the race, the host's game
chooses the showdown's place and lays out the pick-ups. A race with no people left in it ends.

Rockets: a hit on a bot is reported like a hit on a player (`hitId`), and the host's game wrecks it.

## Remote cars

For each other player the client keeps the last reported state and its age. Every frame it aims at
`position + velocity * age` (age capped at 0.4 s) and eases the drawn car towards that point at
14 /s; jumps of more than 40 m snap. Wheel spin is derived from speed. Name tags are projected
with the last view-projection matrix.

## Testing

`python3 tests/run_tests.py net` (about three minutes, real time because the server is):

- `net_server_runs_a_full_race_cycle` and `net_server_rejects_bad_clients_and_drops_silent_ones`
  talk to a real server with a 60-line Python client that packs the structs above directly. They
  cover the whole cycle, places, the finish check, version and full rejections, slot reuse, garbage
  and NaN packets, leave and timeout. These double as a second implementation of the protocol:
  if you change `net.h`, they fail until the Python layouts match.
- `net_two_game_clients_race_each_other` starts a server and two headless game clients
  (`--sim --connect ... --ready --auto`) and checks that each sees the other move and that they
  agree on places.
- `net_client_gives_up_when_there_is_no_server`.
- `net_showdown_server_runs_the_podium_fight`: three protocol-level clients take the server through
  a whole showdown (the wait for the last car, pick-up claims and their return, the order of going down).
- `net_showdown_game_client_fights_a_remote_fighter`: a real headless game, driven by the autopilot
  and the bot brain, drives to the ring and beats a protocol-level fighter that stands still.

To watch a session by hand without a display:

    ./fogcityfury-server --laps 1 --countdown 2 &
    ./fogcityfury --sim --connect 127.0.0.1 --name bot1 --ready --auto --seconds 100 &
    ./fogcityfury --sim --connect 127.0.0.1 --name bot2 --ready --auto --car 1 --seconds 100

The JSON report gains `net` (connected), `netId`, `netPhase`, `netPlayers`, `netMaxPlayers`,
`netPlace`, `netRace`, `netRemoteMoved` (metres the most-travelled remote car was seen to move)
and `netError`.

The Windows client and server use the same code through four small shims in `net.h` (`init`,
`closeSock`, `sleepMs`, `prepare`). Windows and Linux builds interoperate; `tests/run_tests.py
--windows` checks that under Wine.

Not tested: real latency, loss and jitter (everything so far is loopback); more than two game
clients at once; the join screen's text entry; NAT traversal (there is none - the server needs a
reachable port).

## Next steps

- Cheat resistance: send inputs, simulate on the server, reconcile on the client.
- Interpolate remote cars between two buffered snapshots instead of extrapolating one, which will
  look better under jitter.
- Lobby options from the client: vote track, laps; chat.
- Server browser or LAN discovery by broadcast.
- Shared wreck/explosion effects for remote cars, and their engine sounds.
- IPv6.

## ARM builds of the server

`make arm` cross-compiles the server for ARM Linux as two static binaries: `fogcityfury-server-arm64`
(64-bit: Raspberry Pi 3/4/5 on a 64-bit OS, most ARM cloud machines) and `fogcityfury-server-armhf`
(32-bit ARMv7 hard-float: Raspberry Pi 2 or later on a 32-bit OS; not the ARMv6 Pi 1 / Zero). It needs
`g++-aarch64-linux-gnu` and `g++-arm-linux-gnueabihf`. The wire format is little-endian packed structs,
which both targets share with x86, so no code differs. Check them here with
`qemu-aarch64-static ./fogcityfury-server-arm64 --port 27999` and two `--sim --connect` clients.

## Server list

The game's online screen keeps a list of servers (saved in `fogcityfury.cfg`; a fresh install has
`thadav.com:27015` and `127.0.0.1:27015`). For each it resolves the address on a background thread
(`srvLookup`, so slow DNS never freezes the menu), sends `C_QUERY` from a separate socket and shows
the `S_INFO` reply: name (`--name` on the server), track, players, phase and round-trip time. It
asks again every 4 s; 1.5 s without an answer shows as "no reply". A server answers queries from
any protocol version and puts its own in the header, so the list can say "older/newer version"
instead of failing at join time. The question is longer than the answer on purpose: a server
cannot be used to amplify traffic towards a forged address. Servers built before this was added
ignore `C_QUERY` and show as "no reply", but can still be joined. `PROTO` did not change.
