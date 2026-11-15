#!/usr/bin/env python3
"""Fog City Fury test suite. Standard library only.

    python3 tests/run_tests.py            # everything that needs no display (about 20 s)
    python3 tests/run_tests.py handling   # only tests whose name contains "handling"
    python3 tests/run_tests.py --render   # also the rendering smoke tests (needs xvfb-run + Mesa)
    python3 tests/run_tests.py --windows  # also run the Windows build under Wine (needs `make windows` and wine)
    python3 tests/run_tests.py -v         # print the measured numbers for every check

Simulation tests run the game with --sim: no window, no OpenGL, no audio device. The game steps at a
fixed 60 Hz and prints one JSON line of state and metrics (fields are listed in docs/TESTING.md).
The simulation is deterministic, so thresholds here can be tight. If you change handling or a track
on purpose, re-measure and update the numbers in the same commit, and say why in the commit message.
"""
import json
import math, os, shutil, socket, struct, subprocess, sys, tempfile, time, wave

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BIN = os.path.join(ROOT, "fogcityfury")
VERBOSE = "-v" in sys.argv
RENDER = "--render" in sys.argv
WINDOWS = "--windows" in sys.argv
FILTERS = [a for a in sys.argv[1:] if not a.startswith("-")]
CARS = ["stallion", "katana", "bruiser"]
NTRACKS = 4
PI = "3.1416"

results = []          # (name, status, detail)
_current = []         # failures of the running test


def sim(*args, seconds=5.0):
    """Run a headless simulation and return the JSON report as a dict."""
    cmd = [BIN, "--sim", "--seconds", str(seconds)] + [str(a) for a in args]
    p = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
    if p.returncode != 0 or not p.stdout.strip():
        raise RuntimeError("sim failed: %s\n%s" % (" ".join(cmd), p.stderr[-400:]))
    return json.loads(p.stdout.strip().splitlines()[-1])


def dump(track):
    """The built world: track length, checkpoints, keys (with their scripted routes) and ramps."""
    p = subprocess.run([BIN, "--sim", "--track", str(track), "--dump"], capture_output=True, text=True, timeout=60)
    return json.loads(p.stdout)


def keyrun(track, key, *extra, seconds=None):
    """Drive the route the level registered for a key (see World::addKey). `--gas` in extra = flat out, no steering."""
    secs = seconds or dump(track)["keys"][key]["seconds"] + 3
    return sim("--track", track, "--state", 3, "--mode", 1, "--keyrun", key, *extra, seconds=secs)


def find_key(kind):
    """First (track, key index) whose secret is of this kind, e.g. 'pier'."""
    for t in range(NTRACKS):
        for k, key in enumerate(dump(t)["keys"]):
            if key["kind"] == kind:
                return t, k
    raise RuntimeError("no secret of kind " + kind)


def flat(car, speed, script, seconds=5.0):
    """Handling run on the endless flat test plane, starting at `speed` m/s heading +Z."""
    return sim("--flat", "--state", 3, "--mode", 1, "--car", car, "--pos", 0, 0, 0, "--speed", speed, "--script", script, seconds=seconds)


def check(cond, what, value=None):
    if VERBOSE or not cond:
        print("      %s %s%s" % ("ok  " if cond else "FAIL", what, "" if value is None else "  [got %s]" % (value,)))
    if not cond:
        _current.append(what if value is None else "%s [got %s]" % (what, value))


def between(v, lo, hi, what):
    check(lo <= v <= hi, "%s in [%g, %g]" % (what, lo, hi), round(v, 3))


TESTS = []


def test(fn):
    TESTS.append(fn)
    return fn


# ----------------------------------------------------------------------------- boot / world
@test
def boot_reports_valid_state():
    for t in range(NTRACKS):
        d = sim("--track", t, "--state", 3, "--mode", 1, seconds=1)
        check(d["track"] == t, "track id echoed", d["track"])
        between(d["trackLen"], 3100, 3900, "track %d length (m): all tracks are 3.1 to 3.9 km" % t)
        check(d["dead"] == 0 and d["y"] > 1.0, "car spawns alive and on land", (d["dead"], d["y"]))


@test
def world_dump_has_secrets():
    kinds = set()
    for t in range(NTRACKS):
        d = dump(t)
        check(len(d["keys"]) == 5, "track %d has 5 keys" % t, len(d["keys"]))
        check([k["kind"] for k in d["keys"]].count("garage") == 1, "track %d has one key inside a building" % t, [k["kind"] for k in d["keys"]])
        g = [k for k in d["keys"] if k["kind"] == "garage"][0]
        check(len(d["keyStreet"]) == 4 and 60 < math.hypot(g["x"] - g["startX"], g["z"] - g["startZ"]) < 140, "track %d: that building stands at the end of a backstreet" % t, d["keyStreet"])
        check(len(d["checkpoints"]) == 8, "track %d has 8 checkpoints" % t, len(d["checkpoints"]))
        check(d["onStreet"] == [], "track %d: no building stands on a backstreet (walls line up with the kerbs)" % t, d["onStreet"][:4])
        check(len(d["ramps"]) >= 1, "track %d has a ramp" % t, len(d["ramps"]))
        check(d["boxes"] > 150, "track %d has buildings" % t, d["boxes"])
        for c in d["checkpoints"]:
            check(c[1] >= 2.0, "checkpoint above water", c[1])
        for k in d["keys"]:
            kinds.add(k["kind"])
            check(k["startGround"] > 1.0, "track %d '%s' route starts on dry land" % (t, k["kind"]), k["startGround"])
            check(max(abs(k["x"]), abs(k["z"])) < 740, "track %d '%s' key is inside the world" % (t, k["kind"]), (k["x"], k["z"]))
    check(len(kinds) >= 6, "at least six different kinds of secret", sorted(kinds))


@test
def simulation_is_deterministic():
    a = sim("--track", 0, "--state", 3, "--auto", seconds=40)
    b = sim("--track", 0, "--state", 3, "--auto", seconds=40)
    check(a == b, "two identical runs give identical reports")


# ----------------------------------------------------------------------------- full races
def _race(track, car):
    d = sim("--track", track, "--car", car, "--state", 3, "--auto", seconds=330)
    name = "track %d %s" % (track, CARS[car])
    check(d["finished"] == 1, "%s: autopilot finishes 3 laps" % name, (d["lap"], d["raceT"], d["timeUp"]))
    check(d["sinks"] == 0, "%s: never drives into the water" % name, d["sinks"])
    check(d["wrecks"] <= 1, "%s: at most one wreck" % name, d["wrecks"])
    check(d["camOffAxis"] == 0, "%s: chase camera stays behind the car" % name, d["camOffAxis"])
    between(d["bestLap"], 52, 100, "%s: best lap (s)" % name)
    check(1 <= d["place"] <= 6, "%s: place is valid" % name, d["place"])
    return d


def _race_track(track, min_air):
    for car in range(3):
        d = _race(track, car)
        if car < 2:
            check(d["maxAir"] > min_air, "track %d: %s gets air on the crests" % (track, CARS[car]), d["maxAir"])


@test
def race_hilltop_all_cars():
    _race_track(0, 1.2)


@test
def race_bayside_all_cars():
    _race_track(1, 0.9)


@test
def race_summit_all_cars():
    _race_track(2, 0.8)


@test
def race_midnight_all_cars():
    _race_track(3, 0.2)   # a flat city course: the only crests are the two bridges


@test
def grid_cars_hold_their_slots_through_the_countdown():
    d = sim("--track", 1, "--state", 3, "--countdown", seconds=2)
    check(d["intro"] > 0 and d["countdown"] == 3.6, "first the camera flies in from above the grid, and the countdown waits for it", (d["intro"], d["countdown"]))
    d = sim("--track", 1, "--state", 3, "--countdown", *_presses("RETURN"), seconds=2)
    check(d["intro"] == 0 and d["countdown"] < 3.6, "Enter skips the fly-in", (d["intro"], d["countdown"]))
    d = sim("--track", 1, "--state", 3, "--countdown", seconds=6.5)
    check(d["intro"] == 0 and d["countdown"] > 0 and d["raceT"] == 0, "then the countdown runs", (d["intro"], d["countdown"], d["raceT"]))
    check(d["gridDrift"] < 0.02, "no opponent has moved off its grid box", d["gridDrift"])
    check(d["dist"] < 0.5, "nor has the player", d["dist"])
    d = sim("--track", 1, "--state", 3, "--countdown", "--auto", seconds=12)
    check(d["countdown"] == 0 and d["dist"] > 50, "then the race starts", (d["countdown"], d["dist"]))


@test
def race_time_runs_out_when_parked():
    d = sim("--track", 0, "--state", 3, seconds=50)
    check(d["timeUp"] == 1 and d["finished"] == 0, "doing nothing ends in TIME UP", (d["timeUp"], d["finished"], d["timeLeft"]))


# ----------------------------------------------------------------------------- hidden keys
# Each secret registers its own scripted route in the level code, so these follow the layout when it changes.
@test
def keys_all_twenty_reachable():
    for t in range(NTRACKS):
        for k, key in enumerate(dump(t)["keys"]):
            d = keyrun(t, k)
            check(d["keys"] & (1 << k), "track %d key %d (%s) collected" % (t, k, key["kind"]), d["keys"])


@test
def keys_water_jump_works_in_every_car():
    t, k = find_key("waterjump")
    for car in range(3):
        d = keyrun(t, k, "--car", car)
        check(d["keys"] & (1 << k), "%s collects the mid-air key" % CARS[car], d["keys"])
        check(d["sinks"] == 0, "%s lands on the islet" % CARS[car], d["sinks"])
        check(d["maxAir"] > 0.8, "%s flies" % CARS[car], d["maxAir"])


