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
