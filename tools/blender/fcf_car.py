# fcf_car.py - brings any car model into Fog City Fury: one command, no hand edits.
#
#   python3 tools/blender/fcf_car.py -- <model> <name> [options]
#
# <model>: a .blend, .glb / .gltf, .fbx, .obj, .dae, .usd(z/a/c), .abc, .ply, .stl, or a .zip holding one of them
#          (the best one inside is used: .blend, then .glb, .gltf, .fbx, .obj, ...). <name>: short, a-z 0-9 _ (the
#          files are models/<name>/ and src/model_<name>.h).
#
# It works out for itself what the conduit (fcf_model.py) needs drawn by hand:
#   - up, the length of the car and which end is the front (from the wheels and the names of parts and materials:
#     head/tail lights, exhaust, bonnet, boot, front/rear wheels...); the scale (cm, mm, inches: it lands near a car's size)
#   - the four wheels (by name: wheel, tyre, tire, rim...; else by shape: four discs at the corners, low down), the
#     front right one kept as the game's wheel, the other three taken out (the game draws all four from that one)
#   - the materials: paint (takes the player's colour), glass, head and tail lights (one "lights" material is split by
#     which end of the car each part is at), tyres; anything else keeps its colour (a texture: its average colour)
#   - ground planes, collision meshes, shadow cards, lower levels of detail, the engine and the driver: left out
#   - the budget: the body cut to --tris triangles (20000: kind to the Xbox 360), the wheel to --wheel-tris
# Then it bakes the soft shadows (ambient occlusion), writes src/model_<name>.h, models/<name>/car.json (its name, blurb,
# unlock code, handling and sizes), two preview pictures (models/<name>/preview_front.png, preview_rear.png: check the
# front is the front and the paint is the paint), and src/cars_extra.h + src/cars_extra_spec.inc, which add every car in
# models/*/car.json to the game. Rebuild the game and it is there.
#
# Options:
#   --title "CUBANO GT"      its name in the game (default: <name> in capitals)
#   --blurb "WIDE AND LOW"   the line under it on the car screen
#   --code CUBANO            hidden until this is typed on the main menu (letters and digits; none: there from the start)
#   --like 1                 handling taken from car 0-4 (STALLION GT, KATANA RS, BRUISER 4X4, RIPDOG, RONIN 120; or its name)
#   --top 70 --acc 15 --turn 2.4 --grip 8 --drive rwd|fwd|awd    handling changes on top of --like
#   --front auto|-y|+y|-x|+x where the front is in the file, if auto guesses wrong (the previews show it)
#   --length 4.3             its real length in metres, if the file's units are odd
#   --tris 20000 --wheel-tris 2000 --ao 256
#   --paint NAME[,NAME]      materials that are the paint, if it guesses wrong (likewise --glass, --tyre, --headlight, --taillight)
#   --drop WORD[,WORD]       also leave out objects or materials whose names have these in them
#   --keep WORD[,WORD]       keep these even if a rule would leave them out (e.g. --keep engine)
#   --licence "CC-BY 4.0, author"   recorded in car.json and models/README.md's table
#   --remove                 take <name> out of the game again (its folder and header go; the list is written again)
#   --list                   the cars added so far
#
# Needs Blender as a Python module (pip install bpy), here at build time; nothing is read from disk by the game.
import sys, os, re, math, json, zipfile, tempfile, shutil

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
MODELS, SRC = os.path.join(REPO, 'models'), os.path.join(REPO, 'src')
BASE = ['STALLION GT', 'KATANA RS', 'BRUISER 4X4', 'RIPDOG', 'RONIN 120']
# handling of the five built-in cars (carspec.h): top, acc, turn, grip, offroad, snd, stat[4], gF, gR, k2, drv, trac
BASE_H = [
    (66, 13, 2.3, 8.5, 1.0, 1.0, [7, 6, 7, 5], 14.3, 17.4, 2.3, 1.0, 0.98),
    (75, 15, 2.65, 6.8, 1.4, 1.38, [10, 8, 6, 2], 16.1, 18.0, 1.8, 1.0, 1.35),
    (58, 14, 2.1, 10.0, 0.3, 0.70, [5, 7, 8, 10], 13.0, 15.6, 2.9, 0.5, 1.30),
    (70, 15.5, 2.35, 8.0, 0.65, 0.85, [8, 9, 6, 7], 14.8, 17.2, 2.5, 1.0, 1.12),
    (63, 14.5, 2.55, 8.2, 0.85, 1.25, [6, 8, 9, 5], 14.6, 15.9, 1.75, 1.0, 1.0)]
GAME_SCALE = 1.12   # the game's cars are a little bigger than life (model.h)

def die(msg): print('fcf_car: ' + msg); sys.exit(1)

# ---- the list of added cars: src/cars_extra.h and src/cars_extra_spec.inc, from models/*/car.json (no Blender needed) ----
def cars():
    out = []
    if os.path.isdir(MODELS):
        for d in sorted(os.listdir(MODELS)):
            f = os.path.join(MODELS, d, 'car.json')
            if os.path.isfile(f) and os.path.isfile(os.path.join(SRC, 'model_%s.h' % d)): out.append(json.load(open(f)))
    return out

def cstr(s): return '"' + s.replace('\\', '\\\\').replace('"', '\\"') + '"'

