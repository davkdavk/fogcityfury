# ronin.py - an original early-70s Japanese compact (in the spirit of a 120Y / B210 sedan), modelled in code in Blender.
# No badges or names. Real size, metres, Z up, front facing -Y. Objects: body (paint, glass, trim, chrome, lights),
# wheel_fl / wheel_fr / wheel_rl / wheel_rr (origin at the hub, axle along X).
# Run: python3 ronin.py -- out.glb [out.blend]   (or into the game: python3 tools/blender/fcf_model.py -- models/ronin/ronin.py ronin)
import bpy, bmesh, math, sys
from mathutils import Vector

args = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []   # (none: build the scene only, as the conduit does)
bpy.ops.wm.read_factory_settings(use_empty=True)

# ---------------- materials ----------------
def mat(name, col, metal=0.0, rough=0.5, emit=None, alpha=1.0):
    m = bpy.data.materials.new(name); m.use_nodes = True; b = m.node_tree.nodes['Principled BSDF']
    b.inputs['Base Color'].default_value = (*col, 1); b.inputs['Metallic'].default_value = metal; b.inputs['Roughness'].default_value = rough
    if emit: b.inputs['Emission Color'].default_value = (*emit, 1); b.inputs['Emission Strength'].default_value = 1.0
    if alpha < 1: b.inputs['Alpha'].default_value = alpha; m.blend_method = 'BLEND' if hasattr(m, 'blend_method') else None
    return m
M = {'paint': mat('paint', (0.55, 0.06, 0.03), 0.3, 0.32), 'glass': mat('glass', (0.03, 0.05, 0.06), 0.0, 0.05, alpha=0.6),
     'trim': mat('trim', (0.02, 0.02, 0.022), 0.0, 0.6), 'chrome': mat('chrome', (0.85, 0.86, 0.88), 1.0, 0.12),
     'headlight': mat('headlight', (0.9, 0.9, 0.85), 0.0, 0.1, emit=(0.3, 0.3, 0.28)), 'taillight': mat('taillight', (0.6, 0.02, 0.02), 0.0, 0.2, emit=(0.15, 0, 0)),
     'indicator': mat('indicator', (0.9, 0.45, 0.02), 0.0, 0.2), 'tyre': mat('tyre', (0.025, 0.025, 0.028), 0.0, 0.85), 'rim': mat('rim', (0.22, 0.23, 0.24), 0.6, 0.4)}
MI = {k: i for i, k in enumerate(['paint', 'glass', 'trim', 'chrome', 'headlight', 'taillight', 'indicator'])}

# ---------------- body: lofted sections ----------------
L = 3.96; YF, YR = -L / 2, L / 2          # front at -Y
AXF, AXR, WR = -1.20, 1.14, 0.29          # axles (wheelbase 2.34 m), wheel radius (155/80 R13)
def lerp(a, b, t): return a + (b - a) * t
def smooth(e0, e1, x): t = min(max((x - e0) / (e1 - e0), 0), 1); return t * t * (3 - 2 * t)
def pw(y, pts):                            # piecewise-linear through (y, v) points
    for (y0, v0), (y1, v1) in zip(pts, pts[1:]):
        if y <= y1: return lerp(v0, v1, smooth(y0, y1, y) if False else (y - y0) / (y1 - y0))
    return pts[-1][1]
def zbot(y): return pw(y, [(YF, 0.36), (YF + 0.25, 0.26), (-0.9, 0.22), (0.9, 0.22), (YR - 0.35, 0.27), (YR, 0.40)])
def zbelt(y): return pw(y, [(YF, 0.70), (YF + 0.12, 0.79), (YF + 0.45, 0.83), (-0.62, 0.87), (0.40, 0.90), (0.62, 0.95), (0.80, 0.97), (YR - 0.10, 0.93), (YR, 0.86)])
def zdeck(y): return pw(y, [(YF, 0.71), (YF + 0.12, 0.80), (YF + 0.45, 0.845), (-0.62, 0.885), (0.62, 0.95), (1.12, 0.965), (YR - 0.08, 0.95), (YR, 0.88)])
def ztop(y): return pw(y, [(YF, zdeck(YF)), (-0.64, zdeck(-0.64)), (-0.12, 1.335), (0.10, 1.36), (0.55, 1.35), (0.70, 1.32), (1.14, 0.975), (YR, zdeck(YR))])
def wbody(y): return pw(y, [(YF, 0.62), (YF + 0.10, 0.71), (YF + 0.40, 0.75), (-0.5, 0.765), (0.5, 0.765), (YR - 0.40, 0.75), (YR - 0.10, 0.72), (YR, 0.66)])
def cabin(y): return smooth(-0.72, -0.40, y) * (1 - smooth(1.02, 1.22, y))   # 1 where there is a greenhouse