# ----------------------------------------------------------------------------- hazards
@test
def hazard_wall_at_speed_wrecks_then_respawns():
    t, k = find_key("courtyard")
    d = keyrun(t, k, "--gas", seconds=10)
    check(d["wrecks"] == 1, "hitting the courtyard's back wall flat out wrecks the car", d["wrecks"])
    check(d["dead"] == 0, "car is back after the wreck", d["dead"])


@test
def hazard_deep_water_sinks_then_respawns():
    t, k = find_key("pier")
    d = keyrun(t, k, "--gas", seconds=11)
    check(d["sinks"] == 1, "driving off the end of the pier sinks the car", d["sinks"])
    check(d["dead"] == 0 and d["y"] > 1.5, "car respawns on the road", (d["dead"], d["y"]))


# ----------------------------------------------------------------------------- handling (flat plane)
@test
def handling_straight_line_is_stable():
    tops = []
    for car in range(3):
        d = flat(car, 0, "0:0,1,0,0", seconds=25)
        tops.append(d["maxSpeed"])
        check(d["maxBeta"] < 0.01, "%s tracks straight" % CARS[car], d["maxBeta"])
        check(abs(d["heading"]) < 0.01, "%s holds heading" % CARS[car], d["heading"])
    between(tops[0], 56, 62, "stallion top speed (m/s)")
    between(tops[1], 64, 70, "katana top speed (m/s)")
    between(tops[2], 50, 56, "bruiser top speed (m/s)")


@test
def handling_understeers_at_the_limit():
    """Full lock at 160 km/h: the front tyres saturate first and the car pushes wide instead of spinning."""
    for car in range(3):
        d = flat(car, 45, "0:0,1,0,0;0.5:1,1,0,0")
        n = CARS[car]
        check(d["maxSlipF"] > 1.5, "%s front tyres go past peak slip" % n, d["maxSlipF"])
        check(d["maxSlipR"] < 1.1, "%s rear stays hooked up" % n, d["maxSlipR"])
        check(d["maxBeta"] < 0.2, "%s does not spin" % n, d["maxBeta"])
        between(d["vf"] * d["yawRate"], 19, 33, "%s lateral accel at the limit (m/s^2)" % n)


@test
def handling_is_not_on_rails():
    """Half lock still asks more than the front tyres have at speed; the car must slip, not follow the wheel."""
    for car in range(3):
        d = flat(car, 45, "0:0,1,0,0;0.5:0.5,1,0,0")
        check(d["maxSlipF"] > 1.05, "%s scrubs its front tyres at half lock" % CARS[car], d["maxSlipF"])
        check(d["maxBeta"] > 0.05, "%s carries a slip angle" % CARS[car], d["maxBeta"])


@test
def handling_handbrake_drifts_and_recovers():
    for car in range(3):
        d = flat(car, 35, "0:0,1,0,0;0.5:1,0.5,0,0;1.0:1,0,0,1;1.7:-0.5,1,0,0;2.6:0,1,0,0", seconds=4)
        n = CARS[car]
        check(d["maxBeta"] > 0.8, "%s swings well sideways on the handbrake" % n, d["maxBeta"])
        check(d["maxSlipR"] > 3, "%s rear tyres slide" % n, d["maxSlipR"])
        check(abs(d["yawRate"]) < 0.1 and d["vf"] > 20, "%s is straight and moving again afterwards" % n, (d["yawRate"], d["vf"]))


@test
def handling_brake_steer():
    """Braking into a corner must rotate the car (brake steer) without it becoming a spin."""
    k = flat(1, 45, "0:0,1,0,0;0.5:0.7,0,0.6,0", seconds=3)
    s = flat(0, 45, "0:0,1,0,0;0.5:0.7,0,0.6,0", seconds=3)
    b = flat(2, 45, "0:0,1,0,0;0.5:0.7,0,0.6,0", seconds=3)
    n = flat(0, 45, "0:0,1,0,0;0.5:0.7,0.3,0,0", seconds=3)
    check(s["maxSlipR"] > 1.5, "stallion's rear steps out under trail braking", s["maxSlipR"])
    between(s["maxBeta"], 0.25, 0.65, "stallion brake-steer angle: clearly rotated, still catchable (rad)")
    check(s["maxYawRate"] > n["maxYawRate"] * 1.3, "braking turns the stallion harder than steering alone", (s["maxYawRate"], n["maxYawRate"]))
    check(k["maxSlipR"] > s["maxSlipR"] + 0.5, "katana is looser than the stallion", (k["maxSlipR"], s["maxSlipR"]))
    check(k["maxBeta"] < 1.2, "katana does not go fully round", k["maxBeta"])
    check(b["maxBeta"] < 0.35, "bruiser stays tidy", b["maxBeta"])
    lift = flat(1, 40, "0:0.6,1,0,0;2:0.6,0,0,0", seconds=4)
    check(0.3 < lift["maxBeta"] < 1.2, "katana lift-off oversteer: sideways but not spun", lift["maxBeta"])


@test
def handling_brakes_are_progressive():
    """Brakes build over ~0.3 s and peak around 26 m/s^2: firm, not a wall."""
    early = flat(0, 50, "0:0,0,1,0", seconds=0.15)
    check(early["vf"] > 48.0, "almost no speed lost in the first 0.15 s", early["vf"])
    one = flat(0, 50, "0:0,0,1,0", seconds=1.0)
    between(50 - one["vf"], 17, 27, "speed shed in the first second (m/s)")
    stop = flat(0, 50, "0:0,0,1,0", seconds=2.4)
    check(stop["vf"] < 3, "stopped from 180 km/h within 2.4 s", stop["vf"])
    between(stop["dist"], 50, 75, "stopping distance from 180 km/h (m)")
    check(stop["maxBeta"] < 0.02, "stays straight under braking", stop["maxBeta"])


@test
def handling_power_slide_in_low_gear():
    """Rear-drive: flooring it at part lock in second gear breaks the rear loose but keeps accelerating."""
    d = flat(0, 16, "0:0.6,1,0,0")
    check(d["maxSlipR"] > 2, "stallion spins up its rear tyres", d["maxSlipR"])
    between(d["maxBeta"], 0.3, 1.1, "stallion power-slide angle (rad)")
    check(d["vf"] > 20, "stallion keeps driving forward out of the slide", d["vf"])
    b = flat(2, 16, "0:0.6,1,0,0")
    check(b["maxSlipR"] < 1.2, "four-wheel-drive bruiser does not", b["maxSlipR"])


@test
def handling_left_right_symmetric():
    for car in range(3):
        l = flat(car, 40, "0:0,1,0,0;0.5:0.8,0.6,0,0", seconds=4)
        r = flat(car, 40, "0:0,1,0,0;0.5:-0.8,0.6,0,0", seconds=4)
        check(abs(l["heading"] + r["heading"]) < 0.02, "%s turns equally both ways" % CARS[car], (l["heading"], r["heading"]))
        check(l["heading"] > 0.5, "%s: positive steer turns towards +X (left)" % CARS[car], l["heading"])


@test
def handling_low_speed_and_reverse():
    d = flat(0, 5, "0:1,0.05,0,0", seconds=6)
    check(d["maxSlipR"] < 0.5, "no sliding at parking speed", d["maxSlipR"])
    check(d["heading"] > 2.0, "tight turning circle at parking speed", d["heading"])
    r = flat(0, 0, "0:0,0,1,0", seconds=3)
    check(r["vf"] < -6, "brake from rest selects reverse", r["vf"])


# ----------------------------------------------------------------------------- multiplayer
# Wire format, mirrored from src/net.h. If these tests fail after a protocol change, update both and bump PROTO.
SERVER = os.path.join(ROOT, "fogcityfury-server")
NET_MAGIC, NET_PROTO = 0x59464346, 3
HDR, CARSTATE, PSNAP = "<IHBB", "<10fBBBBHffBB", "<BB16s"
CS_SIZE, PS_SIZE = 56, 74   # CarState, PlayerSnap (bytes)
C_JOIN, C_STATE, C_LEAVE, S_WELCOME, S_SNAPSHOT, S_REJECT = 1, 2, 3, 16, 17, 18
LOBBY, COUNTDOWN, RACE, RESULTS = 0, 1, 2, 3
F_READY, F_FINISHED, F_NITRO, F_RACING = 1, 8, 16, 32


def free_port():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.bind(("127.0.0.1", 0))
    port = s.getsockname()[1]
    s.close()
    return port


