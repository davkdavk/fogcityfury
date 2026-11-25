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
import json, sys
import math, os, shutil, socket, struct, subprocess, sys, tempfile, time, wave

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BIN = os.path.join(ROOT, "fogcityfury")
VERBOSE = "-v" in sys.argv
RENDER = "--render" in sys.argv
WINDOWS = "--windows" in sys.argv
FILTERS = [a for a in sys.argv[1:] if not a.startswith("-")]
CARS = ["stallion", "katana", "bruiser", "ripdog", "ronin"]
NTRACKS = 7
NTRACKS_KEYS = 6   # (Twin Isles, the seventh, has no hidden keys)
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
        between(d["trackLen"], 2900, 3900, "track %d length (m): all tracks are 2.9 to 3.9 km" % t)
        check(d["dead"] == 0 and d["y"] > 1.0, "car spawns alive and on land", (d["dead"], d["y"]))


@test
def world_dump_has_secrets():
    kinds = set()
    for t in range(NTRACKS_KEYS):
        d = dump(t)
        check(len(d["keys"]) == 5, "track %d has 5 keys" % t, len(d["keys"]))
        check([k["kind"] for k in d["keys"]].count("garage") == 1, "track %d has one key inside a building" % t, [k["kind"] for k in d["keys"]])
        g = [k for k in d["keys"] if k["kind"] == "garage"][0]
        check(len(d["keyStreet"]) == 4 and 60 < math.hypot(g["x"] - g["startX"], g["z"] - g["startZ"]) < 140, "track %d: that building stands at the end of a backstreet" % t, d["keyStreet"])
        check(len(d["checkpoints"]) == 8, "track %d has 8 checkpoints" % t, len(d["checkpoints"]))
        check(d["onStreet"] == [], "track %d: no building stands on a backstreet (walls line up with the kerbs)" % t, d["onStreet"][:4])
        check(len(d["ramps"]) >= 1, "track %d has a ramp" % t, len(d["ramps"]))
        check(d["boxes"] > 150, "track %d has buildings" % t, d["boxes"])
        ta, tb = d["tunnel"][0], d["tunnel"][1]
        for ci, c in enumerate(d["checkpoints"]):   # (one in an undersea tunnel is under the sea, dry inside it)
            if ta <= ci * d["trackLen"] // 8 <= tb: continue
            check(c[1] >= 2.0, "checkpoint above water", c[1])
        for k in d["keys"]:
            kinds.add(k["kind"])
            check(k["startGround"] > 1.0, "track %d '%s' route starts on dry land" % (t, k["kind"]), k["startGround"])
            check(max(abs(k["x"]), abs(k["z"])) < 740, "track %d '%s' key is inside the world" % (t, k["kind"]), (k["x"], k["z"]))
    check(len(kinds) >= 6, "at least six different kinds of secret", sorted(kinds))


@test
def alternate_route():
    for t in range(NTRACKS):
        d = sim("--track", t, "--state", 3, "--mode", 1, seconds=0.5)
        sc = [[float(v) for v in s.split(",")] for s in d["shortcuts"].split(";") if s]
        check(len(sc) == (3 if t == 6 else 2), "track %d has %s shortcuts" % (t, "three" if t == 6 else "two"), d["shortcuts"])
        ta, tb = d["tunnel"][0], d["tunnel"][1]
        for a, b, L, blocked in sc:
            if ta >= 0 and a < ta and b > tb:   # the way over the water instead of under it: an alternative, not a shortcut
                check(150 < L < (b - a) * 1.25, "track %d: the surface route round the tunnel is a fair alternative (%d m against %d)" % (t, L, b - a), (a, b, L))
                continue
            check(150 < L < b - a, "track %d: a shortcut is shorter than the road it cuts (%d m against %d)" % (t, L, b - a), (a, b, L))
            check(blocked == 0, "track %d: no building stands on a shortcut" % t, blocked)
        check(all(p[1] + 30 < q[0] or q[1] + 30 < p[0] for i, p in enumerate(sc) for q in sc[i + 1:]), "track %d: they are on different parts of the lap" % t, sc)
    # driving down it: on the route, counted as the matching stretch of the loop, never 'wrong way'
    d = sim("--track", 1, "--state", 3, "--mode", 1, "--pos", -428, -120, 3.14, "--speed", 25, seconds=2.5)
    check(d["onBranch"] >= 0 and d["branchA"] < d["loopPos"] < d["branchB"], "a car on the backstreet is placed between the fork and the rejoin", (d["onBranch"], d["loopPos"]))
    check(d["maxWrongT"] == 0 and d["dead"] == 0 and d["roadDist"] < 6.5, "driving down it is the right way and stays on the road", (d["maxWrongT"], d["dead"], d["roadDist"]))
    d = sim("--track", 1, "--state", 3, "--mode", 1, "--pos", -428, -120, 0, "--speed", 25, seconds=2.5)
    check(d["maxWrongT"] > 1, "driving up it is the wrong way", d["maxWrongT"])
    # bots take it on some laps and still finish
    d = sim("--track", 1, "--state", 3, "--auto", "--nobotguns", seconds=200)   # (a bot rocket can cost the autopilot the clock)
    between(d["aiBranch"], 8, 60, "bot trips down the shortcuts over three laps")
    check(d["lap"] >= 3, "the race still runs to the end", d["lap"])


@test
def shield_on_the_alternate_route():
    d = sim("--track", 1, "--state", 3, "--mode", 1, seconds=0.5)
    check(d["shieldT"] == 0 and d["shields"] == 0 and d["shieldsUp"] == 2, "two shields wait on the backstreet, none on the car", (d["shieldT"], d["shields"], d["shieldsUp"]))
    x, z = d["shieldX"], d["shieldZ"]
    d = sim("--track", 1, "--state", 3, "--mode", 1, "--pos", x, z, 3.14, seconds=3)
    check(d["onBranch"] >= 0, "the shield sits on the backstreet", (x, z, d["onBranch"]))
    check(d["shields"] == 1 and 10 < d["shieldT"] < 15 and d["shieldsUp"] == 1, "driving through one puts a shield up, and it is gone for everyone else", (d["shields"], d["shieldT"], d["shieldsUp"]))
    t, k = find_key("courtyard")
    d = keyrun(t, k, "--gas", "--shield", seconds=10)
    check(d["wrecks"] == 0 and d["shieldSaves"] == 1 and d["shieldT"] == 0, "a flat-out crash breaks the shield instead of the car", (d["wrecks"], d["shieldSaves"], d["shieldT"]))
    d = sim("--seed", 5, "--track", 1, "--state", 3, "--auto", seconds=330)
    between(d["aiShields"] + d["shields"], 2, 12, "over three laps the bots take shields (each shield at most once a lap, and free again once its holder has finished)")


@test
def bots_fire_rockets():
    # us just ahead of the grid, every bot holding one rocket (positions vary: who fires at whom is up to the bots)
    spots = ((-455, -245), (-440, -262), (-445, -255))
    bare = [sim("--seed", 5, "--track", 0, "--state", 3, "--botrockets", 1, "--pos", x, z, 2.463, seconds=12) for x, z in spots]
    check(sum(d["rocketsAtMe"] for d in bare) >= 1 and sum(d["wrecks"] for d in bare) >= 1, "a bot with a rocket fires at us when we are just ahead", [(d["rocketsAtMe"], d["wrecks"]) for d in bare])
    shd = [sim("--seed", 5, "--track", 0, "--state", 3, "--botrockets", 1, "--pos", x, z, 2.463, "--shield", seconds=12) for x, z in spots]
    check(sum(d["rocketsAtMe"] for d in shd) >= 1 and all(d["shieldSaves"] == min(d["rocketsAtMe"], 1) and d["wrecks"] <= d["rocketsAtMe"] - d["shieldSaves"] for d in shd), "a shield takes the first rocket (only the first)", [(d["rocketsAtMe"], d["shieldSaves"], d["wrecks"]) for d in shd])
    for t in range(NTRACKS):
        d = sim("--seed", 5, "--track", t, "--state", 3, "--auto", *(["--laps", 2] if t == 6 else []), seconds=330)   # (Twin Isles races twice the laps: two picked, four raced)
        between(d["aiRocketsFired"], 3, 20, "track %d: bots pick up crates now and then and fire (not a war)" % t)
        check(d["finished"] == 1, "track %d: the race still finishes" % t, d["finished"])


@test
def cars_bump_each_other():
    # just after the start, run into the back of the bot ahead, then clip one from the side
    d = sim("--seed", 5, "--track", 0, "--state", 3, "--nobotguns", "--pos", -501.1, -182.8, 2.698, "--speed", 25, seconds=2)
    check(d["bumps"] >= 1 and d["wrecks"] == 0 and d["vf"] < 15, "a rear-end knocks our speed off, and is no wreck", (d["bumps"], d["wrecks"], d["vf"]))
    between(d["maxWob"], 0.3, 6, "the bot we hit wobbles")
    d = sim("--seed", 5, "--track", 0, "--state", 3, "--nobotguns", "--pos", -500.0, -186.0, 2.40, "--speed", 25, seconds=2)
    check(d["bumps"] >= 1 and d["maxShove"] > 1 and d["wrecks"] == 0, "a hit from the side shoves the bot across the road", (d["bumps"], d["maxShove"], d["wrecks"]))


@test
def the_warden_walks_its_beat():
    for t in range(NTRACKS):
        a = sim("--track", t, "--state", 3, seconds=4)
        b = sim("--track", t, "--state", 3, seconds=20)
        check(b["mechs"] == (4 if t == 6 else 2), "track %d: %s Wardens" % (t, "four" if t == 6 else "two"), b["mechs"])
        moved = math.hypot(b["mech"][0] - a["mech"][0], b["mech"][2] - a["mech"][2])
        between(moved, 30, 75, "track %d: the first Warden walks (m in 16 s)" % t)
        check(min(b["routeClear"]) >= 7, "track %d: both routes go round the buildings (m to the nearest)" % t, b["routeClear"])
        check(b["mechFootErr"] < 0.05, "track %d: every foot on its spot on the ground (worst miss, m)" % t, b["mechFootErr"])
        check(b["mech"][1] >= -9.01 and b["stomps"] >= 6, "track %d: on the ground (or wading), its feet coming down" % t, (b["mech"][1], b["stomps"]))
    d = sim("--track", 1, "--state", 3, "--nomech", seconds=20)
    check(d["stomps"] == 0 and d["mechs"] == 0, "--nomech: no Wardens", (d["stomps"], d["mechs"]))


@test
def simulation_is_deterministic():
    a = sim("--track", 0, "--state", 3, "--auto", seconds=40)
    b = sim("--track", 0, "--state", 3, "--auto", seconds=40)
    check(a == b, "two identical runs give identical reports")


# ----------------------------------------------------------------------------- full races
def _race(track, car):
    d = sim("--track", track, "--car", car, "--state", 3, "--auto", "--nobotguns", seconds=330)   # driving only: bots hold fire
    name = "track %d %s" % (track, CARS[car])
    check(d["finished"] == 1, "%s: autopilot finishes 3 laps" % name, (d["lap"], d["raceT"], d["timeUp"]))
    check(d["sinks"] == 0, "%s: never drives into the water" % name, d["sinks"])
    check(d["wrecks"] <= 1, "%s: at most one wreck" % name, d["wrecks"])
    check(d["camOffAxis"] == 0, "%s: chase camera stays behind the car" % name, d["camOffAxis"])
    between(d["bestLap"], 52, 100, "%s: best lap (s)" % name)
    check(1 <= d["place"] <= 10, "%s: place is valid" % name, d["place"])
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
    d = sim("--track", 1, "--state", 3, "--countdown", "--hold", "Left Shift", seconds=6.5)
    check(d["countdown"] > 0 and d["dist"] < 0.5 and d["nitro"] == 1, "holding nitro on the grid moves nothing and uses none", (d["countdown"], d["dist"], d["nitro"]))
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
NET_MAGIC, NET_PROTO = 0x59464346, 11
HDR, CARSTATE, PSNAP = "<IHBB", "<10fBBBBHffBB", "<BB16s"
CS_SIZE, PS_SIZE = 56, 74   # CarState, PlayerSnap (bytes)
C_JOIN, C_STATE, C_LEAVE, C_FOOT, C_SDSETUP, S_WELCOME, S_SNAPSHOT, S_REJECT, S_SHOWDOWN = 1, 2, 3, 5, 6, 16, 17, 18, 20
LOBBY, COUNTDOWN, RACE, RESULTS, SHOWDOWN = 0, 1, 2, 3, 4
FOOTSTATE, SDDROP = "<6f8B5HH", "<3fBB"   # FootState (44 bytes), SdDrop (14 bytes)
SD_DRIVE, SD_PREP, SD_LIVE, SD_OVER = 0, 1, 2, 3
FF_ON, FF_ALIVE, FF_ARRIVED, NOBODY = 1, 2, 32, 255
F_READY, F_FINISHED, F_NITRO, F_RACING, F_BOT, F_HOST = 1, 8, 16, 32, 64, 128
C_BOTS = 7


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
        if "--bots" not in self.args:   # the tests count players: no computer-driven cars unless one asks for them
            self.args += ["--bots", "0"]
        if "--min-lap" not in self.args:   # the protocol tests report finishes within a second of the start
            self.args += ["--min-lap", "0"]

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
        self.sock.sendto(self.hdr(C_STATE) + struct.pack("<I", self.seq) + struct.pack(CARSTATE, x, 1, 2, 0, 0, 0, 0, 0, 0, 0, 1, 2, flags, lap, 0, progress, 0.0, hit[0], hit[1]) + b"\0" * 4, self.addr)   # + the track we have, and padding

    def send_raw(self, data):
        self.sock.sendto(data, self.addr)

    def foot(self, pos=(0.0, 0.0, 0.0), hp=100, alive=True, killed_by=NOBODY, dealt=(0, 0, 0, 0, 0), claim=0, fire=0, arrived=True):
        """Report our fighter in the showdown (dealt: tenths of a health point, by slot)."""
        self.fseq = getattr(self, "fseq", 0) + 1
        fs = struct.pack(FOOTSTATE, pos[0], pos[1], pos[2], 0, 0, 0, max(0, min(255, int(hp))), 0, 0, FF_ON | (FF_ALIVE if alive else 0) | (FF_ARRIVED if arrived else 0), fire, NOBODY, killed_by, 0, dealt[0], dealt[1], dealt[2], dealt[3], dealt[4], claim)
        self.sock.sendto(self.hdr(C_FOOT) + struct.pack("<I", self.fseq) + fs, self.addr)

    def setup(self, drops, site=(0.0, 0.0, 0.0)):
        """The race winner chooses the place and lays out the pick-ups: a list of (x, y, z, kind)."""
        body = b"".join(struct.pack(SDDROP, x, y, z, kind, NOBODY) for x, y, z, kind in drops) + b"\0" * (14 * (12 - len(drops)))
        self.sock.sendto(self.hdr(C_SDSETUP) + struct.pack("<B3f", len(drops), *site) + body, self.addr)

    def showdown(self):
        """Newest showdown message (drains the socket), or None. Also remembers the newest snapshot's phase in self.phase_seen."""
        last = None
        self.sock.settimeout(0.3)
        try:
            while True:
                data, _ = self.sock.recvfrom(4096)
                kind = struct.unpack(HDR, data[:8])[2]
                if kind == S_SHOWDOWN:
                    last = data
                elif kind == S_SNAPSHOT:
                    self.phase_seen = data[12]
                self.sock.settimeout(0.0)
        except (socket.timeout, BlockingIOError):
            pass
        if last is None:
            return None
        tick, stage, stage_t, ring, sx, sy, sz, site_kind = struct.unpack("<IBfffffB", last[8:34])
        b = last[34:51]   # five fighters, five places, five kill counts, the winner, the number of pick-ups
        feet = [dict(zip(("x", "y", "z", "vx", "vz", "yaw", "hp", "armour", "gun", "flags", "fire", "target", "killedBy", "pad", "d0", "d1", "d2", "d3", "d4", "claim"), struct.unpack(FOOTSTATE, last[51 + k * 44:95 + k * 44]))) for k in range(5)]
        drops = [dict(zip(("x", "y", "z", "kind", "owner"), struct.unpack(SDDROP, last[271 + i * 14:285 + i * 14]))) for i in range(b[16])]
        return dict(stage=stage, stageT=stage_t, ring=ring, site=(sx, sy, sz), siteKind=site_kind, fighter=list(b[0:5]), place=list(b[5:10]), kills=list(b[10:15]), winner=b[15], feet=feet, drops=drops, size=len(last))

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
    with Server("--laps", 1, "--countdown", 0.4, "--results", 0.5, "--track", 2, "--weather", 3, "--no-showdown") as srv:
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
        clients = [FakeClient(srv.port, "P%d" % i) for i in range(10)]
        check(all(c.join()[0] == S_WELCOME for c in clients), "ten players fit")
        extra = FakeClient(srv.port, "ELEVENTH")
        kind, payload = extra.join()
        check(kind == S_REJECT and payload[0] == 1, "the eleventh is rejected: server full", (kind, payload[:1]))
        again = clients[0].join()
        check(again[0] == S_WELCOME and clients[0].id == 0, "re-joining from the same address keeps the same slot", clients[0].id)
        clients[0].send_raw(b"garbage")
        clients[0].send_raw(struct.pack(HDR, 0x12345678, 1, C_STATE, 0) + b"\0" * (8 + CS_SIZE))
        clients[0].send_raw(clients[0].hdr(C_STATE) + struct.pack("<I", 10 ** 6) + struct.pack(CARSTATE, float("nan"), 1, 2, 0, 0, 0, 0, 0, 0, 0, 1, 2, 0, 0, 0, 0.0, 0.0, 0, 0) + b"\0" * 4)
        clients[0].state(x=42.0)
        time.sleep(0.1)
        sn = clients[0].snapshot()
        check(sn is not None and sn["count"] == 10, "garbage, bad magic and NaN packets do not disturb the server", sn and sn["count"])
        check(abs(sn["players"]["P0"]["x"] - 42.0) < 1e-3, "the NaN position was refused; the valid one stands", sn["players"]["P0"]["x"])
        clients[1].send_raw(clients[1].hdr(C_LEAVE))
        _keepalive(clients[:1], 1.6)
        sn = clients[0].snapshot()
        check(sn["count"] == 1, "one player left, eight went silent: only the active one remains", sn["count"])
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
def net_server_sends_its_event_file():
    """2.3: a server with an event file for its track sends it (S_EVENTS, in pieces); the game puts it together and its
    race director plays it in the online race (here a message, a blast and a dolphin pod on Hilltop Heights)."""
    import tempfile
    d = tempfile.mkdtemp()
    open(os.path.join(d, "track0.fcfe"), "w").write('pod p1  -544 -311  r 40  n 2  k 1\nevent s1 say  text "FROM THE SERVER"  on time 2\n'
                                                     'event b1 blast  at -300 20 -260  k 1  on time 3\n')
    with Server("--laps", 1, "--countdown", 1, "--events", d) as srv:
        p = subprocess.Popen([BIN, "--sim", "--connect", "127.0.0.1:%d" % srv.port, "--name", "eve", "--ready", "--auto", "--seconds", "12"], stdout=subprocess.PIPE, text=True)
        r = json.loads(p.communicate(timeout=60)[0].strip().splitlines()[-1])
    check(r["netRace"] == 1 and r["netError"] == "", "in the online race", (r["netRace"], r["netError"]))
    check(r["netEventFiles"] >= 1 and r["eventsFrom"] == "server", "the game has the server's event file", (r["netEventFiles"], r["eventsFrom"]))
    check(r["eventsLoaded"] == 2 and r["events"] == 2 and r["pods"] == 1 and r["podDolphins"] == 2, "and plays it: both events, the pod", (r["eventsLoaded"], r["events"], r["pods"], r["podDolphins"]))
    check("events for track 0" in srv.log, "the server read it", srv.log[-300:])

