#!/usr/bin/env python3
"""Transform the game's GLSL (src/shaders.h) into the subset xsdk's xsc compiler accepts.

Repo-relative: reads <repo>/src/shaders.h, writes <outdir> (default <repo>/xenon/build/gen).
Usage: transform.py [outdir]

Adaptations: #version 450 + layout locations + uniform blocks + sampler bindings; single-return
shadow(); world's texture fetches hoisted out of divergent material branches; post's integer
equality comparisons turned into float ones. The result is compiled by xenon/build_shaders.sh.
"""
import re, os, sys, json

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

SAMPLER_UNITS = {'uShadow':0, 'uEnv':1, 'uHeight':2, 'uFont':3, 'uTex':4, 'uBloom':5, 'uDetail':6}

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

SHADOW_OLD = '''float shadow(vec3 wp, vec3 n){
  vec4 lp = uLightVP*vec4(wp + n*0.12, 1.0); vec3 s = lp.xyz/lp.w*0.5+0.5;
  if(s.x<0.0||s.x>1.0||s.y<0.0||s.y>1.0||s.z>1.0) return 1.0;
  float r = 0.0; vec2 t = vec2(1.0/2048.0);
  for(int x=-1;x<=1;x++) for(int y=-1;y<=1;y++) r += texture(uShadow, vec3(s.xy + vec2(x,y)*t, s.z-0.0012));
  float edge = 1.0-smoothstep(0.42, 0.5, max(abs(s.x-0.5), abs(s.y-0.5)));
  return mix(1.0, r/9.0, edge);
}'''
SHADOW_NEW = '''float shadow(vec3 wp, vec3 n){
  vec4 lp = uLightVP*vec4(wp + n*0.12, 1.0); vec3 s = lp.xyz/lp.w*0.5+0.5;
  vec2 t = vec2(1.0/2048.0);
  float r = 0.0;
  for(int x=-1;x<=1;x++) for(int y=-1;y<=1;y++) r += texture(uShadow, vec3(s.xy + vec2(x,y)*t, s.z-0.0012));
  float edge = 1.0-smoothstep(0.42, 0.5, max(abs(s.x-0.5), abs(s.y-0.5)));
  float inside = (s.x<0.0||s.x>1.0||s.y<0.0||s.y>1.0||s.z>1.0) ? 0.0 : 1.0;
  return mix(1.0, mix(1.0, r/9.0, edge), inside);
}'''

# world: the full town/wet shader exceeds a Xenos pixel-shader size field (12-bit address) and puts
# texture fetches in divergent branches. Emit a lighter world main that keeps the game's interface,
# materials and lighting/fog/shadow, with the detail fetch hoisted to uniform control flow.
WORLD_MAIN = '''void main(){
  vec3 n = normalize(vN) * (gl_FrontFacing ? 1.0 : -1.0);
  vec3 V = normalize(uCamPos-vP);
  vec3 alb = vC; float spec = 0.0; float gloss = 40.0; vec3 emis = vec3(0.0); vec3 refl = vec3(0.0);
  int m = int(vMat+0.5);
  vec4 detB = texture(uDetail, vP.xz*0.21);
  if(m==0){
    float nz = fbm(vP.xz*0.21);
    alb *= (0.72+0.56*nz)*(0.9+0.2*detB.r)*(1.0-0.28*uWet);
  } else if(m==1){
    float u = abs(vUV.x), v = vUV.y;
    alb = vec3(0.105,0.105,0.115)*(0.62+0.76*(vnoise(vP.xz*3.1)*0.4+detB.r*0.6));
    float yl = 1.0-smoothstep(0.05,0.09,abs(u-0.17));
    float dash = (1.0-smoothstep(0.05,0.09,abs(u-4.0)))*step(fract(v/9.0),0.42);
    float edge = 1.0-smoothstep(0.06,0.10,abs(u-7.5));
    alb = mix(alb, vec3(0.85,0.62,0.05), yl*0.9);
    alb = mix(alb, vec3(0.82), max(dash,edge)*0.85);
    spec = 0.25; gloss = 30.0;
  } else if(m==6){
    emis = vC*3.0; alb = vC;
  } else if(m==2){
    alb *= 0.85+0.3*vnoise(vP.xz*0.7+vP.y*0.9);
  } else if(m==4){
    vec2 c = floor(vUV/0.7); alb = mix(vec3(0.03), vec3(0.95), mod(c.x+c.y,2.0));
  } else if(m==5){
    float s = step(0.5, fract(vUV.x*3.0+vUV.y*6.0));
    alb = mix(vec3(0.95,0.72,0.02), vec3(0.04), s);
  }
  float ndl = max(dot(n,uSunDir),0.0); float sh = shadow(vP, n);
  vec3 H = normalize(V+uSunDir);
  vec3 col = alb*(ambient(n) + uSunCol*ndl*sh + headlights(vP, n)) + refl + uSunCol*spec*sh*pow(max(dot(n,H),0.0), gloss)*(gloss>100.0?2.0:0.35) + emis;
  finish(applyFog(col, vP), 1.0);
}
'''
def fix_world(f):
    return f.split('void main(){')[0] + WORLD_MAIN

# post: xsc refuses integer equality (OpIEqual); use float comparisons instead.
def fix_post(f):
    f = f.replace('int uMode;', 'float uMode;')
    f = f.replace('if(uMode==0)', 'if(uMode<0.5)')
    f = f.replace('else if(uMode==1)', 'else if(uMode<1.5)')
    f = f.replace('L == 0 ?', 'float(L) < 0.5 ?')
    return f

progs = {
 'world':('VS_MESH','FS_COMMON','FS_WORLD',MESH),
 'car':('VS_MESH','FS_COMMON','FS_CAR',MESH),
 'water':('VS_MESH','FS_COMMON','FS_WATER',MESH),
 'sky':('VS_SKY','FS_COMMON','FS_SKY',SKY),
 'part':('VS_PART','FS_COMMON','FS_PART',PART),
 'depth':('VS_MESH','FS_COMMON','FS_DEPTH',MESH),
 'ui':('VS_UI','','FS_UI_FULL',UI),
 'mark':('VS_PART','FS_COMMON','FS_MARK',PART),
 'ring':('VS_PART','FS_COMMON','FS_RING',PART),
 'post':('VS_SKY','','FS_POST',SKY),
}
os.makedirs(OUT, exist_ok=True)
for name, (vs, fc, fs, loc) in progs.items():
    v = version(sh[vs]); v = add_locations(v, loc); v, vu, vsam = uniforms_to_block(v)
    f = (sh[fc] if fc else '') + sh[fs]
    f = f.replace(SHADOW_OLD, SHADOW_NEW)
    f = version(f); f = ocol_loc(f); f = add_locations(f, loc)
    if name == 'world': f = fix_world(f)
    if name == 'post': f = fix_post(f)
    f, fu, fsam = uniforms_to_block(f)
    if not f.lstrip().startswith('#version'): f = '#version 450\n' + f
    open(os.path.join(OUT, name + '.vert'), 'w').write(v)
    open(os.path.join(OUT, name + '.frag'), 'w').write(f)
    with open(os.path.join(OUT, name + '.uniforms.json'), 'w') as mf:
        json.dump({'program':name, 'vertex':vu, 'fragment':fu, 'samplers':vsam+fsam}, mf, indent=2)
print("generated", len(progs), "programs into", OUT)
