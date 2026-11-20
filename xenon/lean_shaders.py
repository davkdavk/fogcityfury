"""Fragment shaders written for the Xbox 360's GPU (FCF_PERF_TEST=lean).

The PC shaders work everything out per pixel with hash noise: 468 to 2990 microcode slots a
pixel, which the console draws at 5 to 9 frames a second (and the largest hangs it). These do
the same jobs from two texture lookups and a few dozen instructions. Measured on the console:
10 slots = 30 fps and 468 slots = 8.5 fps with the whole map drawn, so about 0.2 ms a slot.

Noise comes from uNoise (unit 7, made in game.cpp under FCF_XENOS_LEAN), 256 x 256, repeating:
  r, g  two different four-octave value noises (8 lattice cells across the texture, so
        texture(uNoise, p/8).r stands in for fbm(p))
  b     white noise          a  one smooth octave (16 cells across)
This backend has no mip maps, so fine detail is faded out with distance instead.

Everything is written to the 8-bit screen target (pass level 0), so every shader tone maps.
Uniform names are the game's own; ones a shader does not declare are skipped by Prog.
"""

COMMON = '''#version 330 core
uniform vec3 uSunDir, uSunCol, uSkyTop, uSkyHor, uCamPos;
uniform float uTime, uFogDen, uNight, uOvercast, uWet;
uniform sampler2D uNoise;
out vec4 oCol;
// the game's tone curve (ACES fit, then gamma 2.2) to within 0.03 above the deepest shadows: a cubic under a square root
vec3 tone(vec3 c){ c = min(c, vec3(2.5)); return sqrt(max(c*(1.3246 + c*(c*0.1284 - 0.6999)) - 0.0257, 0.0)); }
vec3 ambient(float ny){ return mix(uSkyHor*0.55 + vec3(0.05,0.045,0.04), uSkyTop*0.6 + uSkyHor*0.35, ny*0.5+0.5); }
'''

# The sky sits a hair in front of the far plane. Drawn behind the world (FCF_SKY_BEHIND) it is depth-tested against
# the cleared depth, 1.0; at exactly 1.0 the GPU's depth rounding across the screen-covering triangle failed the
# test on part of the screen, which cut the sky off along a diagonal.
SKY_VS = '''#version 330 core
out vec2 vNdc;
void main(){ vec2 p = vec2((gl_VertexID<<1)&2, gl_VertexID&2)*2.0-1.0; vNdc = p; gl_Position = vec4(p, 0.99998, 1.0); }
'''

SKY = COMMON + '''
in vec2 vNdc; uniform vec3 uCamR, uCamU, uCamF; uniform vec2 uTan;
void main(){
  vec3 d = normalize(uCamF + uCamR*vNdc.x*uTan.x + uCamU*vNdc.y*uTan.y);
  vec3 c = mix(uSkyHor, uSkyTop, sqrt(clamp(d.y, 0.0, 1.0)));
  float sd = max(dot(d, uSunDir), 0.0), s4 = sd*sd; s4 *= s4; float s16 = s4*s4; s16 *= s16;
  c += uSunCol*((clamp(sd*4000.0 - 3997.6, 0.0, 1.0)*30.0 + s16*s16*0.3 + s4*0.08)*(1.0 - 0.92*uOvercast));
  float cn = texture(uNoise, d.xz/(abs(d.y) + 0.14)*0.175 + uTime*vec2(0.00075, 0.00025)).r;                 // one layer of cloud, drifting
  float cl = clamp((cn - 0.48 + 0.36*uOvercast)*3.3, 0.0, 1.0)*clamp(d.y*10.0, 0.0, 1.0)*(0.85 + 0.13*uOvercast);
  c = mix(c, mix(uSkyHor, vec3(1.0 - 0.5*uNight), 0.55 - 0.41*uOvercast), cl);
  c = mix(c, uSkyHor*0.8, clamp(-d.y*4.0, 0.0, 1.0));                                                         // below the horizon
  oCol = vec4(tone(c), 1.0);
}
'''

