# fcf_model.py - the Blender conduit: turns a model into a header the game compiles in (src/model_<name>.h).
#
#   python3 tools/blender/fcf_model.py -- <model.blend|.glb|.gltf|.fbx|generator.py> <name> [--tris N] [--ao SIZE]
#
# Needs Blender as a Python module (pip install bpy). Blender runs here, at build time, never in the game.
# The model: metres, Z up, front facing -Y (Blender's defaults). Objects named wheel_* are wheels; the one on +X is
# kept as the wheel (the game mirrors it for the other side and places all four from the car's spec). Everything else
# is joined into the body. Materials are matched by name to the car shader's slots:
#   paint (takes the player's colour), glass, headlight, taillight, tyre / trim / rubber (dark, matte),
#   anything else (chrome, rim, indicator...): its own colour, lit, with a little sky reflection.
# What it writes, in the game's axes (X the same, Y up, Z forward; metres, unscaled):
#   body and wheel vertices (position, normal, colour, uv, material) and triangles, the wheel's radius, and the body's
#   ambient occlusion baked by Cycles into a SIZE x SIZE greyscale texture (the soft shadow in arches, under the
#   bumpers, between panels), which the car shader multiplies in. Bigger than N triangles: the body is decimated.
import bpy, bmesh, sys, os, math
from mathutils import Vector

argv = sys.argv[sys.argv.index('--') + 1:]
src, name = argv[0], argv[1]
opt = {argv[i]: argv[i + 1] for i in range(2, len(argv) - 1, 2)}
MAX_TRIS, AO = int(opt.get('--tris', 40000)), int(opt.get('--ao', 256))
REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT = os.path.join(REPO, 'src', 'model_%s.h' % name)

# ---- load ----
ext = os.path.splitext(src)[1].lower()
if ext == '.blend': bpy.ops.wm.open_mainfile(filepath=src)
elif ext == '.py':   # a generator script that builds the model in the current scene
    sys.argv = [src, '--']; exec(compile(open(src).read(), src, 'exec'), {'__name__': '__main__'})
else:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    if ext in ('.glb', '.gltf'): bpy.ops.import_scene.gltf(filepath=src)
    elif ext == '.fbx': bpy.ops.import_scene.fbx(filepath=src)
    else: raise SystemExit('fcf_model: cannot read ' + src)
if bpy.context.object and bpy.context.object.mode != 'OBJECT': bpy.ops.object.mode_set(mode='OBJECT')
meshes = [o for o in bpy.context.scene.objects if o.type == 'MESH']
wheels = [o for o in meshes if o.name.lower().startswith('wheel')]
parts = [o for o in meshes if o not in wheels]
if not parts or not wheels: raise SystemExit('fcf_model: need a body and objects named wheel_*')

def select(objs, active):
    bpy.ops.object.select_all(action='DESELECT')
    for o in objs: o.select_set(True)
    bpy.context.view_layer.objects.active = active

# ---- the body: one object, modifiers applied, decimated to the budget ----
select(parts, parts[0])
for o in parts:
    bpy.context.view_layer.objects.active = o
    for m in list(o.modifiers): bpy.ops.object.modifier_apply(modifier=m.name)
select(parts, parts[0]); bpy.ops.object.join(); body = bpy.context.view_layer.objects.active; body.name = 'body'
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
def tris_of(o): o.data.calc_loop_triangles(); return len(o.data.loop_triangles)
if tris_of(body) > MAX_TRIS:
    d = body.modifiers.new('dec', 'DECIMATE'); d.ratio = MAX_TRIS / tris_of(body); bpy.ops.object.modifier_apply(modifier='dec')

# ---- the wheel: the one on +X, its hub at the origin ----
wheel = max(wheels, key=lambda o: o.matrix_world.translation.x)
hub = wheel.matrix_world.translation.copy()
select([wheel], wheel); bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
wr = max(max(abs(v.co.y), abs(v.co.z)) for v in wheel.data.vertices)

# ---- ambient occlusion, baked onto the body's own UVs (made here) with the wheels in place ----
select([body], body); bpy.ops.object.mode_set(mode='EDIT'); bpy.ops.mesh.select_all(action='SELECT')
bpy.ops.uv.smart_project(angle_limit=math.radians(66), island_margin=0.02); bpy.ops.object.mode_set(mode='OBJECT')
img = bpy.data.images.new('ao', AO, AO, float_buffer=False)
for m in body.data.materials:
    if m is None: continue
    m.use_nodes = True; n = m.node_tree.nodes.new('ShaderNodeTexImage'); n.image = img; m.node_tree.nodes.active = n