@test
def dolphins_roam_the_sea_and_leap_where_told():
    """2.3 dolphin.h: a pod lives in its ring of sea all race, wandering, keeping to deep water and surfacing to breathe;
    a leap event takes the nearest one there (a small ring as it comes out, a splash as it goes back in). With no pod,
    one swims in for the leap. A leap on dry land is ignored."""
    import tempfile
    tmp = tempfile.mkdtemp(); f = os.path.join(tmp, "e.fcfe")
    open(f, "w").write("pod p1  -455 -530  r 55  n 3  k 1\nevent l1 dolphin  at -430 0 -540  to -400 0 -550  on time 2\n"
                       "event l2 dolphin  at -470 0 -515  to -500 0 -500  on time 9\n")
    d = sim("--track", 1, "--state", 3, "--events", f, seconds=25)
    check(d["pods"] == 1 and d["podDolphins"] == 3 and d["dolphinsNow"] == 3, "a pod of three, all still there", (d["pods"], d["podDolphins"], d["dolphinsNow"]))
    check(d["swum"] > 200 and d["dolphinMinDepth"] > 2.5, "they roam, in deep water", (d["swum"], d["dolphinMinDepth"]))
    check(d["podLeaps"] == 2 and d["leaps"] == 2 and d["dolphinSplashes"] == 2 and d["dolphinRings"] == 2, "two leaps by the pod: out with a ring, in with a splash", (d["podLeaps"], d["leaps"], d["dolphinSplashes"], d["dolphinRings"]))
    open(f, "w").write("event l1 dolphin  at -430 0 -540  to -400 0 -550  on time 1\nevent l2 dolphin  at -200 0 -118  to -190 0 -118  on time 1\n")
    e = sim("--track", 1, "--state", 3, "--events", f, seconds=18)
    check(e["leaps"] == 1 and e["podLeaps"] == 0 and e["dolphinsNow"] == 0, "no pod: one swims in, leaps and goes; the one on land never comes", (e["leaps"], e["podLeaps"], e["dolphinsNow"]))


@test
def weather_turns_in_a_race():
    """2.3: a weather event turns it (over its seconds, blending fog, light, grip and the sea); a track whose weather
    drifts turns it now and then by itself, telling you. Online the server's weather holds."""
    import tempfile
    tmp = tempfile.mkdtemp(); f = os.path.join(tmp, "e.fcfe")
    open(f, "w").write("event w1 weather  wx storm  over 4  on time 2\n")
    d = sim("--track", 1, "--state", 3, "--weather", 0, "--events", f, seconds=4)
    check(d["wxNow"] == 3 and 0.2 < d["wxMix"] < 0.8, "half way through turning to storm", (d["wxNow"], d["wxMix"]))
    d = sim("--track", 1, "--state", 3, "--weather", 0, "--events", f, seconds=9)
    check(d["wxNow"] == 3 and d["wxMix"] == 1 and d["sea"] > 0.95, "a storm, and a rough sea", (d["wxNow"], d["wxMix"], d["sea"]))
    open(f, "w").write("drift 20\n")
    d = sim("--track", 1, "--state", 3, "--weather", 0, "--events", f, seconds=70)
    check(d["drift"] == 20 and d["wxTurns"] >= 2, "drifting weather turns by itself", (d["drift"], d["wxTurns"]))


@test
def mini_wardens_jetpack_off_a_roof():
    """2.3 jetdrop.h: a minis event puts mini Wardens on a roof; one by one they jetpack down to the roadside, and
    take a shot at you as you come by."""
    import tempfile
    tmp = tempfile.mkdtemp(); f = os.path.join(tmp, "e.fcfe")
    open(f, "w").write("event m1 minis  at -147 30 -93  to -170 0 -110  k 3  on time 1\n")
    d = sim("--track", 0, "--state", 3, "--events", f, seconds=8)
    check(d["jetDrops"] == 3 and d["jetLanded"] == 3, "three came down", (d["jetDrops"], d["jetLanded"]))
    m = sim("--track", 3, "--state", 3, "--auto", "--nobotguns", seconds=110)
    check(m["jetLanded"] == 3 and m["jetShots"] >= 1 and m["jetDrops"] == 5, "Midnight Mile's drop: they land and shoot (and two snipers stay up)", (m["jetLanded"], m["jetShots"], m["jetDrops"]))


@test
def the_editor_makes_dolphins_minis_and_weather():
    """2.3 fcfedit: the DOLPHINS moment (click the sea for the pod, then each leap spot), MINI WARDENS (where cars set it
    off, the roof, where they land) and WEATHER CHANGE, saved and loaded back."""
    import tempfile
    tmp = tempfile.mkdtemp()
    script = ("moment 7; click -455 -530; click -430 -540; click -470 -515; enter; "
              "moment 8; lapstart; click -363 -93; click -396 -97; moment 9; lapstart; drift 90; save")
    subprocess.run([BIN, "--sim", "--seconds", "0.1", "--evedit", "--track", "1", "--evscript", script], cwd=tmp, capture_output=True, timeout=120)
    f = os.path.join(tmp, "events", "track1.fcfe")
    check(os.path.exists(f), "saved")
    if not os.path.exists(f): return
    t = open(f).read()
    check(t.count("\npod ") == 2 and t.count(" dolphin ") == 3, "a pod and two leaps (with Bayside's own pod and its leap)", t[:300])
    check(" minis " in t and " weather " in t and "drift 90" in t, "the minis, the weather and the drift", t[:400])

@test
def aim_down_the_sights_and_drop_the_rooftop_snipers():
    """2.4: mini Wardens that stay on a roof ("stay") shoot you once you are on foot and in their sight; down the
    sights (--ads: aiming, firing) a shot goes to the middle of the view, up on to the roof, and brings them down."""
    import tempfile
    tmp = tempfile.mkdtemp(); f = os.path.join(tmp, "e.fcfe")
    open(f, "w").write("event s1 minis  at -471 70 -174  to -480 9 -175  k 2  stay  on time 1\n")
    d = sim("--track", 0, "--state", 3, "--onfoot", "--events", f, seconds=25)
    check(d["meOn"] == 1 and d["jetNow"] == 2 and d["jetLanded"] == 0, "on foot; two snipers stay up", (d["meOn"], d["jetNow"], d["jetLanded"]))
    check(d["snipes"] >= 10 and 0 < d["snipeHits"] < d["snipes"] and d["meHp"] < 100, "they shoot at you, some hit", (d["snipes"], d["snipeHits"], d["meHp"]))
    a = sim("--track", 0, "--state", 3, "--onfoot", "--ads", "--aimmini", "--gun", 2, "--events", f, seconds=20)
    check(a["adsK"] > 0.95 and a["aimPt"][1] > 60, "down the sights, looking up at the roof", (a["adsK"], a["aimPt"]))
    check(a["jetDowned"] == 2 and a["jetNow"] == 0 and a["gibs"] == 2, "both brought down, in pieces", (a["jetDowned"], a["jetNow"], a["gibs"]))
    c = sim("--track", 0, "--state", 3, "--events", f, seconds=25)
    check(c["snipes"] == 0, "drivers they leave alone", c["snipes"])


@test
def ammo_and_medkits_lie_about_the_town():
    """2.4: about 30 pick-ups on every track, on the paths between buildings (not the road): boxes of rounds and
    medkits. Walk over a box of rounds and every gun you have gets a clip more."""
    for t in range(7):
        d = sim("--track", t, "--state", 3, seconds=0.3)
        check(d["townDrops"] >= 20, "track %d has its pick-ups" % t, d["townDrops"])
    d = sim("--track", 0, "--state", 3, seconds=0.3); x, z, kind = d["drop0"]
    check(kind == 8, "the first is a box of rounds", kind)
    far = sim("--track", 0, "--state", 3, "--pos", x - 4.2, z, 0, "--onfoot", "--keys", 6, seconds=4)
    on = sim("--track", 0, "--state", 3, "--pos", x - 2.2, z, 0, "--onfoot", "--keys", 6, seconds=4)
    check(far["footPickups"] == 0 and on["footPickups"] == 1 and on["meAmmoTotal"] > far["meAmmoTotal"], "walk over it: more rounds", (far["meAmmoTotal"], on["meAmmoTotal"], on["footPickups"]))


@test
def a_shot_dolphin_laughs():
    """2.42: dolphins cannot be hurt; shot (or near a grenade) one laughs at you and comes up for a look."""
    import tempfile
    tmp = tempfile.mkdtemp(); f = os.path.join(tmp, "e.fcfe")
    open(f, "w").write("pod p1  -600 -200  r 25  n 2  k 1\n")
    d = sim("--track", 0, "--state", 3, "--pos", -570, -200, 4.2, "--onfoot", "--ads", "--aimdolphin", "--gun", 2, "--events", f, seconds=20)
    check(d["footShots"] > 10 and d["laughs"] > 5, "shot at, it laughs", (d["footShots"], d["laughs"]))
    check(d["dolphinsNow"] == 2, "and they are both still there", d["dolphinsNow"])

@test
def manic_is_a_time_trial():
    """2.5 MANIC (--mode 4): set laps against the clock, alone from the middle of the start line (2.6.1) with your best
    lap as a ghost; no rocket crates or shield pick-ups, nobody fires, no countdown clock; each lap timed; the best
    total kept per track and lap count, and the best lap's ghost per track."""
    import tempfile
    home = tempfile.mkdtemp(); env = dict(os.environ, HOME=home, XDG_DATA_HOME=home, APPDATA=home)
    def run():
        p = subprocess.run([BIN, "--sim", "--save", "--seconds", "200", "--track", "0", "--state", "3", "--mode", "4", "--laps", "2", "--auto"], capture_output=True, text=True, env=env, timeout=400)
        return json.loads(p.stdout.strip().splitlines()[-1])
    d = run()
    check(d["manic"] == 1 and d["raceLaps"] == 2 and d["finished"] == 1 and d["timeUp"] == 0, "two laps, finished, no clock ran out", (d["manic"], d["raceLaps"], d["finished"], d["timeUp"]))
    check(len(d["manicLaps"]) == 2 and abs(sum(d["manicLaps"]) - d["raceT"]) < 6, "each lap timed", (d["manicLaps"], d["raceT"]))
    check(d["bumps"] == 0 and d["wrecks"] == 0 and d["rocketsAtMe"] == 0, "no contact, nothing fired at you", (d["bumps"], d["wrecks"], d["rocketsAtMe"]))
    check(d["crateCount"] == 0 and d["shieldPUs"] == 0 and d["ammo"] == 0, "no rockets or shields on the track", (d["crateCount"], d["shieldPUs"], d["ammo"]))
    check(d["manicNew"] == 1, "the first run is a record", d["manicNew"])
    check(d["aiMoved"] < 1, "alone on the track: the field stays parked, out of sight", d["aiMoved"])
    check(d["ghostN"] > 400 and abs(d["ghostT"] - min(d["manicLaps"])) < 0.05, "the best lap becomes the ghost", (d["ghostN"], d["ghostT"], d["manicLaps"]))
    e = run()
    check(e["manicRecord"] > 0 and abs(e["manicRecord"] - d["raceT"]) < 0.05, "the record is kept for next time", (e["manicRecord"], d["raceT"]))
    check(e["ghostN"] > 400 and e["ghostT"] <= d["ghostT"] + 1e-3, "and the ghost: loaded, raced, kept if it is still the best", (e["ghostN"], e["ghostT"], d["ghostT"]))

@test
def headshots_numbers_bloom_and_recoil():
    """2.6: a round to the head does double (the rifle drops a rooftop sniper in one where the body takes two); each
    hit you land puts up a number; firing opens the crosshair (bloom) and the view climbs (recoil), both settling."""
    import tempfile
    tmp = tempfile.mkdtemp(); f = os.path.join(tmp, "e.fcfe")
    open(f, "w").write("event s1 minis  at -471 70 -174  to -480 9 -175  k 2  stay  on time 1\n")
    body = sim("--track", 0, "--state", 3, "--onfoot", "--ads", "--aimmini", "--gun", 2, "--events", f, seconds=20)
    head = sim("--track", 0, "--state", 3, "--onfoot", "--ads", "--aimhead", "--gun", 2, "--events", f, seconds=20)
    check(body["jetDowned"] == 2 and body["headshots"] == 0 and body["dmgNums"] == 4, "body shots: two each, a number each", (body["jetDowned"], body["headshots"], body["dmgNums"]))
    check(head["jetDowned"] == 2 and head["headshots"] == 2 and head["dmgNums"] == 2, "headshots: one each", (head["jetDowned"], head["headshots"], head["dmgNums"]))
    smg = sim("--track", 0, "--state", 3, "--onfoot", "--ads", "--aimat", -400, 30, -100, "--gun", 3, seconds=6)
    check(smg["meBloom"] > 0.3, "holding the SMG's trigger opens the crosshair", smg["meBloom"])

@test
def net_roam_server_shares_mini_wardens():
    """A --roam server: everyone free-roams the same city. The host's game runs the mini Wardens; a guest sees them,
    punches one out and gets its flowers."""
    with Server("--roam", "--track", 1) as srv:
        def client(name, secs, *extra):
            return subprocess.Popen([BIN, "--sim", "--nosave", "--connect", "127.0.0.1:%d" % srv.port, "--name", name, "--ready", "--seconds", str(secs)] + [str(x) for x in extra], stdout=subprocess.PIPE, text=True)
        host = client("host", 40, "--minisnow", "--minishold"); time.sleep(1.5); guest = client("guest", 36, "--outcar", "--footai")
        h, g = [json.loads(p.communicate(timeout=120)[0].strip().splitlines()[-1]) for p in (host, guest)]
    check(h["netPhase"] == 5 and g["netPhase"] == 5 and h["state"] == 3 and g["state"] == 3, "both are in online roam", (h["netPhase"], g["netPhase"]))
    check(h["miniSpawned"] == 1 and g["miniSpawned"] == 0, "only the host's game lands them", (h["miniSpawned"], g["miniSpawned"]))
    check(h["miniKilled"] == 1 and g["roamFlowers"] >= 2, "the guest's fists beat it, and the guest gets the flowers", (h["miniKilled"], g["roamFlowers"]))