# The world is lit and fogged per vertex (the terrain is a 3 m grid and walls are flat), which leaves
# the pixel shader only the surface pattern. vL: light on the surface. vFx: x = how much shows through
# the fog, y = 1 near the camera falling to 0 at 140 m (fine detail fades out there).
WORLD_VS = '''#version 330 core
layout(location=0) in vec3 aP; layout(location=1) in vec3 aN; layout(location=2) in vec3 aC;
layout(location=3) in vec2 aUV; layout(location=4) in float aM;
uniform mat4 uVP, uModel;
uniform vec3 uSunDir, uSunCol, uSkyTop, uSkyHor, uCamPos; uniform float uFogDen, uWet;
out vec3 vP, vL, vC; out vec2 vUV; out float vMat; out vec2 vFx;
void main(){ vec4 w = uModel*vec4(aP,1.0); vP = w.xyz; vec3 n = normalize(mat3(uModel)*aN);
  vL = (mix(uSkyHor*0.55 + vec3(0.05,0.045,0.04), uSkyTop*0.6 + uSkyHor*0.35, n.y*0.5+0.5) + uSunCol*max(dot(n, uSunDir), 0.0))*(1.0 - 0.3*uWet);
  float d = length(uCamPos - w.xyz); vFx = vec2(exp(-d*uFogDen), clamp(1.0 - d*0.007, 0.0, 1.0));
  vC = aC; vUV = aUV; vMat = aM; gl_Position = uVP*w; }
'''
WORLD_LOC = {'vP':0, 'vL':1, 'vC':2, 'vUV':3, 'vMat':4, 'vFx':5}

