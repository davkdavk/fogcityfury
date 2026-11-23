"""Fragment shaders written for the Xbox 360's GPU (FCF_PERF_TEST=lean).

The PC shaders work everything out per pixel with hash noise: 468 to 2990 microcode slots a
pixel, which the console draws at 5 to 9 frames a second (and the largest hangs it). These do
the same jobs from two texture lookups and a few dozen instructions. Measured on the console:
10 slots = 30 fps and 468 slots = 8.5 fps with the whole map drawn, so about 0.2 ms a slot.

Noise comes from uNoise (unit 7, made in game.cpp under FCF_XENOS_LEAN), 256 x 256, repeating:
  r, g  two different four-octave value noises (8 lattice cells across the texture, so
        texture(uNoise, p/8).r stands in for fbm(p))
  b     white noise          a  one smooth octave (16 cells across)
The noise texture has mip maps (four levels); fine detail is also faded out with distance.

Everything is written to the 8-bit screen target (pass level 0), so every shader tone maps.
Uniform names are the game's own; ones a shader does not declare are skipped by Prog.
"""

COMMON = '''#version 330 core
uniform vec3 uSunDir, uSunCol, uSkyTop, uSkyHor, uCamPos;
uniform float uTime, uFogDen, uNight, uOvercast, uWet, uBright;
uniform sampler2D uNoise;
out vec4 oCol;
// the game's tone curve (ACES fit, then gamma 2.2) to within 0.03 above the deepest shadows: a cubic under a square root
vec3 tone(vec3 c){ c = min(c*(uBright > 0.01 ? uBright : 1.0), vec3(2.5)); return sqrt(max(c*(1.3246 + c*(c*0.1284 - 0.6999)) - 0.0257, 0.0)); }
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
in vec2 vNdc; uniform vec3 uCamR, uCamU, uCamF; uniform vec2 uTan; uniform float uSpace; uniform sampler2D uHash;
void main(){
  vec3 d = normalize(uCamF + uCamR*vNdc.x*uTan.x + uCamU*vNdc.y*uTan.y);
  vec3 c = mix(uSkyHor, uSkyTop, sqrt(clamp(d.y, 0.0, 1.0)));
  float sd = max(dot(d, uSunDir), 0.0), s4 = sd*sd; s4 *= s4; float s16 = s4*s4; s16 *= s16;
  c += uSunCol*((clamp(sd*4000.0 - 3997.6, 0.0, 1.0)*30.0 + s16*s16*0.3 + s4*0.08)*(1.0 - 0.92*uOvercast));
  float cn = texture(uNoise, d.xz/(abs(d.y) + 0.14)*0.175 + uTime*vec2(0.00075, 0.00025)).r;                 // one layer of cloud, drifting
  float cl = clamp((cn - 0.48 + 0.36*uOvercast)*3.3, 0.0, 1.0)*clamp(d.y*10.0, 0.0, 1.0)*(0.85 + 0.13*uOvercast);
  c = mix(c, mix(uSkyHor, vec3(1.0 - 0.5*uNight), 0.55 - 0.41*uOvercast), cl);
  c = mix(c, uSkyHor*0.8, clamp(-d.y*4.0, 0.0, 1.0));                                                         // below the horizon
  if (uSpace > 0.5) {   // the space track: black all round, stars, a faint band of nebula, the sun as a hard disc (shaders.h spaceSky)
    vec3 ad = abs(d); float big = max(ad.x, max(ad.y, ad.z));
    // (a cube round the eye, each face its own part of the texture; picked with step and mix, no branches)
    float fx = step(big, ad.x), fy = step(big, ad.y)*(1.0 - fx), fz = 1.0 - fx - fy;
    vec3 sg = step(vec3(0.0), d)*2.0 - 1.0;
    vec2 p = (d.yz/max(ad.x, 0.001) + vec2(sg.x*3.0, 0.0))*fx + (d.xz/max(ad.y, 0.001) + vec2(0.0, sg.y*3.0))*fy + (d.xy/max(ad.z, 0.001) + vec2(sg.z*7.0, 5.0))*fz;
    vec4 h1 = texture(uHash, (floor(p*330.0) + 0.5)*0.00390625), h2 = texture(uHash, (floor(p*120.0) + 0.5)*0.00390625 + 0.37);
    vec3 sc = mix(vec3(1.0, 0.82, 0.65), vec3(0.65, 0.78, 1.0), h1.g);
    c = vec3(0.003, 0.004, 0.010) + sc*(step(0.994, h1.b)*(0.35 + 1.2*h1.r)) + vec3(1.0, 0.95, 0.9)*(step(0.997, h2.b)*(1.0 + 2.5*h2.r));
    float band = clamp(1.0 - abs(dot(d, vec3(0.26, 0.92, 0.29)))*2.2, 0.0, 1.0), neb = texture(uNoise, p*0.35).r;
    c += mix(vec3(0.10, 0.025, 0.16), vec3(0.015, 0.07, 0.15), texture(uNoise, p*0.6 + 0.5).g)*band*band*clamp((neb - 0.45)*3.0, 0.0, 1.0)*0.8;
    c += uSunCol*(clamp(sd*4000.0 - 3997.6, 0.0, 1.0)*30.0 + s16*s16*0.4 + s4*0.03);
  }
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
uniform vec3 uSunDir, uSunCol, uSkyTop, uSkyHor, uCamPos; uniform float uFogDen, uWet, uOvercast;
out vec3 vP, vL, vC; out vec2 vUV; out float vMat; out vec2 vFx; out vec4 vFog;
void main(){ vec4 w = uModel*vec4(aP,1.0); vP = w.xyz; vec3 n = normalize(mat3(uModel)*aN);
  float clear = 1.0 - 0.92*uOvercast, up = max(uSunDir.y, 0.0);
  // sky light from above, the sun, and sunlight bounced warm off the ground onto whatever faces down or sideways
  vL = (mix(uSkyHor*0.55 + vec3(0.05,0.045,0.04), uSkyTop*0.6 + uSkyHor*0.35, n.y*0.5+0.5) + uSunCol*max(dot(n, uSunDir), 0.0)
        + uSunCol*vec3(0.10,0.085,0.06)*((0.5 - 0.5*n.y)*up))*(1.0 - 0.3*uWet);
  float wall = step(1.5, aM)*step(aM, 2.5);
  vL *= mix(1.0, clamp(0.66 + aUV.y*0.04, 0.66, 1.0), wall);                    // buildings darken towards the street
  vec3 toCam = uCamPos - w.xyz; float d = length(toCam); vec3 V = toCam/max(d, 0.001);
  vFx = vec2(exp(-d*uFogDen), clamp(1.0 - d*0.007, 0.0, 1.0));
  // the fog glows towards the sun, as the sky does at the horizon; w: the sun glinting off this wall's windows
  float sd = max(-dot(V, uSunDir), 0.0), s4 = sd*sd; s4 *= s4;
  float nh = max(dot(n, normalize(V + uSunDir)), 0.0), g = nh*nh; g *= g; g *= g;
  vFog = vec4(uSkyHor + uSunCol*(s4*0.10*clear), g*wall*clear*up*dot(uSunCol, vec3(0.3)));
  vC = aC; vUV = aUV; vMat = aM; gl_Position = uVP*w; }
'''
WORLD_LOC = {'vP':0, 'vL':1, 'vC':2, 'vUV':3, 'vMat':4, 'vFx':5, 'vFog':6}