@test
def space_track_is_a_slime_ring_round_a_planet():
    """The secret space track: a ring of see-through slime with a planet in the middle. The autopilot gets round it
    without falling off; a car put past the edge falls into space and is put back on the road."""
    d = sim("--space", "--state", 3, "--mode", 0, "--auto", seconds=40)
    check(d["space"] == 1 and d["slimeTris"] > 5000, "the slime road is built", (d["space"], d["slimeTris"]))
    check(d["spaceFalls"] == 0 and d["lap"] >= 1, "the autopilot drives it without falling off", (d["spaceFalls"], d["lap"]))
    off = sim("--space", "--state", 3, "--mode", 0, "--pos", 600, 0, 0, seconds=4)
    check(off["spaceFalls"] >= 1, "past the edge you fall off", off["spaceFalls"])


@test
def the_golden_ufo_leads_to_space():
    """Every track has a golden UFO in free roam. Parking under it starts a ten-second countdown, then the space race."""
    for t in range(7):
        d = sim("--track", t, "--state", 3, "--mode", 1, seconds=0.2)
        check(d["goldUfo"] == 1, "track %d has a golden UFO" % t, d["goldUfoAt"])
    d = sim("--track", 0, "--state", 3, "--mode", 1, "--ufogo", seconds=16)
    check(d["spaceLaunches"] == 1 and d["space"] == 1 and d["state"] == 3, "parked under it: off to space", (d["spaceLaunches"], d["space"], d["state"]))
    r = sim("--track", 0, "--state", 3, "--mode", 0, "--ufogo", seconds=16)
    check(r["goldUfo"] == 1 and r["spaceLaunches"] == 0 and r["space"] == 0, "not in an ordinary race", (r["spaceLaunches"], r["space"]))


@test
def net_golden_ufo_takes_a_roam_server_to_space():
    """On a roam server, one player parks under the golden UFO: ten seconds later everyone is on the space track, and
    when they have all gone the server goes back to roaming its own track."""
    with Server("--roam", "--track", 0, "--countdown", 2, "--timeout", 3) as srv:
        def client(name, secs, *extra):
            return subprocess.Popen([BIN, "--sim", "--nosave", "--connect", "127.0.0.1:%d" % srv.port, "--name", name, "--ready", "--seconds", str(secs)] + [str(x) for x in extra], stdout=subprocess.PIPE, text=True)
        a = client("finder", 22, "--ufogo"); time.sleep(1.0); b = client("other", 20)
        fa, fb = [json.loads(p.communicate(timeout=120)[0].strip().splitlines()[-1]) for p in (a, b)]
        time.sleep(5)
    check(fa["space"] == 1 and fb["space"] == 1, "both are racing in space", (fa["space"], fb["space"]))
    check(fa["netPhase"] in (1, 2) and fb["netPhase"] in (1, 2), "in the race", (fa["netPhase"], fb["netPhase"]))
    check("found the golden UFO" in srv.log and "track 255 (SPACE RING)" in srv.log, "the server counted down and went to space", srv.log[-600:])
    check("phase ROAM  track 0" in srv.log.split("SPACE RING")[-1], "and back to roaming once they left", srv.log[-400:])


@test
def net_server_sends_custom_tracks():
    """A server with --tracks DIR: a game that has not got the track downloads it, builds it and races on it."""
    with Server("--tracks", os.path.join(ROOT, "tests", "tracks"), "--track", NTRACKS, "--laps", 1, "--countdown", 1) as srv:
        procs = [subprocess.Popen([BIN, "--sim", "--nosave", "--connect", "127.0.0.1:%d" % srv.port, "--name", n, "--ready", "--auto", "--seconds", "14"], stdout=subprocess.PIPE, text=True) for n in ("alice", "bob")]
        reports = [json.loads(p.communicate(timeout=60)[0].strip().splitlines()[-1]) for p in procs]
    for name, d in zip(("alice", "bob"), reports):
        check(d["netError"] == "" and d["netTrackId"] == NTRACKS, "%s is on the server's custom track" % name, (d["netError"], d["netTrackId"]))
        check(d["netPhase"] == RACE and d["state"] == 3 and d["trackLen"] > 500 and d["dist"] > 100, "%s built it and is racing on it" % name, (d["netPhase"], d["state"], d["trackLen"], d["dist"]))


@test
def net_player_shares_a_track_from_the_lobby():
    """SHARE TRACK in the lobby: one player sends a track of their own; the lobby votes on it (with a preview of its
    road). If more say yes than no, everyone races it and keeps it; afterwards the server goes back to its own track."""
    import tempfile, shutil
    mine, none = tempfile.mkdtemp(), tempfile.mkdtemp()
    shutil.copy(os.path.join(ROOT, "tests", "tracks", "test-loop.fcft"), mine)
    try:
        with Server("--laps", 1, "--countdown", 1, "--results", 1, "--no-showdown", "--lobby", 3) as srv:   # (--lobby: nobody alone can start a race before the other is in)
            def client(name, folder, secs, *extra):
                return subprocess.Popen([BIN, "--sim", "--nosave", "--connect", "127.0.0.1:%d" % srv.port, "--name", name, "--ready", "--auto", "--seconds", str(secs), "--tracksdir", folder] + list(extra), stdout=subprocess.PIPE, text=True)
            procs = [client("alice", mine, 130, "--share", "--sharewhen", "2"), client("bob", none, 130, "--vote", "yes")]
            a, b = [json.loads(p.communicate(timeout=260)[0].strip().splitlines()[-1]) for p in procs]
            time.sleep(2.5)   # (the server goes back to its own track a second after the results: let it, before it is stopped)
        kept = os.listdir(none)
    finally:
        shutil.rmtree(mine); shutil.rmtree(none)
    check(a["shared"] == 1 and "the lobby votes on it" in srv.log, "alice's game sent her track and the lobby voted on it", a["shared"])
    check("2 yes, 0 no: playing it" in srv.log and b["votesPassed"] == 1, "bob said yes: it passed", b["votesPassed"])
    check("phase RACE  track %d" % NTRACKS in srv.log, "and they raced it")
    # its own track: the one its lobby was on when the shared race began (a rotation race may have run before the
    # share landed, so it is not always track 0)
    import re
    race = srv.log.find("phase RACE  track %d" % NTRACKS); res = srv.log.find("RESULTS  track %d" % NTRACKS)
    own = [t for t in re.findall(r"phase LOBBY  track (\d+)", srv.log[:race]) if int(t) != NTRACKS] if race >= 0 else []
    back = re.search(r"phase LOBBY  track (\d+)", srv.log[res:]) if res >= 0 else None
    check(own and back and back.group(1) == own[-1], "afterwards the server went back to its own track", (own[-1:] if own else None, back.group(1) if back else None))
    check(b["kept"] == 1 and kept == ["my-track.fcft"], "bob kept the track in his tracks", (b["kept"], kept))

    mine, none = tempfile.mkdtemp(), tempfile.mkdtemp()
    shutil.copy(os.path.join(ROOT, "tests", "tracks", "test-loop.fcft"), mine)
    try:
        with Server("--countdown", 1) as srv:
            procs = [subprocess.Popen([BIN, "--sim", "--nosave", "--connect", "127.0.0.1:%d" % srv.port, "--name", n, "--auto", "--seconds", "10", "--tracksdir", f] + x, stdout=subprocess.PIPE, text=True)
                     for n, f, x in (("alice", mine, ["--share", "--sharewhen", "2"]), ("bob", none, ["--vote", "no"]))]   # (alice waits until bob is in the lobby)
            b = [json.loads(p.communicate(timeout=60)[0].strip().splitlines()[-1]) for p in procs][1]
    finally:
        shutil.rmtree(mine); shutil.rmtree(none)
    check("1 yes, 1 no: staying" in srv.log and b["votesPassed"] == 0 and b["netTrackId"] == 0, "a tie: the lobby stays on its track", (b["votesPassed"], b["netTrackId"]))

    with Server("--no-uploads") as srv:   # a server that does not take them
        c = FakeClient(srv.port, "mallory"); c.join(); body = open(os.path.join(ROOT, "tests", "tracks", "test-loop.fcft"), "rb").read()
        import zlib
        c.send_raw(c.hdr(11) + struct.pack("<BBBBHHI", 0, 0, 1, 0, len(body), len(body), zlib.crc32(body)) + body); time.sleep(0.4)
    check("shared a track" not in srv.log, "--no-uploads: the server ignores shared tracks")

@test
def net_server_holds_up_against_misbehaving_players():
    """Hardening: a flood of packets from one address is cut off, one IP can only fill so many slots, a finish that
    comes too soon is not believed, one address cannot share track after track, uploaded names are cleaned, and when the
    shared tracks fill up the oldest one not in use makes room."""
    import zlib
    with Server("--rate", 50) as srv:
        c = FakeClient(srv.port, "flood")
        q = struct.pack(HDR, NET_MAGIC, NET_PROTO, 4, 0xFF) + struct.pack("<I", 7) + b"\0" * 36
        for _ in range(600): c.send_raw(q)
        time.sleep(0.5); got = 0
        c.sock.settimeout(0.2)
        try:
            while True: c.sock.recvfrom(4096); got += 1
        except Exception: pass
    check(0 < got <= 130, "600 queries at once: only a burst's worth answered", got)
    with Server("--per-ip", 2) as srv:
        cs = [FakeClient(srv.port, "p%d" % k) for k in range(3)]
        kinds = [c.join()[0] for c in cs]
    check(kinds == [S_WELCOME, S_WELCOME, S_REJECT], "a third player from the same IP is turned away", kinds)
    with Server("--laps", 1, "--countdown", 0.3, "--min-lap", 30) as srv:
        a = FakeClient(srv.port, "SPEEDY"); a.join(); a.state(flags=F_READY); time.sleep(0.6)
        a.state(flags=F_FINISHED, lap=2, progress=3400); time.sleep(0.3)
        sn = a.snapshot()
    check(not (sn["players"]["SPEEDY"]["flags"] & F_FINISHED) and "not believed" in srv.log, "a finish a second after the start is not believed", sn["players"])
    def upload(c, body):
        c.send_raw(c.hdr(11) + struct.pack("<BBBBHHI", 0, 0, 1, 0, len(body), len(body), zlib.crc32(body)) + body); time.sleep(0.3)
    base = open(os.path.join(ROOT, "tests", "tracks", "test-loop.fcft"), "rb").read()
    variant = lambda k, name: base.replace(b"name    MY TRACK", b"name    " + name).replace(b"seed    100", b"seed    %d" % (200 + k))
    with Server() as srv:
        c = FakeClient(srv.port, "sharer"); c.join(); upload(c, variant(1, b"ONE")); upload(c, variant(2, b"TWO"))
    check(srv.log.count("shared a track") == 1 and "shared again too soon" in srv.log, "a second share from the same address straight after is ignored")
    with Server("--share-gap", 0, "--shared-max", 2) as srv:
        c = FakeClient(srv.port, "sharer"); c.join()
        for k, n in enumerate((b"FIRST", b"SECOND", b"BAD\x01NAME")): upload(c, variant(k, n))
    check(srv.log.count("shared a track") == 3 and "dropping the oldest shared track (FIRST)" in srv.log, "with two kept, the third pushes out the oldest", srv.log[-400:])
    check("track %d, BAD?NAME" % (NTRACKS + 0) in srv.log, "names from the file are cleaned (and the new one took the old one's place)", srv.log[-300:])



@test
def net_slow_track_build_is_not_a_timeout():
    """A game that takes seconds to build the track (the 360) tells the server first, and is not dropped meanwhile."""
    with Server("--timeout", 1.0, "--track", 2) as srv:
        p = subprocess.Popen([BIN, "--sim", "--nosave", "--connect", "127.0.0.1:%d" % srv.port, "--name", "slow", "--loadstall", "3", "--seconds", "8"], stdout=subprocess.PIPE, text=True)
        d = json.loads(p.communicate(timeout=60)[0].strip().splitlines()[-1])
    check(d["loadingNotices"] >= 1 and d["net"] == 1 and d["netError"] == "", "it said it was loading, took 3 s, and is still connected", (d["loadingNotices"], d["net"], d["netError"]))
    check("timed out" not in srv.log, "the server waited for it", srv.log[-300:])


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
        check((track, count, maxp, phase) == (2, 0, 10, LOBBY) and name.split(b"\0")[0] == b"TEST BOX", "with its track, players and name", (track, count, maxp, name))
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
    want = {0: 1, 1: 0, 2: 0, 3: 2, 4: 0, 5: 0, 6: 0}
    for t in range(NTRACKS):
        d = dump(t)
        check(len(d["bridges"]) == want[t], "track %d has %d bridge(s)" % (t, want[t]), len(d["bridges"]))
        for b in d["bridges"]:
            between(b["deck"] - b["under"], 6.5, 8.5, "track %d: the deck clears the road underneath (m)" % t)
    for t in (0, 3):
        d = sim("--track", t, "--state", 3, "--auto", "--nobotguns", seconds=110)
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
    soon = sim("--track", 1, "--state", 3, "--auto", "--nobotguns", "--rockets", 1, "--fire", 3.0, seconds=11)
    later = sim("--track", 1, "--state", 3, "--auto", "--nobotguns", "--rockets", 1, "--fire", 3.0, seconds=16)
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


def _race_to_the_line(clients):
    """Ready up, wait for the race, and return once it is on."""
    for _ in range(100):
        for c in clients:
            c.state(flags=F_READY)
        sn = clients[0].snapshot()
        if sn and sn["phase"] == RACE:
            return
    raise RuntimeError("the race never started")


@test
def net_showdown_server_runs_the_podium_fight():
    """Three protocol-level clients: the server picks the podium, takes the pick-ups from the winner, gives each to one claimer, and places the fallen."""
    with Server("--laps", 1, "--countdown", 0.3, "--results", 0.6) as srv:
        a, b, c = cl = [FakeClient(srv.port, n) for n in ("ALICE", "BOB", "CAROL")]
        for x in cl:
            x.join()
        _race_to_the_line(cl)
        for x in cl:   # they cross the line in this order
            x.state(flags=F_FINISHED, lap=2, progress=3400)
            time.sleep(0.12)
        dealt = [0, 0, 0, 0, 0]

        def tick(alive=(True, True, True), killed_by=NOBODY, claims=(0, 0, 0), arrived=(True, True, True)):
            for k, x in enumerate(cl):
                x.state(flags=F_FINISHED, lap=2, progress=3400)
                x.foot(pos=(10.0 * k, 1.0, 5.0), hp=100 if alive[k] else 0, alive=alive[k], killed_by=killed_by, dealt=dealt if k == 0 else (0, 0, 0, 0, 0), claim=claims[k], arrived=arrived[k])
            time.sleep(0.05)
            return a.showdown()

        def until(pred, limit, **kw):
            t0, m = time.time(), None
            while time.time() - t0 < limit:
                m = tick(**kw) or m
                if m and pred(m):
                    return m
            raise RuntimeError("timed out waiting on the showdown: %s" % (m and {k: m[k] for k in ("stage", "stageT", "place", "winner")}))

        m = until(lambda m: True, 2, arrived=(True, True, False))
        check(a.phase_seen == SHOWDOWN and m["fighter"] == [a.id, b.id, c.id, NOBODY, NOBODY] and m["stage"] == SD_DRIVE, "three finishers: the showdown, in finishing order (there is room for five)", (a.phase_seen, m["fighter"], m["stage"]))
        check(m["size"] == 439 and m["siteKind"] == 0, "the showdown message is 439 bytes; the place is not chosen yet", (m["size"], m["siteKind"]))
        b.setup([(9.0, 9.0, 9.0, 0)], site=(1.0, 1.0, 1.0))
        a.setup([(1.0, 2.0, 3.0, 1), (4.0, 5.0, 6.0, 4)], site=(70.0, 8.0, -90.0))
        m = until(lambda m: len(m["drops"]) > 0, 2, arrived=(True, True, False))
        check(len(m["drops"]) == 2 and abs(m["drops"][1]["x"] - 4.0) < 1e-4 and m["drops"][1]["kind"] == 4, "only the race winner lays out the pick-ups", m["drops"])
        check(m["siteKind"] == 2 and m["site"] == (70.0, 8.0, -90.0), "and chooses where the ring goes, anywhere on the map", (m["siteKind"], m["site"]))
        m = until(lambda m: m["drops"][0]["owner"] != NOBODY, 3, claims=(0, 1, 1), arrived=(True, True, False))
        check(m["drops"][0]["owner"] in (1, 2) and m["drops"][1]["owner"] == NOBODY, "two claim the same pick-up: one of them gets it", m["drops"])
        check(m["stage"] == SD_DRIVE, "the countdown waits while one car is still on its way", m["stage"])
        m = until(lambda m: m["stage"] == SD_PREP, 2)
        m = until(lambda m: m["drops"][0]["owner"] == NOBODY, 17)
        check(m["stage"] == SD_PREP, "all in: the countdown runs, and the pick-up that was taken is back before it ends", (m["stage"], m["stageT"]))
        m = until(lambda m: m["stage"] == SD_LIVE, 22)
        check(all(f["flags"] & FF_ALIVE for f in m["feet"][:3]) and abs(m["feet"][2]["x"] - 20.0) < 1e-3 and m["ring"] > 45, "the fight starts with everyone standing, their reports relayed", (m["feet"][2]["x"], m["ring"]))
        dealt[2] = 1000
        m = until(lambda m: m["place"][2] != 0, 3, alive=(True, True, False), killed_by=0)
        check(m["place"][:3] == [0, 0, 3] and m["kills"][0] == 1 and m["feet"][0]["d2"] == 1000 and m["stage"] == SD_LIVE, "first down is third, the kill is credited, and the damage report is relayed", (m["place"], m["kills"], m["feet"][0]["d2"]))
        m = until(lambda m: m["stage"] == SD_OVER, 3, alive=(True, False, False), killed_by=0)
        check(m["place"][:3] == [1, 2, 3] and m["winner"] == 0 and m["kills"][0] == 2, "last one standing wins", (m["place"], m["winner"], m["kills"]))
        until(lambda m: a.phase_seen == RESULTS, 9, alive=(True, False, False))
    check("showdown:" in srv.log and "1. ALICE" in srv.log and "2 down" in srv.log, "the server log has the showdown table", srv.log[-300:])