# vMat: 0 terrain (vUV.x = how much town) 1 road 2 building wall 3 flat 4 checker 5 hazard stripes 6 lamp
# One program per kind of surface. The compiler turns a test on vMat into "work out every material, then
# pick one", so a single world shader pays for all of them on every pixel (124 slots against 40 to 60).
#   ground  the terrain mesh (always material 0)
#   road, wall, misc   one material class each, for when the props mesh is drawn in per-material ranges
#   world   road + wall + misc picked by vMat: the whole props mesh in one draw, until it is split
_HEAD = '''#version 330 core
uniform vec3 uSkyHor; uniform float uNight, uWet;
uniform sampler2D uNoise;
in vec3 vP, vL, vC; in vec2 vUV; in float vMat; in vec2 vFx;
out vec4 oCol;
vec3 tone(vec3 c){ c = min(c, vec3(2.5)); return sqrt(max(c*(1.3246 + c*(c*0.1284 - 0.6999)) - 0.0257, 0.0)); }
void main(){
  // fine grain from the noise texture's white-noise channel (8 cm a texel), faded out with distance: no mip maps here.
  // (The game's own detail texture, unit 6, is not used: on the console the ground and road that sampled it came out black.)
  float det = mix(0.5, texture(uNoise, vP.xz*0.05).b, vFx.y);
  float nz = texture(uNoise, vP.xz*0.026).r;
  vec3 alb = vC, lit = vL; vec3 glow = vec3(0.0);
'''
_TAIL = '''  oCol = vec4(tone(mix(uSkyHor, alb*lit + glow, vFx.x)), 1.0);
}
'''
_GROUND = '''  alb *= (0.72 + 0.56*nz)*(0.8 + 0.4*det);                    // grass, sand, rock; in town, paving with a backstreet every 27 m
  vec2 a = abs(mod(vP.xz + 660.0, 27.0) - 13.5);
  vec3 cty = mix(vec3(0.27,0.262,0.25), vec3(0.078,0.078,0.088), clamp((3.7 - min(a.x, a.y))*4.0, 0.0, 1.0))*(0.6 + 0.8*det);
  alb = mix(alb, cty, vUV.x);
'''
_ROAD = '''  { float u = abs(vUV.x);                                        // tarmac, centre line, lane dashes, edge line, red and white kerbs
    vec3 k = clamp(1.0 - abs(vec3(u) - vec3(0.17, 4.0, 7.5))*12.0, 0.0, 1.0);
    vec3 r = vec3(0.105,0.105,0.115)*(0.62 + 0.76*det)*(0.8 + 0.4*nz);
    r = mix(r, vec3(0.85,0.62,0.05), k.x*0.9);
    r = mix(r, vec3(0.82), max(k.y*step(fract(vUV.y/9.0), 0.42), k.z)*0.85);
    alb = mix(r, mix(vec3(0.75,0.08,0.06), vec3(0.85), step(0.5, fract(vUV.y*0.25))), step(8.0, u));
    lit = uSkyHor*0.55 + vec3(0.05,0.045,0.04); glow = uSkyHor*(uWet*0.22); }               // lit by the sky alone, as the PC game's road is; a wet road shines with it
'''
_WALL = '''  { vec2 q = vUV*vec2(0.2941, 0.3030), f = abs(fract(q) - vec2(0.5, 0.55));   // windows 3.4 m apart, a floor every 3.3 m; some lit at night
    float win = step(f.x, 0.3)*step(f.y, 0.27)*step(0.0, vUV.y);
    alb = mix(alb*(0.8 + 0.4*nz), vec3(0.06,0.08,0.11) + uSkyHor*0.25, win);
    glow = vec3(2.0,1.56,0.9)*(uNight*win*step(0.55, fract(dot(floor(q), vec2(0.37, 0.71))))); }
'''
_MISC = '''  { vec2 c = floor(vUV*1.4286);                                  // plain (3), checker (4), hazard stripes (5), lamps (6)
    if(vMat > 3.5) alb = mix(vec3(0.03), vec3(0.95), fract((c.x + c.y)*0.5)*2.0);
    if(vMat > 4.5) alb = mix(vec3(0.95,0.72,0.02), vec3(0.04), step(0.5, fract(vUV.x*3.0 + vUV.y*6.0)));
    if(vMat > 5.5) glow = vC*3.0; }
'''
# Rain on the road (the "roadwet" program, drawn instead of "road" when the weather is wet): puddles in the hollows,
# raindrop rings on them, and the sky reflected through a bumped, Fresnel-weighted surface, after the PC road shader
# (shaders.h, FS_WORLD material 1) but on the noise texture instead of hash noise, and one ring layer instead of two.
_ROAD_WET = '''  {
    float u = abs(vUV.x);
    vec3 V = normalize(uCamPos - vP);
    // standing water where the slow noise is high; more of it the harder it rains; none on the kerbs
    float pud = clamp((texture(uNoise, vP.xz*0.0101 + 0.37).g - 0.55 + 0.15*uRain)*10.0, 0.0, 1.0)*step(0.2, uRain)*step(u, 8.0);
    // raindrop rings: one a 70 cm cell, its place and timing from the noise texture's white noise
    vec2 q = vP.xz*1.43, f = fract(q) - 0.5;
    vec2 h = texture(uNoise, (floor(q) + 0.5)*0.00390625).br;
    vec2 d = f - (h - 0.5)*0.5; float r = max(length(d), 0.001), t = fract(uTime*(0.8 + 0.7*h.y) + h.x), x = r - t*0.42;
    float ring = (abs(fract(x*8.28) - 0.5)*4.0 - 1.0)*clamp(1.0 - abs(x)*14.3, 0.0, 1.0)*(1.0 - t);   // a ripple's crest and trough
    vec2 bump = d*(ring*0.42*pud*min(uRain, 1.3)*vFx.y/r);
    vec3 nb = vec3(bump.x, 1.0, bump.y);                                         // a slight tilt: no need to normalise
    float dn = dot(nb, V), ry = abs(2.0*dn - V.y) + 0.02;                    // the reflected ray's height: reflect(-V, nb).y with nb.y = 1
    float c5 = 1.0 - max(dn, 0.0), c2 = c5*c5, F = 0.04 + 0.96*c2*c2*c5;
    vec3 sky = mix(uSkyHor, uSkyTop, clamp(ry*1.5, 0.0, 1.0));
    alb *= 1.0 - (0.40 + 0.28*pud)*uWet;
    glow = sky*uWet*mix(0.03 + 0.30*F, 0.06 + 0.80*F, pud); }
'''
GROUND = _HEAD + _GROUND + _TAIL
ROAD = _HEAD + _ROAD + _TAIL
ROAD_WET = (_HEAD.replace('uniform vec3 uSkyHor;', 'uniform vec3 uSkyHor, uSkyTop, uCamPos; uniform float uRain, uTime;')
            + _ROAD + _ROAD_WET + _TAIL)