# vMat: 0 terrain (vUV.x = how much town) 1 road 2 building wall 3 flat 4 checker 5 hazard stripes 6 lamp
# One program per kind of surface. The compiler turns a test on vMat into "work out every material, then
# pick one", so a single world shader pays for all of them on every pixel (124 slots against 40 to 60).
#   ground  the terrain mesh (always material 0)
#   road, wall, misc   one material class each, for when the props mesh is drawn in per-material ranges
#   world   road + wall + misc picked by vMat: the whole props mesh in one draw, until it is split
_HEAD = '''#version 330 core
uniform vec3 uSkyHor, uHeadPos, uHeadDir; uniform float uNight, uWet, uBright, uHeadOn;
uniform sampler2D uNoise;
in vec3 vP, vL, vC; in vec2 vUV; in float vMat; in vec2 vFx; in vec4 vFog;
out vec4 oCol;
vec3 tone(vec3 c){ c = min(c*(uBright > 0.01 ? uBright : 1.0), vec3(2.5)); return sqrt(max(c*(1.3246 + c*(c*0.1284 - 0.6999)) - 0.0257, 0.0)); }
void main(){
  // fine grain from the noise texture's white-noise channel (8 cm a texel), faded out with distance: no mip maps here.
  // (The game's own detail texture, unit 6, is not used: on the console the ground and road that sampled it came out black.)
  float det = mix(0.5, texture(uNoise, vP.xz*0.05).b, vFx.y);
  float nz = texture(uNoise, vP.xz*0.026).r;
  vec3 alb = vC, lit = vL; vec3 glow = vec3(0.0);
'''
_TAIL = '''  if (uNight*uHeadOn > 0.01) {   // the player's headlights at night: the PC's cone and fall-off, without the surface's angle (no normal here)
    vec3 L = vP - uHeadPos; float d = length(L); float cone = smoothstep(0.78, 0.97, dot(L, uHeadDir)/max(d, 0.001));
    lit += vec3(1.0,0.93,0.78)*(uNight*uHeadOn*cone*13.0/(1.0 + 0.012*d*d)); }
  oCol = vec4(tone(mix(vFog.rgb, alb*lit + glow, vFx.x)), 1.0);
}
'''
_GROUND = '''  alb *= (0.72 + 0.56*nz)*(0.8 + 0.4*det);                    // grass, sand, rock; in town, paving with a backstreet every 27 m
  vec2 a = abs(mod(vP.xz + 660.0, 27.0) - 13.5);
  vec3 cty = mix(vec3(0.27,0.262,0.25), vec3(0.078,0.078,0.088), clamp((3.7 - min(a.x, a.y))*4.0, 0.0, 1.0)*clamp(vUV.y - 16.5, 0.0, 1.0))*(0.6 + 0.8*det);   // vUV.y: metres to the main road; paving to its kerb
  float strip = clamp(vUV.x - 1.0, 0.0, 1.0);                       // the open strip along the main road: grass verge, then red brick
  vec3 prom = mix(vec3(0.40,0.20,0.14)*(0.7 + 0.6*det), alb, 1.0 - clamp(vUV.y - 13.8, 0.0, 1.0));
  cty = mix(cty, prom, strip);
  alb = mix(alb, cty, clamp(vUV.x, 0.0, 1.0));
'''
_ROAD = '''  { float u = abs(vUV.x);                                        // tarmac, centre line, lane dashes, edge line, red and white kerbs
    vec3 k = clamp(1.0 - abs(vec3(u) - vec3(0.17, 4.0, 7.5))*12.0, 0.0, 1.0);
    vec3 r = vec3(0.105,0.105,0.115)*(0.62 + 0.76*det)*(0.8 + 0.4*nz);
    r = mix(r, vec3(0.85,0.62,0.05), k.x*0.9);
    r = mix(r, vec3(0.82), max(k.y*step(fract(vUV.y/9.0), 0.42), k.z)*0.85);
    alb = mix(r, mix(vec3(0.75,0.08,0.06), vec3(0.85), step(0.5, fract(vUV.y*0.25))), step(8.0, u));
    lit = uSkyHor*0.55 + vec3(0.05,0.045,0.04); glow = uSkyHor*(uWet*0.22*vC.b); }   // (vC.b: 0 on the tunnel's road, which stays dry)               // lit by the sky alone, as the PC game's road is; a wet road shines with it
'''
_WALL = '''  { vec2 q = vUV*vec2(0.2941, 0.3030), f = abs(fract(q) - vec2(0.5, 0.55));   // windows 3.4 m apart, a floor every 3.3 m; some lit at night
    float win = step(f.x, 0.3)*step(f.y, 0.27)*step(0.0, vUV.y);
    alb = mix(alb*(0.8 + 0.4*nz), vec3(0.06,0.08,0.11) + uSkyHor*0.25, win);
    glow = vec3(2.0,1.56,0.9)*(uNight*win*step(0.55, fract(dot(floor(q), vec2(0.37, 0.71))))) + vec3(1.0,0.9,0.72)*(vFog.w*win); }
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
    float wk = uWet*vC.b;   // (vC.b: 0 on the tunnel's road, which stays dry)
    float pud = clamp((texture(uNoise, vP.xz*0.0101 + 0.37).g - 0.55 + 0.15*uRain)*10.0, 0.0, 1.0)*step(0.2, uRain)*step(u, 8.0)*vC.b;
    // raindrop rings: one a 70 cm cell, its place and timing from the noise texture's white noise
    vec2 q = vP.xz*1.43, f = fract(q) - 0.5;
    vec2 h = texture(uHash, (floor(q) + 0.5)*0.00390625).br;   // (uHash: the same texture without mip maps - this lookup jumps from cell to cell, which a mip-mapped one would blur along the cell edges)
    vec2 d = f - (h - 0.5)*0.5; float r = max(length(d), 0.001), t = fract(uTime*(0.8 + 0.7*h.y) + h.x), x = r - t*0.42;
    float ring = (abs(fract(x*8.28) - 0.5)*4.0 - 1.0)*clamp(1.0 - abs(x)*14.3, 0.0, 1.0)*(1.0 - t);   // a ripple's crest and trough
    vec2 bump = d*(ring*0.42*pud*min(uRain, 1.3)*vFx.y/r);
    vec3 nb = vec3(bump.x, 1.0, bump.y);                                         // a slight tilt: no need to normalise
    float dn = dot(nb, V), ry = abs(2.0*dn - V.y) + 0.02;                    // the reflected ray's height: reflect(-V, nb).y with nb.y = 1
    float c5 = 1.0 - max(dn, 0.0), c2 = c5*c5, F = 0.04 + 0.96*c2*c2*c5;
    vec3 sky = mix(uSkyHor, uSkyTop, clamp(ry*1.5, 0.0, 1.0));
    alb *= 1.0 - (0.40 + 0.28*pud)*wk;
    glow = sky*wk*mix(0.03 + 0.30*F, 0.06 + 0.80*F, pud); }
'''
GROUND = _HEAD + _GROUND + _TAIL
ROAD = _HEAD + _ROAD + _TAIL
ROAD_WET = (_HEAD.replace('uniform vec3 uSkyHor, uHeadPos, uHeadDir;', 'uniform vec3 uSkyHor, uHeadPos, uHeadDir, uSkyTop, uCamPos; uniform float uRain, uTime; uniform sampler2D uHash;')
            + _ROAD + _ROAD_WET + _TAIL)
