# Handling model

The player's car is simulated in `stepPlayer()` in `src/game.cpp` at 120 Hz. Opponents are not: they
ride the centre line (`stepAI`).

The goal is an arcade car that still has a front and a rear: slight understeer when you ask too much
of it, oversteer you can provoke (handbrake, lifting or braking mid-corner, power in a low gear) and
catch with opposite lock. The earlier model set the yaw rate straight from the steering wheel and
damped sideways velocity, which is why it felt glued down.

Read `AGENTS.md` for sign conventions first: +steer, +yaw rate and +lateral velocity are all *left*.

## State

Per car: world velocity `vel`, heading `yaw`, yaw rate `yawRate`. Each step the velocity is split
into forward `vf` and lateral `vs` along the current heading, forces are applied, and it is put back
together. The heading is advanced afterwards, so no `-vf * r` term is needed.

## Longitudinal

Arcade rules: throttle adds `acc` m/s^2, tapering to zero at `top`. Brake pressure (`brakeS`) builds
at 3.5 /s and releases at 8 /s, so full braking arrives over about 0.3 s and peaks at 26 m/s^2
(180 km/h to rest in 53 m);
drag is proportional to speed, with more off-road (scaled by `offroad`) and in shallow water. Gear
changes cut power for 0.13 s (`shiftCut`). Slopes add gravity along the car.

## Lateral: two axles, two tyres

    slip angle front  aF = atan((vs + wbF * r) / v) - delta
    slip angle rear   aR = atan((vs - wbR * r) / v)
    force             F  = -peak * tyre(a / aPk)

`tyre(x)` rises as a sine to 1 at x = 1 (peak grip), then falls smoothly to 0.8 by x = 3.5: a
sliding tyre keeps most of its grip, which is what makes slides long and catchable rather than
snappy. `aPk` is the slip angle of peak grip: 0.13 rad on tarmac, 0.21 off-road and in water
(looser, more sideways).

    lateral acceleration   d(vs)/dt = fF * cos(delta) + fR
    yaw acceleration       d(r)/dt  = (wbF * fF * cos(delta) - wbR * fR) / k2  -  0.5 * r

Forces are expressed as accelerations of the whole car, so `peak` is in m/s^2: `gF` and `gR` in the
car spec are what the front and rear axle can each contribute. Whichever axle runs out first decides
the balance: front first is understeer, rear first is oversteer.

What changes the peaks from moment to moment:

| Effect | Code | Result |
|---|---|---|
| Surface | `surf`: 1.0 road, 0.72 off-road, 0.6 water | less grip everywhere |
| Weight transfer | `lt = accS * 0.006`, clamped -0.075..0.09; front `* (1 - 2 lt)`, rear `* (1 + 2 lt)` | braking or lifting loads the front and lightens the rear (turn-in, lift-off oversteer); power does the reverse |
| Traction circle | `torque = drive * (1 + clamp(1 - v/25))`; `u = drv * torque / (peakR * trac)`, capped 0.8; rear `* sqrt(1 - u^2)` | throttle spends rear side grip. The speed term stands in for low gears, so power slides happen below ~90 km/h and fade out above |
| Handbrake | rear `* 0.30` | the rear lets go: drift |
| Landing | rear `* (1 + landT)` for 0.5 s | assist: no spin on touchdown |
| Well sideways | rear `* (1 + 0.5 * clamp(abs(beta) - 0.25, 0, 0.5))` unless handbrake | assist: the slide stabilises instead of becoming a spin |

Steering:

    dMax  = min(0.55, L * 24 / v^2 + 1.35 * aPk)        lock available at this speed
    delta = steer * dMax + caster(beta)

The first term of `dMax` is the lock needed to hold a 24 m/s^2 corner; the second lets full lock
push the front tyres 35% past their peak. So full lock at speed always scrubs the front a little
(that is the slight understeer) but never demands several times the grip there is.