WALL = _HEAD + _WALL + _TAIL
MISC = _HEAD + _MISC + _TAIL
WORLD = (_HEAD + '  vec3 base = alb, g0 = glow;\n' + _ROAD + '  vec3 aRoad = alb, lRoad = lit; alb = base; lit = vL;\n' + _WALL
         + '  vec3 aWall = alb, gWall = glow; alb = base; glow = g0;\n' + _MISC
         + '  alb = vMat < 1.5 ? aRoad : vMat < 2.5 ? aWall : alb; lit = vMat < 1.5 ? lRoad : vL; glow = vMat < 1.5 ? g0 : vMat < 2.5 ? gWall : glow;\n' + _TAIL)

# vMat: 0 plain 1 paint 2 glass 3 headlight 4 taillight 5 rubber
CAR = COMMON + '''
in vec3 vP, vN, vC; in vec2 vUV; in float vMat;
uniform vec3 uPaint; uniform float uBrake, uChrome, uBurnt, uGone;
void main(){
  vec3 n = normalize(vN)*(gl_FrontFacing ? 1.0 : -1.0);
  vec3 toCam = uCamPos - vP; float dist = length(toCam); vec3 V = toCam/dist;
  float ndv = max(dot(n, V), 0.0), f1 = 1.0 - ndv, F = 0.05 + 0.95*f1*f1*f1*f1;
  // the sky reflected, from its two colours: the reflection cube is only drawn at pass level 3, and unwritten it read
  // as dark red on the console
  vec3 R = reflect(-V, n); vec3 env = mix(uSkyHor*0.5, mix(uSkyHor, uSkyTop, sqrt(clamp(R.y, 0.0, 1.0))), clamp(R.y*4.0 + 1.0, 0.0, 1.0));
  vec3 lit = ambient(n.y) + uSunCol*max(dot(n, uSunDir), 0.0);
  float ndh = max(dot(n, normalize(V + uSunDir)), 0.0), s = ndh*ndh; s *= s; s *= s; s *= s; s *= s;
  vec3 spec = uSunCol*s*0.5;
  if(fract(dot(gl_FragCoord.xy, vec2(0.7548777, 0.5698403))) < uGone) discard;
  vec3 col;
  if(vMat < 0.5)      col = vC*lit*(1.0 - F*0.4) + env*F*0.4 + spec*0.3;
  else if(vMat < 1.5) col = mix(uPaint*(lit + env*0.4)*(1.0 - F) + env*(F + 0.07), env*mix(vec3(1.0), uPaint, 0.75), uChrome) + spec;
  else if(vMat < 2.5) col = vec3(0.012,0.016,0.02)*lit + env*(0.22 + 0.78*F) + spec;
  else if(vMat < 3.5) col = vC*2.1 + env*0.3;
  else if(vMat < 4.5) col = vC*(0.25*lit + 0.25 + uNight*1.2 + uBrake*2.6);
  else                col = vC*lit;
  col = mix(col, vec3(0.03,0.026,0.022)*lit, clamp(uBurnt, 0.0, 1.0));
  col = mix(col, uSkyHor, 1.0 - exp(-dist*uFogDen));
  oCol = vec4(tone(col), 1.0);
}
'''