WALL = _HEAD + _WALL + _TAIL
MISC = _HEAD + _MISC + _TAIL
WORLD = (_HEAD + '  vec3 base = alb, g0 = glow;\n' + _ROAD + '  vec3 aRoad = alb, lRoad = lit; alb = base; lit = vL;\n' + _WALL
         + '  vec3 aWall = alb, gWall = glow; alb = base; glow = g0;\n' + _MISC
         + '  alb = vMat < 1.5 ? aRoad : vMat < 2.5 ? aWall : alb; lit = vMat < 1.5 ? lRoad : vL; glow = vMat < 1.5 ? g0 : vMat < 2.5 ? gWall : glow;\n' + _TAIL)


# The space track's road (the "slime" program, drawn after everything solid, premultiplied over it): green slime you
# see through to the planet and the stars. It heaves slowly, shines like something wet, and holds gold and silver
# sparkles at two depths that slide against each other as you move (shaders.h FS_WORLD material 9, made cheap)
SLIME = _HEAD.replace('uniform vec3 uSkyHor, uHeadPos, uHeadDir;', 'uniform vec3 uSkyHor, uHeadPos, uHeadDir, uSkyTop, uCamPos, uSunDir, uSunCol; uniform float uTime; uniform sampler2D uHash;').replace('void main(){', '''// one layer of sparkles: a four-pointed one in about a quarter of the cells (cs metres), gold or silver, twinkling.
// rgb: its light; a: how much of this pixel it covers
vec4 spark(vec2 p, float cs, float off){
  vec2 pp = p/cs, f = fract(pp) - 0.5; vec4 h = texture(uHash, (floor(pp) + 0.5 + off)*0.00390625);
  vec2 q = f - (h.rg - 0.5)*0.5; float r = length(q);
  float k = clamp(1.0 - (abs(q.x*q.y)*160.0 + r*(6.0 + 4.0*h.a)), 0.0, 1.0)*step(0.74, h.b);
  return vec4(mix(vec3(1.0, 0.74, 0.22), vec3(0.82, 0.88, 1.0), step(0.5, h.a))*k*(0.9 + 0.6*sin(uTime*3.0 + h.b*40.0)), k);
}
void main(){''') + '''  {
    vec3 V = normalize(uCamPos - vP);
    vec2 w = texture(uNoise, vP.xz*0.04 + uTime*vec2(0.006, 0.014)).rg, w2 = texture(uNoise, vP.xz*0.28 - uTime*vec2(0.04, 0.03)).gr;
    vec3 nb = vec3((w.x - 0.5)*0.9 + (w2.x - 0.5)*0.12, 1.0, (w.y - 0.5)*0.9 + (w2.y - 0.5)*0.12);
    float c5 = 1.0 - clamp(abs(dot(nb, V))*0.85, 0.0, 1.0), c2 = c5*c5, F = 0.04 + 0.96*c2*c2*c5;
    alb = mix(vec3(0.02, 0.22, 0.07), vec3(0.12, 0.50, 0.16), clamp((w.x - 0.35)*2.5, 0.0, 1.0))*(0.8 + 0.3*w2.x);
    vec3 R = reflect(-V, normalize(nb)); float sd = max(dot(R, uSunDir), 0.0), s = sd*sd; s *= s; s *= s; s *= s; s *= s; s *= s; s *= s;
    glow = uSunCol*s*1.8 + uSkyTop*F*0.8 + vec3(0.02, 0.16, 0.05)*(0.6 + 0.4*w.x);
    float vy = max(abs(V.y), 0.12);   // two layers of sparkles, 0.15 m and 0.45 m down
    vec4 s0 = spark(vP.xz - V.xz*(0.15/vy), 1.1, 0.0), s1 = spark(vP.xz - V.xz*(0.45/vy), 1.5, 91.0);
    vec3 sc = s0.rgb + s1.rgb*0.7; float st = max(s0.a, s1.a);
    glow += sc*1.6; alb = mix(alb, vec3(1.0, 0.85, 0.5), st*0.5);
    float a = clamp(0.38 + 0.40*F + 0.5*st + 0.5*s, 0.0, 0.95);   // (mostly see-through: the stars and the planet show under it)
    oCol = vec4(tone(mix(vFog.rgb, alb*lit + glow, vFx.x))*a, a); }
}
'''
# The space track's planet (the "planet" program; on the PC, FS_WORLD materials 10 and 11): lit per vertex like the
# world; on top, clouds drifting over it (looked up on two planes, mixed by latitude), the sea catching the sun, the
# air glowing blue round the rim, and the towns lit on the night side. vMat: 10 the sea and ice, 11 an island's
# ground (vUV.x: how much town), 2 the walls of its buildings, 3 their roofs and the roads
PLANET = _HEAD.replace('uniform vec3 uSkyHor, uHeadPos, uHeadDir;', 'uniform vec3 uSkyHor, uHeadPos, uHeadDir, uCamPos, uSunDir, uSunCol; uniform float uTime;') + '''  {
    vec3 rn = normalize(vP - vec3(0.0, 60.0, 0.0)), V = normalize(uCamPos - vP);   // (60: World's SPACE_Y, the planet's middle)
    float cn = mix(texture(uNoise, rn.xz*0.9 + vec2(uTime*0.0012, 0.0)).r, texture(uNoise, vec2(rn.x + rn.z, rn.y)*0.9 + vec2(uTime*0.0012, 0.3)).g, clamp(1.0 - abs(rn.y)*1.4, 0.0, 1.0));
    float cl = clamp((cn - 0.56)*5.0, 0.0, 1.0), up = dot(rn, uSunDir), day = clamp(up*3.0 + 0.3, 0.0, 1.0), night = clamp(-up*4.0 + 0.2, 0.0, 1.0);
    float sea = step(9.5, vMat)*step(vMat, 10.5)*clamp((vC.b - max(vC.r, vC.g))*5.0, 0.0, 1.0);
    vec3 R = reflect(-V, rn); float sd = max(dot(R, uSunDir), 0.0), s = sd*sd; s *= s; s *= s; s *= s; s *= s; s *= s;
    alb = mix(alb, vec3(0.95), cl*0.9);
    float rim = 1.0 - max(dot(rn, V), 0.0); rim = rim*rim; rim = rim*rim;
    float town = step(10.5, vMat)*clamp(vUV.x, 0.0, 1.0) + step(1.5, vMat)*step(vMat, 2.5)*0.6;
    glow = uSunCol*s*sea*(1.0 - cl)*0.9 + vec3(0.20, 0.45, 1.0)*rim*(0.05 + 0.8*day) + vec3(1.0, 0.72, 0.38)*town*night*0.5; }
''' + _TAIL


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