`caster(beta)` turns the front wheels into a slide by 0.7 of the sideslip beyond 0.06 rad, up to
0.45 rad. Without it the speed-limited lock is smaller than the slide angle and opposite lock cannot
reach: the front keeps pulling the nose round and every slide becomes a spin. This was found with
`FCF_TRACE` (steering fully right, yaw rate still left for two seconds).

Other assists: 0.5 /s yaw damping always; extra damping `4 * (abs(beta) - 0.3)` /s past 0.3 rad
unless the handbrake is on. In the air the yaw rate decays at 3 /s and steering adds 0.6 rad/s.

Below 9 m/s the model blends into plain kinematic steering (`r = vf * tan(delta) / L`, sideways
velocity damped), fully kinematic under 3 m/s and in reverse. Slip-angle maths divides by speed and
is neither stable nor useful at parking speeds.

A turned front tyre also drags: `vf -= fF * sin(delta)`. Scrubbing speed in a corner comes from this.

## Nitro

`NITRO_TANK` 2.5 s of boost, `NITRO_REFILL` 10 s from empty to full, `NITRO_ARM` 0.18 (the level the
tank must reach before a dry one can fire again, and only after the button is released).

While `in.nitro` is held and the car is on the ground, not braking, not in water and not locked out:
the tank drains, `c.boost` ramps to 1 (up at 9 /s, down at 7 /s), the speed ceiling rises by 22% and
a separate `boostA = acc * 1.3 * boost` pushes the car along, tapering to zero at the new ceiling.
`boostA` counts 45% towards the traction circle, so nitro spins the driven wheels but does not
light them up the way full throttle in first gear does. In the air the boost decays and the tank
refills. Net effect on the flat: about +25% top speed and a standing start about 1.7x quicker.

## Weather

`WX[]` in `game.cpp` holds one row per weather with a grip multiplier (clear 1.0, rain 0.82, fog
1.0, storm 0.74) applied to `surf`, and braking scaled by `0.55 + 0.45 * grip`.

One subtlety: the traction-circle term divides the throttle's bite by `0.45 + 0.55 * grip`, so power
eats roughly the same *fraction* of the rear tyres' grip whatever the weather. Without that, a wet
road made every car spin at full lock, because the rear lost grip to both the surface and the
throttle while the front only lost it to the surface. With it, wet weather makes the car slower and
a little looser, and the front still gives up first. `weather_changes_grip_and_braking` pins this
down for all four.

Measured at full lock at 45 m/s (lateral m/s^2) and braking distance from 50 m/s:

| | Clear | Rain | Fog | Storm |
|---|---|---|---|---|
| Stallion lateral | 26.4 | 21.5 | 24.7 | 19.4 |
| Braking from 180 km/h | 53 m | 56 m | 53 m | 58 m |

## Car specs

`SPECS` in `game.cpp`. The handling fields:

| | gF | gR | k2 | drv | trac | Character |
|---|---|---|---|---|---|---|
| Stallion GT | 14.3 | 17.4 | 2.3 | 1.0 | 0.98 | understeers on entry, rear steps out a little on the brakes, power-slides in low gears |
| Katana RS | 16.1 | 18.0 | 1.8 | 1.0 | 1.35 | most grip, lowest yaw inertia; loose on a lift or trail brake; hard to power-slide |
| Bruiser 4x4 | 13.0 | 15.6 | 2.9 | 0.5 | 1.30 | least grip, strong understeer, drive split front/rear so no power oversteer; loses least off-road |

- `gF`, `gR`: peak lateral acceleration from each axle, m/s^2. The *ratio* sets the balance, the
  *sum* sets cornering speed. Raise `gF` towards `gR` for a pointier, looser car.
- `k2`: yaw inertia (radius of gyration squared, m^2). Lower = snappier response and quicker spins.
- `drv`: share of drive on the rear axle. 1 = rear-drive, 0.5 = four-wheel drive.
- `trac`: how much throttle the driven tyres take before side grip suffers. Lower = easier power
  oversteer. This one is sensitive: a change of 0.1 can be the difference between a power slide
  and none at 16 m/s. Re-run `handling_power_slide_in_low_gear` after touching it.