WATER = COMMON + '''
in vec3 vP, vN, vC; in vec2 vUV; in float vMat;
uniform sampler2D uHeight;
void main(){
  vec2 huv = (vP.xz + 768.0)/1536.0;
  float depth = -texture(uHeight, clamp(huv, 0.0, 1.0)).r;
  vec2 o = abs(huv - 0.5); if(max(o.x, o.y) > 0.5) depth = 30.0;
  if(depth < -0.02) discard;
  vec3 toCam = uCamPos - vP; float dist = length(toCam); vec3 V = toCam/dist;
  vec2 w = texture(uNoise, vP.xz*0.011 + vec2(uTime*0.014, uTime*0.009)).rg + texture(uNoise, vP.xz*0.07 + vec2(-uTime*0.06, uTime*0.04)).gr - 1.0;
  vec3 n = normalize(vec3(w.x, 1.4 + dist*0.02, w.y));
  float f1 = 1.0 - max(dot(n, V), 0.0), f2 = f1*f1, F = 0.03 + 0.97*f2*f2*f1;
  vec3 R = reflect(-V, n);
  vec3 sky = mix(uSkyHor, uSkyTop, sqrt(clamp(abs(R.y), 0.0, 1.0)));
  float sd = max(dot(R, uSunDir), 0.0), s = sd*sd; s *= s; s *= s; s *= s; s *= s; s *= s;
  vec3 body = mix(vec3(0.07,0.30,0.30), vec3(0.012,0.07,0.11), clamp(depth*0.2, 0.0, 1.0))*(ambient(n.y)*1.4 + uSunCol*0.35);
  vec3 col = mix(body, sky, F) + uSunCol*s*(1.0 - uOvercast);
  float foam = clamp(1.0 - depth*2.2, 0.0, 1.0)*step(0.45, w.x + 0.6);
  col = mix(col, vec3(0.9)*(ambient(1.0) + uSunCol*0.6), foam*0.8);
  col = mix(col, uSkyHor, 1.0 - exp(-dist*uFogDen));
  oCol = vec4(tone(col), clamp(depth*1.1 + 0.25, 0.0, 1.0));
}
'''

# smoke, dust, sparks, fire. vMisc.x: a seed, vMisc.y: 1 = glows (no light or fog on it)
PART = COMMON + '''
in vec3 vP; in vec2 vUV; in vec4 vC; in vec2 vMisc;
void main(){
  float nz = texture(uNoise, vUV*0.21 + vMisc.x*3.9).r;
  float a = clamp((1.0 - length(vUV) - (nz - 0.5)*0.7)*1.33, 0.0, 1.0)*vC.a*clamp((vUV.y + 1.0)*1.43, 0.0, 1.0);
  vec3 lit = vC.rgb*(ambient(1.0) + uSunCol*0.45)*(0.75 + 0.5*nz);
  lit = mix(lit, uSkyHor, 1.0 - exp(-length(vP - uCamPos)*uFogDen));
  vec3 c = tone(mix(lit, vC.rgb, vMisc.y));
  oCol = vec4(c*a, a*(1.0 - vMisc.y));
}
'''

# skid marks and the soft shadow under each car
MARK = COMMON + '''
in vec3 vP; in vec2 vUV; in vec4 vC; in vec2 vMisc;
void main(){
  float e = mix(clamp((1.0 - abs(vUV.x))*2.0, 0.0, 1.0), clamp((1.0 - length(vUV))*1.33, 0.0, 1.0), vMisc.y);
  vec3 c = vC.rgb*(ambient(1.0) + uSunCol*0.5);
  c = mix(c, uSkyHor, 1.0 - exp(-length(vP - uCamPos)*uFogDen));
  oCol = vec4(tone(c), vC.a*e);
}
'''

# the showdown's ring: a wall of light
RING = COMMON + '''
in vec3 vP; in vec2 vUV; in vec4 vC; in vec2 vMisc;
void main(){
  float v = vUV.y;
  float bars = clamp((abs(fract(vUV.x*0.35 - uTime*0.5 + v*1.6) - 0.5) - 0.30)*5.0, 0.0, 1.0);
  float lines = clamp((fract(v*5.0 - uTime*0.3) - 0.90)*10.0, 0.0, 1.0);
  float foot = clamp(1.0 - v*6.0, 0.0, 1.0); foot *= foot;
  float fade = clamp(1.0 - v, 0.0, 1.0);
  float near = clamp((45.0 - length(vP - uCamPos))*0.0256, 0.0, 1.0);
  float a = (0.05 + 0.16*bars + 0.30*lines + 1.5*foot)*fade*vC.a*(0.45 + 0.55*near);
  vec3 c = tone(vC.rgb*(1.0 + 2.0*foot)*(1.0 + vMisc.y*0.35*(abs(fract(uTime*1.6) - 0.5)*4.0 - 1.0)));
  oCol = vec4(c*a, 0.0);
}
'''