@test
def championship_runs_four_rounds_with_points():
    """Offline: four rampage races on tracks 1-4 with 1, 2, 2 and 3 laps; every round scores all ten cars and a showdown winner gets 5 more."""
    d = sim("--champ", "--seed", 5, "--state", 3, "--sdnow", "--auto", "--footai", "--keys", 5, seconds=2000)
    check(d["champDone"] == 1 and d["champRound"] == 3, "all four rounds were run", (d["champDone"], d["champRound"]))
    check(d["track"] == 3 and d["raceLaps"] == 3, "the last round is track 4 over 3 laps", (d["track"], d["raceLaps"]))
    check(d["champTotal"] == 4 * 101 + 4 * 5, "101 race points a round (25 to 1 for the top ten) and 5 for each showdown won", d["champTotal"])


@test
def ring_goes_somewhere_new_each_race():
    spots = []
    for seed in (1, 2, 3):
        d = sim("--seed", seed, "--state", 3, "--mode", 2, "--sdnow", seconds=3)
        spots.append((d["shdSiteX"], d["shdSiteZ"]))
    gaps = [((a[0] - b[0]) ** 2 + (a[1] - b[1]) ** 2) ** 0.5 for i, a in enumerate(spots) for b in spots[i + 1:]]
    check(min(gaps) > 100, "three games, three different places for the ring", spots)


@test
def bots_drive_the_backstreets_to_an_off_road_ring():
    d = sim("--seed", 5, "--state", 3, "--mode", 2, "--sdnow", "--auto", "--footai", "--keys", 5, seconds=120)
    feet = d["fighters"].split(";")
    check(d["shdOffRoad"] == 1, "the ring is off the road (the bots find a way in: straight, or along the backstreets)", (d["shdOffRoad"], d["shdPath"]))
    check(all(f.split("|")[0] == "1" for f in feet) and d["prepAt"] > 0, "all five get out of their cars at the ring", (feet, d["prepAt"]))


@test
def net_bots_fill_the_grid_and_the_host_reports_them():
    """Protocol level: the server adds four bots, names one player their host, and takes their cars only from that player."""
    with Server("--laps", 1, "--countdown", 0.5, "--bots", 4) as srv:
        a, b = FakeClient(srv.port, "ANNA"), FakeClient(srv.port, "BEN")
        a.join()
        b.join()
        sn = None
        for _ in range(60):
            a.state(flags=F_READY)
            b.state(flags=F_READY)
            sn = a.snapshot()
            if sn and sn["phase"] == RACE:
                break
        check(sn and sn["phase"] == RACE and sn["count"] == 6, "two people and four bots on the grid", sn and (sn["phase"], sn["count"]))
        bots = {n: p for n, p in sn["players"].items() if p["flags"] & F_BOT}
        check(sorted(bots) == ["KATO", "MOSS", "REYES", "VEGA"], "the four bots are flagged as such", sorted(bots))
        check(sn["players"]["ANNA"]["flags"] & F_HOST and not sn["players"]["BEN"]["flags"] & F_HOST, "the first player in is their host")
        bid = bots["VEGA"]["id"]

        def report(c, seq, x):
            cs = struct.pack(CARSTATE, x, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 123.0, 0, NOBODY, 0)
            c.sock.sendto(c.hdr(C_BOTS) + struct.pack("<IB", seq, 1) + bytes([bid]) + cs + bytes(3 * (1 + len(cs))), c.addr)

        report(b, 1, 666.0)
        report(a, 2, 42.0)
        for _ in range(10):
            a.state()
            b.state()
            sn = b.snapshot() or sn
        check(abs(sn["players"]["VEGA"]["x"] - 42.0) < 0.01 and abs(sn["players"]["VEGA"]["progress"] - 123.0) < 0.01, "the host's word on a bot is relayed; anybody else's is ignored", sn["players"]["VEGA"])


@test
def net_bots_race_and_fight_under_a_real_host():
    """A real (headless) game hosts the four bots: they finish behind it, drive to the ring, and one fighter is left standing."""
    with Server("--laps", 1, "--countdown", 0.5, "--results", 60, "--bots", 4, "--showdown-site", 0.3) as srv:
        p = subprocess.run([BIN, "--sim", "--nosave", "--connect", "127.0.0.1:%d" % srv.port, "--name", "hero", "--ready", "--sdnow", "--auto", "--footai", "--keys", "5", "--seconds", "170"], capture_output=True, text=True, timeout=260)
        d = json.loads(p.stdout.strip().splitlines()[-1])
    check(d["netBots"] == 4 and d["netShdCount"] == 1 and d["netSlot"] == 0, "the game hosted four bots and went into the showdown with them", (d["netBots"], d["netShdCount"], d["netSlot"]))
    for name in ("VEGA", "MOSS", "KATO", "REYES"):
        check("(%s) finished" % name in srv.log and name in srv.log.split("phase SHOWDOWN")[1], "%s finished the race and is in the showdown" % name)
    check("showdown: all in the ring" in srv.log and " takes it" in srv.log and "phase RESULTS" in srv.log, "five fighters reached the ring and the server saw it through to a winner", srv.log[-600:])
    downs = [x for x in srv.log.splitlines() if x.startswith("showdown:") and " is down" in x]
    check(len(downs) == 4, "four of the five went down", downs)


@test
def net_showdown_game_client_fights_a_remote_fighter():
    """A real (headless) game client, driven by the bot brain, against a protocol-level fighter that just stands there."""
    with Server("--laps", 1, "--countdown", 0.5, "--results", 60, "--showdown-site", 0.3) as srv:
        game = subprocess.Popen([BIN, "--sim", "--nosave", "--connect", "127.0.0.1:%d" % srv.port, "--name", "hero", "--ready", "--sdnow", "--auto", "--footai", "--keys", "5", "--seconds", "95"], stdout=subprocess.PIPE, text=True)
        f = FakeClient(srv.port, "DUMMY")
        f.join()
        hero_done = False
        for _ in range(200):   # let the game cross the line first, then follow it over
            f.state(flags=F_READY)
            sn = f.snapshot()
            if sn and sn["phase"] == RACE and "HERO" in sn["players"] and sn["players"]["HERO"]["flags"] & F_FINISHED:
                hero_done = True
                break
        check(hero_done, "the game finished its race")
        pos, hp, seen, m, took, max_hp_lost_in_prep = None, 100.0, 0, None, False, 0
        t0 = time.time()
        while time.time() - t0 < 88 and getattr(f, "phase_seen", RACE) != RESULTS:
            f.state(flags=F_FINISHED, lap=2, progress=3400)
            m = f.showdown() or m
            if not m:
                continue
            hero = m["feet"][0]
            if pos is None and hero["flags"] & FF_ON:   # the game is out of its car, in the ring: stand a few metres off
                pos = (hero["x"] + 7.0, hero["y"], hero["z"])
            if m["stage"] == SD_LIVE:
                hp -= ((hero["d1"] - seen) & 0xFFFF) / 10.0
            seen = hero["d1"]
            took = took or any(d["owner"] == 0 for d in m["drops"])
            if pos:
                f.foot(pos=pos, hp=hp, alive=hp > 0, killed_by=0 if hp <= 0 else NOBODY)
            else:   # still "driving": reported as in the ring, but not out of the car yet
                f.sock.sendto(f.hdr(C_FOOT) + struct.pack("<I", 1) + struct.pack(FOOTSTATE, 0, 0, 0, 0, 0, 0, 100, 0, 0, FF_ALIVE | FF_ARRIVED, 0, NOBODY, NOBODY, 0, 0, 0, 0, 0, 0, 0), f.addr)
        check(m is not None and m["fighter"][0] != f.id and m["fighter"][1] == f.id and m["fighter"][2] == NOBODY, "two finishers: a two-way showdown, the game in slot 0", m and m["fighter"])
        check(len(m["drops"]) == 11 and took, "the game laid out eleven pick-ups and took at least one through the server", (len(m["drops"]), took))
        check(hp <= 0 and m["winner"] == 0 and m["place"][:2] == [1, 2] and m["kills"][0] == 1, "its shots brought the other fighter down and the server gave it the win", (hp, m["winner"], m["place"], m["kills"]))
        d = json.loads(game.communicate(timeout=60)[0].strip().splitlines()[-1])
    fs = fighters(d)
    check(d["netShdCount"] == 1 and d["netSlot"] == 0 and d["netError"] == "", "the game went through one showdown, connected throughout", (d["netShdCount"], d["netSlot"], d["netError"]))
    check(fs[0]["alive"] == 1 and fs[1]["on"] == 1 and fs[1]["alive"] == 0 and fs[0]["kills"] == 1 and d["footHits"] > 0, "on its side: the other fighter is down, and the kill is its own", (d["fighters"], d["footHits"]))
    check(d["state"] == 5 and d["netPhase"] == RESULTS, "and it ends on the online results", (d["state"], d["netPhase"]))


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
    check(d["bright"] == 85, "brightness starts at 85%", d["bright"])
    d = run(*_presses("F1", "TAB", "UP", "UP", "UP", "UP", "LEFT", "LEFT", "ESCAPE"))
    check(d["bright"] == 75, "brightness: two steps down is 75%", d["bright"])
    d = run()
    check(d["bright"] == 75, "and it is remembered", d["bright"])
    check(d["fx"] == [100, 100, 100], "water rings, splash mist and grass start at 100%", d["fx"])
    d = run(*_presses("F1", "TAB", "UP", "LEFT", "LEFT", "UP", "RIGHT", "UP", "LEFT", "LEFT", "LEFT", "LEFT", "ESCAPE"))
    check(d["fx"] == [0, 125, 50], "grass two steps down, mist one up, rings off", d["fx"])
    d = run()
    check(d["fx"] == [0, 125, 50], "and they are remembered", d["fx"])
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
    check(len(binds) == 12 and binds[2] == (27, 26), "accelerate is now X, with W still as the second key", binds[2])
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
    check(d["binds"] == "80,4;79,7;82,26;81,22;44,0;225,229;9,8;21,42;6,0;10,0;20,0;21,0", "F5 restores the defaults", d["binds"])


# ----------------------------------------------------------------------------- touch screen
@test
def touch_buttons_drive_the_car_and_the_menus():
    d = sim("--state", 3, "--mode", 1, "--track", 1, seconds=1)
    check(d["touch"] == 0 and d["touchBtns"] == "", "no on-screen buttons unless the screen is touched", d["touchBtns"])
    d = sim("--touch", "--state", 3, "--mode", 1, "--track", 1, "--touchhold", "GAS", seconds=5)
    check({"<", ">", "GAS", "BRAKE", "NITRO", "DRIFT", "II"} <= set(d["touchBtns"].split(",")) and "ROCKET" not in d["touchBtns"] and "WEAPON" not in d["touchBtns"], "the race screen has steering, gas, brake, nitro and drift buttons; ROCKET only with rockets, no WEAPON in the car", d["touchBtns"])
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
    check(d["onFoot"] == 1 and set(d["touchBtns"].split(",")) == {"FIRE", "ROLL", "RUN", "GUN", "RELOAD", "CAR", "II", "JUMP", "DUCK"}, "OUT gets out of the car, and the buttons change to the fighter's", d["touchBtns"])
    check(d["footShots"] >= 3, "a thumb held on FIRE shoots", d["footShots"])
    out = sim("--touch", "--state", 3, "--mode", 1, "--track", 0, "--tap", "0.2:OUT", seconds=1)
    d = sim("--touch", "--state", 3, "--mode", 1, "--track", 0, "--tap", "0.2:OUT", "--stick", "0,1", seconds=3)
    check(math.hypot(d["walkX"] - out["walkX"], d["walkZ"] - out["walkZ"]) > 10, "the thumb-stick pushed forward runs", (d["walkX"], d["walkZ"]))
    d = sim("--touch", "--state", 3, "--mode", 1, "--track", 0, "--tap", "0.2:OUT", "--tap", "0.8:CAR", seconds=1.2)
    check(d["onFoot"] == 0 and "GAS" in d["touchBtns"].split(","), "CAR gets back in", (d["onFoot"], d["touchBtns"]))
    d = sim("--touch", *SHOW, seconds=prep_at() + 2)
    check(d["onFoot"] == 1 and set(d["touchBtns"].split(",")) == {"FIRE", "ROLL", "RUN", "GUN", "RELOAD", "II", "SKIP", "JUMP", "DUCK"}, "in the showdown there is no car to go back to", d["touchBtns"])


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
        d = sim("--track", track, "--car", 1, "--weather", w, "--state", 3, "--auto", "--nobotguns", seconds=330)
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
# Test hooks: --faceai (step out facing the nearest racer), --gun N (holding that gun), --keys N (as if N keys were found),
# --sdnow / --sdplace N (as if you had just finished the race in that place), --sdsite U (the ring goes that far round the
# lap), --footai (a bot plays your fighter), --auto (the autopilot drives). Fighters report as on|alive|hp|gun|kills|place.
FOOT = ("--track", 0, "--state", 3, "--mode", 1, "--press", "0.05:G")
# the showdown: you have just won the race, the autopilot drives you to the ring (30% of the way round the lap), all guns unlocked
SHOW = ("--track", 0, "--state", 3, "--mode", 2, "--sdnow", "--auto", "--keys", 5, "--sdsite", 0.3)


def fighters(d):
    return [dict(zip(("on", "alive", "hp", "gun", "kills", "place"), (int(v) for v in f.split("|")))) for f in d["fighters"].split(";")]


def prep_at():
    """When the showdown's countdown starts in the SHOW scenario (seconds into the run): all three cars are in the ring by then."""
    if "t" not in _prep:
        d = sim(*SHOW, seconds=60)
        check(d["shdPhase"] >= 2 and 5 < d["prepAt"] < 58, "everyone reaches the ring and the countdown starts", (d["shdPhase"], d["prepAt"]))
        _prep["t"] = d["prepAt"]
    return _prep["t"]


_prep = {}


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
    d = sim(*FOOT, "--hold", "W", "--hold", "V", seconds=0.4)
    check(d["rolling"] == 1 and d["walkSpeed"] > 7, "V is a dodge roll, a burst of speed (2.8: Space jumps)", (d["rolling"], d["walkSpeed"]))
    between(d["stamina"], 0.7, 0.85, "a roll costs a quarter of the stamina")
    d = sim(*FOOT, "--hold", "W", "--hold", "V", seconds=1.5)
    check(d["rolling"] == 0, "one roll per press", d["rolling"])


# 2.8 moving: jump, crouch, slide, mantle, and the body that shows it
@test
def foot_jump_is_a_snappy_arc():
    d = sim(*FOOT, "--holdat", "1.0:Space", seconds=2.5)
    check(d["fJumps"] == 1 and d["fMantles"] == 0, "Space jumps (nothing in front to climb)", (d["fJumps"], d["fMantles"]))
    between(d["fPeak"], 0.85, 1.15, "jump height (m)")
    between(d["fAirT"], 0.45, 0.65, "time in the air (s)")
    check(d["fLands"] == 1 and d["fAir"] == 0 and abs(d["walkY"] - d["fGround"]) < 0.05, "and lands back on the ground", (d["fLands"], d["walkY"], d["fGround"]))
    held = sim(*FOOT, "--hold", "Space", seconds=2.5)
    check(held["fJumps"] == 1, "one jump per press", held["fJumps"])
    still = sim(*FOOT, "--holdat", "1.0:Space", seconds=1.05)
    air = sim(*FOOT, "--holdat", "1.0:Space", "--holdat", "1.05:A:0.5", seconds=1.55)
    side = math.hypot(air["walkX"] - still["walkX"], air["walkZ"] - still["walkZ"])
    between(side, 0.2, 2.0, "a little air control: steering mid-jump moves you, but not like running (m)")
    run = sim(*FOOT, "--hold", "W", "--holdat", "1.5:Space", seconds=1.75)
    check(run["fAir"] == 1 and run["walkSpeed"] > 4.5, "a running jump keeps its speed", (run["fAir"], run["walkSpeed"]))