def write_registry():
    cs = cars()
    with open(os.path.join(SRC, 'cars_extra_spec.inc'), 'w') as f:
        f.write('// cars_extra_spec.inc - written by tools/blender/fcf_car.py from models/*/car.json; do not edit. The cars added\n'
                '// with fcf_car.py, after the five built-in ones in SPECS (carspec.h).\n')
        for c in cs:
            h, s = c['handling'], c['spec']
            fl = lambda x: ('%.4f' % float(x)).rstrip('0').rstrip('.') + ('.0f' if float(x) == int(float(x)) else 'f')   # (C++: 1.0f, never 1f)
            f.write('{ %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, { %d, %d, %d, %d }, %s, %s, %s, %s, %s },\n' % (
                cstr(c['title']), cstr(c['blurb']), *[fl(x) for x in (h['top'], h['acc'], h['turn'], h['grip'], h['offroad'], s['wr'], s['wbF'], s['wbR'], s['tw'], h['snd'])],
                *h['stat'], *[fl(x) for x in (h['gF'], h['gR'], h['k2'], h['drv'], h['trac'])]))
    with open(os.path.join(SRC, 'cars_extra.h'), 'w') as f:
        f.write('// cars_extra.h - written by tools/blender/fcf_car.py from models/*/car.json; do not edit. The added cars\' models\n'
                '// and unlock codes (an empty code: there from the start). Included by model.h.\n#pragma once\n')
        for c in cs: f.write('#include "model_%s.h"\n' % c['name'])
        for c in cs:
            n = c['name']
            f.write('static const CarModel EXTRA_%s = { model_%s::BODY_V, model_%s::BODY_NV, model_%s::BODY_I, model_%s::BODY_NI,\n'
                    '    model_%s::WHEEL_V, model_%s::WHEEL_NV, model_%s::WHEEL_I, model_%s::WHEEL_NI, model_%s::WHEEL_R, model_%s::AO, model_%s::AO_N, %gf };\n'
                    % ((n,) * 12 + (GAME_SCALE,)))
        f.write('static const int NEXTRA = %d;\n' % len(cs))
        # (both end with nullptr, so neither is ever empty)
        f.write('static const CarModel* const EXTRA_MODELS[] = { %snullptr };\n' % ''.join('&EXTRA_%s, ' % c['name'] for c in cs))
        f.write('static const char* const EXTRA_CODES[] = { %snullptr };\n' % ''.join(cstr(c.get('code', '')) + ', ' for c in cs))
    # models/README.md: the table of added cars
    rd = os.path.join(MODELS, 'README.md')
    if os.path.isfile(rd):
        t = open(rd).read(); a, b = '<!-- fcf_car: added cars -->', '<!-- /fcf_car -->'
        rows = '\n'.join('| %s (`%s`)%s | `%s` | %s body, %s wheel | %s |' % (c['title'], c['name'], ' code `%s`' % c['code'] if c.get('code') else '',
                         c.get('source', ''), c.get('bodyTris', '?'), c.get('wheelTris', '?'), c.get('licence', '') or 'not recorded') for c in cs)
        block = a + '\n\n| Added car | Source | Triangles | Licence |\n| --- | --- | --- | --- |\n' + (rows or '| (none yet) | | | |') + '\n\n' + b
        t = t[:t.index(a)] + block + t[t.index(b) + len(b):] if a in t and b in t else t.rstrip() + '\n\n' + block + '\n'
        open(rd, 'w').write(t)
    return cs

args = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else sys.argv[1:]
flags = {'--remove', '--list', '--registry'}
pos, opt, i = [], {}, 0
while i < len(args):
    a = args[i]
    if a in flags: opt[a] = True; i += 1
    elif a.startswith('--'):
        if i + 1 >= len(args): die('%s needs a value' % a)
        opt[a] = args[i + 1]; i += 2
    else: pos.append(a); i += 1

if '--list' in opt:
    for c in cars(): print('%-12s %-20s code %-10s like %s  %s' % (c['name'], c['title'], c.get('code') or '-', c.get('like'), c.get('source', '')))
    sys.exit(0)
if '--registry' in opt: write_registry(); print('fcf_car: list written (%d cars)' % len(cars())); sys.exit(0)
if '--remove' in opt:
    if not pos: die('--remove needs the name')
    n = pos[0]; shutil.rmtree(os.path.join(MODELS, n), ignore_errors=True)
    if os.path.isfile(os.path.join(SRC, 'model_%s.h' % n)): os.remove(os.path.join(SRC, 'model_%s.h' % n))
    write_registry(); print('fcf_car: %s removed' % n); sys.exit(0)
if len(pos) < 2: die('usage: fcf_car.py -- <model> <name> [options] (see the top of this file)')

src, name = os.path.abspath(pos[0]), pos[1].lower()
if not re.fullmatch(r'[a-z][a-z0-9_]{0,23}', name): die('the name: a letter, then letters, digits or _ (up to 24)')
if name in ('ronin', 'dolphin'): die('%s is taken' % name)
if not os.path.exists(src): die('no such file: ' + src)
def font(t): return re.sub(r' +', ' ', re.sub(r"[^A-Z0-9 .,!?'&:+/-]", ' ', t.upper())).strip()   # (what the game's font has)
title = font(opt.get('--title', name.replace('_', ' ')))[:20]
blurb = font(opt.get('--blurb', ''))[:34]
code = re.sub(r'[^A-Z0-9]', '', opt.get('--code', '').upper())[:12]
like = opt.get('--like', '0')
like = int(like) if like.isdigit() else next((k for k, b in enumerate(BASE) if b.replace(' ', '').lower() == like.replace(' ', '').lower()), None)
if like is None or not 0 <= like < 5: die('--like: 0-4 or one of ' + ', '.join(BASE))
MAX_TRIS, WHEEL_TRIS, AO = int(opt.get('--tris', 20000)), int(opt.get('--wheel-tris', 2000)), int(opt.get('--ao', 256))
def words(k): return [w.strip().lower() for w in opt.get(k, '').split(',') if w.strip()]
for c in cars():
    if c['name'] != name and code and c.get('code') == code: die('code %s is %s\'s already' % (code, c['name']))

import bpy, bmesh
from mathutils import Vector, Matrix
try: import numpy as np
except ImportError: np = None

def log(*a): print('fcf_car:', *a); sys.stdout.flush()