The older fields `turn` and `grip` are no longer read by the physics.

## Measured behaviour

Flat plane, from `tests/run_tests.py -v handling` and the recipes in `TESTING.md`. Slip ratios above
1 mean that axle is past its peak. beta is sideslip in radians.

| Manoeuvre | Stallion | Katana | Bruiser |
|---|---|---|---|
| Top speed (m/s) | 59.2 | 67.3 | 53.0 |
| Full lock at 45 m/s, on power: front / rear slip | 1.99 / 0.86 | 1.98 / 0.88 | 1.98 / 0.69 |
| ... lateral acceleration held (m/s^2) | 26.5 | 29.9 | 22.9 |
| Half lock at 45 m/s: front / rear slip | 1.21 / 0.90 | 1.21 / 0.93 | 1.16 / 0.70 |
| Trail brake at 45 m/s (0.7 lock, 0.6 brake): rear slip, beta | 3.94, 0.43 | 6.33, 0.80 | 1.52, 0.20 |
| Lift off mid-corner at 40 m/s: rear slip, beta | 1.25, 0.13 | 5.93, 0.75 | 0.87, 0.09 |
| Handbrake flick at 35 m/s: peak beta, speed after recovery | 1.34, 29.4 | 1.38, 30.9 | 1.31, 35.0 |
| 0.6 lock + full throttle from 16 m/s: rear slip, beta | 7.5, 0.94 (slides, recovers) | holds | 0.55, 0.05 |
| Brakes from 50 m/s: speed lost in the first 0.15 s / first second, stopping distance | 0.9 / 23.6 m/s, 53 m | | |

So: all three push the front at the limit and none spins from steering alone. Braking into a corner
rotates the car (brake steer): clearly in the Stallion (25 degrees of slip, still catchable), hard in
the Katana, a little in the Bruiser. The Stallion will also power-slide out of slow corners.

The front axle values were raised 10% (and the rear about 9% to keep the balance) when the player
asked for more steering and brake steer; cornering acceleration went up about 11%.

## Feedback the player gets

- `slipF`, `slipR` on the car drive tyre squeal (either axle), and smoke plus skid marks (rear
  sliding past 1.3, wheelspin, or heavy braking).
- Body roll follows `yawRate * vf`; squat and dive follow smoothed acceleration `accS`.
- The chase camera aims between the heading and the direction of travel, so a slide shows as the car
  turning in frame.

## Tuning guide

| Complaint | Try |
|---|---|
| Still feels glued | lower `gF` and `gR` together; lower the tyre plateau (0.20 drop in `tyre`); raise `aPk` |
| Too much understeer | raise `gF`; lower the 1.35 in `dMax` so full lock overshoots less |
| Not enough brake steer | widen the weight-transfer clamp (-0.075); raise `gF` |
| Brakes too sharp / too soft | the 26 (peak) and 3.5 (build rate) in the longitudinal block; `handling_brakes_are_progressive` |
| Spins too easily | raise `gR`; raise the caster gain (0.7); raise yaw damping (0.5) |
| Slides will not start | lower handbrake factor (0.30); lower `trac`; widen weight-transfer clamp |
| Slides end too soon | lower the "well sideways" rear bonus (0.5) and the extra damping (4) |
| Twitchy at speed | raise `k2`; lower `dMax`'s 24 |
| Lands and spins | raise the landing bonus or its 0.5 s |

After any change run `python3 tests/run_tests.py handling -v`. The thresholds there encode the
intended character; move them only when the intent changes. Then run the race tests: the autopilot
holds full throttle and full lock, so it finds instability a careful driver would not.

Not modelled: tyre load sensitivity, camber, differentials, aerodynamic downforce, separate wheel
speeds, suspension geometry. Add them only if a specific feel problem needs them.
