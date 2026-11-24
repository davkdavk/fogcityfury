# Dolphin

"Dolphin" by **Tarnyloo**, from Blend Swap (blend #83681), released under Creative Commons Attribution 3.0
(CC-BY 3.0, https://creativecommons.org/licenses/by/3.0/). Used in Fog City Fury with its rig posed for a leap; the
game's copy is made by `dolphin_export.py` and is a derivative of the original.

- `dolphin_midpoly.blend`: the original file (its smoothing modifier is left off in the game: 6,440 triangles)
- `textures/dolphin_colour.png`: its colour map, read onto the vertices
- `dolphin_export.py`: writes `src/model_dolphin.h`: seven poses of the spine, from bent up to bent down

    python3 models/dolphin/dolphin_export.py