# ---- load ----
ORDER = ['.blend', '.glb', '.gltf', '.fbx', '.obj', '.dae', '.usdz', '.usd', '.usdc', '.usda', '.abc', '.ply', '.stl']
tmpdir = None
if src.lower().endswith('.zip'):
    tmpdir = tempfile.mkdtemp(prefix='fcf_car_'); zipfile.ZipFile(src).extractall(tmpdir)
    found = []
    for root, _, fs in os.walk(tmpdir):
        for f in fs:
            e = os.path.splitext(f)[1].lower()
            if e in ORDER and not f.startswith('._'): found.append((ORDER.index(e), -os.path.getsize(os.path.join(root, f)), os.path.join(root, f)))
            elif e == '.zip':   # a zip in the zip (textures, often): opened beside it
                try: zipfile.ZipFile(os.path.join(root, f)).extractall(root)
                except Exception: pass
    if not found: die('nothing in the zip that Blender reads')
    found.sort(); src_used = found[0][2]; log('from the zip:', os.path.basename(src_used))
else: src_used = src
ext = os.path.splitext(src_used)[1].lower()
if ext == '.blend': bpy.ops.wm.open_mainfile(filepath=src_used)
elif ext == '.py': sys.argv = [src_used, '--']; exec(compile(open(src_used).read(), src_used, 'exec'), {'__name__': '__main__'})
else:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    try:
        if ext in ('.glb', '.gltf'): bpy.ops.import_scene.gltf(filepath=src_used)
        elif ext == '.fbx': bpy.ops.import_scene.fbx(filepath=src_used)
        elif ext == '.obj': bpy.ops.wm.obj_import(filepath=src_used)
        elif ext == '.dae': bpy.ops.wm.collada_import(filepath=src_used)
        elif ext.startswith('.usd'): bpy.ops.wm.usd_import(filepath=src_used)
        elif ext == '.abc': bpy.ops.wm.alembic_import(filepath=src_used)
        elif ext == '.ply': bpy.ops.wm.ply_import(filepath=src_used)
        elif ext == '.stl': bpy.ops.wm.stl_import(filepath=src_used)
        else: die('cannot read ' + ext)
    except Exception as e: die('Blender could not read %s (%s); try the .fbx, .glb or .obj of it' % (os.path.basename(src_used), str(e).splitlines()[0] if str(e) else e))
scene = bpy.context.scene
if bpy.context.object and bpy.context.object.mode != 'OBJECT': bpy.ops.object.mode_set(mode='OBJECT')

# ---- everything as plain meshes in world space (modifiers, rigs, instances and curves applied) ----
dg = bpy.context.evaluated_depsgraph_get()
items = []   # (name, mesh)
for inst in dg.object_instances:
    o = inst.object
    if o.type not in ('MESH', 'CURVE', 'SURFACE', 'META', 'FONT'): continue
    orig = o.original if not inst.is_instance else inst.instance_object.original
    if orig.hide_render or (not inst.is_instance and not orig.visible_get()): continue
    try: me = bpy.data.meshes.new_from_object(o, preserve_all_data_layers=False, depsgraph=dg)
    except Exception: continue
    if not me.polygons: bpy.data.meshes.remove(me); continue
    me.transform(inst.matrix_world.copy()); items.append((orig.name, me))
for o in list(scene.objects): bpy.data.objects.remove(o, do_unlink=True)
if not items: die('no meshes in the file')
objs = []
for n, me in items:
    o = bpy.data.objects.new(n, me); scene.collection.objects.link(o); objs.append(o)
log('%d parts, %d triangles' % (len(objs), sum(sum(len(p.vertices) - 2 for p in o.data.polygons) for o in objs)))

def mat_names(o): return [m.name.lower() for m in o.data.materials if m]
def has(text, ws): return any(w in text for w in ws)
KEEP = words('--keep')
def bbox(o):
    vs = o.data.vertices; n = len(vs)
    if np is not None:
        a = np.empty(n * 3, np.float32); vs.foreach_get('co', a); a = a.reshape(-1, 3); return Vector(a.min(0)), Vector(a.max(0))
    xs = [v.co for v in vs]; return Vector([min(c[i] for c in xs) for i in range(3)]), Vector([max(c[i] for c in xs) for i in range(3)])
def bbox_all(os_):
    bs = [bbox(o) for o in os_]; return Vector([min(b[0][i] for b in bs) for i in range(3)]), Vector([max(b[1][i] for b in bs) for i in range(3)])

# ---- what to leave out ----
DROP = ['collision', 'collider', 'ucx_', 'ubx_', 'ups_', 'usp_', 'shadow', 'proxy', 'damage', 'broken', 'dummy', 'helper',
        'backdrop', 'studio', 'turntable', 'ground', 'floor', 'engine', 'driver', 'helmet', 'occluder', 'decal_proj'] + words('--drop')
def drop_name(o):
    t = o.name.lower()
    if KEEP and has(t, KEEP): return False
    return has(t, DROP) or re.fullmatch(r'plane(\.\d+)?', t) is not None
lods = {}
for o in objs:
    m = re.search(r'lod[_ ]?([0-9a-f])\b|lod[_ ]?([0-9a-f])(?=[_.\s]|$)', o.name.lower())
    if m: lods.setdefault((m.group(1) or m.group(2)), []).append(o)
gone = [o for o in objs if drop_name(o)]
if len(lods) > 1:   # several levels of detail: only the finest
    best = min(lods, key=lambda k: int(k, 16)); gone += [o for k, v in lods.items() if k != best for o in v]
for o in set(gone): objs.remove(o); bpy.data.objects.remove(o, do_unlink=True)
# a big flat thing round the car (a floor, a backdrop): out
for _ in range(2):
    lo, hi = bbox_all(objs); ext_ = hi - lo
    for o in list(objs):
        a, b = bbox(o); e = b - a; k = sorted(range(3), key=lambda i: e[i])
        if e[k[0]] < 0.02 * max(ext_) and e[k[1]] > 0.85 * ext_[k[1]] and e[k[2]] > 0.85 * ext_[k[2]] and len(objs) > 1 and not (KEEP and has(o.name.lower(), KEEP)):
            objs.remove(o); bpy.data.objects.remove(o, do_unlink=True)
