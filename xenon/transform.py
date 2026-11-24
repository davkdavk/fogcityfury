#!/usr/bin/env python3
"""Transform the game's GLSL (src/shaders.h) into the subset xsdk's xsc compiler accepts.

Repo-relative: reads <repo>/src/shaders.h, writes <outdir> (default <repo>/xenon/build/gen).
Usage: transform.py [outdir]

Every program's fragment shader is the 360's own, from xenon/lean_shaders.py, except post (the game's FS_POST with
its integer comparisons made float ones). Adaptations: #version 450 + layout locations + uniform blocks + sampler
bindings. Each program's vertex layout (mesh, part, ui or none) goes into its .uniforms.json, and from there into the
game (pack_shaders.py, gfx_xsdk.h makeProg). The result is compiled by xenon/build_shaders.sh.
"""
import re, os, sys, json
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); sys.dont_write_bytecode = True

HERE = os.path.dirname(os.path.abspath(__file__))          # <repo>/xenon
REPO = os.path.dirname(HERE)
SRC = os.path.join(REPO, 'src', 'shaders.h')
OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, 'build', 'gen')
s = open(SRC).read()
sh = {m.group(1): m.group(2) for m in re.finditer(r'static const char\* (\w+) = R"\((.*?)\)";', s, re.S)}

def strip_comments(t): return re.sub(r'//[^\n]*', '', t)

MESH = {'vP':0,'vN':1,'vC':2,'vUV':3,'vMat':4}
PART = {'vP':0,'vUV':1,'vC':2,'vMisc':3}
SKY = {'vNdc':0}
UI = {'vUV':0,'vC':1}

def add_locations(t, locmap):
    def repl(m):
        io, typ, names = m.group(1), m.group(2), m.group(3)
        out = []
        for n in [x.strip() for x in names.split(',')]:
            out.append(f'layout(location={locmap[n]}) {io} {typ} {n};' if n in locmap else f'{io} {typ} {n};')
        return ' '.join(out)
    return re.sub(r'(?<!layout\(location=\d\) )\b(in|out)\s+(vec[234]|float|int)\s+([A-Za-z_][\w, ]*);', repl, t)

SAMPLER_UNITS = {'uShadow':0, 'uEnv':1, 'uHeight':2, 'uFont':3, 'uTex':4, 'uBloom':5, 'uDetail':6, 'uNoise':7, 'uHash':8, 'uCarTex':9}

def uniforms_to_block(t):
    t = strip_comments(t)
    samplers, members = [], []
    def coll(m):
        typ, names = m.group(1), m.group(2)
        for n in [x.strip() for x in names.split(',')]:
            if typ.startswith('sampler'):
                if n not in SAMPLER_UNITS: raise ValueError(f'no fixed sampler unit for {n}')
                samplers.append((typ, n, SAMPLER_UNITS[n]))
            else:
                members.append((typ, n))
        return ''
    t = re.sub(r'uniform\s+(sampler\w+|vec[234]|float|int|mat[234])\s+([A-Za-z_][\w, ]*);', coll, t)
    hdr = ''
    uniforms = []
    offset = 0
    align_size = {'float':(4,4), 'int':(4,4), 'vec2':(8,8), 'vec3':(16,12), 'vec4':(16,16),
                  'mat2':(16,32), 'mat3':(16,48), 'mat4':(16,64)}
    for typ, n in members:
        align, size = align_size[typ]
        offset = (offset + align - 1) // align * align
        uniforms.append({'name':n, 'type':typ, 'offset':offset, 'size':size})
        offset += size
    if members: hdr += 'layout(binding=0) uniform Globals {\n' + '\n'.join(f'{typ} {n};' for typ,n in members) + '\n};\n'
    for slot, (typ, n, unit) in enumerate(samplers): hdr += f'layout(binding={slot}) uniform {typ} {n};\n'
    if hdr:
        lines = t.split('\n'); vi = next((i for i, l in enumerate(lines) if l.strip().startswith('#version')), None)
        if vi is not None: lines.insert(vi+1, hdr); t = '\n'.join(lines)
        else: t = '#version 450\n' + hdr + t
    return t, uniforms, [{'name':n, 'type':typ, 'unit':unit, 'slot':slot} for slot,(typ,n,unit) in enumerate(samplers)]

def version(t): return t.replace('#version 330 core', '#version 450').replace('gl_VertexID', 'gl_VertexIndex')
def ocol_loc(t): return t.replace('out vec4 oCol;', 'layout(location=0) out vec4 oCol;')

from lean_shaders import LEAN, LEAN_VS, LEAN_EXTRA

# post: xsc refuses integer equality (OpIEqual); use float comparisons instead.
def fix_post(f):
    f = f.replace('int uMode;', 'float uMode;')
    f = f.replace('if(uMode==0)', 'if(uMode<0.5)')
    f = f.replace('else if(uMode==1)', 'else if(uMode<1.5)')
    f = f.replace('L == 0 ?', 'float(L) < 0.5 ?')
    return f

# name: (vertex shader, its varyings' locations, vertex layout). Fragment shaders: lean_shaders.LEAN (post: FS_POST)
LAYOUT = {'VS_MESH': 'mesh', 'VS_WATER': 'mesh', 'VS_PART': 'part', 'VS_UI': 'ui', 'VS_SKY': 'none'}
progs = {n: ('VS_MESH', MESH) for n in ('world', 'car', 'depth')}
progs['water'] = ('VS_WATER', MESH)   # (the swells: shaders.h VS_WATER)
progs.update({n: ('VS_PART', PART) for n in ('part', 'mark', 'ring', 'glass')})
progs.update({'sky': ('VS_SKY', SKY), 'ui': ('VS_UI', UI), 'post': ('VS_SKY', SKY)})
progs.update({n: ('VS_MESH', MESH) for n in LEAN_EXTRA})
missing = [n for n in progs if n != 'post' and n not in LEAN]
if missing: raise ValueError('no lean fragment shader for %s' % missing)
os.makedirs(OUT, exist_ok=True)
open(os.path.join(OUT, 'programs.txt'), 'w').write(' '.join(progs))
for name, (vs, loc) in progs.items():
    if name in LEAN_VS: vsrc, loc = LEAN_VS[name]
    else: vsrc = sh[vs]
    v = version(vsrc); v = add_locations(v, loc); v, vu, vsam = uniforms_to_block(v)
    f = fix_post(sh['FS_POST']) if name == 'post' else LEAN[name]
    f = f.replace('#ifdef FCF_PERF_NOISE\nuniform sampler2D uNoise;\n#endif\n', '')
    f = version(f); f = ocol_loc(f); f = add_locations(f, loc)
    f, fu, fsam = uniforms_to_block(f)
    if not f.lstrip().startswith('#version'): f = '#version 450\n' + f
    open(os.path.join(OUT, name + '.vert'), 'w').write(v)
    open(os.path.join(OUT, name + '.frag'), 'w').write(f)
    with open(os.path.join(OUT, name + '.uniforms.json'), 'w') as mf:
        json.dump({'program':name, 'layout':LAYOUT[vs], 'vertex':vu, 'fragment':fu, 'samplers':vsam+fsam}, mf, indent=2)
print("generated", len(progs), "programs into", OUT)