# the undersea tunnel's glass: the sea seen through it (shaders.h FS_GLASS): blue-green deepening with depth, the sun's
# caustics, shafts of light, thin window joins every 4 m and three round the arch; premultiplied over the world
GLASS = COMMON + '''
in vec3 vP; in vec2 vUV; in vec4 vC; in vec2 vMisc;
void main(){
  float wl = 0.22*sin(vP.x*0.40 + uTime*1.3) + 0.18*sin(vP.z*0.33 - uTime*1.1);
  float under = clamp((wl - vP.y)*10.0 + 0.5, 0.0, 1.0), depth = max(wl - vP.y, 0.0), k = clamp(depth*0.0357, 0.0, 1.0);
  vec3 water = mix(vec3(0.07, 0.38, 0.44), vec3(0.012, 0.11, 0.17), k)*(0.55 + 0.0833*(uSunCol.r + uSunCol.g + uSunCol.b));
  vec2 q = vP.xz*0.32;
  float c1 = sin(q.x + uTime*1.1 + sin(q.y*1.3 + uTime*0.7)) + sin(q.y*1.1 - uTime*0.9 + sin(q.x*1.7 - uTime*0.5));
  float c2 = sin((q.x + q.y)*1.7 + uTime*1.6)*sin((q.x - q.y)*1.4 - uTime*1.2);
  float e1 = clamp(1.0 - abs(c1)*0.55, 0.0, 1.0), e2 = clamp(1.0 - abs(c2), 0.0, 1.0); e1 = e1*e1*e1; e2 = e2*e2; e2 = e2*e2;
  float caus = (e1*e1 + 0.5*e2*e2)*exp(-depth*0.06);
  float sh = 0.5 + 0.5*sin(vUV.x*0.11 + vP.y*0.05 + uTime*0.25); sh = sh*sh*sh; float shaft = sh*sh*0.5*exp(-depth*0.05);
  vec3 cw = water*(1.0 + 1.4*caus) + vec3(0.25, 0.45, 0.42)*shaft*uSunCol.g*0.4;
  float aw = mix(0.84, 0.62, sin(vUV.y*3.14159))*(0.9 + 0.1*k) - 0.15*caus;
  vec3 ca = vec3(0.055, 0.07, 0.075) + uSkyHor*0.04;
  vec3 c = mix(ca, cw, under); float a = mix(0.10, aw, under);
  float band = exp(-abs(vP.y - wl)*7.0)*(0.75 + 0.25*sin(vUV.x*2.3 + uTime*3.0));
  c += vec3(0.75, 0.92, 0.95)*band*0.9; a = max(a, band*0.8);
  float frame = clamp(1.0 - abs(fract(vUV.x*0.1 + 0.5) - 0.5)*83.3, 0.0, 1.0);
  c = mix(c, vec3(0.05, 0.06, 0.07), frame*0.9); a = mix(a, 0.96, frame);
  c = tone(c);
  oCol = vec4(c*a, a);
}
'''