def ring(y):
    zb, zl, zt, w, c = zbot(y), zbelt(y), ztop(y), wbody(y), cabin(y)
    za = 0.0
    for ax in (AXF, AXR):
        d = abs(y - ax); Ra = 0.355
        if d < Ra: za = max(za, WR + 0.03 + math.sqrt(Ra * Ra - d * d))
    zd = zdeck(y); gb = w - 0.06
    # the greenhouse: straight pillars. t runs 0 -> 1 up the windscreen pillar, 1 along the roof, 1 -> 0 down the rear one,
    # and every point on a pillar moves linearly with it, so the pillar edges are straight lines, not blends
    YA0, YA1, YC0, YC1 = -0.66, -0.12, 0.70, 1.14
    if y <= YA0 or y >= YC1: t = 0.0
    elif y < YA1: t = (y - YA0) / (YA1 - YA0)
    elif y <= YC0: t = 1.0
    else: t = (YC1 - y) / (YC1 - YC0)
    zbase = zdeck(YA0) if y < 0.3 else zdeck(YC1)
    zb6, zb7 = (zd + 0.006, zd + 0.01) if t == 0 else (zbase + 0.006, zbase + 0.01)
    zt = lerp(zd + 0.012, 1.36, t) if t > 0 else zd + 0.012
    zm = lerp(zb, zl, 0.55)
    right = [(w - 0.10, max(zb, za)), (w - 0.005, max(zb + 0.10, za + 0.01)), (w, max(zm, min(za + 0.03, zl - 0.09))), (w - 0.012, zl - 0.03),   # p1..p4
             (gb, max(zl + 0.012, zd)),                                                             # p5 window base
             (lerp(gb - 0.02, 0.615, t), lerp(zb6, 1.295, t)),                                       # p6 window top: a straight pillar edge
             (lerp(gb - 0.12, 0.545, t), lerp(zb7, 1.340, t))]                                       # p7 roof edge: straight too
    pts = [(0.0, zb - 0.01)] + right + [(0.0, max(zt, (zbase if t > 0 else zd) + 0.012))] + [(-x, z) for x, z in reversed(right)]
    return [Vector((x, y, z)) for x, z in pts]

NS = 84; ys = [lerp(YF, YR, (i / (NS - 1))) for i in range(NS)]
bm = bmesh.new(); rings = []
for y in ys: rings.append([bm.verts.new(p) for p in ring(y)])
NR = len(rings[0])
SEG = {0: 'under', 1: 'side', 2: 'side', 3: 'side', 4: 'shoulder', 5: 'win', 6: 'edge', 7: 'roof'}   # right half: segment k = p_k -> p_k+1
def seg_kind(j): return SEG[j] if j < 8 else SEG[NR - 1 - j]
for i in range(NS - 1):
    y = (ys[i] + ys[i + 1]) / 2
    for j in range(NR):
        a, b, c, d = rings[i][j], rings[i][(j + 1) % NR], rings[i + 1][(j + 1) % NR], rings[i + 1][j]
        f = bm.faces.new((a, b, c, d)); k = seg_kind(j); m = 'paint'
        if k == 'under': m = 'trim'
        elif k == 'win' and -0.40 < y < 0.98 and not (0.36 < y < 0.45): m = 'glass'      # side windows, B-pillar
        elif k == 'roof' and (-0.62 < y < -0.15 or 0.73 < y < 1.11): m = 'glass'                              # windscreen, rear window
        f.material_index = MI[m]
for rr, sgn in ((rings[0], -1), (rings[-1], 1)):                                                              # nose and tail caps
    cen = sum((v.co for v in rr), Vector()) / NR; c = bm.verts.new(cen + Vector((0, sgn * 0.004, 0)))
    for j in range(NR):
        f = bm.faces.new((rr[j], rr[(j + 1) % NR], c) if sgn > 0 else (rr[(j + 1) % NR], rr[j], c)); f.material_index = MI['paint']
CR = {1: 0.6, 4: 0.85, 5: 0.9, 6: 0.9, 7: 0.85}   # right-half point index -> crease of the line it draws along the car
cl = bm.edges.layers.float.get('crease_edge') or bm.edges.layers.float.new('crease_edge')
for i in range(NS - 1):
    for j, v in CR.items():
        for jj in (j, NR - j):
            e = bm.edges.get((rings[i][jj], rings[i + 1][jj]))
            if e: e[cl] = v
for rr in (rings[0], rings[-1]):
    for j in range(NR):
        e = bm.edges.get((rr[j], rr[(j + 1) % NR]))
        if e: e[cl] = 0.95
bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
me = bpy.data.meshes.new('body'); bm.to_mesh(me); bm.free()
body = bpy.data.objects.new('body', me); bpy.context.scene.collection.objects.link(body)
for k in MI: me.materials.append(M[k])
for p in me.polygons: p.use_smooth = True
sub = body.modifiers.new('sub', 'SUBSURF'); sub.levels = sub.render_levels = 1
bpy.context.view_layer.objects.active = body; bpy.ops.object.modifier_apply(modifier='sub')

def add(obj, name=None):
    bpy.context.scene.collection.objects.link(obj) if obj.name not in bpy.context.scene.collection.objects else None; return obj
def prim(kind, loc, scale=(1, 1, 1), rot=(0, 0, 0), m='chrome', **kw):
    getattr(bpy.ops.mesh, 'primitive_%s_add' % kind)(location=loc, rotation=rot, **kw); o = bpy.context.active_object; o.scale = scale
    o.data.materials.append(M[m]); bpy.ops.object.transform_apply(scale=True, rotation=True, location=False); return o

for p in body.data.polygons: p.use_smooth = True

parts = []
def keep(o): parts.append(o); return o
# bumpers: chrome bars that wrap the corners
for yb, sg in ((YF - 0.035, -1), (YR + 0.035, 1)):
    b = keep(prim('cube', (0, yb, 0.44), scale=(0.78, 0.045, 0.042), m='chrome'))
    bv = b.modifiers.new('bv', 'BEVEL'); bv.width = 0.04; bv.segments = 3
    for sx in (-1, 1):
        c = keep(prim('cube', (sx * 0.76, yb - sg * 0.16, 0.44), scale=(0.035, 0.17, 0.04), m='chrome')); c.modifiers.new('bv', 'BEVEL').width = 0.02
# grille: dark panel with three chrome bars, between the headlights
keep(prim('cube', (0, YF - 0.006, 0.585), scale=(0.40, 0.012, 0.085), m='trim'))
for k in range(3): keep(prim('cube', (0, YF - 0.016, 0.53 + k * 0.055), scale=(0.40, 0.008, 0.007), m='chrome'))
keep(prim('cube', (0, YF - 0.012, 0.675), scale=(0.58, 0.01, 0.01), m='chrome'))
# headlights: one round lamp each side in a chrome bezel; indicators under them
for sx in (-1, 1):
    keep(prim('cylinder', (sx * 0.52, YF - 0.012, 0.585), rot=(math.pi / 2, 0, 0), m='headlight', radius=0.085, depth=0.05, vertices=24))
    keep(prim('torus', (sx * 0.52, YF - 0.022, 0.585), rot=(math.pi / 2, 0, 0), m='chrome', major_radius=0.092, minor_radius=0.014, major_segments=24, minor_segments=6))
    keep(prim('cube', (sx * 0.56, YF - 0.012, 0.48), scale=(0.07, 0.012, 0.022), m='indicator'))
# tail: wide rectangular lamps at the corners, a chrome strip between, the number-plate recess
for sx in (-1, 1):
    keep(prim('cube', (sx * 0.47, YR + 0.016, 0.72), scale=(0.17, 0.012, 0.05), m='taillight'))
    keep(prim('cube', (sx * 0.47, YR + 0.008, 0.72), scale=(0.18, 0.012, 0.058), m='chrome'))
    keep(prim('cube', (sx * 0.605, YR + 0.016, 0.72), scale=(0.035, 0.012, 0.05), m='indicator'))
keep(prim('cube', (0, YR + 0.012, 0.72), scale=(0.28, 0.008, 0.03), m='trim'))
keep(prim('cube', (0, YR + 0.01, 0.56), scale=(0.18, 0.008, 0.06), m='trim'))
# side: a chrome rub strip, door handles, the window frames, fender mirrors on stalks
for sx in (-1, 1):
    keep(prim('cube', (sx * 0.767, (AXF + AXR) / 2, 0.58), scale=(0.006, (AXR - AXF) / 2 - 0.37, 0.008), m='chrome'))
    for hy in (-0.08, 0.62): keep(prim('cube', (sx * 0.772, hy, 0.82), scale=(0.008, 0.06, 0.008), m='chrome'))
    # door mirror: a chrome bullet on a short arm at the front corner of the front window, glass facing back
    my = -0.36; base = Vector((sx * 0.72, my, 0.915)); head = Vector((sx * 0.84, my + 0.02, 0.965))
    arm = keep(prim('cylinder', (base + head) / 2, m='chrome', radius=0.011, depth=(head - base).length, vertices=8))
    arm.rotation_mode = 'QUATERNION'; arm.rotation_quaternion = Vector((0, 0, 1)).rotation_difference(head - base)
    bpy.context.view_layer.objects.active = arm; bpy.ops.object.transform_apply(rotation=True, location=False)
    keep(prim('uv_sphere', tuple(head), scale=(0.075, 0.05, 0.048), m='chrome', segments=14, ring_count=8, radius=1.0))
    keep(prim('cylinder', (head.x, head.y + 0.045, head.z), rot=(math.pi / 2, 0, 0), scale=(1.45, 1, 1), m='glass', radius=0.042, depth=0.008, vertices=14))
