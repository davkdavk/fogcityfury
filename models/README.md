# Models

Models made in Blender, and how they get into the game.

Each folder holds a model's source: a `.blend`, `.glb` or `.fbx`, or (like `ronin/ronin.py`) a script that builds it
in Blender. The conduit turns it into a header the game compiles in:

    pip install bpy                                         # Blender as a Python module, once
    python3 tools/blender/fcf_model.py -- models/ronin/ronin.py ronin     # writes src/model_ronin.h

Conventions the conduit relies on (see the top of `tools/blender/fcf_model.py`):

- metres, Z up, front facing -Y (Blender's defaults)
- wheels are separate objects named `wheel_*`; everything else is the body
- materials named `paint` (takes the player's colour), `glass`, `headlight`, `taillight`, `tyre` / `trim`;
  anything else keeps its own colour
- budget: up to 40k triangles for a car body (bigger ones are decimated); about 20k is kind to the Xbox 360

| Model | Source | Triangles | Licence |
| --- | --- | --- | --- |
| Ronin 120 (car 5) | `ronin/ronin.py` | 13k body, 0.7k wheel | made for this game |

## Adding any car: `fcf_car.py` (2.9.8.6)

One command takes a car model as it comes, from any site or package, and puts it in the game:

    pip install bpy                                    # Blender as a Python module, once
    python3 tools/blender/fcf_car.py -- ~/Downloads/mycar.zip mycar --title "MY CAR" --blurb "WIDE AND LOW" --code SECRET --like 1
    make                                               # rebuild: the car is on the car screen (after SECRET is typed on the main menu)

It reads `.blend`, `.glb`/`.gltf`, `.fbx`, `.obj`, `.dae`, `.usd`, `.abc`, `.ply`, `.stl`, or a `.zip` with one of them in
(it picks the best). Nothing has to be renamed or set up by hand. The tool:

- turns it the right way up and round (from the wheels, and from names and colours: head and tail lights, red lenses,
  exhaust, bonnet, boot), and scales it if it was made in centimetres, millimetres or inches
- finds the four wheels (by name, or by shape: four discs at the corners), keeps the front right one as the game's wheel
- sorts the materials into paint (takes the player's colour), glass, head and tail lights, tyres; the rest keep their
  colour (a textured part: the texture's average colour)
- leaves out floors, collision and shadow meshes, lower levels of detail, the engine and the driver
- cuts the body to 20,000 triangles (`--tris`), bakes its soft shadows, and works out its wheelbase, track and wheel size
- writes `src/model_<name>.h`, `models/<name>/car.json` and two previews (`preview_front.png`, `preview_rear.png`)

**Look at the previews before you build.** The paint shows red. If the front is the back, run it again with
`--front +y` (or `-y`); if the wrong part is the paint, `--paint MATERIALNAME`. Every option is listed at the top of
`tools/blender/fcf_car.py`. `--list` shows the cars added; `--remove NAME` takes one out.

Handling comes from one of the five built-in cars (`--like 0`-`4`, or its name), with `--top`, `--acc`, `--turn`,
`--grip` and `--drive rwd|fwd|awd` on top. Online, everyone sees an added car if their game has it too (the same
build); an older game shows it as car 1.

**Only add a car you have the rights to ship in a public game** (made it yourself, CC0, CC-BY with credit, or bought
under a licence that allows games, from its real author). Record it with `--licence "..."`: it goes in the table below.
A model ripped from another game is not yours to ship, whatever a store charged for it.

<!-- fcf_car: added cars -->

| Added car | Source | Triangles | Licence |
| --- | --- | --- | --- |
| (none yet) | | | |

<!-- /fcf_car -->