LEAN_VS = {n: (WORLD_VS, WORLD_LOC) for n in ('world', 'ground', 'road', 'roadwet', 'wall', 'misc', 'slime', 'planet')}
LEAN_VS['sky'] = (SKY_VS, {'vNdc': 0})
# The depth pre-pass must place every vertex exactly where the colour pass does, or the colour pass's depth test
# (less-or-equal, depth writes off) fails on the ground and the road and leaves them black. So it runs the colour
# pass's own vertex shader; its pixel shader writes nothing that is kept (colour writes are off).
LEAN_VS['depth'] = (WORLD_VS, WORLD_LOC)
DEPTH = '''#version 330 core
out vec4 oCol;
void main(){ oCol = vec4(1.0); }
'''
LEAN_EXTRA = ('ground', 'road', 'roadwet', 'wall', 'misc', 'slime', 'planet')   # programs only the lean build has
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

LEAN = {'ui': UI, 'depth': DEPTH, 'sky': SKY, 'world': WORLD, 'ground': GROUND, 'road': ROAD, 'roadwet': ROAD_WET, 'wall': WALL, 'misc': MISC, 'car': CAR, 'slime': SLIME, 'planet': PLANET, 'water': WATER, 'part': PART, 'mark': MARK, 'ring': RING, 'glass': GLASS}

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