class Server:
    """Context manager: a dedicated server on a free port with short timers."""

    def __init__(self, *args):
        self.port = free_port()
        self.args = [SERVER, "--port", str(self.port)] + [str(a) for a in args]

    def __enter__(self):
        self.proc = subprocess.Popen(self.args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        time.sleep(0.25)
        return self

    def __exit__(self, *exc):
        self.proc.terminate()
        self.log = self.proc.communicate(timeout=5)[0]


class FakeClient:
    """A protocol-level client: speaks the wire format directly, no game needed."""

    def __init__(self, port, name, proto=NET_PROTO):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.addr, self.name, self.proto, self.id, self.seq = ("127.0.0.1", port), name, proto, 0xFF, 0

    def hdr(self, kind):
        return struct.pack(HDR, NET_MAGIC, self.proto, kind, self.id)

    def join(self):
        """Returns (packet type, payload after the header)."""
        self.sock.settimeout(1.0)
        self.sock.sendto(self.hdr(C_JOIN) + struct.pack("<16sBB", self.name.encode(), 1, 2), self.addr)
        kind = S_SNAPSHOT
        while kind == S_SNAPSHOT:   # snapshots may already be flowing if we joined before
            data, _ = self.sock.recvfrom(4096)
            kind = struct.unpack(HDR, data[:8])[2]
        if kind == S_WELCOME:
            self.id, self.track, self.laps, self.phase, self.weather = struct.unpack("<BBBBB", data[8:13])
        return kind, data[8:]

    def state(self, flags=0, lap=0, progress=0.0, x=0.0, hit=(0, 0)):
        self.seq += 1
        self.sock.sendto(self.hdr(C_STATE) + struct.pack("<I", self.seq) + struct.pack(CARSTATE, x, 1, 2, 0, 0, 0, 0, 0, 0, 0, 1, 2, flags, lap, 0, progress, 0.0, hit[0], hit[1]), self.addr)

    def send_raw(self, data):
        self.sock.sendto(data, self.addr)

    def snapshot(self):
        """Newest snapshot the server has sent us (drains the socket), or None."""
        last = None
        self.sock.settimeout(0.4)
        try:
            while True:
                data, _ = self.sock.recvfrom(4096)
                if struct.unpack(HDR, data[:8])[2] == S_SNAPSHOT:
                    last = data
                self.sock.settimeout(0.0)
        except (socket.timeout, BlockingIOError):
            pass
        if last is None:
            return None
        tick, phase, track, laps, count, wx, phase_t = struct.unpack("<IBBBBBf", last[8:21])
        players = {}
        for k in range(count):
            o = 21 + k * PS_SIZE
            pid, place, name = struct.unpack(PSNAP, last[o:o + 18])
            cs = struct.unpack(CARSTATE, last[o + 18:o + PS_SIZE])
            players[name.split(b"\0")[0].decode()] = dict(id=pid, place=place, flags=cs[12], progress=cs[15], finishT=cs[16], x=cs[0], hitId=cs[17], hitSeq=cs[18])
        return dict(phase=phase, track=track, laps=laps, count=count, weather=wx, phaseT=phase_t, players=players, size=len(last))


def _keepalive(clients, seconds, **kw):
    t0 = time.time()
    while time.time() - t0 < seconds:
        for c in clients:
            c.state(**kw)
        time.sleep(0.04)


@test
def net_server_runs_a_full_race_cycle():
    with Server("--laps", 1, "--countdown", 0.4, "--results", 0.5, "--track", 2, "--weather", 3) as srv:
        a, b = FakeClient(srv.port, "ALICE"), FakeClient(srv.port, "BOB")
        check(a.join()[0] == S_WELCOME and b.join()[0] == S_WELCOME, "both clients are welcomed")
        check(a.id != b.id and a.id < 8 and b.id < 8, "distinct player ids", (a.id, b.id))
        check(a.track == 2 and a.laps == 1 and a.weather == 3, "welcome carries track, laps and weather", (a.track, a.laps, a.weather))
        a.state(); b.state()
        sn = a.snapshot()
        check(sn["phase"] == LOBBY and sn["count"] == 2 and set(sn["players"]) == {"ALICE", "BOB"}, "lobby lists both players", sn)
        check(sn["size"] == 21 + 2 * PS_SIZE, "snapshot is 21 + 74 bytes per player", sn["size"])
        a.state(flags=F_READY); b.state()
        time.sleep(0.1)
        check(a.snapshot()["phase"] == LOBBY, "race does not start until everyone is ready")
        b.state(flags=F_READY)
        time.sleep(0.1)
        sn = a.snapshot()
        check(sn["phase"] == COUNTDOWN and 0 < sn["phaseT"] <= 0.4, "all ready -> countdown", (sn["phase"], sn["phaseT"]))
        _keepalive([a], 0.7, progress=10)
        b.state(progress=25)
        time.sleep(0.1)
        sn = a.snapshot()
        check(sn["phase"] == RACE, "countdown -> race", sn["phase"])
        check(sn["players"]["BOB"]["place"] == 1 and sn["players"]["ALICE"]["place"] == 2, "places follow distance covered", sn["players"])
        check(all(p["flags"] & F_RACING for p in sn["players"].values()), "both are flagged as racing")
        check(sn["weather"] == 3, "the server's weather reaches the clients", sn["weather"])
        b.state(flags=F_NITRO, progress=26)
        time.sleep(0.1)
        check(a.snapshot()["players"]["BOB"]["flags"] & F_NITRO, "a player's nitro flag is relayed to the others")
        a.state(flags=F_FINISHED, lap=1, progress=9999)
        time.sleep(0.1)
        check(not (b.snapshot()["players"]["ALICE"]["flags"] & F_FINISHED), "a finish claim without the laps done is ignored")
        a.state(flags=F_FINISHED, lap=2, progress=3400)
        time.sleep(0.12)
        b.state(progress=3000)
        sn = b.snapshot()
        check(sn["phase"] == RACE and sn["players"]["ALICE"]["flags"] & F_FINISHED and sn["players"]["ALICE"]["place"] == 1, "first finisher takes first place, race continues", sn["players"])
        check(sn["players"]["ALICE"]["finishT"] > 0.5, "server stamps the finish time", sn["players"]["ALICE"]["finishT"])
        b.state(flags=F_FINISHED, lap=2, progress=3400)
        time.sleep(0.12)
        a.state()
        sn = a.snapshot()
        check(sn["phase"] == RESULTS and sn["players"]["BOB"]["place"] == 2, "everyone finished -> results", (sn["phase"], sn["players"]))
        _keepalive([a, b], 0.8)
        sn = a.snapshot()
        check(sn["phase"] == LOBBY and sn["track"] == 3, "results -> lobby on the next track", (sn["phase"], sn["track"]))
        check(not any(p["flags"] & (F_READY | F_RACING) for p in sn["players"].values()), "ready and racing flags are cleared for the new lobby")
    check("1. ALICE" in srv.log and "2. BOB" in srv.log, "server log has the results table")


@test
def net_server_rejects_bad_clients_and_drops_silent_ones():
    with Server("--timeout", 1.0, "--lobby", 0) as srv:
        old = FakeClient(srv.port, "OLD", proto=99)
        kind, payload = old.join()
        check(kind == S_REJECT and payload[0] == 2, "wrong protocol version is rejected with reason 2", (kind, payload[:1]))
        clients = [FakeClient(srv.port, "P%d" % i) for i in range(8)]
        check(all(c.join()[0] == S_WELCOME for c in clients), "eight players fit")
        ninth = FakeClient(srv.port, "NINTH")
        kind, payload = ninth.join()
        check(kind == S_REJECT and payload[0] == 1, "the ninth is rejected: server full", (kind, payload[:1]))
        again = clients[0].join()
        check(again[0] == S_WELCOME and clients[0].id == 0, "re-joining from the same address keeps the same slot", clients[0].id)
        clients[0].send_raw(b"garbage")
        clients[0].send_raw(struct.pack(HDR, 0x12345678, 1, C_STATE, 0) + b"\0" * (4 + CS_SIZE))
        clients[0].send_raw(clients[0].hdr(C_STATE) + struct.pack("<I", 10 ** 6) + struct.pack(CARSTATE, float("nan"), 1, 2, 0, 0, 0, 0, 0, 0, 0, 1, 2, 0, 0, 0, 0.0, 0.0, 0, 0))
        clients[0].state(x=42.0)
        time.sleep(0.1)
        sn = clients[0].snapshot()
        check(sn is not None and sn["count"] == 8, "garbage, bad magic and NaN packets do not disturb the server", sn and sn["count"])
        check(abs(sn["players"]["P0"]["x"] - 42.0) < 1e-3, "the NaN position was refused; the valid one stands", sn["players"]["P0"]["x"])
        clients[1].send_raw(clients[1].hdr(C_LEAVE))
        _keepalive(clients[:1], 1.6)
        sn = clients[0].snapshot()
        check(sn["count"] == 1, "one player left, six went silent: only the active one remains", sn["count"])
    check(srv.proc.returncode in (0, -15), "server shuts down cleanly on SIGTERM", srv.proc.returncode)


@test
def net_two_game_clients_race_each_other():
    """Two real (headless) game clients on a real server for 13 s of wall-clock time."""
    with Server("--laps", 1, "--countdown", 1) as srv:
        def client(name, car, *extra):
            return subprocess.Popen([BIN, "--sim", "--connect", "127.0.0.1:%d" % srv.port, "--name", name, "--ready", "--auto", "--car", str(car), "--seconds", "13"] + [str(x) for x in extra], stdout=subprocess.PIPE, text=True)
        procs = [client("alice", 0), client("bob", 1, "--rockets", 2, "--fire", 4.0)]
        reports = [json.loads(p.communicate(timeout=60)[0].strip().splitlines()[-1]) for p in procs]
    for name, d in zip(("alice", "bob"), reports):
        check(d["net"] == 1 and d["netError"] == "", "%s stayed connected" % name, d["netError"])
        check(d["netPhase"] == RACE and d["netRace"] == 1 and d["state"] == 3, "%s is in the online race" % name, (d["netPhase"], d["netRace"], d["state"]))
        check(d["netMaxPlayers"] == 2, "%s sees two players" % name, d["netMaxPlayers"])
        check(d["dist"] > 150, "%s drove its own car" % name, d["dist"])   # less than a clean run: bob shoots alice at 4 s
        check(d["netRemoteMoved"] > 150, "%s saw the other car move" % name, d["netRemoteMoved"])
    places = (reports[0]["netPlaceFull"], reports[1]["netPlaceFull"])   # "Full": while both were still connected (one always exits a moment before the other)
    check(set(places) == {1, 2}, "the two clients agree on who is ahead", places)
    check(reports[0]["netId"] != reports[1]["netId"], "distinct ids")
    check(reports[1]["rocketHits"] == 1, "bob's rocket found alice", reports[1]["rocketHits"])
    check(reports[0]["wrecks"] >= 1 and reports[0]["wrecksLeft"] >= 1, "alice's own game blew her up and left the wreck", (reports[0]["wrecks"], reports[0]["wrecksLeft"]))
    check(reports[1]["wrecksLeft"] >= 1, "bob sees alice's wreck too", reports[1]["wrecksLeft"])
    check("joined" in srv.log and "phase RACE" in srv.log, "server log shows the session")


@test
def net_server_list_shows_what_each_server_is_running():
    """The online screen asks every listed server what it is running, without joining, and joins the one picked."""
    dead = free_port()
    with Server("--track", 2, "--name", "Test Box") as srv:
        # the wire format: a 48-byte question, a 42-byte answer (shorter than the question, so no amplification)
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.settimeout(1.0)
        # UDP has no delivery guarantee and the server may still be starting, so resend the question until it answers
        query = struct.pack(HDR, NET_MAGIC, NET_PROTO, 4, 0xFF) + struct.pack("<I", 0xC0FFEE) + b"\0" * 36
        for attempt in range(10):
            s.sendto(query, ("127.0.0.1", srv.port))
            try:
                data, _ = s.recvfrom(256); break
            except socket.timeout:
                if attempt == 9: raise
        s.settimeout(3.0)
        magic, proto, kind, _id = struct.unpack(HDR, data[:8])
        nonce, track, laps, phase, wx, count, maxp, name = struct.unpack("<IBBBBBB24s", data[8:42])
        check(len(data) == 42 and kind == 19 and nonce == 0xC0FFEE, "the server answers a query and echoes the nonce", (len(data), kind, hex(nonce)))
        check((track, count, maxp, phase) == (2, 0, 8, LOBBY) and name.split(b"\0")[0] == b"TEST BOX", "with its track, players and name", (track, count, maxp, name))
        other = struct.pack(HDR, NET_MAGIC, 1, 4, 0xFF) + struct.pack("<I", 7) + b"\0" * 36
        for attempt in range(10):
            s.sendto(other, ("127.0.0.1", srv.port))
            try:
                data, _ = s.recvfrom(256); break
            except socket.timeout:
                if attempt == 9: raise
        check(struct.unpack(HDR, data[:8])[1:3] == (NET_PROTO, 19), "and answers other versions of the game too, saying which protocol it speaks", struct.unpack(HDR, data[:8]))
    check("joined" not in srv.log, "asking does not join the server", srv.log[-200:])
    with Server("--track", 2, "--name", "Test Box") as srv:   # the game's own list
        here = "127.0.0.1:%d" % srv.port
        d = sim("--server", here, "--server", "127.0.0.1:%d" % dead, "--press", "0.2:O", seconds=3)
        rows = [r.split("|") for r in d["servers"].split(";")]
        check(d["state"] == 6 and rows[0][:5] == [here, "2", "0", "2", "TEST BOX"], "the game lists the running server with its details", rows[0])
        check(rows[1][1] == "3", "and marks the dead one as not answering", rows[1])
        d = sim("--server", "127.0.0.1:%d" % dead, "--server", here, "--press", "0.2:O", "--press", "1.0:DOWN", "--press", "1.4:RETURN", seconds=3)
        check(d["net"] == 1 and d["state"] == 1, "picking the second row and pressing Enter joins that server", (d["net"], d["state"], d["netError"]))
    home = tempfile.mkdtemp(prefix="fcf_srv_")
    env = dict(os.environ, HOME=home, APPDATA=home)
    run = lambda *a: json.loads(subprocess.run([BIN, "--sim", "--save", "--seconds", "1.5"] + [str(x) for x in a], capture_output=True, text=True, timeout=60, env=env).stdout.strip().splitlines()[-1])
    d = run()
    check([r.split("|")[0] for r in d["servers"].split(";")] == ["thadav.com:27015", "127.0.0.1:27015"], "a fresh install lists the home server and this computer", d["servers"])
    run("--press", "0.2:O", "--press", "0.4:DOWN", "--press", "0.6:DELETE", "--press", "0.8:ESCAPE")
    d = run()
    check([r.split("|")[0] for r in d["servers"].split(";")] == ["thadav.com:27015"], "removing a server is remembered", d["servers"])


@test
def net_client_gives_up_when_there_is_no_server():
    d = sim("--connect", "127.0.0.1:%d" % free_port(), "--name", "lonely", seconds=7.2)
    check(d["net"] == 0 and d["netError"] == "NO RESPONSE FROM SERVER", "client reports that nothing answered", d["netError"])
    check(d["state"] == 6, "client stays on the join screen", d["state"])


# ----------------------------------------------------------------------------- bridges, rockets, wreckage, reset
@test
def bridges_on_the_figure_eight_tracks():
    want = {0: 1, 1: 0, 2: 0, 3: 2}
    for t in range(NTRACKS):
        d = dump(t)
        check(len(d["bridges"]) == want[t], "track %d has %d bridge(s)" % (t, want[t]), len(d["bridges"]))
        for b in d["bridges"]:
            between(b["deck"] - b["under"], 6.5, 8.5, "track %d: the deck clears the road underneath (m)" % t)
    for t in (0, 3):
        d = sim("--track", t, "--state", 3, "--auto", seconds=110)
        check(d["lap"] >= 2, "track %d: the autopilot laps it, over and under the bridge" % t, (d["lap"], d["raceT"]))
        between(d["maxDeck"], 6.5, 9.5, "track %d: the car was up on the deck (m above the ground)" % t)
        check(d["wrecks"] == 0 and d["sinks"] == 0, "track %d: nothing hit under or on the bridge" % t, (d["wrecks"], d["sinks"]))


@test
def rockets_lock_on_and_wreck_the_nearest_car():
    none = sim("--track", 1, "--state", 3, "--auto", "--fire", 3.0, seconds=7)
    check(none["rocketsFired"] == 0, "no rockets, no shot", none["rocketsFired"])
    d = sim("--track", 1, "--state", 3, "--auto", "--rockets", 2, "--fire", 3.0, "--fire", 4.5, seconds=7.5)
    check(d["rocketsFired"] == 2 and d["rockets"] == 0, "two rockets, both fired", (d["rocketsFired"], d["rockets"]))
    check(d["rocketHits"] == 2, "both found a car", d["rocketHits"])
    check(d["wrecksLeft"] == 2 and d["debris"] >= 14, "each hit leaves a shell and parts on the road", (d["wrecksLeft"], d["debris"]))
    check(d["wrecks"] == 0, "our own car is unharmed", d["wrecks"])
    check(d["place"] < none["place"], "wrecked opponents lose places", (d["place"], none["place"]))
    roam = sim("--track", 1, "--state", 3, "--mode", 1, "--rockets", 1, "--fire", 1.0, seconds=8)
    check(roam["rocketsFired"] == 1 and roam["rocketHits"] == 0 and roam["wrecksLeft"] == 0, "with nobody to lock on to, the rocket just flies off", (roam["rocketsFired"], roam["rocketHits"]))


@test
def rockets_come_from_pickups_two_at_a_time():
    crates = dump(1)["crates"]
    check(len(crates) == 12, "six pairs of crates round the lap", len(crates))
    here = ("--track", 1, "--state", 3, "--mode", 1, "--pos", crates[0]["x"], crates[0]["z"], 0)
    d = sim(*here, seconds=2)
    check(d["pickups"] == 1 and d["rockets"] == 2, "driving into a crate gives two rockets", (d["pickups"], d["rockets"]))
    d = sim(*here, seconds=14)
    check(d["pickups"] == 1 and d["rockets"] == 2, "sitting on the crate gives nothing more: one set per lap", (d["pickups"], d["rockets"]))
    d = sim(*here, "--fire", 0.5, seconds=2)
    check(d["rocketsFired"] == 1 and d["rockets"] == 1, "one fired, one left", (d["rocketsFired"], d["rockets"]))
    race = sim("--track", 1, "--state", 3, "--auto", seconds=330)
    check(race["finished"] == 1 and 1 <= race["pickups"] <= 4, "over a whole race the autopilot gets at most one set per lap (the run to the line counts as one)", race["pickups"])


@test
def wreck_leaves_shell_and_parts_behind():
    t, k = find_key("courtyard")
    d = keyrun(t, k, "--gas", seconds=10)
    check(d["wrecks"] == 1 and d["wrecksLeft"] == 1, "one crash, one burnt-out shell", (d["wrecks"], d["wrecksLeft"]))
    between(d["debris"], 7, 11, "panels and wheels scattered")
    key = dump(t)["keys"][k]
    check(math.hypot(d["wreckX"] - key["x"], d["wreckZ"] - key["z"]) < 40, "the shell is where the car died, not where it respawned", (d["wreckX"], d["wreckZ"]))
    check(d["dead"] == 0, "the car itself is back", d["dead"])
    soon = sim("--track", 1, "--state", 3, "--auto", "--rockets", 1, "--fire", 3.0, seconds=11)
    later = sim("--track", 1, "--state", 3, "--auto", "--rockets", 1, "--fire", 3.0, seconds=16)
    check(soon["rocketHits"] == 1 and soon["wrecksLeft"] == 1, "a wreck is still there seven seconds after the hit", (soon["rocketHits"], soon["wrecksLeft"]))
    check(later["rocketHits"] == 1 and later["wrecksLeft"] == 0 and later["debris"] == 0, "after ten seconds the shell and its parts have faded away", (later["wrecksLeft"], later["debris"]))


@test
def offroad_car_sits_on_the_ground_as_drawn():
    """Off the road the car must stand on the terrain mesh itself, not on a smoothed version of it."""
    t, k = find_key("summit")
    for car in (0, 2):
        d = keyrun(t, k, "--car", car)
        check(d["offroadT"] > 3, "%s spent the run off-road" % CARS[car], d["offroadT"])
        check(d["maxGap"] < 0.03, "%s: the ground it drives on is the ground that is drawn, to 3 cm" % CARS[car], d["maxGap"])


@test
def reset_puts_the_car_back_on_the_road():
    t, k = find_key("yard")
    key = dump(t)["keys"][k]
    pos = ("--pos", key["x"], key["z"], 0)
    lost = sim("--track", t, "--state", 3, "--mode", 1, *pos, seconds=3)
    check(lost["roadDist"] > 30 and lost["resets"] == 0, "the test starts well off the road", lost["roadDist"])
    d = sim("--track", t, "--state", 3, "--mode", 1, *pos, "--reset", 1.0, seconds=3)
    check(d["resets"] == 1, "reset happened", d["resets"])
    check(d["roadDist"] < 2 and d["dead"] == 0, "the car is back on the centre line", d["roadDist"])
    check(abs(d["vf"]) < 1, "and standing still", d["vf"])


@test
def net_rocket_hits_are_relayed():
    with Server("--countdown", 1) as srv:
        a, b = FakeClient(srv.port, "A"), FakeClient(srv.port, "B")
        a.join(); b.join()
        for _ in range(8):
            a.state(hit=(b.id, 7)); b.state(); time.sleep(0.05)
        sn = b.snapshot()
        check(sn is not None and sn["players"]["A"]["hitId"] == b.id and sn["players"]["A"]["hitSeq"] == 7, "B is told that A's rocket hit it", sn and sn["players"]["A"])
        check(sn["players"]["B"]["hitSeq"] == 0, "B itself has hit nobody", sn["players"]["B"]["hitSeq"])


# ----------------------------------------------------------------------------- options: controls and video
def _home():
    home = tempfile.mkdtemp(prefix="fcf_cfg_")
    env = dict(os.environ, HOME=home, APPDATA=home)

    def run(*args, seconds=3):
        p = subprocess.run([BIN, "--sim", "--save", "--seconds", str(seconds)] + [str(a) for a in args], capture_output=True, text=True, timeout=60, env=env)
        return json.loads(p.stdout.strip().splitlines()[-1])
    return run


def _presses(*keys, start=0.2, step=0.1):
    out = []
    for i, k in enumerate(keys):
        out += ["--press", "%.2f:%s" % (start + i * step, k)]
    return out


@test
def options_video_settings_change_and_are_remembered():
    run = _home()
    d = run()
    check((d["aa"], d["af"], d["full"], d["resW"], d["resH"], d["vsync"]) == (4, 8, 0, 1280, 720, 1), "defaults: window 1280x720, 4x MSAA, 8x AF, vsync", (d["aa"], d["af"], d["full"], d["resW"], d["resH"], d["vsync"]))
    d = run(*_presses("F1"))
    check(d["state"] == 8 and d["optPage"] == 0, "F1 on the title screen opens the options at the controls page", (d["state"], d["optPage"]))
    # TAB to video; resolution two steps up; AA one step down; AF one step up; vsync off; leave
    d = run(*_presses("F1", "TAB", "DOWN", "RIGHT", "RIGHT", "DOWN", "LEFT", "DOWN", "RIGHT", "DOWN", "RIGHT", "ESCAPE"))
    want = (2, 16, 0, 1600, 900, 0)
    check(d["state"] == 0, "Esc returns to the title screen", d["state"])
    check((d["aa"], d["af"], d["full"], d["resW"], d["resH"], d["vsync"]) == want, "the changes took effect", (d["aa"], d["af"], d["full"], d["resW"], d["resH"], d["vsync"]))
    d = run()
    check((d["aa"], d["af"], d["full"], d["resW"], d["resH"], d["vsync"]) == want, "and are still there next time", (d["aa"], d["af"], d["full"], d["resW"], d["resH"], d["vsync"]))
    d = run(*_presses("F1", "TAB", "RIGHT", "ESCAPE"))
    check(d["full"] == 1, "display toggles to full screen", d["full"])
    d = run("--state", 3, "--mode", 1, *_presses("ESCAPE", "F1", "ESCAPE"))
    check(d["state"] == 4, "from the pause menu, options return to the pause menu", d["state"])


@test
def options_controls_can_be_rebound():
    run = _home()
    drive = ("--track", 1, "--state", 3, "--mode", 1)
    check(run(*drive, "--hold", "Up")["dist"] > 10, "default: Up accelerates", None)
    check(run(*drive, "--hold", "X")["dist"] < 1, "default: X does nothing", None)
    d = run(*_presses("F1", "DOWN", "DOWN", "RETURN", "X", "ESCAPE"))     # third row is ACCELERATE, first slot
    binds = [tuple(int(v) for v in b.split(",")) for b in d["binds"].split(";")]
    check(len(binds) == 11 and binds[2] == (27, 26), "accelerate is now X, with W still as the second key", binds[2])
    check(run(*drive, "--hold", "X")["dist"] > 10, "X accelerates after rebinding (and after a restart)", None)
    check(run(*drive, "--hold", "Up")["dist"] < 1, "Up no longer does", None)
    check(run(*drive, "--hold", "W")["dist"] > 10, "the second key still works", None)
    d = run(*_presses("F1", "RETURN", "X", "ESCAPE"))                     # give X to STEER LEFT instead
    binds = [tuple(int(v) for v in b.split(",")) for b in d["binds"].split(";")]
    check(binds[0][0] == 27 and binds[2][0] == 0, "a key belongs to one action: it was taken away from accelerate", (binds[0], binds[2]))
    d = run(*_presses("F1", "RETURN", "ESCAPE", "ESCAPE"))
    check(d["state"] == 0 and d["binds"].startswith("27,"), "Esc cancels a pending change without binding Esc", d["binds"][:8])
    d = run("--state", 3, "--mode", 1, "--rockets", 1, *_presses("G"), seconds=2)
    check(d["rocketsFired"] == 0, "G is not a fire key by default", d["rocketsFired"])
    run(*_presses("F1", "DOWN", "DOWN", "DOWN", "DOWN", "DOWN", "DOWN", "RETURN", "G", "ESCAPE"))
    d = run("--state", 3, "--mode", 1, "--rockets", 1, *_presses("G"), seconds=2)
    check(d["rocketsFired"] == 1, "after rebinding FIRE ROCKET to G, G fires", d["rocketsFired"])
    d = run(*_presses("F1", "F5", "ESCAPE"))
    check(d["binds"] == "80,4;79,7;82,26;81,22;44,0;225,229;9,8;21,42;6,0;10,0;20,0", "F5 restores the defaults", d["binds"])


# ----------------------------------------------------------------------------- touch screen
@test
def touch_buttons_drive_the_car_and_the_menus():
    d = sim("--state", 3, "--mode", 1, "--track", 1, seconds=1)
    check(d["touch"] == 0 and d["touchBtns"] == "", "no on-screen buttons unless the screen is touched", d["touchBtns"])
    d = sim("--touch", "--state", 3, "--mode", 1, "--track", 1, "--touchhold", "GAS", seconds=5)
    check(d["touchBtns"].split(",")[:4] == ["<", ">", "GAS", "BRAKE"], "the race screen has steering, gas and brake buttons", d["touchBtns"])
    check(d["dist"] > 40, "a finger held on GAS drives the car", d["dist"])
    left = sim("--touch", "--state", 3, "--mode", 1, "--track", 1, "--touchhold", "GAS", "--touchhold", "<", seconds=4)
    right = sim("--touch", "--state", 3, "--mode", 1, "--track", 1, "--touchhold", "GAS", "--touchhold", ">", seconds=4)
    check(left["heading"] > 0.5 and right["heading"] < -0.5, "the two arrows steer opposite ways", (left["heading"], right["heading"]))
    d = sim("--touch", "--state", 3, "--mode", 1, "--track", 1, "--rockets", 1, "--tap", "0.5:ROCKET", "--tap", "1.0:II", seconds=2)
    check(d["rocketsFired"] == 1 and d["state"] == 4, "ROCKET fires and II pauses", (d["rocketsFired"], d["state"]))
    check(d["touchBtns"] == "RESUME,RESET,OPTIONS,QUIT", "the pause screen has its own buttons", d["touchBtns"])
    d = sim("--touch", "--tap", "0.3:RACE", "--tap", "0.6:>", "--tap", "0.9:OK", "--tap", "1.2:>", "--tap", "1.5:START", seconds=2.5)
    check(d["state"] == 3 and d["car"] == 1 and d["track"] == 1, "title, car select and track select can be worked by tapping alone", (d["state"], d["car"], d["track"]))
    d = sim("--touch", "--tap", "0.3:OPTIONS", "--tap", "0.6:VIDEO", "--tap", "0.9:DOWN", "--tap", "1.2:DOWN", "--tap", "1.5:<", seconds=2)
    check(d["state"] == 8 and d["aa"] == 2, "so can the options", (d["state"], d["aa"]))
    d = sim("--touch", "--state", 3, "--mode", 0, "--track", 0, "--faceai", "--tap", "0.2:OUT", "--touchhold", "FIRE", seconds=2)
    check(d["onFoot"] == 1 and d["touchBtns"] == "FIRE,ROLL,RUN,GUN,CAR,II", "OUT gets out of the car, and the buttons change to the fighter's", d["touchBtns"])
    check(d["footShots"] >= 3, "a thumb held on FIRE shoots", d["footShots"])
    out = sim("--touch", "--state", 3, "--mode", 1, "--track", 0, "--tap", "0.2:OUT", seconds=1)
    d = sim("--touch", "--state", 3, "--mode", 1, "--track", 0, "--tap", "0.2:OUT", "--stick", "0,1", seconds=3)
    check(math.hypot(d["walkX"] - out["walkX"], d["walkZ"] - out["walkZ"]) > 10, "the thumb-stick pushed forward runs", (d["walkX"], d["walkZ"]))
    d = sim("--touch", "--state", 3, "--mode", 1, "--track", 0, "--tap", "0.2:OUT", "--tap", "0.8:CAR", seconds=1.2)
    check(d["onFoot"] == 0 and d["touchBtns"].startswith("<,>,GAS"), "CAR gets back in", (d["onFoot"], d["touchBtns"]))
    d = sim("--touch", "--state", 3, "--mode", 2, "--track", 0, "--sdnow", seconds=6)
    check(d["touchBtns"] == "FIRE,ROLL,RUN,GUN,II,SKIP", "in the showdown there is no car to go back to", d["touchBtns"])


# ----------------------------------------------------------------------------- nitro
@test
def nitro_boosts_and_the_tank_runs_out():
    plain = flat(0, 0, "0:0,1,0,0", seconds=14)
    boost = flat(0, 0, "0:0,1,0,0,0;9:0,1,0,0,1", seconds=14)   # press it once the car is already flat out
    check(boost["maxSpeed"] > plain["maxSpeed"] + 8, "nitro lifts the top speed well clear of the normal one", (boost["maxSpeed"], plain["maxSpeed"]))
    between(boost["maxSpeed"] / plain["maxSpeed"], 1.15, 1.35, "top speed multiplier on nitro")
    quick = flat(0, 0, "0:0,1,0,0,1", seconds=3)
    check(quick["dist"] > flat(0, 0, "0:0,1,0,0", seconds=3)["dist"] * 1.6, "standing start is far quicker on nitro", quick["dist"])
    nogas = flat(0, 0, "0:0,0,0,0,1", seconds=3)
    check(nogas["vf"] > 25, "nitro drives the car with the throttle shut", nogas["vf"])
    held = flat(0, 20, "0:0,1,0,0,1", seconds=4)
    between(held["boostT"], 2.3, 2.9, "a full tank is about 2.5 s of boost")
    check(held["minNitro"] == 0.0 and held["boosts"] == 1, "the tank empties and it was one continuous burn", (held["minNitro"], held["boosts"]))


@test
def nitro_refills_in_ten_seconds():
    half = flat(0, 20, "0:0,1,0,0,1;3:0,1,0,0,0", seconds=8)
    between(half["nitro"], 0.45, 0.60, "5 s after running dry the tank is about half full")
    full = flat(0, 20, "0:0,1,0,0,1;3:0,1,0,0,0", seconds=13.2)
    check(full["nitro"] > 0.99, "10 s after running dry it is full again", full["nitro"])
    taps = flat(0, 20, "0:0,1,0,0,1;0.5:0,1,0,0,0;2:0,1,0,0,1;2.5:0,1,0,0,0", seconds=6)
    check(taps["boosts"] == 2 and 0.8 < taps["boostT"] < 1.3, "short taps use only what they burn", (taps["boosts"], taps["boostT"]))
    mash = flat(0, 20, "0:0,1,0,0,1;3:0,1,0,0,0;3.2:0,1,0,0,1", seconds=10)
    between(mash["boostT"], 2.3, 2.9, "holding the button on an empty tank does not fire again")
    check(mash["nitro"] > 0.6, "and the tank refills while the button is held", mash["nitro"])


# ----------------------------------------------------------------------------- weather
@test
def weather_changes_grip_and_braking():
    """Clear, rain, fog and storm are selectable per track; each still understeers first and stays catchable."""
    lat, stop = {}, {}
    for w in range(4):
        d = sim("--flat", "--weather", w, "--state", 3, "--mode", 1, "--car", 0, "--pos", 0, 0, 0, "--speed", 45, "--script", "0:0,1,0,0;0.5:1,1,0,0", seconds=5)
        check(d["weather"] == w, "weather %d is reported back" % w, d["weather"])
        check(d["maxSlipF"] > 1.5 and d["maxBeta"] < 0.3, "weather %d: still understeers at the limit, no spin" % w, (d["maxSlipF"], d["maxBeta"]))
        lat[w] = d["vf"] * d["yawRate"]
        stop[w] = sim("--flat", "--weather", w, "--state", 3, "--mode", 1, "--car", 0, "--pos", 0, 0, 0, "--speed", 50, "--script", "0:0,0,1,0", seconds=2.4)["dist"]
    check(lat[1] < lat[0] * 0.90, "rain costs grip", (lat[1], lat[0]))
    check(lat[3] < lat[1], "a storm costs more grip than rain", (lat[3], lat[1]))
    check(abs(lat[2] - lat[0]) < 2.0, "fog is a visibility problem, not a grip one", (lat[2], lat[0]))
    check(stop[1] > stop[0] + 2 and stop[3] > stop[1], "wet roads lengthen the braking distance", stop)


@test
def weather_races_finish_on_every_track():
    for w, track in ((1, 1), (2, 3), (3, 0)):
        d = sim("--track", track, "--car", 1, "--weather", w, "--state", 3, "--auto", seconds=330)
        name = "track %d in %s" % (track, ("clear", "rain", "fog", "storm")[w])
        check(d["finished"] == 1, "%s: the race finishes" % name, (d["lap"], d["timeUp"]))
        check(d["sinks"] == 0 and d["wrecks"] <= 1, "%s: the bot keeps it on the road" % name, (d["sinks"], d["wrecks"]))
    dry = sim("--track", 1, "--car", 1, "--weather", 0, "--state", 3, "--auto", seconds=330)
    wet = sim("--track", 1, "--car", 1, "--weather", 3, "--state", 3, "--auto", seconds=330)
    check(wet["bestLap"] > dry["bestLap"] * 1.03, "a storm lap is slower than a dry one", (wet["bestLap"], dry["bestLap"]))


@test
def weather_is_remembered_per_track():
    """The weather picked for a track is written to the save file and comes back next time."""
    home = tempfile.mkdtemp(prefix="fcf_save_")
    env = dict(os.environ, HOME=home, APPDATA=home)

    def run(*args):
        p = subprocess.run([BIN, "--sim", "--save", "--state", "3", "--seconds", "1"] + [str(a) for a in args], capture_output=True, text=True, timeout=60, env=env)
        return json.loads(p.stdout.strip().splitlines()[-1])

    run("--track", 2, "--weather", 3)
    run("--track", 1, "--weather", 1)
    check(run("--track", 2)["weather"] == 3, "track 2 remembers storm", run("--track", 2)["weather"])
    check(run("--track", 1)["weather"] == 1, "track 1 remembers rain", run("--track", 1)["weather"])
    check(run("--track", 0)["weather"] == 0, "a track that was never set stays clear", run("--track", 0)["weather"])
    save = os.path.join(home, ".fogcityfury.sav")
    check(os.path.exists(save) and open(save).read().startswith("v3"), "save file is version 3")
    shutil.rmtree(home, ignore_errors=True)


# ----------------------------------------------------------------------------- audio
@test
def audio_soundtrack_renders_sane():
    path = os.path.join(tempfile.gettempdir(), "fcf_test_music.wav")
    subprocess.run([BIN, "--wav", path, "12"], check=True, timeout=60)
    w = wave.open(path)
    check(w.getnchannels() == 2 and w.getframerate() == 44100, "stereo 44.1 kHz", (w.getnchannels(), w.getframerate()))
    n = w.getnframes()
    s = struct.unpack("<%dh" % (n * 2), w.readframes(n))
    L, R = s[0::2], s[1::2]
    peak = max(max(L), -min(L)) / 32768.0
    rms = math.sqrt(sum(x * x for x in L[::7]) / len(L[::7])) / 32768.0
    diff = sum(abs(a - b) for a, b in zip(L[::50], R[::50])) / len(L[::50]) / 32768.0
    between(peak, 0.3, 0.98, "peak level (no clipping)")
    between(rms, 0.08, 0.35, "RMS level")
    check(diff > 0.01, "left and right differ (the piano drifts across, the echo sits right)", round(diff, 4))
    # rhythm: energy per sixteenth note in the third bar must show strong and weak steps
    step = 44100 * 60.0 / 94 / 4
    e = [math.sqrt(sum(x * x for x in L[int((32 + i) * step):int((33 + i) * step):5])) for i in range(16)]
    check(max(e) > 1.25 * min(e), "sixteenth-note steps differ in energy (there is a rhythm)", round(max(e) / max(min(e), 1), 2))
    os.remove(path)


# ----------------------------------------------------------------------------- rendering (optional)
def _shot(name, *args, dark_ok=False, binary=None):
    out = os.path.join(tempfile.gettempdir(), "fcf_%s.bmp" % name)
    cmd = ["xvfb-run", "-a", "-s", "-screen 0 640x360x24", binary or BIN, "--nosave", "--shot", out, "--size", "640", "360", "--frames", "2"] + [str(a) for a in args]
    p = subprocess.run(cmd, capture_output=True, text=True, timeout=300)
    check(p.returncode == 0, "%s: renderer exits cleanly" % name, p.stderr[-300:])
    check("ERROR" not in p.stderr, "%s: no shader errors" % name, p.stderr[-300:])
    if not os.path.exists(out):
        check(False, "%s: screenshot written" % name)
        return
    data = open(out, "rb").read()[54:]
    os.remove(out)
    _shot.last = (data, json.loads(p.stdout.strip().splitlines()[-1]))
    px = data[::97]
    mean = sum(px) / len(px)
    var = sum((x - mean) ** 2 for x in px) / len(px)
    between(mean, 8 if dark_ok else 25, 215, "%s: image is neither black nor white (mean)" % name)
    check(var > 300, "%s: image has detail (variance)" % name, round(var))


def render_title_and_race():
    _shot("title", "--state", 0, "--skip", 120)
    _shot("carsel", "--state", 1)
    _shot("race", "--state", 3, "--auto", "--skip", 400)
    _shot("race2", "--track", 1, "--car", 1, "--state", 3, "--auto", "--skip", 400)
    _shot("night", "--track", 3, "--car", 2, "--state", 3, "--auto", "--skip", 300, dark_ok=True)
    _shot("rain", "--track", 1, "--weather", 1, "--state", 3, "--auto", "--skip", 400)
    _shot("tracksel", "--state", 2, "--track", 2)
    _shot("options_controls", "--state", 8)
    _shot("options_video", "--state", 8, "--optpage", 1)
    _shot("touch_race", "--touch", "--state", 3, "--auto", "--skip", 200)
    _shot("touch_menu", "--touch", "--state", 2)
    _shot("foot_race", "--state", 3, "--press", "0.05:G", "--hold", "F", "--faceai", "--gun", 1, "--skip", 90)
    _shot("showdown_intro", "--state", 3, "--mode", 2, "--sdnow", "--skip", 170)
    _shot("showdown_fight", "--track", 1, "--state", 3, "--mode", 2, "--sdnow", "--footai", "--skip", 2300)
    _shot("showdown_night", "--track", 3, "--weather", 3, "--state", 3, "--mode", 2, "--sdnow", "--footai", "--skip", 2300, dark_ok=True)
    _shot("showdown_touch", "--touch", "--state", 3, "--mode", 2, "--sdnow", "--stick", "0.5,1", "--skip", 500)


def render_gles_build_draws_everything():
    """The OpenGL ES 3.0 build (what Android runs; `make gles`) renders the same screens without shader errors."""
    gles = os.path.join(ROOT, "fogcityfury-gles")
    if not os.path.exists(gles):
        check(False, "fogcityfury-gles is built (make gles)")
        return
    _shot("es_title", "--state", 0, "--skip", 120, binary=gles)
    _shot("es_race", "--state", 3, "--auto", "--skip", 300, binary=gles)
    _shot("es_storm_night", "--track", 3, "--weather", 3, "--state", 3, "--auto", "--skip", 300, dark_ok=True, binary=gles)
    _shot("es_wreck", "--track", 1, "--state", 3, "--auto", "--rockets", 2, "--fire", 3.0, "--skip", 300, binary=gles)
    _shot("es_showdown", "--track", 1, "--state", 3, "--mode", 2, "--sdnow", "--footai", "--skip", 2300, binary=gles)
    _shot("es_showdown_touch", "--touch", "--state", 3, "--mode", 2, "--sdnow", "--stick", "0.5,1", "--skip", 500, binary=gles)
    _shot("es_touch", "--touch", "--state", 3, "--auto", "--skip", 200, binary=gles)
    check(_shot.last[1]["hdr"] == 1 and _shot.last[1]["touch"] == 1, "half-float render targets in use, touch buttons on", (_shot.last[1]["hdr"], _shot.last[1]["touch"]))
    _shot("es_8bit", "--ldr", "--state", 3, "--auto", "--skip", 200, binary=gles)
    check(_shot.last[1]["hdr"] == 0, "and it still draws with the 8-bit fallback targets", _shot.last[1]["hdr"])


def render_video_settings_change_the_picture():
    """Anti-aliasing, anisotropic filtering and the render size each really change what is drawn."""
    def shot(name, *flags):
        _shot(name, "--track", 1, "--state", 3, "--mode", 1, "--skip", 30, "--noui", *flags)
        return _shot.last

    def differ(a, b):
        return sum(abs(x - y) for x, y in zip(a[::7], b[::7])) / (len(a) / 7)

    base, rep = shot("v_base", "--aa", 4, "--af", 16)
    check(rep["aa"] == 4 and rep["af"] == 16 and (rep["renderW"], rep["renderH"]) == (640, 360), "settings from the command line are reported", (rep["aa"], rep["af"], rep["renderW"], rep["renderH"]))
    same, _ = shot("v_same", "--aa", 4, "--af", 16)
    check(differ(base, same) < 0.01, "the same settings give the same picture", differ(base, same))
    noaa, rep = shot("v_noaa", "--aa", 0, "--af", 16)
    check(rep["aa"] == 0 and differ(base, noaa) > 0.05, "turning MSAA off changes the edges", differ(base, noaa))
    noaf, rep = shot("v_noaf", "--aa", 4, "--af", 1)
    if rep["maxAF"] > 1:
        check(differ(base, noaf) > 0.05, "turning anisotropic filtering off changes the road", differ(base, noaf))
    half, rep = shot("v_half", "--aa", 4, "--af", 16, "--renderh", 180)
    check((rep["renderW"], rep["renderH"]) == (320, 180), "the scene is drawn at the requested size", (rep["renderW"], rep["renderH"]))
    check(len(half) == len(base) and differ(base, half) > 0.5, "and scaled up to fill the same window", differ(base, half))


# ----------------------------------------------------------------------------- Windows build (optional)
def windows_build_matches_linux():
    """The cross-compiled .exe, run under Wine, must simulate exactly what the Linux build does, and talk to a Linux server."""
    exe = os.path.join(ROOT, "fogcityfury.exe")
    env = dict(os.environ, WINEDEBUG="-all")
    args = ["--sim", "--track", "2", "--car", "1", "--state", "3", "--auto", "--seconds", "60"]
    w = json.loads(subprocess.run(["wine", exe] + args, capture_output=True, text=True, timeout=300, env=env).stdout.strip().splitlines()[-1])
    l = json.loads(subprocess.run([BIN] + args, capture_output=True, text=True, timeout=120).stdout.strip().splitlines()[-1])
    # the maths libraries differ in the last bit, so allow rounding-level drift but nothing more
    worst = max((abs(w[k] - l[k]) / max(1.0, abs(l[k])), k) for k in l if isinstance(l[k], (int, float)))
    check(worst[0] < 0.01 and w["lap"] == l["lap"] and w["cp"] == l["cp"], "60 s autopilot run matches the Linux build to within rounding", worst)
    # the Windows client joins first; the server's long countdown lets the Linux client make the same grid
    with Server("--laps", 1, "--countdown", 9) as srv:
        addr = "127.0.0.1:%d" % srv.port
        win = subprocess.Popen(["wine", exe, "--sim", "--connect", addr, "--name", "windows", "--ready", "--auto", "--car", "1", "--seconds", "27"], stdout=subprocess.PIPE, text=True, env=env)
        time.sleep(4)
        lin = subprocess.Popen([BIN, "--sim", "--connect", addr, "--name", "linux", "--ready", "--auto", "--seconds", "19"], stdout=subprocess.PIPE, text=True)
        dw = json.loads(win.communicate(timeout=120)[0].strip().splitlines()[-1])
        dl = json.loads(lin.communicate(timeout=60)[0].strip().splitlines()[-1])
    check(dw["net"] == 1 and dw["netRace"] == 1 and dw["netMaxPlayers"] == 2, "Windows client races on a Linux server", (dw["net"], dw["netRace"], dw["netMaxPlayers"], dw["netError"]))
    check(dw["netRemoteMoved"] > 150, "Windows client sees the Linux player move", dw["netRemoteMoved"])
    check(dl["netRemoteMoved"] > 150, "Linux client sees the Windows player move", dl["netRemoteMoved"])


def main():
    if not os.path.exists(BIN):
        print("binary not found - run `make` first")
        return 2
    tests = list(TESTS)
    if RENDER:
        if shutil.which("xvfb-run"):
            tests.append(render_title_and_race)
            tests.append(render_video_settings_change_the_picture)
            tests.append(render_gles_build_draws_everything)
        else:
            results.append(("render_title_and_race", "SKIP", "xvfb-run not installed"))
    if WINDOWS:
        if shutil.which("wine") and os.path.exists(os.path.join(ROOT, "fogcityfury.exe")):
            tests.append(windows_build_matches_linux)
        else:
            results.append(("windows_build_matches_linux", "SKIP", "needs `make windows` and wine"))
    for fn in tests:
        if FILTERS and not any(f in fn.__name__ for f in FILTERS):
            continue
        _current.clear()
        if VERBOSE:
            print("  %s" % fn.__name__)
        try:
            fn()
            status = "FAIL" if _current else "PASS"
            detail = "; ".join(_current)
        except Exception as ex:  # a crash in the game or the harness is a failure too
            status, detail = "ERROR", repr(ex)[:300]
        results.append((fn.__name__, status, detail))
    width = max(len(r[0]) for r in results) if results else 10
    for name, status, detail in results:
        print("%-5s %-*s %s" % (status, width, name, detail))
    bad = sum(1 for r in results if r[1] in ("FAIL", "ERROR"))
    print("\n%d passed, %d failed, %d skipped" % (sum(1 for r in results if r[1] == "PASS"), bad, sum(1 for r in results if r[1] == "SKIP")))
    return 1 if bad else 0


# ----------------------------------------------------------------------------- on foot, guns, the showdown
# Test hooks: --faceai (step out facing the nearest racer), --gun N (holding that gun), --sdnow / --sdplace N (as if you had
# just finished the race in that place), --footai (a bot plays your fighter). Fighters report as on|alive|hp|gun|kills|place.
FOOT = ("--track", 0, "--state", 3, "--mode", 1, "--press", "0.05:G")
SHOW = ("--state", 3, "--mode", 2, "--sdnow")
T_PREP, T_FIGHT = 4.6, 19.6          # when the countdown and the fight start, seconds after the finish (1.3 beat + 3.2 fly-down + 15)


def fighters(d):
    return [dict(zip(("on", "alive", "hp", "gun", "kills", "place"), (int(v) for v in f.split("|")))) for f in d["fighters"].split(";")]


@test
def foot_get_out_run_and_get_back_in():
    out = sim(*FOOT, seconds=1)
    check(out["onFoot"] == 1 and abs(out["vf"]) < 0.01, "G gets out of a stopped car, which stays put", (out["onFoot"], out["vf"]))
    run = sim(*FOOT, "--hold", "W", seconds=3)
    dist = math.hypot(run["walkX"] - out["walkX"], run["walkZ"] - out["walkZ"])
    between(run["walkSpeed"], 5.0, 5.4, "running speed (m/s)")
    between(dist, 12, 16.5, "distance run in three seconds (m)")
    fwd = (run["walkX"] - out["walkX"]) * math.sin(out["yaw"]) + (run["walkZ"] - out["walkZ"]) * math.cos(out["yaw"])
    check(fwd > 0.9 * dist, "forward is the way the camera looks: the way the car was pointing", (fwd, dist))
    back = sim(*FOOT, "--press", "0.5:G", seconds=1)
    check(back["onFoot"] == 0, "G beside the car gets back in", back["onFoot"])
    far = sim(*FOOT, "--hold", "W", "--press", "2.5:G", seconds=3)
    check(far["onFoot"] == 1, "but not from fifteen metres away", far["onFoot"])
    moving = sim("--track", 0, "--state", 3, "--mode", 1, "--hold", "W", "--press", "2:G", seconds=3)
    check(moving["onFoot"] == 0 and moving["vf"] > 10, "no jumping out of a moving car", (moving["onFoot"], moving["vf"]))


@test
def foot_running_is_relative_to_the_camera():
    out = sim(*FOOT, seconds=1)
    left = sim(*FOOT, "--hold", "A", seconds=2)
    side = (left["walkX"] - out["walkX"]) * -math.cos(out["yaw"]) + (left["walkZ"] - out["walkZ"]) * math.sin(out["yaw"])
    check(side < -1, "A runs to the left of where the camera looks", side)
    turn = (left["footYaw"] - out["footYaw"] + math.pi) % (2 * math.pi) - math.pi
    check(turn > 0.8, "and the camera swings left with it, the way A steers the car", turn)
    back = sim(*FOOT, "--hold", "S", seconds=2)
    fwd = (back["walkX"] - out["walkX"]) * math.sin(out["yaw"]) + (back["walkZ"] - out["walkZ"]) * math.cos(out["yaw"])
    check(fwd < -6, "S runs back towards the camera", fwd)
    car = sim(*FOOT, "--hold", "D", seconds=2)
    check(math.hypot(car["walkX"] - car["x"], car["walkZ"] - car["z"]) > 1.3, "the car is solid: running into it does not go through", (car["walkX"], car["walkZ"]))


@test
def foot_sprint_and_roll_cost_stamina():
    d = sim(*FOOT, "--hold", "W", "--hold", "Left Shift", seconds=3)
    between(d["walkSpeed"], 7.5, 8.0, "sprinting speed (m/s)")
    between(d["stamina"], 0.2, 0.5, "stamina left after three seconds of it")
    d = sim(*FOOT, "--hold", "W", "--hold", "Left Shift", seconds=5.2)
    check(d["stamina"] < 0.3 and d["walkSpeed"] < 5.5, "out of breath: back to a run until some of it recovers", (d["stamina"], d["walkSpeed"]))
    d = sim(*FOOT, "--hold", "W", "--hold", "Left Shift", seconds=6.6)
    check(d["walkSpeed"] > 7, "and then the sprint comes back", (d["stamina"], d["walkSpeed"]))
    d = sim(*FOOT, "--hold", "W", "--hold", "Space", seconds=0.4)
    check(d["rolling"] == 1 and d["walkSpeed"] > 7, "the handbrake key is a dodge roll, a burst of speed", (d["rolling"], d["walkSpeed"]))
    between(d["stamina"], 0.7, 0.85, "a roll costs a quarter of the stamina")
    d = sim(*FOOT, "--hold", "W", "--hold", "Space", seconds=1.5)
    check(d["rolling"] == 0, "one roll per press", d["rolling"])


@test
def guns_lock_on_and_hurt_the_racers():
    base = sim("--track", 0, "--state", 3, "--mode", 0, "--press", "0.05:G", "--faceai", seconds=1.5)
    check(base["aiMinHp"] == 100.0 and base["footShots"] == 0 and base["lock"] == -1, "nothing is locked or hit before the trigger is pulled", (base["aiMinHp"], base["lock"]))
    for gun, name, shots in ((0, "pistol", 3), (1, "SMG", 8), (2, "shotgun", 1), (3, "launcher", 1)):
        d = sim("--track", 0, "--state", 3, "--mode", 0, "--press", "0.05:G", "--hold", "F", "--faceai", "--gun", gun, seconds=1.5)
        check(d["footShots"] >= shots, "%s: holding fire keeps shooting" % name, d["footShots"])
        check(d["aiMinHp"] < 100 or d["aiDead"] > 0, "%s: the racer it locked on to is hurt" % name, (d["aiMinHp"], d["aiDead"]))
    d = sim("--track", 0, "--state", 3, "--mode", 0, "--press", "0.05:G", "--hold", "F", "--faceai", "--gun", 2, seconds=4)
    check(d["aiDead"] >= 1 and d["wrecks"] >= 1, "enough of it wrecks the car", (d["aiDead"], d["wrecks"]))


@test
def guns_run_dry_and_fall_back_to_the_pistol():
    d = sim("--track", 0, "--state", 3, "--mode", 0, "--press", "0.05:G", "--hold", "F", "--faceai", seconds=12)
    check(d["gun"] == 0 and d["footShots"] > 90, "the SMG you carry empties (90 rounds); the pistol never does", (d["gun"], d["footShots"]))


@test
def showdown_is_for_the_top_three():
    d = sim("--track", 0, *SHOW, "--sdplace", 4, seconds=3)
    check(d["state"] == 5 and d["shdPhase"] == 0 and d["onFoot"] == 0, "fourth place: straight to the results", (d["state"], d["shdPhase"]))
    for place, armour in ((1, 50), (2, 25), (3, 0)):
        d = sim("--track", 0, *SHOW, "--sdplace", place, seconds=3)
        check(d["shdPhase"] == 1 and d["onFoot"] == 1 and d["armour"] == armour, "place %d: into the showdown with %d armour" % (place, armour), (d["shdPhase"], d["armour"]))


@test
def showdown_runs_intro_countdown_fight():
    d = sim("--track", 0, *SHOW, seconds=1)
    check(d["shdPhase"] == 1 and d["onFoot"] == 0, "the finish: still in the car, pulling up", (d["shdPhase"], d["onFoot"]))
    d = sim("--track", 0, *SHOW, seconds=3)
    fs = fighters(d)
    check(d["shdPhase"] == 1 and all(f["on"] and f["alive"] for f in fs) and d["drops"] == 9, "the cut: three drivers out of their cars, nine pick-ups down", (d["shdPhase"], d["fighters"], d["drops"]))
    check(d["aiDead"] == 3, "the rest of the field is gone", d["aiDead"])
    d = sim("--track", 0, *SHOW, "--hold", "F", "--faceai", seconds=T_FIGHT - 1)
    check(d["shdPhase"] == 2 and d["footShots"] == 0, "the countdown: nobody can shoot yet", (d["shdPhase"], d["footShots"]))
    check(all(f["hp"] == 100 for f in fighters(d)), "so nobody is hurt", d["fighters"])
    check(sum(f["gun"] > 0 for f in fighters(d)[1:]) >= 1 and d["drops"] < 9, "the others have used it to fetch guns", (d["fighters"], d["drops"]))
    between(d["shdRing"], 51.9, 52.1, "the ring waits at full size")
    d = sim("--track", 0, *SHOW, "--hold", "F", seconds=T_FIGHT + 6)
    check(d["shdPhase"] >= 3 and d["footShots"] > 0, "then the guns are live", (d["shdPhase"], d["footShots"]))


@test
def showdown_ring_burns_and_closes():
    d = sim("--track", 0, *SHOW, "--hold", "S", seconds=T_FIGHT - 1)
    check(d["walkHp"] == 100, "running off during the countdown costs nothing", d["walkHp"])
    d = sim("--track", 0, *SHOW, "--hold", "S", seconds=T_FIGHT + 30)
    fs = fighters(d)
    check(fs[0]["alive"] == 0 and fs[0]["place"] == 3 and d["footShots"] == 0, "outside the ring you burn: first out is third", d["fighters"])
    check(d["shdRing"] < 24, "with two left the ring closes right in", d["shdRing"])


@test
def showdown_always_ends_with_one_winner():
    for track in range(NTRACKS):
        d = sim("--track", track, *SHOW, "--footai", seconds=170)
        fs = fighters(d)
        check(d["state"] == 5 and d["shdResult"] == 1, "track %d: the showdown reaches its results" % track, (d["state"], d["shdPhase"], d["fighters"]))
        check(sorted(f["place"] for f in fs) == [1, 2, 3], "track %d: first, second and third are all decided" % track, d["fighters"])
        check(fs[d["shdWinner"]]["place"] == 1 and sum(f["alive"] for f in fs) <= 1, "track %d: the winner is the last one standing" % track, d["fighters"])
        check(sum(f["kills"] for f in fs) >= 1, "track %d: it was settled with guns, not just the ring" % track, d["fighters"])


@test
def showdown_can_be_skipped_once_you_are_out():
    d = sim("--track", 0, *SHOW, "--hold", "S", "--press", "%.1f:RETURN" % (T_FIGHT + 13), seconds=T_FIGHT + 13.5)
    check(d["state"] == 5 and d["shdResult"] == 1 and d["shdPlace"] == 3, "Enter goes to the results; the two still standing are placed by health", (d["state"], d["fighters"]))
    check(sorted(f["place"] for f in fighters(d)) == [1, 2, 3], "all three places are filled", d["fighters"])
    d = sim("--track", 0, *SHOW, "--press", "2:RETURN", seconds=2.5)
    check(d["shdPhase"] == 2, "and Enter skips the fly-down", d["shdPhase"])
    d = sim("--track", 0, *SHOW, "--press", "10:R", "--press", "11:G", seconds=12)
    check(d["onFoot"] == 1 and d["resets"] == 0, "no resetting or driving out of a showdown", (d["onFoot"], d["resets"]))


if __name__ == "__main__":
    sys.exit(main())