@test
def foot_crouch_is_low_slow_and_steady():
    stand = sim(*FOOT, seconds=1.5)
    d = sim(*FOOT, "--hold", "C", seconds=1.5)
    check(d["fCrouchK"] > 0.95, "C crouches", d["fCrouchK"])
    check(d["fHeadY"] < stand["fHeadY"] - 0.35, "the head (the headshot) comes down with it", (stand["fHeadY"], d["fHeadY"]))
    d2 = sim(*FOOT, "--hold", "Left Ctrl", seconds=1.5)
    check(d2["fCrouchK"] > 0.95, "so does Ctrl", d2["fCrouchK"])
    walk = sim(*FOOT, "--hold", "C", "--hold", "W", seconds=2)
    between(walk["walkSpeed"], 2.2, 3.0, "crouched, you move slower (m/s)")
    up = sim(*FOOT, "--holdat", "0.5:C:1", seconds=2)
    check(up["fCrouchK"] < 0.05 and up["fHeadY"] > 1.7, "letting go stands you back up", (up["fCrouchK"], up["fHeadY"]))
    a = sim("--track", 0, "--state", 3, "--mode", 0, "--press", "0.05:G", "--hold", "F", "--faceai", "--gun", 3, seconds=0.7)
    b = sim("--track", 0, "--state", 3, "--mode", 0, "--press", "0.05:G", "--hold", "F", "--hold", "C", "--faceai", "--gun", 3, seconds=0.7)
    check(b["meBloom"] < a["meBloom"], "crouched, a burst opens the crosshair less", (a["meBloom"], b["meBloom"]))


@test
def foot_slide_from_a_sprint():
    d = sim(*FOOT, "--hold", "W", "--hold", "Left Shift", "--holdat", "2.0:C:2", seconds=3.5)
    check(d["fSlides"] == 1, "crouch while sprinting: a slide", d["fSlides"])
    between(d["fSlideT"], 0.7, 0.9, "it lasts about 0.8 s")
    between(d["fSlideDist"], 4.5, 6.5, "and carries you on (m)")
    check(d["fMinHead"] < 1.1, "low: under a standing head shot", d["fMinHead"])
    check(d["fCrouchK"] > 0.9 and d["walkSpeed"] < 3, "it ends in a crouch", (d["fCrouchK"], d["walkSpeed"]))
    walk = sim(*FOOT, "--hold", "W", "--holdat", "2.0:C:2", seconds=3.5)
    check(walk["fSlides"] == 0, "crouching from a run is just a crouch", walk["fSlides"])
    f = sim(*FOOT, "--hold", "W", "--hold", "Left Shift", "--holdat", "2.0:C:2", "--holdat", "2.15:F:0.5", seconds=2.7)
    check(f["fSlides"] == 1 and f["footShots"] >= 1, "you can shoot from a slide", (f["fSlides"], f["footShots"]))


@test
def foot_mantle_onto_a_car():
    # you get out on the car's left; D runs into its side, Space climbs onto it
    d = sim(*FOOT, "--holdat", "0.5:D:1.0", "--holdat", "1.0:Space", seconds=2.5)
    check(d["fMantles"] == 1 and d["fJumps"] == 0, "jumping at a car climbs it", (d["fMantles"], d["fJumps"]))
    between(d["fMantleRise"], 0.9, 1.45, "up onto its roof or bonnet (m)")
    between(d["walkY"] - d["y"], 0.9, 1.45, "and you stand up there")
    air = sim(*FOOT, "--holdat", "0.3:Space", "--holdat", "0.3:D:0.6", seconds=2.5)
    check(air["fMantles"] + air["fJumps"] >= 1 and air["walkY"] - air["y"] > 0.9, "a jump towards it from a step away gets up on it too", (air["fMantles"], air["fJumps"], air["walkY"] - air["y"]))


@test
def character_model_poses_are_sound():
    d = sim(*FOOT, seconds=0.5)
    check(d["poseParts"] == 6 and d["poseTris"] < 4000, "the body: six rigid parts, well inside the 360's budget", (d["poseParts"], d["poseTris"]))
    check(d["poseChecked"] >= 1000 and d["poseBad"] == 0, "every pose (each gun; stand, run, sprint, crouch, air, land, slide, mantle, roll, down, cheer, ADS) has sound numbers", (d["poseChecked"], d["poseBad"]))
    check(d["poseHandErr"] < 0.05, "the hands are on the gun in every two-handed pose (m)", d["poseHandErr"])


@test
def guns_lock_on_and_hurt_the_racers():
    base = sim("--track", 0, "--state", 3, "--mode", 0, "--press", "0.05:G", "--faceai", seconds=1.5)
    check(base["aiMinHp"] == 100.0 and base["footShots"] == 0 and base["lock"] == -1, "nothing is locked or hit before the trigger is pulled", (base["aiMinHp"], base["lock"]))
    for gun, name, shots in ((1, "pistol", 3), (2, "rifle", 2), (3, "SMG", 8), (4, "shotgun", 1), (5, "launcher", 1)):
        d = sim("--track", 0, "--state", 3, "--mode", 0, "--press", "0.05:G", "--hold", "F", "--faceai", "--gun", gun, seconds=1.5)
        check(d["footShots"] >= shots, "%s: holding fire keeps shooting" % name, d["footShots"])
        check(d["aiMinHp"] < 100 or d["aiDead"] > 0, "%s: the racer it locked on to is hurt" % name, (d["aiMinHp"], d["aiDead"]))
    d = sim("--track", 0, "--state", 3, "--mode", 0, "--press", "0.05:G", "--hold", "F", "--faceai", "--gun", 4, seconds=4)
    check(d["aiDead"] >= 1 and d["wrecks"] >= 1, "enough of it wrecks the car", (d["aiDead"], d["wrecks"]))


@test
def reloads_take_their_time_and_refill_the_magazine():
    """2.7: each gun has a magazine. Run it dry and it clicks and reloads by itself, in the gun's own time (pistol 0.85 s,
    SMG 1.3 s), then fires on; R reloads a part-used one early; a shotgun loads shell by shell, and a shot cuts that short."""
    for gun, t, mag in ((1, 0.85, 12), (3, 1.30, 30)):
        d = sim("--track", 0, "--state", 3, "--mode", 0, "--press", "0.05:G", "--hold", "F", "--faceai", "--gun", gun, seconds=9)
        check(d["dryClicks"] >= 1 and d["reloadsDone"] >= 1, "gun %d: emptied, it clicked and reloaded by itself" % gun, (d["dryClicks"], d["reloadsDone"]))
        between(d["reloadTook"], t, t + 0.05, "gun %d: the reload took its time" % gun)
        check(d["footShots"] > mag, "gun %d: and it fired on after it" % gun, d["footShots"])
    aim = ("--track", 0, "--state", 3, "--onfoot", "--ads", "--aimat", -400, 30, -100, "--gun", 2)
    d = sim(*aim, "--press", "1.5:R", seconds=3.4)
    check(d["reloadsDone"] == 1 and d["dryClicks"] == 0, "R reloads a rifle before it is empty", (d["reloadsDone"], d["dryClicks"]))
    between(d["reloadTook"], 1.45, 1.5, "a rifle's reload")
    d = sim(*aim, seconds=1.0)
    check(d["meMag"] < 8, "firing takes rounds out of the magazine", d["meMag"])
    full = sim(*aim, "--press", "1.5:R", seconds=3.4)
    check(full["meMag"] + full["meReserve"] == d["meMag"] + d["meReserve"] - (full["footShots"] - d["footShots"]), "a reload moves rounds from what you carry into the gun, none lost", (full["meMag"], full["meReserve"], d["meMag"], d["meReserve"]))
    d = sim("--track", 0, "--state", 3, "--mode", 0, "--press", "0.05:G", "--hold", "F", "--faceai", "--gun", 4, seconds=9)
    check(d["dryClicks"] >= 1 and d["footShots"] > 6, "the shotgun: emptied, a shell goes in and the trigger fires it", (d["dryClicks"], d["footShots"]))


@test
def quick_swaps_and_the_last_weapon():
    """2.7: 1-5 pick a gun, Q goes back to the last; a swap takes a quarter of a second, and drops a reload under way."""
    d = sim("--track", 0, "--state", 3, "--onfoot", "--keys", 6, "--press", "1:3", seconds=2)
    check(d["gun"] == 3 and d["swapsDone"] == 1, "3 picks the SMG", (d["gun"], d["swapsDone"]))
    between(d["swapTook"], 0.25, 0.27, "the swap")
    first = sim("--track", 0, "--state", 3, "--onfoot", "--keys", 6, seconds=0.5)["gun"]
    d = sim("--track", 0, "--state", 3, "--onfoot", "--keys", 6, "--press", "1:3", "--press", "1.5:Q", seconds=2)
    check(d["gun"] == first and first != 3 and d["swapsDone"] == 2, "Q: straight back to the gun before", (first, d["gun"], d["swapsDone"]))
    d = sim("--track", 0, "--state", 3, "--onfoot", "--keys", 6, "--press", "1:1", seconds=2)
    check(d["gun"] == 1, "1 is the pistol", d["gun"])
    d = sim("--track", 0, "--state", 3, "--onfoot", "--ads", "--aimat", -400, 30, -100, "--gun", 3, "--keys", 6, "--press", "1.2:R", "--press", "1.5:1", seconds=3)
    check(d["meReloads"] == 1 and d["reloadsDone"] == 0 and d["gun"] == 1, "a swap cancels the reload", (d["meReloads"], d["reloadsDone"], d["gun"]))


@test
def rarity_tiers_on_the_guns_lying_about():
    """2.7: guns on the ground come common to legendary (grey, green, blue, purple, gold), a little better each tier.
    The roll is a hash of where they lie: the same every run for a seed, and the race is untouched."""
    spec = [r.split("|") for r in sim("--track", 0, "--state", 3, seconds=0.2)["rarSpec"].split(";")]
    check([r[0] for r in spec] == ["COMMON", "UNCOMMON", "RARE", "EPIC", "LEGENDARY"], "five tiers", [r[0] for r in spec])
    cols = [tuple(float(v) for v in r[1].split(",")) for r in spec]
    grey, green, blue, purple, gold = cols
    check(abs(grey[0] - grey[1]) < 0.05 and abs(grey[1] - grey[2]) < 0.05, "common is grey", grey)
    check(green[1] > green[0] + 0.3 and green[1] > green[2] + 0.3, "uncommon is green", green)
    check(blue[2] > blue[0] + 0.3 and blue[2] > blue[1], "rare is blue", blue)
    check(purple[0] > purple[1] + 0.2 and purple[2] > purple[1] + 0.2, "epic is purple", purple)
    check(gold[0] > 0.9 and gold[1] > 0.6 and gold[2] < 0.3, "legendary is gold", gold)
    dmg, spread, rel = [float(r[2]) for r in spec], [float(r[3]) for r in spec], [float(r[4]) for r in spec]
    check(dmg[0] == 1 and all(dmg[i] < dmg[i + 1] for i in range(4)) and dmg[4] <= 1.3, "each tier hits a little harder", dmg)
    check(all(spread[i] > spread[i + 1] for i in range(4)) and all(rel[i] > rel[i + 1] for i in range(4)), "and is steadier and quicker to reload", (spread, rel))
    a = sim(*SHOW, "--seed", 7, seconds=3); b = sim(*SHOW, "--seed", 7, seconds=3); c = sim(*SHOW, "--seed", 8, seconds=3)
    tiers = lambda d: [tuple(int(v) for v in x.split(":")) for x in d["dropRar"].split(";")]
    check(a["dropRar"] == b["dropRar"] and len(tiers(a)) == 11, "the same seed, the same tiers", (a["dropRar"], b["dropRar"]))
    check(all(r == 0 for k, r in tiers(a) if k >= 6), "medkits and armour have no tier", a["dropRar"])
    check(a["x"] == b["x"] and a["z"] == b["z"], "and the same drive", (a["x"], b["x"]))
    allt = set()
    for t in range(4):
        d = sim("--track", t, "--state", 3, seconds=0.2)
        guns = [(k, r) for k, r in tiers(d) if k < 6]
        check(len(guns) >= 3, "track %d: some guns lie about the town" % t, d["dropRar"])
        allt |= {r for k, r in guns}
    allt |= {r for k, r in tiers(a) + tiers(c) if k < 6}
    check(len(allt) >= 4, "the tiers turn up", sorted(allt))


@test
def bots_reload_too():
    """2.7: the computer's fighters use the same magazines: they reload when they run dry and top up when nobody is in sight."""
    d = sim(*SHOW, "--sdplace", 7, seconds=150)
    check(d["botReloads"] >= 2, "the bots reloaded", d["botReloads"])
    check(d["shdPhase"] >= 3, "and still fought it out", d["shdPhase"])


@test
def guns_need_keys():
    for keys, gun, name in ((0, 0, "fists"), (1, 1, "the pistol"), (2, 2, "the rifle"), (4, 4, "the shotgun"), (5, 5, "the launcher")):
        d = sim(*FOOT, "--keys", keys, seconds=1)
        check(d["gun"] == gun, "%d keys found: the best you carry is %s" % (keys, name), d["gun"])
    d = sim(*SHOW[:-4], "--keys", 1, "--sdsite", 0.3, "--footai", seconds=prep_at() + 19)
    check(d["shdPhase"] == 2 and all(f["gun"] <= 1 for f in fighters(d)), "one key: nobody can pick a gun up off the ground", d["fighters"])
    d = sim(*SHOW, "--footai", seconds=prep_at() + 19)
    check(max(f["gun"] for f in fighters(d)) >= 3, "five keys: the guns on the ground can be taken", d["fighters"])


@test
def showdown_is_for_the_top_three():
    d = sim(*SHOW, "--sdplace", 6, seconds=3)
    check(d["state"] == 3 and d["shdPhase"] == 1 and d["shdSpectate"] == 1 and d["onFoot"] == 0, "sixth place: no place in it, but you watch", (d["state"], d["shdPhase"], d["shdSpectate"]))
    for place, armour in ((1, 50), (2, 25), (3, 0), (5, 0)):
        d = sim(*SHOW, "--sdplace", place, seconds=3)
        check(d["shdPhase"] == 1 and d["onFoot"] == 0 and d["armour"] == armour, "place %d: off to the ring, with %d armour waiting" % (place, armour), (d["shdPhase"], d["armour"]))


@test
def showdown_without_you_is_fought_by_the_five_who_beat_you():
    d = sim(*SHOW, "--sdplace", 7, seconds=150)
    feet = [f.split("|") for f in d["fighters"].split(";")]
    check(d["shdSpectate"] == 1 and len(feet) == 5 and all(f[0] == "1" for f in feet), "seventh: five computer-driven fighters, and you watching", d["fighters"])
    check(d["shdPhase"] >= 3 and sum(f[1] == "1" for f in feet) < 5, "they fight it out among themselves", (d["shdPhase"], d["fighters"]))


@test
def showdown_drive_to_the_ring_then_countdown_then_fight():
    t = prep_at()
    d = sim(*SHOW, seconds=3)
    check(d["shdPhase"] == 1 and d["onFoot"] == 0 and d["drops"] == 11 and d["aiDead"] == 5, "over the line: still driving, the pick-ups are down, the five who missed the cut are gone", (d["shdPhase"], d["onFoot"], d["drops"], d["aiDead"]))
    start = (d["x"], d["z"])
    d = sim(*SHOW, "--hold", "F", "--faceai", seconds=t + 19)
    fs = fighters(d)
    check(d["shdPhase"] == 2 and all(f["on"] for f in fs) and math.hypot(d["x"] - start[0], d["z"] - start[1]) > 200, "all five have driven to the ring and are out of their cars", (d["shdPhase"], d["fighters"]))
    check(d["footShots"] == 0 and all(f["hp"] == 100 for f in fs), "the countdown: nobody can shoot yet, so nobody is hurt", (d["footShots"], d["fighters"]))
    check(sum(f["gun"] > 1 for f in fs[1:]) >= 1 and d["dropsBack"] >= 1, "the others have fetched guns, and what was taken has started to come back", (d["fighters"], d["dropsBack"]))
    between(d["shdRing"], 51.9, 52.1, "the ring waits at full size")
    d = sim(*SHOW, "--hold", "F", seconds=t + 20 + 6)
    check(d["shdPhase"] >= 3 and d["footShots"] > 0, "twenty seconds after the last car is in, the guns are live", (d["shdPhase"], d["footShots"]))
    other = sim("--track", 0, "--state", 3, "--mode", 2, "--sdnow", "--auto", "--keys", 5, "--sdsite", 0.6, seconds=60)
    check(other["shdPhase"] >= 2 and math.hypot(other["x"] - d["x"], other["z"] - d["z"]) > 150, "another race, another place: the ring is somewhere else", (other["x"], other["z"], d["x"], d["z"]))


