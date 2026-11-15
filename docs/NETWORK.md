# Multiplayer

Up to 8 players race over UDP through a dedicated server. The code is in three files:

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
| `--timeout S` | 5 | drop a player after this much silence |
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

    LOBBY --(all ready, or --lobby timer)--> COUNTDOWN --(timer)--> RACE --(all finished,
      or finish-timeout after the winner, or 20 min)--> RESULTS --(timer)--> LOBBY on the next track

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
Both ends are assumed little-endian (x86-64, ARM64). `PROTO` (currently 3) in `net.h` must be bumped
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
| 4 | `C_QUERY` | anyone to server | `u32 nonce`, 36 bytes of padding (48 bytes in all). Not a player: no join needed |
| 19 | `S_INFO` | server to asker | `u32 nonce` (echoed), `u8 track, laps, phase, weather, count, maxp`, `char name[24]` (42 bytes) |

`CarState` (56 bytes):

| Type | Fields |
|---|---|
| 10 x f32 | `x y z`, `vx vy vz`, `yaw pitch roll`, `steer` |
| 4 x u8 | `car` (0-2), `paint` (0-7), `flags`, `lap` |
| u16 | `cp` checkpoints passed |
| 2 x f32 | `progress` (m), `finishT` (s; filled in by the server in snapshots) |
| 2 x u8 | `hitId`, `hitSeq`: rockets. `hitSeq` goes up by one each time this player's rocket hits player `hitId` |

`PlayerSnap` (74 bytes): `u8 id`, `u8 place` (1-based), `char name[16]`, `CarState`.
A snapshot is 21 + 74 x count bytes (613 at most).

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

## Remote cars

For each other player the client keeps the last reported state and its age. Every frame it aims at
`position + velocity * age` (age capped at 0.4 s) and eases the drawn car towards that point at
14 /s; jumps of more than 40 m snap. Wheel spin is derived from speed. Name tags are projected
with the last view-projection matrix.

## Testing

`python3 tests/run_tests.py net` (about 25 s, real time because the server is):

- `net_server_runs_a_full_race_cycle` and `net_server_rejects_bad_clients_and_drops_silent_ones`
  talk to a real server with a 60-line Python client that packs the structs above directly. They
  cover the whole cycle, places, the finish check, version and full rejections, slot reuse, garbage
  and NaN packets, leave and timeout. These double as a second implementation of the protocol:
  if you change `net.h`, they fail until the Python layouts match.
- `net_two_game_clients_race_each_other` starts a server and two headless game clients
  (`--sim --connect ... --ready --auto`) and checks that each sees the other move and that they
  agree on places.
- `net_client_gives_up_when_there_is_no_server`.

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