if gone: log('left out %d parts (collision, shadow, lower detail, engine, driver...; --keep WORD to keep some)' % len(set(gone)))

def xform(objs_, M):
    for o in objs_: o.data.transform(M)

# ---- the scale: a car is 2.5 - 7.5 m long ----
lo, hi = bbox_all(objs); L0 = max(hi - lo)
if '--length' in opt: k = float(opt['--length']) / L0
else:
    k = 1.0
    if not 2.5 <= L0 <= 7.5:
        k = min([0.001, 0.01, 0.0254, 0.1, 0.3048, 1, 10, 100, 1000], key=lambda f: abs(math.log(L0 * f / 4.3)))
if abs(k - 1) > 1e-6: xform(objs, Matrix.Scale(k, 4)); log('scaled by %g (%.2f m long)' % (k, L0 * k))

# ---- up and along: Z up (Blender's importers turn files to that); the length along Y ----
lo, hi = bbox_all(objs); e = hi - lo
if e.z > max(e.x, e.y):   # standing on its end: the smallest extent is up
    up = min(range(3), key=lambda i: e[i]); xform(objs, Matrix.Rotation(math.pi / 2, 4, 'X' if up == 1 else 'Y')); lo, hi = bbox_all(objs); e = hi - lo
if e.x > e.y: xform(objs, Matrix.Rotation(math.pi / 2, 4, 'Z')); lo, hi = bbox_all(objs); e = hi - lo
L, Wd, H = e.y, e.x, e.z
xform(objs, Matrix.Translation(-Vector(((lo.x + hi.x) / 2, (lo.y + hi.y) / 2, lo.z))))

# ---- the wheels ----
WHEEL_W = ['wheel', 'tyre', 'tire', 'rim', 'reifen', 'felge', 'roue', 'pneu', 'rueda', 'llanta', 'ruota', 'cerchio']
NOT_WHEEL = ['steer', 'arch', 'well', 'house', 'cover', 'spare', 'wheelbase', 'lenk', 'trim_', 'mudflap', 'flap', 'fender', 'guard', 'caliper', 'calliper', 'brake']
def wheelish(o):
    t = o.name.lower(); ms = ' '.join(mat_names(o))
    return (has(t, WHEEL_W) or (has(ms, WHEEL_W) and not has(ms, ['body', 'paint']))) and not has(t, NOT_WHEEL)

def parts_of(o):   # loose parts: lists of vertex indices (union-find over edges)
    me = o.data; n = len(me.vertices); par = list(range(n))
    def f(a):
        while par[a] != a: par[a] = par[par[a]]; a = par[a]
        return a
    for ed in me.edges:
        a, b = f(ed.vertices[0]), f(ed.vertices[1])
        if a != b: par[a] = b
    g = {}
    for v in range(n): g.setdefault(f(v), []).append(v)
    return list(g.values())

def split(o, vsets):   # new objects from these parts of o; the faces go from o
    me = o.data; bm = bmesh.new(); bm.from_mesh(me); bm.verts.ensure_lookup_table(); out = []
    for vs in vsets:
        s = set(vs); tmp = bm.copy(); tmp.verts.ensure_lookup_table()   # a copy of just these parts
        bmesh.ops.delete(tmp, geom=[v for v in tmp.verts if v.index not in s], context='VERTS')
        nme = bpy.data.meshes.new(o.name + '_part'); tmp.to_mesh(nme); tmp.free()
        for m in me.materials: nme.materials.append(m)
        no = bpy.data.objects.new(o.name + '_part', nme); scene.collection.objects.link(no); out.append(no)
    allv = set(v for vs in vsets for v in vs)
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if v.index in allv], context='VERTS'); bm.to_mesh(me); bm.free()
    return out

def disc(a, b):   # bbox of a wheel-shaped thing: a disc on X, low, out at a corner
    e = b - a; c = (a + b) / 2; d = max(e.y, e.z)
    return (0.10 * L < d < 0.32 * L and min(e.y, e.z) > 0.75 * d and e.x < 0.75 * d and c.z < 0.45 * H and abs(c.x) > 0.18 * Wd and abs(c.y) > 0.12 * L)

named = [o for o in objs if wheelish(o)]
cands = []   # (object, bbox lo, bbox hi): one per wheel-ish thing
if named:
    for o in named:
        a, b = bbox(o); e = b - a
        if e.x > 0.5 * Wd or e.y > 0.5 * L:   # several wheels in one object: split into its parts
            ps = parts_of(o); groups = {}
            for vs in ps:
                co = [o.data.vertices[v].co for v in vs]; c = sum(co, Vector()) / len(co)
                groups.setdefault((c.x > 0, c.y > 0), []).extend(vs)
            for no in split(o, list(groups.values())): objs.append(no); cands.append(no)
            objs.remove(o); bpy.data.objects.remove(o, do_unlink=True)
        else: cands.append(o)
else:   # by shape: discs at the corners, in any part
    for o in list(objs):
        me = o.data
        if len(me.vertices) < 12: continue
        ps = parts_of(o); hits = []
        for vs in ps:
            if len(vs) < 12: continue
            if np is not None:
                a = np.array([me.vertices[v].co for v in vs]); lo_, hi_ = Vector(a.min(0)), Vector(a.max(0))
            else:
                co = [me.vertices[v].co for v in vs]; lo_ = Vector([min(c[i] for c in co) for i in range(3)]); hi_ = Vector([max(c[i] for c in co) for i in range(3)])
            if disc(lo_, hi_): hits.append(vs)
        if hits:
            if len(hits) == len(ps): cands.append(o)
            else:
                for no in split(o, hits): objs.append(no); cands.append(no)
# group by corner; the biggest disc at each corner sets its hub; smaller things inside it (rim, hub, nuts) go with it
corners = {}
for o in cands:
    a, b = bbox(o); c = (a + b) / 2; corners.setdefault((c.x > 0, c.y > 0), []).append((o, a, b))