@test
def showdown_ring_burns_and_closes():
    t = prep_at()
    d = sim(*SHOW, "--hold", "S", seconds=t + 19)
    check(d["walkHp"] == 100, "running off during the countdown costs nothing", d["walkHp"])
    d = sim(*SHOW, "--hold", "S", "--press", "%.1f:R" % (t + 5), "--press", "%.1f:G" % (t + 6), seconds=t + 20 + 40)
    fs = fighters(d)
    check(fs[0]["alive"] == 0 and d["footShots"] == 0, "outside the ring you burn", d["fighters"])
    left = sum(f["alive"] for f in fs)
    check(d["shdRing"] < 50, "the ring closes with time", (d["shdRing"], left))
    check(d["onFoot"] == 1 and d["resets"] == 0, "and there is no resetting or driving out of it", (d["onFoot"], d["resets"]))


@test
def blood_and_the_grenade_launcher():
    t = prep_at()
    d = sim(*SHOW, "--hold", "F", "--faceai", "--gun", 3, seconds=t + 20 + 5)
    check(d["footHits"] > 0 and d["splats"] > 0, "bullets draw blood: it is left on the ground", (d["footHits"], d["splats"]))
    d = sim(*SHOW, "--footai", "--gun", 5, seconds=t + 20 + 12)
    fs = fighters(d)
    check(d["gibs"] >= 1 and sum(1 for f in fs if not f["alive"]) >= d["gibs"], "a grenade landing beside someone kills them outright and leaves pieces", (d["gibs"], d["fighters"]))


@test
def the_undersea_tunnels():
    for track in (4, 5):
        d = sim("--track", track, "--state", 3, "--mode", 1, seconds=0.5)
        ta, tb, ga, gb = d["tunnel"]
        check(0 < ta < ga < gb < tb, "track %d: a tunnel with a glass-roofed stretch in the middle" % track, d["tunnel"])
        check(d["tubeDirt"] == 0, "track %d: no ground pokes into the tube anywhere" % track, d["tubeDirt"])
        r = sim("--track", track, "--state", 3, "--auto", "--nobotguns", seconds=260)
        check(r["finished"] == 1 and r["sinks"] == 0 and r["wrecks"] == 0, "track %d: three laps through the tunnel: it is dry, nobody sinks, nothing to hit" % track, (r["finished"], r["sinks"], r["wrecks"]))
        check(r["aiBranch"] >= 10, "track %d: the bots split between the tunnel and the way over the top" % track, r["aiBranch"])
    for t in range(4):
        d = sim("--track", t, "--state", 3, "--mode", 1, seconds=0.5)
        check(d["tunnel"][0] == -1, "track %d has no tunnel" % t, d["tunnel"])


@test
def twin_isles_the_endurance_track():
    """Track 7: two islands, two undersea tunnels, three shortcuts (one a causeway between the islands), four Wardens,
    and twice the laps picked."""
    d = sim("--track", 6, "--state", 3, "--mode", 1, seconds=0.5)
    check(d["tunnels"] == 2, "two undersea tunnels", d["tunnels"])
    ta, tb, ga, gb = d["tunnel"]
    check(0 < ta < ga < gb < tb, "the first has a glass-roofed stretch", d["tunnel"])
    check(d["tubeDirt"] == 0, "no ground pokes into either tube", d["tubeDirt"])
    check(d["shortcuts"].count(";") == 2, "three shortcuts", d["shortcuts"])
    check(d["mechs"] == 4, "four Wardens", d["mechs"])
    r = sim("--track", 6, "--state", 3, "--auto", "--nobotguns", seconds=520)
    check(r["raceLaps"] == 6, "an endurance race: three laps picked, six raced", r["raceLaps"])
    check(r["finished"] == 1 and r["sinks"] == 0 and r["wrecks"] == 0, "six laps: it finishes, nobody sinks, nothing hit", (r["finished"], r["sinks"], r["wrecks"]))
    check(r["aiBranch"] >= 10, "the bots use the shortcuts and the causeway", r["aiBranch"])
    r = sim("--track", 6, "--state", 3, "--auto", "--nobotguns", "--weather", 3, "--laps", 1, seconds=200)
    check(r["finished"] == 1, "two laps in a storm finish in time (stuck behind a UFO wreck, the car comes back past it)", (r["finished"], r["timeLeft"]))


@test
def mini_wardens_in_free_roam():
    """Free roam: mini Wardens land near you (now and then, one to three at a time), jet at you, fight you hand to hand
    on foot and shoot at your car. Every weapon beats them (2.6.3); they leave flowers that add up to levels, each unlocking an
    attribute. Run one down and the flowers towards your next level are lost, and a Warden fires a rocket at you."""
    d = sim("--track", 1, "--state", 3, "--mode", 1, "--minisnow", seconds=40)
    check(d["miniSpawned"] == 1 and d["minis"] == 1, "one lands near you", (d["miniSpawned"], d["minis"]))
    check(d["carHp"] < 90 and d["miniJets"] >= 1, "it jets in and shoots your parked car", (d["carHp"], d["miniJets"]))
    d = sim("--track", 1, "--state", 3, "--mode", 1, "--minisnow", "--minishold", "--outcar", "--footai", "--gun", 2, seconds=20)
    check(d["miniKilled"] == 1 and d["minis"] == 0, "rifle rounds take it down (2.6.3: every weapon does)", (d["miniKilled"], d["minis"]))
    d = sim("--track", 1, "--state", 3, "--mode", 1, "--minisnow", "--outcar", "--footai", seconds=45)
    check(d["miniKilled"] == 1 and d["roamFlowers"] >= 2 and d["footHp"] > 0, "your fists beat it, and you pick up its flowers", (d["miniKilled"], d["roamFlowers"], d["footHp"]))
    d = sim("--track", 1, "--state", 3, "--mode", 1, "--minisnow", "--minishold", "--auto", "--flowers", 30, seconds=12)
    check(d["miniRams"] == 1 and d["miniKilled"] == 1, "driving into it takes it down", (d["miniRams"], d["miniKilled"]))
    check(d["roamFlowers"] == 25 and d["miniRockets"] == 1, "but the flowers towards the next level are gone, and a Warden fires on you", (d["roamFlowers"], d["miniRockets"]))
    d = sim("--track", 1, "--state", 3, "--mode", 1, "--flowers", 300, seconds=1)
    check(d["level"] == 8 and d["footMaxHp"] == 125, "300 flowers: every level, and 125 health on foot", (d["level"], d["footMaxHp"]))
    d = sim("--track", 2, "--state", 3, "--mode", 1, "--auto", seconds=360)
    check(9 <= d["miniSpawned"] <= 24 and d["miniSpawned"] % 3 == 0, "they land on their own, in threes, every minute or so", d["miniSpawned"])
    d = sim("--track", 1, "--state", 3, "--auto", "--minisnow", seconds=60)
    check(d["miniSpawned"] == 0, "never in a race", d["miniSpawned"])

@test
def the_editor_places_jumps_and_keys():
    """The editor's JUMPS and KEYS tools: a jump by the road and a key in the air past it, a key on the ground. Saved
    in the file; the key past the jump is reached off it; a race on the track still finishes (the bots go round)."""
    tmp = tempfile.mkdtemp()
    try:
        script = "tool 4;down 50 -345;up;key plus;key plus;wait;tool 5;down 88 -340;up;" + "key plus;" * 4 + "wait;down -390 -150;up;wait"
        d = sim("--state", 2, "--tracksdir", tmp, "--ednew", "--edscript", script, seconds=1)
        check(d["jumps"] == 1, "a jump on the track", d["jumps"])
        f = os.path.join(tmp, "my-track.fcft"); text = open(f).read()
        check(text.count("jump ") == 1 and text.count("key ") == 2 and "height 5" in text, "the file has the jump (5 m up) and both keys", text[-160:])
        r = sim("--trackfile", f, "--state", 3, "--mode", 1, "--keyrun", 0, seconds=30)
        check(r["keysGot"] & 1, "the key past the jump is reached off it", r["keysGot"])
        r = sim("--trackfile", f, "--state", 3, "--mode", 1, "--keyrun", 1, seconds=40)
        check(r["keysGot"] & 2, "the key by the road is reached too", r["keysGot"])
        r = sim("--trackfile", f, "--state", 3, "--auto", "--nobotguns", seconds=260)
        check(r["finished"] == 1 and r["wrecks"] == 0, "a race on it finishes, nothing hit", (r["finished"], r["wrecks"]))
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


@test
def track_files_round_trip():
    """--exporttracks writes the six built-in tracks as .fcft files; loading one back builds the same road."""
    tmp = tempfile.mkdtemp()
    try:
        subprocess.run([BIN, "--exporttracks", tmp], check=True, timeout=30)
        for t in range(NTRACKS):
            a = sim("--track", t, "--state", 3, "--mode", 1, seconds=0.3)
            b = sim("--trackfile", os.path.join(tmp, "builtin-%d.fcft" % (t + 1)), "--state", 3, "--mode", 1, seconds=0.3)
            check(b["track"] == NTRACKS, "the file loads as a custom track, after the built-in ones", b["track"])
            check(a["trackLen"] == b["trackLen"] and a["tunnel"] == b["tunnel"] and a["shortcuts"] == b["shortcuts"], "track %d: same lap, tunnel and shortcuts from its file" % t, (a["trackLen"], b["trackLen"]))
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


@test
def track_files_cannot_crash_the_game():
    """Hand-made files: nonsense numbers, too few points, a tunnel across the start line. Each is clamped or refused."""
    tmp = tempfile.mkdtemp()
    try:
        bad = os.path.join(tmp, "bad.fcft")
        open(bad, "w").write("fcft 1\nroad 0 0 1e30 nan 5 5 -9999 3 100 100 100 105 -300 -300 300 -300 300 300 -300 300\nhill nan nan nan nan\ntunnel 0.9 0.1\nbranch 0.5 0.5\nbranch 0.1 0.9\nsky top x y z\nrubbish 1 2 3\n")
        d = sim("--trackfile", bad, "--state", 3, "--auto", seconds=3)
        check(d["trackLen"] > 800, "a file full of nonsense still builds a track", d["trackLen"])
        few = os.path.join(tmp, "few.fcft")
        open(few, "w").write("fcft 1\nroad 0 0 100 100\n")
        p = subprocess.run([BIN, "--sim", "--seconds", "1", "--trackfile", few], capture_output=True, text=True, timeout=30)
        check(p.returncode != 0, "a file with no usable road is refused", p.returncode)
        import random
        for k in range(8):
            r = random.Random(k); n = r.randint(4, 14); R = r.uniform(240, 500)
            pts = [(R * r.uniform(0.3, 1.25) * math.cos(2 * math.pi * i / n), R * r.uniform(0.3, 1.25) * math.sin(2 * math.pi * i / n)) for i in range(n)]
            L = ["fcft 1", "name FUZZ %d" % k, "seed %d" % r.randint(0, 9000), "road " + " ".join("%.0f %.0f" % q for q in pts)]
            L += ["hill %.0f %.0f %.0f %.0f" % (r.uniform(-600, 600), r.uniform(-600, 600), r.uniform(-70, 90), r.uniform(20, 260)) for _ in range(r.randint(0, 8))]
            a = r.uniform(0, 0.9); L.append("tunnel %.3f %.3f depth %.0f" % (a, a + r.uniform(0.02, 0.6), r.uniform(5, 40)))
            a = r.uniform(0, 0.9); L.append("branch %.3f %.3f" % (a, a + r.uniform(0, 0.5)))
            f = os.path.join(tmp, "fuzz%d.fcft" % k); open(f, "w").write("\n".join(L) + "\n")
            d = sim("--trackfile", f, "--state", 3, "--auto", seconds=3)
            check(d["trackLen"] > 400, "random track %d builds and runs" % k, d["trackLen"])
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


@test
def the_editor_makes_a_track_you_can_race():
    """The editor, driven by its test script: a new track, a point moved, a point added, a lagoon dug, a tunnel under
    it, a shortcut. It saves the track as a file; the file loads; a race on it finishes, through the tunnel."""
    tmp = tempfile.mkdtemp()
    try:
        script = ("down -380 -60;move -420 -20;up;down 150 -345;move 160 -400;up;tool 1;down 420 -40;up;" + "key minus;" * 24 + "key rb;key rb;key rb;wait;"
                  "tool 2;down 330 -230;up;down 300 240;up;wait;tool 3;down -150 320;up;down -330 -265;up;down 0 362;up;down -330 110;up;wait")
        d = sim("--state", 2, "--tracksdir", tmp, "--ednew", "--edscript", script, seconds=1)
        check(d["track"] == NTRACKS, "the new track is added after the built-in ones", d["track"])
        ta, tb, ga, gb = d["tunnel"]
        check(0 < ta < ga < gb < tb, "its tunnel runs under the lagoon that was dug, glass in the middle", d["tunnel"])
        check(d["tubeDirt"] == 0, "no ground pokes into the tube", d["tubeDirt"])
        check(d["shortcuts"].count(";") == 0 and d["branchLen"] > 100, "one shortcut (the one across the start line was refused)", d["shortcuts"])
        f = os.path.join(tmp, "my-track.fcft")
        check(os.path.exists(f), "the track was saved as my-track.fcft")
        text = open(f).read()
        check("road" in text and "tunnel" in text and "branch" in text and text.count("hill") == 4, "the file has the road, four hills, the tunnel and the shortcut")
        u = sim("--state", 2, "--tracksdir", tmp, "--track", 0, "--edit", "--edscript", "down -460 -240;move -300 -200;up;wait;key z;wait", seconds=1)
        a = sim("--track", 0, "--state", 3, "--mode", 1, seconds=0.3)
        check(u["track"] == NTRACKS + 1 and u["trackLen"] == a["trackLen"], "editing a built-in track edits a copy; undo puts the moved point back", (u["track"], u["trackLen"], a["trackLen"]))
        b = sim("--track", 0, "--state", 3, "--mode", 1, seconds=0.3)
        check(b["trackLen"] == a["trackLen"], "the built-in track itself is untouched")
        r = sim("--trackfile", f, "--state", 3, "--auto", "--nobotguns", seconds=260)
        check(r["finished"] == 1 and r["sinks"] == 0 and r["wrecks"] == 0, "a three-lap race on it finishes: nobody sinks, nothing to hit", (r["finished"], r["sinks"], r["wrecks"]))
        r = sim("--trackfile", f, "--state", 3, "--auto", "--mode", 2, seconds=330)
        check(r["finished"] == 1, "a rampage race on it finishes too", r["finished"])
        t = sim("--trackfile", f, "--state", 2, "--edit", "--edscript", "key enter", "--auto", seconds=40)
        check(t["state"] == 3 and t["maxSpeed"] > 20, "Enter in the editor starts a test drive", (t["state"], round(t["maxSpeed"])))
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


@test
def grass_grows_in_the_fields_only():
    """Open grassland has tufts of grass; the town, the road and its verge have none; --nograss turns it off."""
    field = sim("--track", 3, "--state", 3, "--mode", 1, "--pos", -420, -285, 0, seconds=1)
    off = sim("--track", 3, "--state", 3, "--mode", 1, "--pos", -420, -285, 0, "--nograss", seconds=1)
    flat_ = sim("--flat", "--state", 3, "--mode", 1, seconds=1)
    check(field["grassNear"] > 300, "tufts in the park", field["grassNear"])
    check(off["grassNear"] == 0 and flat_["grassNear"] == 0, "none with --nograss or on the test plane", (off["grassNear"], flat_["grassNear"]))


@test
def some_ufos_come_down_in_the_sea():
    """Each Warden has three UFOs. Now and then one that is shot down lands in the water with a splash and sinks
    (no wreck is left); the rest still leave wrecks on land."""
    d = sim("--track", 0, "--state", 3, "--mode", 1, "--auto", seconds=240)
    check(d["ufoSplashes"] >= 1 and d["ufoWrecks"] >= 3, "splashes in the sea, wrecks on land", (d["ufoSplashes"], d["ufoWrecks"]))
    check(d["ripples"] >= d["ufoSplashes"], "each splash rings the water", (d["ripples"], d["ufoSplashes"]))


@test
def splashes_ring_the_water():
    """Something hitting the sea leaves spreading rings; nothing on dry land does."""
    sea = sim("--track", 0, "--state", 3, "--mode", 1, "--ripat", -700, 0, seconds=3)
    land = sim("--track", 0, "--state", 3, "--mode", 1, "--ripat", 0, 0, seconds=3)
    check(sea["ripples"] >= 3 and land["ripples"] == 0, "rings on the sea, none on land", (sea["ripples"], land["ripples"]))