sc = bpy.context.scene; sc.render.engine = 'CYCLES'; sc.cycles.device = 'CPU'; sc.cycles.samples = 96
sc.render.bake.margin = 4; sc.world = sc.world or bpy.data.worlds.new('w')
bpy.ops.object.bake(type='AO')
px = list(img.pixels)   # RGBA floats, bottom row first, like the UVs

# ---- materials -> the car shader's slots ----
def slot(m):
    n = (m.name if m else '').lower()
    if 'paint' in n: return 1
    if 'glass' in n: return 2
    if 'headlight' in n: return 3
    if 'taillight' in n or 'brake' in n: return 4
    if any(k in n for k in ('tyre', 'tire', 'trim', 'rubber')): return 5
    return 0
def colour(m):
    if not m or not m.use_nodes: return (0.5, 0.5, 0.5)
    b = next((x for x in m.node_tree.nodes if x.type == 'BSDF_PRINCIPLED'), None)
    c = b.inputs['Base Color'].default_value if b else (0.5, 0.5, 0.5, 1)
    k = 0.6 if b and b.inputs['Metallic'].default_value > 0.5 else 1.0   # (metal: the shader's sky reflection adds the rest)
    return (c[0] * k, c[1] * k, c[2] * k)

def dump(o, uvs):
    me = o.data; me.calc_loop_triangles(); uv = me.uv_layers.active.data if (uvs and me.uv_layers.active) else None
    vs, idx, seen = [], [], {}
    for t in me.loop_triangles:
        m = me.materials[t.material_index] if me.materials else None; s, c = slot(m), colour(m)
        for k in (0, 1, 2):   # (Blender -> game is a rotation, X, Z, -Y: counter-clockwise outside stays so, as the game wants)
            li = t.loops[k]; p = me.vertices[t.vertices[k]].co; n = t.split_normals[k]; u = uv[li].uv if uv else (0.0, 0.0)
            key = (round(p.x, 4), round(p.z, 4), round(-p.y, 4), round(n[0], 3), round(n[2], 3), round(-n[1], 3), round(u[0], 4), round(u[1], 4), s, c)
            if key not in seen: seen[key] = len(vs); vs.append(key)
            idx.append(seen[key])
    return vs, idx
bv, bi = dump(body, True); wv, wi = dump(wheel, False)

def floats(vs): return ',\n'.join(','.join('%.4g' % x for x in (*v[:8], *v[9], v[8])) for v in vs)
def ints(a): return ',\n'.join(','.join(str(x) for x in a[i:i + 24]) for i in range(0, len(a), 24))
ao = [max(0, min(255, int(px[(y * AO + x) * 4] * 255 + 0.5))) for y in range(AO) for x in range(AO)]
with open(OUT, 'w') as f:
    f.write('// model_%s.h - generated by tools/blender/fcf_model.py from %s; do not edit (re-run the conduit).\n' % (name, os.path.relpath(src, REPO) if src.startswith(REPO) else os.path.basename(src)))
    f.write('// Game axes (X, Y up, Z forward), metres. Vertex: position, normal, uv, colour, material (the car shader\'s slot).\n#pragma once\n')
    f.write('namespace model_%s {\nstatic const float WHEEL_R = %.4f, HUB[3] = { %.4f, %.4f, %.4f };\n' % (name, wr, hub.x, hub.z, -hub.y))
    f.write('static const int BODY_NV = %d, BODY_NI = %d, WHEEL_NV = %d, WHEEL_NI = %d, AO_N = %d;\n' % (len(bv), len(bi), len(wv), len(wi), AO))
    f.write('static const float BODY_V[] = {\n%s };\nstatic const unsigned BODY_I[] = {\n%s };\n' % (floats(bv), ints(bi)))
    f.write('static const float WHEEL_V[] = {\n%s };\nstatic const unsigned WHEEL_I[] = {\n%s };\n' % (floats(wv), ints(wi)))
    f.write('static const unsigned char AO[] = {\n%s };\n}\n' % ints(ao))
print('fcf_model: %s body %d verts %d tris, wheel %d verts %d tris (r %.3f m), ao %dx%d -> %s' % (name, len(bv), len(bi) // 3, len(wv), len(wi) // 3, wr, AO, AO, OUT))