LEAN_VS = {n: (WORLD_VS, WORLD_LOC) for n in ('world', 'ground', 'road', 'roadwet', 'wall', 'misc')}
LEAN_VS['sky'] = (SKY_VS, {'vNdc': 0})
# The depth pre-pass must place every vertex exactly where the colour pass does, or the colour pass's depth test
# (less-or-equal, depth writes off) fails on the ground and the road and leaves them black. So it runs the colour
# pass's own vertex shader; its pixel shader writes nothing that is kept (colour writes are off).
LEAN_VS['depth'] = (WORLD_VS, WORLD_LOC)
DEPTH = '''#version 330 core
out vec4 oCol;
void main(){ oCol = vec4(1.0); }
'''
LEAN_EXTRA = ('ground', 'road', 'roadwet', 'wall', 'misc')   # programs only the lean build has
# Text: the font texture holds, for the 5x7 pixel font, the signed distance to each glyph's edge in font pixels
# (base.h, UI::init under FCF_XENOS_LEAN), sampled linearly. Each font pixel comes out as a square with a smooth
# one-screen-pixel edge at any text size, where point sampling at sizes like 1.8 or 3.2 gave pixels of uneven widths.
# The edge is one screen pixel wide from the text's size, which UI::glyph carries in vUV.y's whole part (eighths of a
# screen pixel per font pixel; the 720-line UI is drawn 1:1). No fwidth: on the console it drew a line down each letter.
# vUV.x < -0.5 marks the solid quads (panels, bars).
UI = '''#version 330 core
in vec2 vUV; in vec4 vC; uniform sampler2D uFont; out vec4 oCol;
void main(){
  float k = floor(vUV.y + 0.25);
  float d = (texture(uFont, vec2(vUV.x, (vUV.y - k)*2.0)).r - 0.5)*2.0;
  float a = vUV.x < -0.5 ? 1.0 : clamp(d*k*0.125 + 0.5, 0.0, 1.0);
  oCol = vec4(vC.rgb, vC.a*a);
}
'''

LEAN = {'ui': UI, 'depth': DEPTH, 'sky': SKY, 'world': WORLD, 'ground': GROUND, 'road': ROAD, 'roadwet': ROAD_WET, 'wall': WALL, 'misc': MISC, 'car': CAR, 'water': WATER, 'part': PART, 'mark': MARK, 'ring': RING}

# Diagnostic (FCF_LEAN_DEBUG=mat): every world surface in one flat colour per material, so a picture shows which
# geometry is where. Road (vMat 1) red, walls (2) green, the rest of the props (3-6) blue, terrain yellow. Anything
# still black was not drawn by the world colour pass at all.
import os as _os
if _os.environ.get('FCF_LEAN_DEBUG') == 'mat':
    _DBG = '''#version 330 core
in vec3 vP, vL, vC; in vec2 vUV; in float vMat; in vec2 vFx;
out vec4 oCol;
void main(){ oCol = vec4(vMat < 0.5 ? vec3(1.0, 1.0, 0.0) : vMat < 1.5 ? vec3(1.0, 0.0, 0.0) : vMat < 2.5 ? vec3(0.0, 1.0, 0.0) : vec3(0.0, 0.3, 1.0), 1.0); }
'''
    for _n in ('world', 'road', 'roadwet', 'wall', 'misc'): LEAN[_n] = _DBG
    LEAN['ground'] = '''#version 330 core
in vec3 vP, vL, vC; in vec2 vUV; in float vMat; in vec2 vFx;
out vec4 oCol;
void main(){ oCol = vec4(1.0, 1.0, 0.0, 1.0); }
'''