if len(corners) < 4:
    die('could not find four wheels (%d corners found). Name the wheel objects wheel_fl, wheel_fr, wheel_rl, wheel_rr (each '
        'with its rim and tyre) in the file, or check --length.' % len(corners))
hubs, wheel_sets = {}, {}
for key, lst in corners.items():
    big = max(lst, key=lambda t: max((t[2] - t[1]).y, (t[2] - t[1]).z)); a, b = big[1], big[2]
    for t in lst: a = Vector([min(a[i], t[1][i]) for i in range(3)]); b = Vector([max(b[i], t[2][i]) for i in range(3)])
    hubs[key] = ((a + b) / 2, max(b.y - a.y, b.z - a.z) / 2); wheel_sets[key] = [t[0] for t in lst]
# loose bits inside a wheel that were not named (a hub cap in the body object...) stay with the body: fine, they don't spin
log('wheels: radius %.3f m, hubs %s' % (sum(h[1] for h in hubs.values()) / 4, ' '.join('(%.2f %.2f %.2f)' % tuple(h[0]) for h in hubs.values())))
for ws in wheel_sets.values():
    for o in ws: objs.remove(o)
body_objs = objs

def base_colour(m):
    if not m: return (0.5, 0.5, 0.5), 0.0, None
    col, metal, img = tuple(m.diffuse_color[:3]), m.metallic, None
    if m.node_tree:
        b = next((x for x in m.node_tree.nodes if x.type == 'BSDF_PRINCIPLED'), None)
        if b:
            bc = b.inputs['Base Color']; col = tuple(bc.default_value[:3]); metal = b.inputs['Metallic'].default_value
            if bc.is_linked:   # a texture (maybe through a mix or a multiply): the image it comes from
                stack, seen = [bc.links[0].from_node], set()
                while stack:
                    n = stack.pop()
                    if n in seen: continue
                    seen.add(n)
                    if n.type == 'TEX_IMAGE' and n.image: img = n.image; break
                    for i in n.inputs:
                        if i.is_linked: stack.append(i.links[0].from_node)
    if img is not None:
        try:
            w, h = img.size
            if w * h > 0:
                if np is not None:
                    a = np.empty(w * h * 4, np.float32); img.pixels.foreach_get(a); a = a.reshape(-1, 4)[::max(1, (w * h) // 65536)]
                    al = a[:, 3:4]; s = (a[:, :3] * al).sum(0) / max(1e-6, al.sum()); avg = tuple(float(x) for x in s)
                else:
                    px = img.pixels[:]; st = max(1, (w * h) // 4096) * 4; vals = [px[i:i + 3] for i in range(0, len(px), st)]
                    avg = tuple(sum(v[i] for v in vals) / len(vals) for i in range(3))
                if img.colorspace_settings.name.lower().startswith('srgb'): avg = tuple(x ** 2.2 for x in avg)
                k = col if not all(abs(x - 0.8) < 0.01 for x in col) else (1, 1, 1)   # (Blender's untouched 0.8 grey: no tint)
                col = tuple(avg[i] * (k[i] if max(k) > 0.05 else 1) for i in range(3))
        except Exception: pass
    return col, metal, img

# ---- which end is the front ----
FRONT_W = ['headl', 'head_l', 'head l', 'headlamp', 'front', 'bonnet', 'hood', 'grille', 'grill', 'windscreen', 'windshield', 'frontbumper', 'splitter', 'license_f', '_fl', '_fr', ' fl', ' fr', '.fl', '.fr', 'scheinwerfer']
REAR_W = ['tail', 'rear', 'boot', 'trunk', 'exhaust', 'muffler', 'spoiler', 'diffuser', 'brakel', 'brake_l', 'stoplight', 'reverse', '_rl', '_rr', ' rl', ' rr', '.rl', '.rr', 'heck', 'licence_r', 'license_r']
vote = 0.0
def centre_y(o): a, b = bbox(o); return (a.y + b.y) / 2
for o in body_objs + [w for ws in wheel_sets.values() for w in ws]:
    t = o.name.lower(); cy = centre_y(o); w = 1.0
    if has(t, FRONT_W): vote += math.copysign(w, cy) if abs(cy) > 0.1 else 0   # (vote > 0: the front is at +Y)
    if has(t, REAR_W): vote -= math.copysign(w, cy) if abs(cy) > 0.1 else 0
# materials: area-weighted centre of the faces of head / tail light materials
for o in body_objs:
    me = o.data; ms = [m.name.lower() if m else '' for m in me.materials]
    if not ms: continue
    acc = {}
    for p in me.polygons:
        if p.material_index < len(ms): t = ms[p.material_index]; s = acc.setdefault(t, [0.0, 0.0]); s[0] += p.area * p.center.y; s[1] += p.area
    for t, (sy, sa) in acc.items():
        if sa <= 0: continue
        cy = sy / sa
        if abs(cy) < 0.15 * L: continue
        if has(t, ['headl', 'head_l', 'headlamp', 'frontlight', 'front_light', 'grille', 'windscreen', 'windshield']): vote += math.copysign(3, cy)
        if has(t, ['taill', 'tail_l', 'rearlight', 'rear_light', 'brakel', 'brake_l', 'exhaust', 'reverse']): vote -= math.copysign(3, cy)
# colours: a red lens near one end is a tail light (that end is the back); the roof sits behind the middle on most cars
reds, roof = [0.0, 0.0], [0.0, 0.0]
for o in body_objs:
    me = o.data; mats = list(me.materials); red = [False] * max(1, len(mats))
    for q, m in enumerate(mats):
        if m: c3 = base_colour(m)[0]; red[q] = c3[0] > 0.25 and c3[1] < 0.35 * c3[0] and c3[2] < 0.35 * c3[0]
    for p in me.polygons:
        if red[min(p.material_index, len(red) - 1)] and abs(p.center.y) > 0.3 * L: reds[0] += p.area * p.center.y; reds[1] += p.area
        if p.center.z > 0.9 * H: roof[0] += p.area * p.center.y; roof[1] += p.area
if reds[1] > 0: vote -= math.copysign(2.5, reds[0] / reds[1])
if roof[1] > 0 and abs(roof[0] / roof[1]) > 0.03 * L: vote -= math.copysign(0.5, roof[0] / roof[1])
fopt = opt.get('--front', 'auto').lower()
if fopt in ('-y', '+y', 'y', '-x', '+x', 'x'): pass
front_is_plus = {'+y': True, 'y': True, '-y': False}.get(fopt)
if fopt in ('+x', 'x', '-x'): front_is_plus = None   # (handled below: it was along X in the file; it was turned so +X became +Y)
if fopt in ('+x', 'x'): front_is_plus = True
if fopt == '-x': front_is_plus = False
if front_is_plus is None: front_is_plus = vote > 0; log('front: %s (from the names, colours and shape: %+.1f; the previews show it, --front if wrong)' % ('+Y, turned round' if front_is_plus else '-Y', vote) if vote else 'front: -Y (nothing said otherwise: Blender\'s convention; see the previews, --front if wrong)')
if front_is_plus:
    R = Matrix.Rotation(math.pi, 4, 'Z'); xform(body_objs, R)
    for ws in wheel_sets.values(): xform(ws, R)
    hubs = {(not k[0], not k[1]): (R @ v[0], v[1]) for k, v in hubs.items()}; wheel_sets = {(not k[0], not k[1]): v for k, v in wheel_sets.items()}
# up the right way: the wheels low
lo, hi = bbox_all(body_objs + [w for ws in wheel_sets.values() for w in ws])
if sum(h[0].z for h in hubs.values()) / 4 > (lo.z + hi.z) / 2:
    R = Matrix.Rotation(math.pi, 4, 'Y'); xform(body_objs, R)
    for ws in wheel_sets.values(): xform(ws, R)
    hubs = {(not k[0], k[1]): (R @ v[0], v[1]) for k, v in hubs.items()}; wheel_sets = {(not k[0], k[1]): v for k, v in wheel_sets.items()}; log('it was upside down: turned over')

# ---- centred: X between the wheels, Y between the axles, Z 0 at the bottom of the tyres ----
fr, fl, rr, rl = hubs[(True, False)], hubs[(False, False)], hubs[(True, True)], hubs[(False, True)]
R_W = sum(h[1] for h in hubs.values()) / 4
c = Vector(((fr[0].x + fl[0].x + rr[0].x + rl[0].x) / 4, (fr[0].y + fl[0].y + rr[0].y + rl[0].y) / 4, min(h[0].z - h[1] for h in hubs.values())))
T = Matrix.Translation(-c); xform(body_objs, T)
for ws in wheel_sets.values(): xform(ws, T)
hubs = {k: (v[0] - c, v[1]) for k, v in hubs.items()}
fr = hubs[(True, False)]; rr = hubs[(True, True)]
spec = {'wbF': round(-(hubs[(True, False)][0].y + hubs[(False, False)][0].y) / 2 * GAME_SCALE, 3),
        'wbR': round((hubs[(True, True)][0].y + hubs[(False, True)][0].y) / 2 * GAME_SCALE, 3),
        'tw': round(sum(abs(h[0].x) for h in hubs.values()) / 4 * GAME_SCALE, 3), 'wr': round(R_W * GAME_SCALE, 3)}
if spec['wbF'] <= 0 or spec['wbR'] <= 0: die('the wheels are not two at the front and two at the back; name them wheel_fl, wheel_fr, wheel_rl, wheel_rr')
log('wheelbase %.2f m, track %.2f m (in the game: x%.2f)' % ((spec['wbF'] + spec['wbR']) / GAME_SCALE, 2 * spec['tw'] / GAME_SCALE, GAME_SCALE))

# ---- the game's wheel: the front right one (+X; front is -Y), its hub at the origin; the other three go ----
wset = wheel_sets[(True, False)]
for key, ws in wheel_sets.items():
    if key != (True, False):
        for o in ws: bpy.data.objects.remove(o, do_unlink=True)
def join(os_, nm):
    bpy.ops.object.select_all(action='DESELECT')
    for o in os_: o.select_set(True)
    bpy.context.view_layer.objects.active = os_[0]
    if len(os_) > 1: bpy.ops.object.join()
    o = bpy.context.view_layer.objects.active; o.name = nm; return o
wheel = join(wset, 'wheel'); wheel.data.transform(Matrix.Translation(-fr[0]))
body = join(body_objs, 'body')
def tris(o): o.data.calc_loop_triangles(); return len(o.data.loop_triangles)
def decimate(o, n):
    t0 = tris(o)
    if t0 > n:
        bpy.context.view_layer.objects.active = o; d = o.modifiers.new('dec', 'DECIMATE'); d.ratio = n / t0; d.use_collapse_triangulate = True
        bpy.ops.object.modifier_apply(modifier='dec')
    return t0
bt0, wt0 = decimate(body, MAX_TRIS), decimate(wheel, WHEEL_TRIS)
log('body %d -> %d triangles, wheel %d -> %d' % (bt0, tris(body), wt0, tris(wheel)))

# ---- materials -> the car shader's slots: 1 paint, 2 glass, 3 headlight, 4 taillight, 5 tyre/trim, 0 its own colour ----
P_W = ['paint', 'carpaint', 'car_paint', 'body', 'livery', 'exterior', 'karosserie', 'lack', 'carrosserie', 'coat'] + words('--paint')
G_W = ['glass', 'window', 'windscreen', 'windshield', 'glas', 'vitre', 'scheibe', 'lens'] + words('--glass')
HL_W = ['headl', 'head_l', 'headlamp', 'frontlight', 'front_light', 'xenon'] + words('--headlight')
TL_W = ['taill', 'tail_l', 'rearlight', 'rear_light', 'brakel', 'brake_l', 'stop'] + words('--taillight')
LIGHT_W = ['light', 'lamp', 'emissive', 'emission', 'licht', 'feu', 'faro']
T_W = ['tyre', 'tire', 'rubber', 'trim', 'reifen', 'pneu', 'gummi', 'plastic_black', 'blackplastic'] + words('--tyre')
NOT_P = ['interior', 'inside', 'seat', 'dash', 'base', 'under', 'chassis', 'carbon']
info = {}
def classify(m):
    if m in info: return info[m]
    n = (m.name.lower() if m else '')
    col, metal, img = base_colour(m)
    if img is not None: n += ' ' + img.name.lower()
    s = 0
    if has(n, G_W) or (m and m.node_tree and any(x.type == 'BSDF_PRINCIPLED' and x.inputs['Alpha'].default_value < 0.6 for x in m.node_tree.nodes)): s = 2
    elif has(n, HL_W): s = 3
    elif has(n, TL_W): s = 4
    elif has(n, LIGHT_W): s = -1   # a light: head or tail by where it is
    elif has(n, T_W): s = 5
    elif has(n, P_W) and not has(n, NOT_P): s = 1
    if words('--paint') and has(m.name.lower() if m else '', words('--paint')): s = 1   # (asked for: paint, whatever else it says)
    k = 0.6 if metal > 0.5 and s == 0 else 1.0
    info[m] = (s, (col[0] * k, col[1] * k, col[2] * k)); return info[m]
for m in body.data.materials: classify(m)
if not any(v[0] == 1 for v in info.values()):   # no paint by name: the material with the most outside surface, not dark, not glass
    area = {}
    for p in body.data.polygons:
        m = body.data.materials[p.material_index] if body.data.materials else None
        if info.get(m, (0,))[0] == 0 and p.center.z > 0.35 * H: area[m] = area.get(m, 0) + p.area
    cand = sorted(area, key=lambda m: -area[m])
    cand = [m for m in cand if sum(info[m][1]) > 0.12] or cand
    if cand: info[cand[0]] = (1, info[cand[0]][1]); log('paint: "%s" (the most of the outside; --paint NAME to choose)' % (cand[0].name if cand[0] else 'none'))
for m, (s, col) in info.items():
    if m: log('  %-44s %s' % (m.name[:44], ['own colour', 'PAINT', 'glass', 'headlight', 'taillight', 'tyre/trim', 'light (by end)'][s]))

# ---- ambient occlusion, baked onto the body's own UVs (made here) with the wheels in place ----
for o in list(scene.objects):
    if o.type != 'MESH' and o.type != 'CAMERA' and o.type != 'LIGHT': bpy.data.objects.remove(o, do_unlink=True)
ghosts = []
for key, (h, r) in hubs.items():   # the wheels for the bake: the kept one at each corner (mirrored on the left)
    g = bpy.data.objects.new('bakewheel', wheel.data.copy()); scene.collection.objects.link(g)
    M = Matrix.Translation(h)
    if not key[0]: M = M @ Matrix.Scale(-1, 4, Vector((1, 0, 0)))
    g.data.transform(M); ghosts.append(g)
bpy.ops.object.select_all(action='DESELECT'); body.select_set(True); bpy.context.view_layer.objects.active = body
bpy.ops.object.mode_set(mode='EDIT'); bpy.ops.mesh.select_all(action='SELECT')
bpy.ops.uv.smart_project(angle_limit=math.radians(66), island_margin=0.02); bpy.ops.object.mode_set(mode='OBJECT')
body.data.uv_layers.active = body.data.uv_layers[-1]
img = bpy.data.images.new('ao', AO, AO, float_buffer=False)
if not body.data.materials: body.data.materials.append(bpy.data.materials.new('plain'))
for m in body.data.materials:
    if m is None: continue
    if not m.node_tree:
        try: m.use_nodes = True
        except Exception: pass
    n = m.node_tree.nodes.new('ShaderNodeTexImage'); n.image = img; m.node_tree.nodes.active = n
sc = scene; sc.render.engine = 'CYCLES'; sc.cycles.device = 'CPU'; sc.cycles.samples = 64
sc.render.bake.margin = 4; sc.world = sc.world or bpy.data.worlds.new('w')
bpy.ops.object.bake(type='AO')
px = list(img.pixels)
for g in ghosts: bpy.data.objects.remove(g, do_unlink=True)

# ---- the header ----
def dump(o, uvs, slot_of):
    me = o.data; me.calc_loop_triangles(); uv = me.uv_layers.active.data if (uvs and me.uv_layers.active) else None
    vs, idx, seen = [], [], {}
    for t in me.loop_triangles:
        m = me.materials[t.material_index] if me.materials and t.material_index < len(me.materials) else None
        s, c = slot_of(m)
        if s == -1: s = 3 if sum(me.vertices[v].co.y for v in t.vertices) < 0 else 4   # (front is -Y)
        for k in (0, 1, 2):
            li = t.loops[k]; p = me.vertices[t.vertices[k]].co; n = t.split_normals[k]; u = uv[li].uv if uv else (0.0, 0.0)
            key = (round(p.x, 4), round(p.z, 4), round(-p.y, 4), round(n[0], 3), round(n[2], 3), round(-n[1], 3), round(u[0], 4), round(u[1], 4), s, tuple(round(x, 3) for x in c))
            if key not in seen: seen[key] = len(vs); vs.append(key)
            idx.append(seen[key])
    return vs, idx
def wheel_slot(m):
    s, c = classify(m)
    return (5 if s in (0, 1) and sum(c) < 0.25 else (0 if s in (1, -1, 3, 4) else s)), c   # (a wheel: never paint; dark: tyre)
bv, bi = dump(body, True, classify); wv, wi = dump(wheel, False, wheel_slot)
wr = max(max(abs(v.co.y), abs(v.co.z)) for v in wheel.data.vertices)
def floats(vs): return ',\n'.join(','.join('%.4g' % x for x in (*v[:8], *v[9], v[8])) for v in vs)
def ints(a): return ',\n'.join(','.join(str(x) for x in a[i:i + 24]) for i in range(0, len(a), 24))
ao = [max(0, min(255, int(px[(y * AO + x) * 4] * 255 + 0.5))) for y in range(AO) for x in range(AO)]
out = os.path.join(SRC, 'model_%s.h' % name)
with open(out, 'w') as f:
    f.write('// model_%s.h - generated by tools/blender/fcf_car.py from %s; do not edit (run the tool again).\n' % (name, os.path.basename(src)))
    f.write('// Game axes (X, Y up, Z forward), metres. Vertex: position, normal, uv, colour, material (the car shader\'s slot).\n#pragma once\n')
    f.write('namespace model_%s {\nstatic const float WHEEL_R = %.4f, HUB[3] = { %.4f, %.4f, %.4f };\n' % (name, wr, fr[0].x, fr[0].z, -fr[0].y))
    f.write('static const int BODY_NV = %d, BODY_NI = %d, WHEEL_NV = %d, WHEEL_NI = %d, AO_N = %d;\n' % (len(bv), len(bi), len(wv), len(wi), AO))
    f.write('static const float BODY_V[] = {\n%s };\nstatic const unsigned BODY_I[] = {\n%s };\n' % (floats(bv), ints(bi)))
    f.write('static const float WHEEL_V[] = {\n%s };\nstatic const unsigned WHEEL_I[] = {\n%s };\n' % (floats(wv), ints(wi)))
    f.write('static const unsigned char AO[] = {\n%s };\n}\n' % ints(ao))

# ---- previews: the game's colours (paint red), from the front and from the back ----
PREV = {1: (0.75, 0.06, 0.04), 2: (0.05, 0.08, 0.12), 3: (1.0, 0.95, 0.7), 4: (0.9, 0.05, 0.05), 5: (0.03, 0.03, 0.03)}
pm = {}
def prev_mat(s, c):
    k = (s, tuple(round(x, 2) for x in c))
    if k not in pm:
        m = bpy.data.materials.new('p%d' % len(pm)); col = PREV.get(s, c); m.diffuse_color = (*[min(1, x ** (1 / 2.2)) for x in col], 1); pm[k] = m
    return pm[k]
for o, fn in ((body, classify), (wheel, wheel_slot)):
    me = o.data; mats = list(me.materials); news = {}
    for p in me.polygons:
        m = mats[p.material_index] if mats and p.material_index < len(mats) else None; s, cc = fn(m)
        if s == -1: s = 3 if p.center.y < 0 else 4
        pmat = prev_mat(s, cc)
        if pmat.name not in news: news[pmat.name] = len(news)
        p.material_index = len(mats) + news[pmat.name]
    for nm in news: me.materials.append(bpy.data.materials[nm])
for key, (h, r) in hubs.items():   # the four wheels as the game puts them
    g = bpy.data.objects.new('w', wheel.data.copy()); scene.collection.objects.link(g)
    M = Matrix.Translation(h)
    if not key[0]: M = M @ Matrix.Scale(-1, 4, Vector((1, 0, 0)))
    g.data.transform(M)
wheel.hide_render = True
d = os.path.join(MODELS, name); os.makedirs(d, exist_ok=True)
sc.render.engine = 'BLENDER_WORKBENCH'; sc.display.shading.light = 'STUDIO'; sc.display.shading.color_type = 'MATERIAL'
sc.display.shading.show_shadows = True; sc.display.shading.show_cavity = True
sc.render.resolution_x, sc.render.resolution_y = 960, 540; sc.render.film_transparent = False
cam = bpy.data.objects.new('cam', bpy.data.cameras.new('cam')); scene.collection.objects.link(cam); sc.camera = cam; cam.data.lens = 50
Lm = max(L, 4.0)
for tag, pos in (('front', Vector((-1.15 * Lm, -1.35 * Lm, 0.55 * Lm))), ('rear', Vector((1.15 * Lm, 1.35 * Lm, 0.55 * Lm)))):
    cam.location = pos; tgt = Vector((0, 0, 0.45)); cam.rotation_euler = (tgt - pos).to_track_quat('-Z', 'Y').to_euler()
    sc.render.filepath = os.path.join(d, 'preview_%s.png' % tag); bpy.ops.render.render(write_still=True)

# ---- car.json, and the list ----
hb = BASE_H[like]
drv = {'rwd': 1.0, 'fwd': 0.0, 'awd': 0.5, '4wd': 0.5}.get(opt.get('--drive', '').lower(), hb[10])
hand = {'top': float(opt.get('--top', hb[0])), 'acc': float(opt.get('--acc', hb[1])), 'turn': float(opt.get('--turn', hb[2])), 'grip': float(opt.get('--grip', hb[3])),
        'offroad': hb[4], 'snd': hb[5], 'stat': hb[6], 'gF': hb[7], 'gR': hb[8], 'k2': hb[9], 'drv': drv, 'trac': hb[11]}
car = {'name': name, 'title': title, 'blurb': blurb, 'code': code, 'like': BASE[like], 'source': os.path.basename(src) + ('' if src_used == src else ' (' + os.path.basename(src_used) + ')'),
       'licence': opt.get('--licence', ''), 'bodyTris': len(bi) // 3, 'wheelTris': len(wi) // 3, 'spec': spec, 'handling': hand,
       'lengthM': round(L, 2), 'widthM': round(Wd, 2), 'heightM': round(H, 2)}
json.dump(car, open(os.path.join(d, 'car.json'), 'w'), indent=1)
cs = write_registry()
if tmpdir: shutil.rmtree(tmpdir, ignore_errors=True)
log('%s: body %d tris, wheel %d tris -> %s; %d added car%s in the game. Rebuild it.%s' % (title, len(bi) // 3, len(wi) // 3, os.path.relpath(out, REPO), len(cs), '' if len(cs) == 1 else 's',
    ('  Unlock: type %s on the main menu.' % code) if code else ''))
log('check models/%s/preview_front.png and preview_rear.png: the front, the paint (red here), the glass, the lights' % name)