@test
def custom_tracks_get_a_beach_and_the_editor_works_by_touch():
    """A track of your own has sand along its shore (the built-in ones keep theirs as drawn). In the editor a finger
    drags a point, two fingers pinch to zoom and pan the map, and a tap with the ROAD tool adds a point."""
    tmp = tempfile.mkdtemp()
    try:
        shutil.copy(os.path.join(ROOT, "tests", "tracks", "test-loop.fcft"), tmp)
        c = sim("--tracksdir", tmp, "--trackfile", os.path.join(tmp, "test-loop.fcft"), "--state", 3, "--mode", 1, seconds=1)
        b = sim("--track", 0, "--state", 3, "--mode", 1, seconds=1)
        check(c["beachLand"] > 50 and b["beachLand"] == 0, "sand along a custom track's shore only", (c["beachLand"], b["beachLand"]))
        e = sim("--tracksdir", tmp, "--trackfile", os.path.join(tmp, "test-loop.fcft"), "--state", 2, "--edit", "--edtouch", seconds=1)
        check(e["edTouch"].split(" ")[0] == "1,1,1,1", "drag, pinch, pan and tap all work", e["edTouch"])
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


@test
def a_shortcut_can_be_a_tunnel():
    """The TUNNEL tool on a shortcut makes it dive under the sea (in the editor: on Twin Isles, the causeway between the
    islands). The file keeps it; a car takes it end to end, down in the tube and out the far side; right click undoes it."""
    tmp = tempfile.mkdtemp()
    try:
        d = sim("--state", 2, "--tracksdir", tmp, "--track", 6, "--edit", "--edscript", "tool 2;down 0 0;up;wait", seconds=1)
        check(d["tunnels"] == 3, "the causeway is now a third tunnel", d["tunnels"])
        files = [os.path.join(tmp, f) for f in os.listdir(tmp) if f.endswith(".fcft")]
        check(len(files) == 1 and "tunnel  shortcut 1  depth 19" in open(files[0]).read(), "the file says so", files)
        r = sim("--trackfile", files[0], "--state", 3, "--autocut", 1, "--nobotguns", "--laps", 1, seconds=200)
        check(r["autoCutRuns"] >= 1 and r["autoCutMinY"] < -15, "a car drives it end to end, down in the tube", (r["autoCutRuns"], r["autoCutMinY"]))
        check(r["finished"] == 1 and r["sinks"] == 0 and r["wrecks"] == 0, "and the race finishes: nobody sinks, nothing hit", (r["finished"], r["sinks"], r["wrecks"]))
        u = sim("--state", 2, "--tracksdir", tmp, "--trackfile", files[0], "--edit", "--edscript", "tool 2;rdown 0 0;up;wait", seconds=1)
        check(u["tunnels"] == 2, "right click: the causeway is back on the surface", u["tunnels"])
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


@test
def keys_jingle_faintly_nearby():
    """A key not yet found gives off a faint metal jingle within about 45 m; none from the start line."""
    far = sim("--track", 0, "--state", 3, "--mode", 1, seconds=6)
    near = sim("--track", 0, "--state", 3, "--mode", 1, "--pos", 0, 276, 0, seconds=6)   # 20 m short of the courtyard key
    check(far["keyJingles"] == 0 and near["keyJingles"] >= 3, "jingles near a key, silence far from them", (far["keyJingles"], near["keyJingles"]))


@test
def the_wardens_shoot_down_ufos_and_plant_flowers():
    for track in range(NTRACKS):
        d = sim("--track", track, "--state", 3, "--auto", "--nobotguns", seconds=200)
        check(d["ufoWrecks"] >= 3, "track %d: UFOs circle the Wardens and get shot down; the wrecks stay where they land" % track, d["ufoWrecks"])
        check(d["ufoOnRoad"] <= 3, "track %d: now and then one lands on the road (and burns out); never a pile-up" % track, d["ufoOnRoad"])
        check(d["flowers"] >= 3, "track %d: seeds fired into the grass grow into flowers" % track, d["flowers"])
        check(d["wrecks"] == 0 and d["lap"] >= 3, "track %d: the autopilot steers round the wrecks and the feet and keeps going" % track, (d["wrecks"], d["lap"]))
    a = sim("--track", 0, "--state", 3, "--auto", "--nobotguns", seconds=90)
    b = sim("--track", 0, "--state", 3, "--auto", "--nobotguns", seconds=90)
    check(a == b, "it all comes from the clock: two runs see the same UFOs fall in the same places and the same flowers")


@test
def the_wardens_minigun_tracks_and_fires_bursts():
    """Each kill is a burst from the six-barrel cannon (about 1.6 s at 22 rounds a second), and in the last moments
    before the hit the barrels point at the UFO (where it will be when the rounds arrive) to within a few degrees."""
    for t in (0, 3, 6):
        d = sim("--track", t, "--state", 3, "--mode", 0, "--auto", seconds=120)
        check(d["ufoWrecks"] >= 3 and d["cannonRounds"] >= 30 * d["ufoWrecks"], "track %d: bursts, not single shots" % t, (d["ufoWrecks"], d["cannonRounds"]))
        check(d["cannonAimErr"] < 6, "track %d: on target when it counts" % t, d["cannonAimErr"])
        check(d["mechFootErr"] < 0.05, "track %d: the feet still land where they should" % t, d["mechFootErr"])

@test
def the_race_director_plays_a_tracks_events():
    """Hilltop Heights' events (events/track0.fcfe): on lap 1, at Warden 1's line, its misfire brings down the tower
    ahead, with a message after it; a pod of two dolphins off the start, one leaping every lap. Three laps: 5 events,
    one building down, three leaps. The file is built into the game too (src/events_builtin.h, for the 360 and
    Android), and that copy is up to date. 2.3: every built-in track has a scripted moment."""
    d = sim("--track", 0, "--state", 3, "--auto", "--nobotguns", seconds=230)
    check(d["eventsLoaded"] == 10 and d["eventLines"] == 4 and d["pods"] == 1, "the track's events are loaded", (d["eventsLoaded"], d["eventLines"], d["pods"]))
    check(d["lap"] >= 3 and d["events"] == 12, "three laps: a misfire on lap 1, a tower down on lap 2 and another on lap 3 (each with a shake and words), a leap a lap, the snipers (2.6.2)", (d["lap"], d["events"]))
    check(d["fallen"] == 3 and d["finished"] == 1 and d["wrecks"] == 0, "three buildings come down, and the race goes on", (d["fallen"], d["finished"], d["wrecks"]))
    check(d["leaps"] == 3 and d["podLeaps"] == 3, "the pod's dolphins take the leaps", (d["leaps"], d["podLeaps"]))
    sys.path.insert(0, os.path.join(ROOT, "tools")); import embed_events
    check(embed_events.build() == open(os.path.join(ROOT, "src", "events_builtin.h")).read(), "src/events_builtin.h matches events/ (run tools/embed_events.py)")
    for t in range(1, 7):
        e = sim("--track", t, "--state", 3, seconds=1)
        check(e["eventsLoaded"] + e["pods"] > 0, "track %d has a scripted moment" % t, (e["eventsLoaded"], e["pods"]))


@test
def buildings_come_down():
    """collapse.h: an event's cue, or a rocket aimed at a building with "fells", brings it down (a building at most
    once; four a race, as the shaders take four). A rocket can leave a Warden's cannon ("from"). The space track has
    no town to bring down."""
    import tempfile
    tmp = tempfile.mkdtemp(); f = os.path.join(tmp, "e.fcfe")
    def run(body, track=0, secs=6):
        open(f, "w").write(body); return sim("--track", track, "--state", 3, "--events", f, seconds=secs)
    d = run("event c1 collapse at -147 0 -93 on time 1\nevent c2 collapse at -147 0 -93 on time 2\n")
    check(d["lots"] > 100 and d["fallen"] == 1, "a building comes down on its cue, and only once", (d["lots"], d["fallen"]))
    d = run("event r rocket at -200 30 -150 to -147 21 -93 fells on time 1\n")
    check(d["fallen"] == 1, "a rocket aimed at it with fells brings it down", d["fallen"])
    d = run("event r rocket at -200 30 -150 to -147 21 -93 on time 1\n")
    check(d["fallen"] == 0, "without fells it only blows up", d["fallen"])
    d = run("event r rocket to -147 21 -93 fells from nearest on time 1\n", secs=8)
    check(d["fallen"] == 1, "a rocket from the nearest Warden's cannon brings it down", d["fallen"])
    d = run("".join("event c%d collapse at %d 0 %d on time 1\n" % (i, x, z) for i, (x, z) in enumerate([(-147, -93), (-12, 69), (-93, -39), (-66, 15), (-12, -66), (-120, -66)])))
    check(d["fallen"] == 4, "four a race at most", d["fallen"])
    open(f, "w").write("event c collapse at 0 0 0 on time 1\n"); e = sim("--space", "--state", 3, "--events", f, seconds=6)
    check(e["lots"] == 0 and e["fallen"] == 0, "the space track has no town to bring down", (e["lots"], e["fallen"]))

@test
def the_event_editor_saves_files_the_game_races():
    """fcfedit (here --evedit, scripted): a line, a rocket set off by it, a blast chained after the rocket, a message on
    lap 2, a shake at 5 s, and a blast with no chance. Saved, the file reads back and a race fires exactly the four that
    can happen, the same way twice; the editor's preview races the events before they are saved."""
    import tempfile
    tmp = tempfile.mkdtemp()
    script = ("line gate1 -143.7 -108.2 -125.5 -165.4; event rocket -175 30 -175; set on gate1; set to -70 14 -95; "
              "event blast -70 14 -95; set on after rocket1; set k 1.4; event say -120 3 -200; set text \"LAP TWO\"; set on lap; set laps 2; "
              "event shake 0 0 0; set on time 5; event blast -134 13 -137; set on gate1; set chance 0; set times always; set by any; save")
    subprocess.run([BIN, "--sim", "--seconds", "0.1", "--evedit", "--track", "1", "--evscript", script], cwd=tmp, capture_output=True, timeout=120)
    f = os.path.join(tmp, "events", "track1.fcfe")
    check(os.path.exists(f), "the editor saved events/track1.fcfe")
    if not os.path.exists(f): return
    txt = open(f).read()
    check(txt.count("\nline ") == 5 and txt.count("\nevent ") == 16, "one line and five events in the file (and Bayside's own four lines and eleven events)", (txt.count("\nline "), txt.count("\nevent ")))
    check("on after rocket1" in txt and "laps 2" in txt and "chance 0" in txt and 'text "LAP TWO"' in txt, "with their settings")
    runs = [sim("--track", 0, "--state", 3, "--auto", "--nobotguns", "--events", f, seconds=230) for _ in range(2)]
    check(runs[0]["eventsLoaded"] == 16 and runs[0]["events"] >= 4, "a race fires the ones that can happen", (runs[0]["eventsLoaded"], runs[0]["events"]))
    check(runs[0] == runs[1], "the same way twice")
    p = subprocess.run([BIN, "--sim", "--seconds", "60", "--auto", "--evedit", "--track", "0", "--evscript", "event shake 0 0 0; set on time 3; preview"],
                       cwd=tmp, capture_output=True, text=True, timeout=200)
    pv = json.loads(p.stdout.strip().splitlines()[-1])
    check(pv["events"] >= 1, "the editor's preview races the unsaved events", pv["events"])
    w = os.path.join(tmp, "w.fcfe"); open(w, "w").write("warden 1  -460 -200\nwarden 2  -134 -175\n")
    d = sim("--track", 0, "--state", 3, "--events", w, seconds=0.1)
    dist = lambda m, x, z: math.hypot(m[0] - x, m[2] - z)
    check(dist(d["mech"], -460, -200) < 25 and dist(d["mech2"], -134, -175) < 25, "Wardens 1 and 2 start walking where the file puts them", (d["mech"][:3], d["mech2"]))

@test
def a_warden_misfire_is_one_click_in_the_editor():
    """2.29.2: a Warden's + MISFIRE makes its own trigger line (~70 m before it), the rocket it fires and the words after
    it; a NEW MOMENT chains click by click (here an explosion at the start of a lap). Saved and raced, they all go off."""
    import tempfile
    tmp = tempfile.mkdtemp()
    script = "warden 3 -134 -175; wmisfire 3; moment 2; lapstart; click -70 -95; save"
    subprocess.run([BIN, "--sim", "--seconds", "0.1", "--evedit", "--track", "0", "--evscript", script], cwd=tmp, capture_output=True, timeout=120)
    f = os.path.join(tmp, "events", "track0.fcfe")
    check(os.path.exists(f), "the editor saved events/track0.fcfe")
    if not os.path.exists(f): return
    txt = open(f).read()
    check("line warden3 " in txt and "from warden3" in txt and "on warden3" in txt and "on after misfire1" in txt, "Warden 3's misfire: its line, its rocket, its words", txt[:400])
    check(txt.count("\nline ") == 5 and txt.count("\nevent ") == 13, "five lines (Hilltop's four, Warden 3's) and thirteen events (Hilltop's ten, three new)", (txt.count("\nline "), txt.count("\nevent ")))
    r = sim("--track", 0, "--state", 3, "--auto", "--nobotguns", "--events", f, seconds=120)
    check(r["eventsLoaded"] == 13 and r["events"] >= 9, "a race sets them off (the lap 2 and 3 ones later)", (r["eventsLoaded"], r["events"]))

