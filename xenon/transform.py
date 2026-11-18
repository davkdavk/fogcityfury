import re, subprocess, os, sys
SRC='/home/davey/Documents/fogcityfurysrc/src/shaders.h'
XSC='/tmp/xsdk/xenos-toolchain'
s=open(SRC).read()
sh={m.group(1):m.group(2) for m in re.finditer(r'static const char\* (\w+) = R"\((.*?)\)";', s, re.S)}

def strip_comments(t): return re.sub(r'//[^\n]*','',t)

# canonical varying locations per interface
MESH={'vP':0,'vN':1,'vC':2,'vUV':3,'vMat':4}
PART={'vP':0,'vUV':1,'vC':2,'vMisc':3}
SKY={'vNdc':0}
UI={'vUV':0,'vC':1}

def add_locations(t, locmap):
    # rewrite in/out varying decls (not vertex inputs which already have layout, not oCol)
    def repl(m):
        io,typ,names=m.group(1),m.group(2),m.group(3)
        out=[]
        for n in [x.strip() for x in names.split(',')]:
            if n in locmap: out.append(f'layout(location={locmap[n]}) {io} {typ} {n};')
            else: out.append(f'{io} {typ} {n};')
        return ' '.join(out)
    # match in/out without an existing layout() prefix
    t=re.sub(r'(?<!layout\(location=\d\) )\b(in|out)\s+(vec[234]|float|int)\s+([A-Za-z_][\w, ]*);', repl, t)
    return t

def uniforms_to_block(t):
    t=strip_comments(t)
    samplers=[]; members=[]
    def coll(m):
        typ,names=m.group(1),m.group(2)
        for n in [x.strip() for x in names.split(',')]:
            if typ.startswith('sampler'): samplers.append((typ,n))
            else: members.append(f'{typ} {n};')
        return ''
    t=re.sub(r'uniform\s+(sampler\w+|vec[234]|float|int|mat[234])\s+([A-Za-z_][\w, ]*);', coll, t)
    hdr=''
    if members: hdr+='layout(binding=0) uniform Globals {\n'+'\n'.join(members)+'\n};\n'
    for i,(typ,n) in enumerate(samplers): hdr+=f'layout(binding={i+1}) uniform {typ} {n};\n'
    if hdr:
        lines=t.split('\n'); vi=next((i for i,l in enumerate(lines) if l.strip().startswith('#version')),None)
        if vi is not None: lines.insert(vi+1,hdr); t='\n'.join(lines)
        else: t='#version 450\n'+hdr+t
    return t

def version(t): return t.replace('#version 330 core','#version 450').replace('gl_VertexID','gl_VertexIndex')

def ocol_loc(t): return t.replace('out vec4 oCol;','layout(location=0) out vec4 oCol;')

# single-return shadow refactor
SHADOW_OLD='''float shadow(vec3 wp, vec3 n){
  vec4 lp = uLightVP*vec4(wp + n*0.12, 1.0); vec3 s = lp.xyz/lp.w*0.5+0.5;
  if(s.x<0.0||s.x>1.0||s.y<0.0||s.y>1.0||s.z>1.0) return 1.0;
  float r = 0.0; vec2 t = vec2(1.0/2048.0);
  for(int x=-1;x<=1;x++) for(int y=-1;y<=1;y++) r += texture(uShadow, vec3(s.xy + vec2(x,y)*t, s.z-0.0012));
  float edge = 1.0-smoothstep(0.42, 0.5, max(abs(s.x-0.5), abs(s.y-0.5)));
  return mix(1.0, r/9.0, edge);
}'''
SHADOW_NEW='''float shadow(vec3 wp, vec3 n){
  vec4 lp = uLightVP*vec4(wp + n*0.12, 1.0); vec3 s = lp.xyz/lp.w*0.5+0.5;
  float r = 1.0;
  if(!(s.x<0.0||s.x>1.0||s.y<0.0||s.y>1.0||s.z>1.0)){
    r = 0.0; vec2 t = vec2(1.0/2048.0);
    for(int x=-1;x<=1;x++) for(int y=-1;y<=1;y++) r += texture(uShadow, vec3(s.xy + vec2(x,y)*t, s.z-0.0012));
    float edge = 1.0-smoothstep(0.42, 0.5, max(abs(s.x-0.5), abs(s.y-0.5)));
    r = mix(1.0, r/9.0, edge);
  }
  return r;
}'''

progs={
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
os.makedirs('/tmp/xsh/gen',exist_ok=True)
for name,(vs,fc,fs,loc) in progs.items():
    v=version(sh[vs]); v=add_locations(v,loc); v=uniforms_to_block(v)
    f=(sh[fc] if fc else '')+sh[fs]
    f=f.replace(SHADOW_OLD,SHADOW_NEW)
    f=version(f); f=ocol_loc(f); f=add_locations(f,loc); f=uniforms_to_block(f)
    if not f.lstrip().startswith('#version'): f='#version 450\n'+f
    open(f'/tmp/xsh/gen/{name}.vert','w').write(v)
    open(f'/tmp/xsh/gen/{name}.frag','w').write(f)
print("generated", len(progs), "programs")