# wipers and a little chrome on the bonnet's leading edge
for sx in (-0.25, 0.20): keep(prim('cube', (sx, -0.62, 0.895), scale=(0.22, 0.006, 0.004), rot=(0, 0, 0.12), m='trim'))

# join the trim into the body (one object, many materials), then smooth shading
for o in parts:
    for mo in list(o.modifiers): bpy.context.view_layer.objects.active = o; bpy.ops.object.modifier_apply(modifier=mo.name)
bpy.ops.object.select_all(action='DESELECT')
for o in parts: o.select_set(True)
body.select_set(True); bpy.context.view_layer.objects.active = body; bpy.ops.object.join()

# ---------------- wheels: tyre, steel rim, chrome hubcap ----------------
def wheel(name, x, y):
    bm = bmesh.new(); N = 28
    prof = [(-0.078, 0.20, 'tyre'), (-0.080, 0.26, 'tyre'), (-0.065, 0.29, 'tyre'), (0.065, 0.29, 'tyre'), (0.080, 0.26, 'tyre'), (0.078, 0.20, 'tyre'),
            (0.060, 0.165, 'rim'), (0.050, 0.10, 'rim'), (0.058, 0.0, 'rim')]
    mats = ['tyre', 'rim', 'chrome']
    rings = [[bm.verts.new((px, math.cos(2 * math.pi * i / N) * r, math.sin(2 * math.pi * i / N) * r)) for i in range(N)] for px, r, _ in prof]
    for k in range(len(prof) - 1):
        for i in range(N):
            f = bm.faces.new((rings[k][i], rings[k][(i + 1) % N], rings[k + 1][(i + 1) % N], rings[k + 1][i])); f.material_index = mats.index(prof[k + 1][2])
    inner = [bm.verts.new((-0.06, math.cos(2 * math.pi * i / N) * 0.20, math.sin(2 * math.pi * i / N) * 0.20)) for i in range(N)]
    for i in range(N): f = bm.faces.new((rings[0][(i + 1) % N], rings[0][i], inner[i], inner[(i + 1) % N])); f.material_index = 1
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    for m in mats: me.materials.append(M[m])
    o = bpy.data.objects.new(name, me); bpy.context.scene.collection.objects.link(o)
    for p in me.polygons: p.use_smooth = True
    # the hubcap: a shallow chrome dome with eight vents
    bpy.ops.mesh.primitive_uv_sphere_add(segments=16, ring_count=8, radius=0.075, location=(0, 0, 0)); cap = bpy.context.active_object
    cap.scale = (0.25, 1, 1); cap.data.materials.append(M['chrome']); bpy.ops.object.transform_apply(scale=True)
    cap.location = (0.055, 0, 0)
    bpy.ops.object.select_all(action='DESELECT'); cap.select_set(True); o.select_set(True); bpy.context.view_layer.objects.active = o; bpy.ops.object.join()
    if x < 0: o.scale = (-1, 1, 1); bpy.ops.object.transform_apply(scale=True)   # the outer face points outward on both sides
    bpy.ops.mesh.normals_make_consistent if False else None
    o.location = (x, y, WR); return o
TW = 0.635
for nm, x, y in (('wheel_fl', -TW, AXF), ('wheel_fr', TW, AXF), ('wheel_rl', -TW, AXR), ('wheel_rr', TW, AXR)): wheel(nm, x, y)
for o in bpy.data.objects:
    if o.type == 'MESH':
        bpy.context.view_layer.objects.active = o; o.select_set(True)
bpy.ops.object.select_all(action='DESELECT')
for o in bpy.data.objects:
    if o.type == 'MESH':
        bpy.context.view_layer.objects.active = o; bpy.ops.object.mode_set(mode='EDIT'); bpy.ops.mesh.select_all(action='SELECT'); bpy.ops.mesh.normals_make_consistent(inside=False); bpy.ops.object.mode_set(mode='OBJECT')

tris = 0
for o in bpy.data.objects:
    if o.type == 'MESH': o.data.calc_loop_triangles(); tris += len(o.data.loop_triangles)
print('RONIN tris=%d objects=%s' % (tris, sorted(o.name for o in bpy.data.objects)))
if args: bpy.ops.export_scene.gltf(filepath=args[0], export_format='GLB', export_apply=True)
if len(args) > 1: bpy.ops.wm.save_as_mainfile(filepath=args[1])