@test
def the_ronin_120_comes_from_blender_and_races():
    """The Ronin 120 is the first car made in Blender and brought in through the conduit (tools/blender/fcf_model.py,
    src/model_ronin.h): within the triangle budget, with its baked shading, and it drives a full race like the others."""
    import re
    h = open(os.path.join(ROOT, "src", "model_ronin.h")).read()
    n = {k: int(v) for k, v in re.findall(r"(BODY_NV|BODY_NI|WHEEL_NV|WHEEL_NI|AO_N) = (\d+)", h)}
    check(n["BODY_NI"] % 3 == 0 and 5000 < n["BODY_NI"] // 3 <= 40000, "body within the PC budget (40k triangles)", n["BODY_NI"] // 3)
    check(n["WHEEL_NI"] // 3 <= 4000, "a light wheel", n["WHEEL_NI"] // 3)
    ao = [int(x) for x in re.search(r"AO\[\] = \{\n(.*?) \};", h, re.S).group(1).replace("\n", "").split(",")]
    check(len(ao) == n["AO_N"] ** 2 and min(ao) < 80 and sum(ao) / len(ao) > 100, "baked shading: dark arches, a mostly light body", (len(ao), min(ao), sum(ao) // len(ao)))
    _race(0, 4)

@test
def every_xbox_program_gets_the_vertex_layout_of_its_vertex_shader():
    """The 360 build's shader table (xenon/transform.py) carries each program's vertex layout, and makeProg sets it from
    there. Every program the game makes is in the table, with the layout of the vertex shader the game gives it (a mesh
    program without the mesh layout draws nothing or garbage: in 2.22-2.23 the space track's planet and road)."""
    import os, re, json, subprocess, tempfile
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    want = {"VS_MESH": "mesh", "VS_PART": "part", "VS_UI": "ui", "VS_SKY": "none"}
    game = open(os.path.join(root, "src", "game.cpp")).read()
    made = {n: want[v] for n, v in re.findall(r'FCF_MAKE_PROG\(\w+, "(\w+)", (VS_\w+),', game) if v in want}
    out = tempfile.mkdtemp()
    subprocess.run(["python3", os.path.join(root, "xenon", "transform.py"), out], check=True, capture_output=True)
    table = {n: json.load(open(os.path.join(out, n + ".uniforms.json")))["layout"] for n in open(os.path.join(out, "programs.txt")).read().split()}
    check(set(made) <= set(table), "every program the 360 makes is in its shader table", sorted(set(made) - set(table)))
    check(all(table.get(n) == l for n, l in made.items()), "each with its vertex shader's layout", {n: (l, table.get(n)) for n, l in made.items() if table.get(n) != l})
    src = open(os.path.join(root, "src", "gfx_xsdk.h")).read()
    check("switch(p.meta->layout)" in src and 'strcmp(name,"world")||!strcmp(name,"car")' not in src, "makeProg sets the layout from the table", None)


@test
def the_town_has_all_kinds_of_buildings():
    """Every lot that had a building still has one, the same size: only the look changed. A city track has every kind
    (plain, stepped, podium, glass, round, flats, warehouse), and the props stay within budget."""
    d = sim("--track", 3, "--state", 3, seconds=0.1)
    check(all(k > 0 for k in d["buildingKinds"][:6]), "towers, podiums, glass, round, flats and plain blocks", d["buildingKinds"])
    n = 0
    for t in range(NTRACKS):
        e = sim("--track", t, "--state", 3, seconds=0.1); n += e["buildingKinds"][6]
        check(e["propTris"] < 400000, "track %d: props within budget" % t, e["propTris"])
    check(n > 0, "warehouses by the water somewhere", n)

@test
def the_wardens_feet_are_solid():
    # a Warden standing on the road at Hilltop's sample 400; we drive at its left foot at 30 m/s
    hit = sim("--track", 0, "--state", 3, "--mode", 1, "--nobotguns", "--mechat", -163, -174.2, 0.609, "--pos", -200.7, -219.8, 0.609, "--speed", 30, seconds=2.5)
    miss = sim("--track", 0, "--state", 3, "--mode", 1, "--nobotguns", "--nomech", "--pos", -200.7, -219.8, 0.609, "--speed", 30, seconds=2.5)
    check(hit["vf"] < 5 and miss["vf"] > 20, "a foot stops a car dead, the way a wall does (no Warden: it drives on)", (hit["vf"], miss["vf"]))


@test
def showdown_bots_always_make_it_back_into_the_ring():
    # the ring never closes faster than you can run (it used to slam in once two were left), and a bot never wanders
    # somewhere it can't climb back out of: on every track, with the ring off the road and on it, nobody burns
    for track in range(NTRACKS):
        for site in (None, 0.1, 0.55):
            a = ["--track", track, "--state", 3, "--mode", 2, "--sdnow", "--auto", "--keys", 5, "--sdplace", 7] + (["--sdsite", site] if site is not None else [])
            d = sim(*a, seconds=200)
            check(d["ringDeaths"] == 0 and d["shdResult"] == 1, "track %d, ring %s: no bot burns, and it reaches its results" % (track, "off the road" if site is None else "at %.2f" % site), (d["ringDeaths"], d["fighters"]))


@test
def showdown_bots_drive_the_shortest_way_into_the_ring():
    # every track, the ring off the road and on it near and far: the five find their own way in (round water, down the
    # alleys) long before the drive times out, and the Wardens' clock keeps going while they do
    for track in range(NTRACKS):
        for site in (None, 0.1, 0.55):
            a = ["--track", track, "--state", 3, "--mode", 2, "--sdnow", "--auto", "--keys", 5, "--sdplace", 7] + (["--sdsite", site] if site is not None else [])
            d = sim(*a, seconds=88)
            check(d["driveLate"] == 0 and 0 < d["driveTook"] < 85, "track %d, ring %s: all five drove into the ring" % (track, "off the road" if site is None else "at %.2f" % site), (d["driveTook"], d["driveLate"]))
            if site == 0.55:   # 2.7.2: half a lap away, they take the road (it is quicker) and leave it only for short cuts
                check(d["driveRoadT"] > 2 * d["driveOffT"], "track %d: a far ring is reached mostly by road" % track, (d["driveRoadT"], d["driveOffT"]))
    a = sim("--track", 3, "--state", 3, "--mode", 2, "--sdnow", "--auto", "--keys", 5, "--sdplace", 7, seconds=4)
    b = sim("--track", 3, "--state", 3, "--mode", 2, "--sdnow", "--auto", "--keys", 5, "--sdplace", 7, seconds=8)
    check(b["mechT"] - a["mechT"] > 3.5, "the Wardens and UFOs keep moving through the showdown", (a["mechT"], b["mechT"]))


@test
def the_throttle_is_a_pedal():
    """2.7.1: an analog trigger held part way holds a lower speed; all the way is full speed, as before."""
    full = sim("--track", 1, "--state", 3, "--hold", "W", seconds=14)
    half = sim("--track", 1, "--state", 3, "--hold", "W", "--pedal", 0.5, seconds=14)
    check(half["maxSpeed"] < full["maxSpeed"] * 0.8 and half["maxSpeed"] > full["maxSpeed"] * 0.4, "half a pull: a good deal slower, still moving well", (half["maxSpeed"], full["maxSpeed"]))


@test
def showdown_always_ends_with_one_winner():
    off, cov, heal = [], [], []
    for track in range(NTRACKS):
        d = sim("--track", track, "--state", 3, "--mode", 2, "--sdnow", "--auto", "--keys", 5, "--footai", seconds=240)
        fs = fighters(d)
        check(d["state"] == 5 and d["shdResult"] == 1, "track %d: the showdown reaches its results" % track, (d["state"], d["shdPhase"], d["prepAt"], d["fighters"]))
        check(sorted(f["place"] for f in fs) == [1, 2, 3, 4, 5], "track %d: all five places are decided" % track, d["fighters"])
        alive = sum(f["alive"] for f in fs)   # (if you go down early and the others take their time, they are placed by health)
        check(alive == 1 or not fs[0]["alive"], "track %d: the winner is the last one standing" % track, d["fighters"])
        off.append(d["shdOffRoad"]); cov.append(d["botCoverT"]); heal.append(d["botHealRuns"])
        check(sum(f["kills"] for f in fs) >= 1, "track %d: it was settled with guns, not just the ring" % track, d["fighters"])
    check(sum(off) >= 2, "left to itself the ring goes up off the road, and everyone still gets there", off)
    check(sum(heal) >= 3, "2.9: badly hurt, the bots run for a medkit (cover is off for now)", (cov, heal))


@test
def rooftop_snipers_warn_with_a_laser():
    """2.9: a rooftop sniper paints you with a red laser for a full second before every shot (a soft note when it lands
    on you). Stand in it and most shots land; break its line of sight while it is on you and that shot never comes;
    run across it and it trails you and misses."""
    import tempfile
    tmp = tempfile.mkdtemp(); f = os.path.join(tmp, "e.fcfe")
    open(f, "w").write("event s1 minis  at -471 70 -174  to -480 9 -175  k 2  stay  on time 1\n")
    still = sim("--track", 0, "--state", 3, "--onfoot", "--events", f, seconds=14)
    check(still["snipes"] >= 6 and still["lasers"] >= still["snipes"] and still["minLaser"] >= 0.95, "every shot comes after a second of laser", (still["snipes"], still["lasers"], still["minLaser"]))
    check(still["laserTones"] >= still["snipes"] and still["snipeHits"] >= 1, "the note sounds as it lands on you; standing in it, you are hit", (still["laserTones"], still["snipeHits"]))
    duck = sim("--track", 0, "--state", 3, "--onfoot", "--hold", "4.5:W", "--events", f, seconds=14)
    check(duck["laserDodged"] >= 1, "out of its sight while the laser is on you: that shot is off", duck["laserDodged"])
    across = sim("--track", 0, "--state", 3, "--onfoot", "--hold", "3:D", "--events", f, seconds=14)
    check(across["snipes"] >= 3 and across["snipeHits"] < still["snipeHits"], "running across the laser shakes it off", (across["snipes"], across["snipeHits"], still["snipeHits"]))


@test
def landed_mini_wardens_fight_from_cover():
    """2.9: with you on foot, mini Wardens that have jetted down take cover (out of your sight, with a step out that
    sees you), peek out to fire short bursts after a charge glows at the muzzle, move between covers, and one of the
    group goes round your flank. Shooting at one in cover mostly hits the wall. All of it is the same every run."""
    import tempfile
    tmp = tempfile.mkdtemp(); f = os.path.join(tmp, "e.fcfe")
    open(f, "w").write("event m1 minis  at -147 30 -93  to -170 0 -110  k 3  on time 1\n")
    a = ["--track", 0, "--state", 3, "--mode", 1, "--pos", -170, -80, 0, "--onfoot", "--ads", "--aimmini", "--gun", 1, "--keys", 5, "--events", f]
    d = sim(*a, seconds=30)
    check(d["covers"] >= 3 and d["flanks"] >= 1 and d["coverT"] > 3, "they take cover, move between covers, and one flanks", (d["covers"], d["flanks"], d["coverT"]))
    check(d["bursts"] >= 3 and 0 < d["miniRoundHits"] < d["miniRounds"] * 0.5, "they peek out and fire bursts; fewer than half land", (d["bursts"], d["miniRounds"], d["miniRoundHits"]))
    check(d["hitsCover"] / max(d["coverT"], 0.01) < d["hitsOpen"] / max(d["openT"], 0.01), "in cover they are hit less often than out of it", (d["hitsCover"], d["coverT"], d["hitsOpen"], d["openT"]))
    e = sim(*a, seconds=30)
    check([e[k] for k in ("covers", "miniRounds", "miniRoundHits", "meHp", "jetDowned")] == [d[k] for k in ("covers", "miniRounds", "miniRoundHits", "meHp", "jetDowned")], "the same every time", (d["miniRounds"], e["miniRounds"]))


@test
def mini_warden_plating_wants_the_right_gun():
    """2.9: a mini Warden's plating sheds pistol and SMG rounds at range (blue numbers, sparks, and once an ARMOURED
    hint); the shotgun, the rifle and a round to the head go straight through."""
    import tempfile
    tmp = tempfile.mkdtemp(); f = os.path.join(tmp, "e.fcfe")
    open(f, "w").write("event s1 minis  at -490 0 -175  to -495 0 -175  k 1  stay  on time 1\n")
    run = lambda *g: sim("--track", 0, "--state", 3, "--onfoot", "--ads", "--keys", 5, "--events", f, *g, seconds=6)
    smg, head, sg, rifle = run("--aimmini", "--gun", 3), run("--aimhead", "--gun", 1), run("--aimmini", "--gun", 4), run("--aimmini", "--gun", 2)
    check(smg["plateHits"] > 5 and smg["plateShed"] > 0.4 * (smg["plateShed"] + smg["jetDmg"]), "the SMG at 19 m: much of it glances off", (smg["plateHits"], smg["plateShed"], smg["jetDmg"]))
    check(head["headshots"] >= 1 and head["plateShed"] == 0 and head["jetDowned"] == 1, "the head: no plating there", (head["headshots"], head["plateShed"], head["jetDowned"]))
    check(sg["plateHits"] == 0 and sg["jetDowned"] == 1 and rifle["plateHits"] == 0 and rifle["jetDowned"] == 1, "shotgun and rifle go through", (sg["plateHits"], rifle["plateHits"], sg["jetDowned"], rifle["jetDowned"]))


# ----------------------------------------------------------------------------- 2.6.3
@test
def car_is_drawn_on_foot_in_every_camera():
    """2.6.3: on foot in the bonnet camera the parked car vanished. It is drawn in every camera unless you sit in it
    (bonnet view) or are watching a showdown you are not in (carHidden)."""
    for cam in range(3):
        d = sim("--track", 0, "--state", 3, "--cammode", cam, "--onfoot", seconds=5)
        check(d["meOn"] == 1 and d["playerCarDrawn"] == 1, "camera %d, on foot: the parked car is drawn" % cam, (d["meOn"], d["playerCarDrawn"]))
    d = sim("--track", 0, "--state", 3, "--cammode", 2, seconds=5)
    check(d["playerCarDrawn"] == 0, "in the car, bonnet camera: not drawn (the view is from inside it)", d["playerCarDrawn"])


@test
def every_weapon_hurts_mini_wardens():
    """2.6.3: fists, every gun, grenades and the car's rockets all take mini Wardens down: free roam's, and those that
    jetpack off a roof, hip fire (lock-on) and down the sights."""
    for g in range(6):
        d = sim("--track", 1, "--state", 3, "--mode", 1, "--minisnow", "--minishold", "--outcar", "--footai", "--gun", g, seconds=25)
        check(d["miniKilled"] == 1 and d["minis"] == 0, "free roam, gun %d: it goes down" % g, (d["miniKilled"], d["minis"]))
    d = sim("--track", 1, "--state", 3, "--mode", 1, "--minisnow", "--minishold", "--rockets", 2, "--fire", 3, seconds=8)
    check(d["rocketsFired"] == 1 and d["miniKilled"] == 1, "a rocket from the car takes it down", (d["rocketsFired"], d["miniKilled"]))
    f = os.path.join(tempfile.mkdtemp(), "ev.fcfe")
    open(f, "w").write("event m1 minis  at -147 30 -93  to -170 0 -110  k 3  on time 1\n")
    for g in range(1, 6):
        h = sim("--track", 0, "--state", 3, "--pos", -160, -98, 3.9, "--onfoot", "--hipfire", "--gun", g, "--events", f, seconds=20)
        check(h["jetDowned"] >= 2, "jetted down, gun %d from the hip: they go down" % g, h["jetDowned"])
    for g in (1, 2, 3):
        a = sim("--track", 0, "--state", 3, "--pos", -160, -98, 3.9, "--onfoot", "--ads", "--aimmini", "--gun", g, "--events", f, seconds=20)
        check(a["jetDowned"] == 3, "jetted down, gun %d down the sights: all three go down" % g, a["jetDowned"])


@test
def car_damage_slows_wrecks_and_mends():
    """2.6.3: in a race knocks hurt the car (the CAR meter). Hurt, it is slower; at none it is wrecked and comes back
    whole; a shield keeps knocks off; it mends as it goes. Normal racing hardly scratches it. Not in MANIC."""
    whole = sim("--track", 0, "--state", 3, "--auto", "--nobotguns", seconds=30)
    hurt = sim("--track", 0, "--state", 3, "--auto", "--nobotguns", "--carhp", 10, seconds=30)
    check(whole["carDmgWrecks"] == 0 and whole["carHp"] > 90, "the autopilot races on unhurt", (whole["carDmgTaken"], whole["carHp"]))
    check(hurt["dist"] < whole["dist"] * 0.92, "a badly hurt car is slower", (hurt["dist"], whole["dist"]))
    check(30 < hurt["carHp"] < 60, "and mends a little as it goes", hurt["carHp"])
    bump = ("--seed", 5, "--track", 0, "--state", 3, "--nobotguns", "--pos", -501.1, -182.8, 2.698, "--speed", 25)
    d = sim(*bump, seconds=2)
    check(d["carDmgTaken"] > 5 and d["wrecks"] == 0, "a rear-end hurts the car, a little", (d["carDmgTaken"], d["wrecks"]))
    d = sim(*bump, "--carhp", 5, seconds=2)
    check(d["carDmgWrecks"] == 1 and d["dead"] == 1, "with nothing left it is wrecked", (d["carDmgWrecks"], d["dead"]))
    d = sim(*bump, "--carhp", 5, seconds=5)
    check(d["dead"] == 0 and d["carHp"] == 100, "and back on the road whole", (d["dead"], d["carHp"]))
    d = sim(*bump, "--carhp", 5, "--shield", seconds=2)
    check(d["carDmgTaken"] == 0 and d["dead"] == 0, "a shield keeps the knock off", (d["carDmgTaken"], d["dead"]))
    d = sim("--track", 0, "--state", 3, "--mode", 4, "--auto", "--carhp", 10, seconds=10)
    check(d["carTopK"] == 1 and d["carDmgTaken"] == 0, "MANIC: no damage at all", (d["carTopK"], d["carDmgTaken"]))


@test
def incoming_warns_of_a_wardens_revenge_rocket():
    """2.6.3: run a mini Warden down and the Warden's rocket comes for you: INCOMING! flashes with a warning tone, and an
    arrow points at it all the way in."""
    d = sim("--track", 1, "--state", 3, "--mode", 1, "--minisnow", "--minishold", "--auto", "--flowers", 30, seconds=12)
    check(d["miniRockets"] == 1 and d["incomingSaid"] == 1, "one revenge rocket, one INCOMING!", (d["miniRockets"], d["incomingSaid"]))
    check(d["incomingMax"] == 1 and d["incomingFlight"] > 1.0, "the arrow is up while it flies (s)", (d["incomingMax"], d["incomingFlight"]))
    d = sim("--track", 1, "--state", 3, "--mode", 1, "--minisnow", "--minishold", seconds=12)
    check(d["incomingSaid"] == 0 and d["incomingFlight"] == 0, "nothing fired at you: no warning", (d["incomingSaid"], d["incomingFlight"]))


@test
def dolphins_leap_higher_with_a_little_variety():
    """2.6.3: a leap is about 5.5 m high (per unit of size) over 16 m and 2.1 s in the air, each one up to 15% higher or
    lower from a hash (not the race's dice), in and out over water. The same in every run."""
    f = os.path.join(tempfile.mkdtemp(), "dol.fcfe")
    open(f, "w").write("pod p1  -455 -530  r 55  n 3  k 1\nevent l1 dolphin  at -430 0 -540  to -400 0 -550  on time 2\n"
                       "event l2 dolphin  at -470 0 -515  to -500 0 -500  on time 9\nevent l3 dolphin  at -440 0 -520  to -420 0 -560  on time 16\n")
    d = sim("--track", 0, "--state", 3, "--events", f, seconds=30)
    check(d["leaps"] == 3 and d["dolphinSplashes"] == 3, "three leaps", (d["leaps"], d["dolphinSplashes"]))
    lo, hi = d["leapApexK"]
    check(5.5 * 0.85 - 0.01 <= lo and hi <= 5.5 * 1.15 + 0.01 and hi - lo > 0.3, "apex 5.5 m +-15% per unit size, and not all the same", (lo, hi))
    check(1.8 <= d["leapAir"][0] and d["leapAir"][1] <= 2.5, "about 2.1 s in the air", d["leapAir"])
    check(d["leapWetMin"] > 1.0, "out of and back into water", d["leapWetMin"])
    e = sim("--track", 0, "--state", 3, "--events", f, seconds=30)
    check(e["leapApexK"] == d["leapApexK"], "the same leaps every run", (e["leapApexK"], d["leapApexK"]))


if __name__ == "__main__":
    sys.exit(main())
