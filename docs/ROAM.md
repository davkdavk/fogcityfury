# Free roam: mini Wardens, flowers and levels

In free roam (offline), **mini Wardens** land now and then: the Warden itself, shrunk to your size (about 2.3 m).
The first landing comes after about 25 s, then a group of three every 45-80 s, with up to six about at once (two
groups; online, one group at a time). They come down 90-220 m from you, a little off the road, on dry open ground.
There is no announcement: each shows as a red dot on the minimap.

They hunt you. Each has a jetpack: a short burst that carries it most of the way to you, then 12 s to recharge. On
foot, they rush in and fight hand to hand. In the car, they keep 12-26 m off, circle and shoot at it.

**Every weapon hurts them** (2.6.3): fists, every gun, grenades and your car's rockets. Each one beaten leaves 2-4 **flowers** where it fell; walk or drive over them to pick them up (they last 90 s).

**Running one down is allowed, but it costs you.** It bursts into pieces of Warden, leaves no flowers, the flowers
you had earned towards your next level are lost (you keep your level), and the nearest big Warden fires a rocket at
your car from wherever it stands, seen or not: the rocket climbs over the town and comes down on you (from the sky if
there are no Wardens).

**Your car has health here.** Their shots wear it down (a bar under the flowers panel); under half it smokes from
under the bonnet, at none it blows up. Back on the road after a wreck it is whole again; a hurt car stays hurt.

Flowers add up to **levels**, and each level unlocks an attribute for good:

| Level | Flowers | Attribute | What it does |
|---|---|---|---|
| 1 | 10 | Tough skin | 125 health on foot |
| 2 | 25 | Iron lungs | sprint drains stamina 40% slower |
| 3 | 45 | Steady aim | your shots on foot spread about half as much |
| 4 | 70 | Armour plating | you step out of the car with 50 armour |
| 5 | 100 | Hard hitter | your shots on foot do 30% more damage |
| 6 | 140 | Reinforced car | gunfire and rockets do 30% less to your car |
| 7 | 190 | Nitro tank | nitro refills 50% faster |
| 8 | 250 | Green thumb | mini Wardens drop twice the flowers |

The attributes work in offline races and the showdown too, but never online, where everyone is equal. The flower
count is saved with the settings (`flowers N` in the config file; not saved on the Xbox 360 yet).

## Online

On a roam server (`fogcityfury-server --roam`) everyone shares the same mini Wardens. They chase whoever is nearest.
The flowers go to the player who landed the last blow; run one over and the penalty and the rocket are yours alone.
Levels and perks stay your own. See NETWORK.md for how it works.

Anyone who parks under the **golden UFO** (see PLAYING.md) sends the whole server to the secret space track: a
ten-second countdown shows on every screen, everyone races one race in space, then the server roams its own track
again.

## For developers

- `src/minis.h`. Mini Wardens are fighters (`fighter.h`) in slots 1-4 with `mini` set, so they walk, aim, shoot and
  die through the showdown's code (`stepFighter`, `shootLine`, `damageFighter`). `mini::think` is their brain.
- They are drawn by `mech::drawMini`: the Warden's own rig, posed from the fighter's stride, run and aim, inside the
  fighter's frame at `mech::MINI` scale.
- Target 7 is your car (`roamCarTarget`): `tgtAlive`, `tgtPos` and `shootLine` know it.
- The attributes are globals in `fighter.h` (`footMaxHp`, `footLungs`, `footAimMul`, `footStartArmour`,
  `footDmgMul`, `carArmourMul`, `nitroMul`), set every frame by `mini::applyPerks`.
- Test flags: `--minisnow` (one lands 40 m ahead on the road straight away), `--minishold` (it stands still),
  `--outcar` (on foot from the start), `--flowers N`; with `--footai` in free roam you hunt them with your fists (with `--gun N` as well, you shoot at them). Report fields:
  `minis`, `miniSpawned`, `miniKilled`, `miniRams`, `miniPenalties`, `miniJets`, `miniRockets`, `roamFlowers`, `level`, `blooms`, `carHp`, `footHp`, `footMaxHp`,
  `miniPos`. Test: `mini_wardens_in_free_roam`.


Near a hidden key you have not found, you hear a faint metal jingle (from about 45 m, louder as you close in).
