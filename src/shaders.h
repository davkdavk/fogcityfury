// shaders.h - all GLSL 3.30 sources
#pragma once

static const char* VS_MESH = R"(#version 330 core
layout(location=0) in vec3 aP; layout(location=1) in vec3 aN; layout(location=2) in vec3 aC;
layout(location=3) in vec2 aUV; layout(location=4) in float aM;
uniform mat4 uVP, uModel;
out vec3 vP, vN, vC; out vec2 vUV; out float vMat;
void main(){ vec4 w = uModel*vec4(aP,1.0); vP = w.xyz; vN = mat3(uModel)*aN; vC = aC; vUV = aUV; vMat = aM; gl_Position = uVP*w; }
)";

static const char* VS_SKY = R"(#version 330 core
out vec2 vNdc;
void main(){ vec2 p = vec2((gl_VertexID<<1)&2, gl_VertexID&2)*2.0-1.0; vNdc = p; gl_Position = vec4(p,1.0,1.0); }
)";

static const char* VS_PART = R"(#version 330 core
layout(location=0) in vec3 aP; layout(location=1) in vec2 aUV; layout(location=2) in vec4 aC; layout(location=3) in vec2 aMisc;
uniform mat4 uVP;
out vec3 vP; out vec2 vUV; out vec4 vC; out vec2 vMisc;
void main(){ vP = aP; vUV = aUV; vC = aC; vMisc = aMisc; gl_Position = uVP*vec4(aP,1.0); }
)";

static const char* VS_UI = R"(#version 330 core
layout(location=0) in vec2 aP; layout(location=1) in vec2 aUV; layout(location=2) in vec4 aC;
uniform vec2 uScr; out vec2 vUV; out vec4 vC;
void main(){ vUV = aUV; vC = aC; gl_Position = vec4(aP.x/uScr.x*2.0-1.0, 1.0-aP.y/uScr.y*2.0, 0.0, 1.0); }
)";

// shared by every lit fragment shader: sky model, fog, shadows, tonemapping
static const char* FS_COMMON = R"(#version 330 core
uniform vec3 uSunDir, uSunCol, uSkyTop, uSkyHor, uCamPos;
uniform float uTime, uFogDen, uHDR, uNight, uHeadOn, uWet, uRain, uOvercast, uFogNoise, uBoltI, uUseEnv; uniform vec3 uHeadPos, uHeadDir, uBoltDir, uEnvPos;
uniform samplerCube uEnv;   // reflection cubemap around the player's car (not bound while it is being rendered)
uniform sampler2DShadow uShadow; uniform mat4 uLightVP;
#ifdef FCF_PERF_NOISE
uniform sampler2D uNoise;
#endif
out vec4 oCol;
float hash(vec2 p){ return fract(sin(dot(p, vec2(127.1,311.7)))*43758.5453); }
float vnoise(vec2 p){ vec2 i = floor(p), f = fract(p); f = f*f*(3.0-2.0*f);
#ifdef FCF_PERF_NOISE
  return texture(uNoise, (i + f + 0.5)/256.0).r;
#else
  return mix(mix(hash(i),hash(i+vec2(1,0)),f.x), mix(hash(i+vec2(0,1)),hash(i+vec2(1,1)),f.x), f.y);
#endif
}
float fbm(vec2 p){ float a = 0.5, s = 0.0; for(int i=0;i<4;i++){ s += a*vnoise(p); p = p*2.03+7.1; a *= 0.5; } return s; }
vec3 skyColor(vec3 d){
  float h = clamp(d.y, 0.0, 1.0);
  vec3 c = mix(uSkyHor, uSkyTop, pow(h, 0.55));
  float sd = max(dot(d, uSunDir), 0.0);
  c += uSunCol*(pow(sd,1800.0)*40.0 + pow(sd,90.0)*0.35 + pow(sd,8.0)*0.10)*(1.0-0.92*uOvercast);
  if(d.y > 0.0){
    vec2 cp = d.xz/(d.y+0.14)*1.4 + vec2(uTime*0.006, uTime*0.002);
    float cn = fbm(cp), ov = uOvercast;
    float cl = smoothstep(0.50-0.36*ov, 0.80-0.32*ov, cn)*smoothstep(0.0, 0.10, d.y);
    vec3 cc = mix(uSkyHor, vec3(1.0), 0.55*(1.0-0.75*ov)) + uSunCol*0.18*pow(sd, 3.0)*(1.0-ov);
    cc *= 1.0 - ov*(0.25 + 0.55*smoothstep(0.35, 0.75, fbm(cp*2.1+9.0)));     // heavy, uneven cloud bases
    if(uBoltI > 0.01 && d.y > 0.02){                                          // a jagged bolt on the horizon, lighting the cloud around it
      vec2 bd = normalize(uBoltDir.xz), dh = normalize(d.xz + vec2(1e-5));
      float ang = dh.x*bd.y - dh.y*bd.x, along = dot(dh, bd);
      float jag = (vnoise(vec2(d.y*24.0, uBoltDir.y*57.0))-0.5)*0.11 + (vnoise(vec2(d.y*95.0, uBoltDir.y*31.0))-0.5)*0.03;
      float w = abs(ang - jag), top = 1.0 - smoothstep(0.38, 0.55, d.y);
      c += vec3(0.75,0.82,1.0)*(exp(-w*1100.0)*16.0*top + exp(-w*14.0)*0.9)*step(0.0, along)*uBoltI;
      cc += vec3(0.6,0.66,0.85)*exp(-w*6.0)*step(0.0, along)*uBoltI*1.5;
    }
    if(uNight > 0.5){ vec2 sp = d.xz/(d.y+0.25)*90.0; vec2 f = fract(sp)-0.5; float st = step(0.975, hash(floor(sp)))*smoothstep(0.22, 0.0, length(f)); c += vec3(0.8,0.85,1.0)*st*(1.0-cl)*1.5*(1.0-uOvercast); cc *= 0.5; }
    c = mix(c, cc, cl*mix(0.85, 0.98, uOvercast));
  }
  return c;
}
vec3 ambient(vec3 n){ return mix(uSkyHor*0.55 + vec3(0.05,0.045,0.04), uSkyTop*0.6 + uSkyHor*0.35, n.y*0.5+0.5); }
#if defined(FCF_PERF_NOISE) && FCF_PERF_PASS_LEVEL < 2
float shadow(vec3 wp, vec3 n){ return 1.0; }
#else
float shadow(vec3 wp, vec3 n){
  vec4 lp = uLightVP*vec4(wp + n*0.12, 1.0); vec3 s = lp.xyz/lp.w*0.5+0.5;
  if(s.x<0.0||s.x>1.0||s.y<0.0||s.y>1.0||s.z>1.0) return 1.0;
  float r = 0.0; vec2 t = vec2(1.0/2048.0);
  for(int x=-1;x<=1;x++) for(int y=-1;y<=1;y++) r += texture(uShadow, vec3(s.xy + vec2(x,y)*t, s.z-0.0012));
  float edge = 1.0-smoothstep(0.42, 0.5, max(abs(s.x-0.5), abs(s.y-0.5)));
  return mix(1.0, r/9.0, edge);
}
#endif
vec3 applyFog(vec3 c, vec3 wp){
  float d = length(wp-uCamPos);
  float bank = uFogNoise > 0.0 ? 1.0 + uFogNoise*(fbm(wp.xz*0.011 + vec2(uTime*0.035, uTime*0.02)) - 0.45)*1.5 : 1.0;
  float f = 1.0-exp(-d*uFogDen*bank);
  vec3 fc = uSkyHor + uSunCol*0.12*pow(max(dot(normalize(wp-uCamPos),uSunDir),0.0), 6.0);
  return mix(c, fc, f);
}
vec3 tonemap(vec3 c){ c *= 0.64; c = (c*(2.51*c+0.03))/(c*(2.43*c+0.59)+0.14); return pow(clamp(c,0.0,1.0), vec3(1.0/2.2)); }
float roadGrain(vec2 p){ return vnoise(p*6.5) + 0.5*vnoise(p*15.0); }
// one raindrop ring per cell, expanding and fading; returns the surface slope it makes
vec2 ripple(vec2 p, float sc, float seed){
  vec2 q = p*sc, id = floor(q), f = fract(q) - 0.5;
  float h1 = hash(id + seed), h2 = hash(id + seed + 17.3);
  vec2 d = f - (vec2(h1, h2) - 0.5)*0.5; float r = length(d), t = fract(uTime*(0.8 + 0.7*h2) + h1), rr = t*0.42;
  float ring = sin((r - rr)*52.0)*(1.0 - smoothstep(0.0, 0.07, abs(r - rr)))*(1.0 - t);
  return r > 0.001 ? d/r*ring : vec2(0.0);
}
// player's headlights (night tracks): a soft cone ahead of the car
vec3 headlights(vec3 wp, vec3 n){
  vec3 L = wp - uHeadPos; float d = length(L); L /= max(d, 0.001);
  float cone = smoothstep(0.78, 0.97, dot(L, uHeadDir));
  return vec3(1.0,0.93,0.78)*uNight*uHeadOn*cone*max(dot(n,-L), 0.15)*22.0/(1.0+0.012*d*d);
}
void finish(vec3 c, float a){ if(uHDR > 0.5) oCol = vec4(c, a); else oCol = vec4(tonemap(c), a); }
)";

static const char* FS_SKY = R"(
in vec2 vNdc; uniform vec3 uCamR, uCamU, uCamF; uniform vec2 uTan;
void main(){ vec3 d = normalize(uCamF + uCamR*vNdc.x*uTan.x + uCamU*vNdc.y*uTan.y);
#ifdef FCF_FLAT_TEST
  oCol = vec4(mix(uSkyHor, uSkyTop, clamp(d.y, 0.0, 1.0)), 1.0);
#else
  vec3 c = skyColor(d); if(d.y<0.0) c = mix(c, uSkyHor*0.8, smoothstep(0.0,0.25,-d.y)); finish(c, 1.0);
#endif
}
)";

// terrain, road, buildings and props. vMat: 0 terrain 1 road 2 building wall 3 flat 4 checker 5 hazard stripes
static const char* FS_WORLD = R"(
in vec3 vP, vN, vC; in vec2 vUV; in float vMat;
uniform vec4 uKeySt;         // the backstreet to the key warehouse: x0, z0, x1, z1 (all zero if there is none)
uniform vec4 uKeyLot;        // the warehouse's plot (min x, min z, max x, max z): one paved yard, no streets through it
uniform sampler2D uDetail;   // unit 6: tileable ground detail (r grain, g speckle); mipmapped, anisotropically filtered per the video settings
void main(){
  vec3 n = normalize(vN); if(!gl_FrontFacing) n = -n;
  vec3 V = normalize(uCamPos-vP); float dist = length(uCamPos-vP);
  vec3 alb = vC; float spec = 0.0; float gloss = 40.0; vec3 emis = vec3(0.0); vec3 refl = vec3(0.0);
  int m = int(vMat+0.5);
  if(m==0){
    float nz = fbm(vP.xz*0.21), n2 = 0.5*vnoise(vP.xz*2.7) + 0.5*texture(uDetail, vP.xz*0.045).r;
    alb *= (0.72+0.56*nz)*(0.9+0.2*n2)*(1.0 - 0.28*uWet);
    if(vUV.x > 0.01){
      // Town. Building lots are 27 m apart; half-way between every two runs a backstreet: worn tarmac with cobbles showing
      // through, a damp gutter, kerbstones, then paving slabs up to the walls. (The grid is World::GRID / GRID0 in world.h.)
      vec2 sgn = mod(vP.xz + 660.0, 27.0) - 13.5, a = abs(sgn); bool az = a.x < a.y;   // sgn: offset from the nearest backstreet centre line, per axis
      float al = min(a.x, a.y), along = az ? vP.z : vP.x, lat = az ? sgn.x : sgn.y;
      float aw = fwidth(al)*1.5 + 0.02, fade = clamp(1.0 - dist/110.0, 0.0, 1.0);
      vec2 dt = texture(uDetail, vP.xz*0.21).rg;
      float worn = smoothstep(0.52, 0.56, fbm(vP.xz*0.23 + 7.0));
      vec3 tar = vec3(0.078,0.078,0.088)*(0.6 + 0.8*dt.r)*(1.0 + 0.8*dt.g)*mix(1.0, 0.62, worn);
      vec2 cb = vec2(along/0.55, lat/0.32 + 0.5*floor(along/0.55));                                  // cobbles, laid like bricks
      float cmask = smoothstep(0.50, 0.58, fbm(vP.xz*0.09 + 2.0))*(1.0 - smoothstep(1.6, 2.2, al));
      float cj = smoothstep(0.36, 0.5, max(abs(fract(cb.x) - 0.5), abs(fract(cb.y) - 0.5)));
      tar = mix(tar, vec3(0.17,0.16,0.15)*(0.65 + 0.7*hash(floor(cb)))*(1.0 - 0.65*cj*fade), cmask*mix(0.35, 1.0, fade));
      float mr = length(vec2(mod(along + 660.0, 27.0) - 7.0, lat - 0.9));                              // a manhole cover in every block
      tar = mix(tar, vec3(0.05,0.045,0.04)*(0.75 + 0.5*step(0.5, fract(mr*8.0))), (1.0 - smoothstep(0.42, 0.42 + aw, mr))*fade);
      tar *= 1.0 - 0.5*smoothstep(2.5, 3.3, al);                                                      // the gutter is dark and damp
      vec3 kerb = vec3(0.40,0.39,0.37)*(0.85 + 0.3*dt.r)*(1.0 - 0.55*smoothstep(0.44, 0.5, abs(fract(along/1.2) - 0.5))*fade);
      vec2 sl = abs(fract(vP.xz/1.5) - 0.5); float joint = smoothstep(0.46, 0.5, max(sl.x, sl.y))*fade;
      vec3 pave = vec3(0.27,0.262,0.25)*(0.80 + 0.4*n2)*(1.0 - 0.4*joint)*(1.0 - 0.25*smoothstep(0.55, 0.7, fbm(vP.xz*0.31)));
      float road = 1.0 - smoothstep(3.5 - aw, 3.5 + aw, al), walk = smoothstep(3.9 - aw, 3.9 + aw, al);
      float yard = step(uKeyLot.x, vP.x)*step(vP.x, uKeyLot.z)*step(uKeyLot.y, vP.z)*step(vP.z, uKeyLot.w); road *= 1.0 - yard; walk = max(walk, yard);
      float clear = smoothstep(16.5, 17.5, vUV.y); road *= clear; walk = max(walk, 1.0 - clear);   // along the main road: a clean band of paving to its kerb; the backstreets end at a kerbstone before it
      vec3 cty = mix(mix(tar, kerb, 1.0 - road), pave, walk);
      // Open strip from the kerb to the first buildings: grass verge, kerbstone and running-bond brick promenade.
      float strip = clamp(vUV.x - 1.0, 0.0, 1.0);
      if(strip > 0.001){
        vec2 bq = vec2(vP.x/0.42 + 0.5*floor(vP.z/0.21), vP.z/0.21), bf = abs(fract(bq) - 0.5);
        float mortar = smoothstep(0.40, 0.48, max(bf.x, bf.y))*fade;
        vec3 brick = vec3(0.40,0.20,0.14)*(0.75 + 0.5*hash(floor(bq)))*(0.85 + 0.3*dt.r)*(1.0 - 0.55*mortar);
        float verge = 1.0 - smoothstep(13.8, 14.0, vUV.y), kstone = smoothstep(13.8, 14.0, vUV.y)*(1.0 - smoothstep(14.4, 14.6, vUV.y));
        vec3 prom = mix(brick, vec3(0.40,0.39,0.37)*(0.85 + 0.3*dt.r), kstone);
        cty = mix(cty, mix(prom, alb, verge), strip); road *= 1.0 - strip;
      }
      alb = mix(alb, cty*(1.0 - 0.30*uWet), clamp(vUV.x, 0.0, 1.0));
      if(uWet > 0.001){                                                                               // rain collects in the hollows of the street
        float F = 0.04 + 0.96*pow(1.0 - max(dot(n, V), 0.0), 5.0), pud = smoothstep(0.50, 0.58, fbm(vP.xz*0.13 + 1.3))*road*step(0.2, uRain);
        spec = mix(0.15, 0.6 + 0.8*pud, road)*uWet*clamp(vUV.x, 0.0, 1.0); gloss = mix(160.0, 600.0, pud);
        vec3 Rw = reflect(-V, n); Rw.y = abs(Rw.y) + 0.02; refl += skyColor(normalize(Rw))*uWet*clamp(vUV.x, 0.0, 1.0)*road*(0.03 + 0.25*F + 0.6*pud*F); alb *= 1.0 - 0.4*pud*vUV.x;
      }
      if(uNight > 0.5){                                                                               // pools of light under the street lamps
        vec2 id = floor((vP.xz + 646.5)/27.0 + 0.5), lp = id*27.0 - 646.5 + 3.15, dl = vP.xz - lp;
        float gl = exp(-dot(dl, dl)/42.0)*(1.0 - mod(id.x + id.y, 2.0));
        vec2 ka = uKeySt.xy, kb = uKeySt.zw - ka; float kl = dot(kb, kb);                              // and the orange ones down the street to the warehouse
        if(kl > 1.0){ vec2 kd = vP.xz - ka - kb*clamp(dot(vP.xz - ka, kb)/kl, -0.4, 1.0); gl += 0.0; emis += cty*vec3(1.0,0.55,0.2)*exp(-dot(kd, kd)/20.0)*2.2*clamp(vUV.x, 0.0, 1.0); }
        emis += cty*vec3(0.85,0.92,1.0)*gl*4.0*clamp(vUV.x, 0.0, 1.0);
      }
    }
  } else if(m==1){
    float u = abs(vUV.x), v = vUV.y, aa = fwidth(vUV.x)*1.2+0.01;
    vec2 dtl = texture(uDetail, vP.xz*0.16).rg;
    float g = vnoise(vP.xz*3.1)*0.4 + dtl.r*0.6;
    alb = vec3(0.105,0.105,0.115)*(0.62+0.76*g)*(0.8+0.4*smoothstep(0.35,0.65,fbm(vP.xz*0.06)))*(1.0 + 0.9*dtl.g);
    // tyre-worn lanes
    alb *= 1.0-0.18*exp(-pow((u-2.0)/1.1,2.0))-0.18*exp(-pow((u-6.0)/1.1,2.0));
    float fade = clamp(1.0-aa*1.5, 0.0, 1.0);
    float yl = 1.0-smoothstep(0.07-aa, 0.07+aa, abs(u-0.17));
    float dash = (1.0-smoothstep(0.07-aa, 0.07+aa, abs(u-4.0)))*step(fract(v/9.0), 0.42);
    float edge = 1.0-smoothstep(0.08-aa, 0.08+aa, abs(u-7.5));
    alb = mix(alb, vec3(0.85,0.62,0.05), yl*fade*0.9);
    alb = mix(alb, vec3(0.82), max(dash,edge)*fade*0.85);
    if(u > 8.0){ alb = mix(vec3(0.75,0.08,0.06), vec3(0.85), step(0.5, fract(v/4.0))); }
    spec = 0.25; gloss = 30.0;
    if(uWet > 0.001){
      vec2 p = vP.xz; float kd = 1.0/(1.0 + dist*0.035);                      // detail fades with distance so it does not shimmer
      // standing water collects in the hollows; more of it the harder it rains
      float pud = smoothstep(0.50-0.15*uRain, 0.60-0.15*uRain, fbm(p*0.10+3.7))*step(0.2, uRain)*(1.0 - 0.5*step(8.0, u));
      // bump map: asphalt grain on damp tarmac, raindrop rings on the puddles
      float e = 0.07, g0 = roadGrain(p), gx = roadGrain(p+vec2(e,0.0)), gz = roadGrain(p+vec2(0.0,e));
      vec2 bump = vec2(g0-gx, g0-gz)/e*0.05*mix(1.0, 0.12, pud) + (ripple(p, 1.15, 0.0) + ripple(p, 2.05, 5.0))*0.42*pud*min(uRain, 1.3);
      vec3 nb = normalize(vec3(bump.x*kd, 1.0, bump.y*kd));
      vec3 Rw = reflect(-V, nb); Rw.y = abs(Rw.y) + 0.02; Rw = normalize(Rw);
      float F = 0.04 + 0.96*pow(1.0-max(dot(nb,V),0.0), 5.0);
      vec3 envc = skyColor(Rw);
      if(uUseEnv > 0.5){
        // sphere-projected cubemap: buildings and lit windows land roughly where they should on the road
        vec3 ro = vP - uEnvPos; float bq = dot(ro, Rw), cq = dot(ro, ro) - 75.0*75.0;
        vec3 dir = ro + Rw*(-bq + sqrt(max(bq*bq - cq, 0.0)));
        vec3 ec = textureLod(uEnv, dir, mix(4.0, 0.7, pud)).rgb;                 // blurred sheen on damp tarmac, near-mirror on water
        envc = mix(envc, ec, 1.0 - smoothstep(55.0, 140.0, length(ro)));
      }
      alb *= 1.0 - (0.40 + 0.28*pud)*uWet;
      refl += envc*uWet*mix(0.04 + 0.42*F, 0.08 + 0.92*F, pud);
      spec = mix(0.55, 1.5, pud)*uWet; gloss = mix(110.0, 800.0, pud); n = nb;
    }
  } else if(m==6){
    emis = vC*3.0; alb = vC;                                                   // lamps
  } else if(m==2){
    vec2 cell = vec2(3.4, 3.3); vec2 q = vUV/cell; vec2 id = floor(q), f = fract(q);
    float aa = clamp(fwidth(q.x)*3.0, 0.0, 1.0);
    float win = step(0.2,f.x)*step(f.x,0.8)*step(0.28,f.y)*step(f.y,0.82);
    if(vUV.y < 3.3) win = step(0.08,f.x)*step(f.x,0.92)*step(0.12,f.y)*step(f.y,0.78);
    win *= step(0.0, vUV.y)*(1.0-aa);
    float r = hash(id + floor(vC.xy*91.0));
    alb *= 0.85+0.3*vnoise(vP.xz*0.7+vP.y*0.9);
    alb *= 1.0 - 0.12*step(f.y, 0.06);
    vec3 R = reflect(-V, n); float F = 0.06+0.94*pow(1.0-max(dot(n,V),0.0), 4.0);
    vec3 glass = mix(vec3(0.03,0.05,0.08)+0.06*r, skyColor(vec3(R.x,abs(R.y),R.z)), 0.25+0.6*F);
    alb = mix(alb, vec3(0.06,0.08,0.11)+0.05*r, win*0.999);
    refl = glass*win; spec = win; gloss = 200.0;
    emis = uNight*win*step(0.52, r)*mix(vec3(1.0,0.72,0.35), vec3(0.7,0.85,1.0), step(0.86, r))*(1.2+1.6*hash(id.yx+3.0));
    if(vUV.y < 0.0) alb = vec3(0.2,0.2,0.21);
  } else if(m==4){
    vec2 c = floor(vUV/0.7); alb = mix(vec3(0.03), vec3(0.95), mod(c.x+c.y, 2.0));
  } else if(m==5){
    float s = step(0.5, fract((vUV.x*3.0+vUV.y*6.0)));
    alb = mix(vec3(0.95,0.72,0.02), vec3(0.04), s);
  }
  float ndl = max(dot(n,uSunDir),0.0); float sh = shadow(vP, n);
  vec3 H = normalize(V+uSunDir);
  vec3 col = alb*(ambient(n) + uSunCol*ndl*sh + headlights(vP, n)) + refl + uSunCol*spec*sh*pow(max(dot(n,H),0.0), gloss)*(gloss>100.0?2.0:0.35) + emis;
  finish(applyFog(col, vP), 1.0);
}
)";

// car paint with dynamic cubemap reflections. vMat: 0 plain 1 paint 2 glass 3 headlight 4 taillight 5 rubber
static const char* FS_CAR = R"(
in vec3 vP, vN, vC; in vec2 vUV; in float vMat;
uniform vec3 uPaint; uniform float uBrake, uChrome, uBurnt, uGone;   // uGone: 0 solid .. 1 faded away (wrecks dissolve through a dither pattern)
void main(){
  vec3 n = normalize(vN); if(!gl_FrontFacing) n = -n;
  vec3 V = normalize(uCamPos-vP); float ndv = max(dot(n,V),0.0);
  vec3 R = reflect(-V, n);
  vec3 env = textureLod(uEnv, R, int(vMat+0.5)==1 ? 1.3 : 0.4).rgb;
  float F = 0.05+0.95*pow(1.0-ndv, 5.0);
  float ndl = max(dot(n,uSunDir),0.0); float sh = shadow(vP, n);
  vec3 H = normalize(V+uSunDir); float ndh = max(dot(n,H),0.0);
  vec3 amb = ambient(n); vec3 col; int m = int(vMat+0.5);
  F = mix(F, min(1.0, F*1.5 + 0.04), uWet);                                   // rain makes everything glossier
  if(uGone > 0.001 && fract(52.9829189*fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715)))) < uGone) discard;
  if(uBurnt > 0.001){                                                        // a burnt-out wreck: soot, bare scorched metal, a few embers
    float soot = fbm(vP.xz*2.3 + vP.y*3.1), em = smoothstep(0.80, 0.93, vnoise(vP.xz*9.0 + vP.y*7.0 + uTime*0.25));
    vec3 base = mix(vec3(0.012,0.011,0.010), vec3(0.10,0.075,0.055), smoothstep(0.45, 0.8, soot));
    col = base*(amb + uSunCol*ndl*sh + headlights(vP, n)) + env*F*0.05 + vec3(1.6,0.45,0.06)*em*clamp(uBurnt - 1.0, 0.0, 1.0);
    finish(applyFog(col, vP), 1.0); return;
  }
  vec3 sunSpec = uSunCol*sh*(pow(ndh,500.0)*5.0 + pow(ndh,40.0)*0.22);
  if(m==1){
    vec3 base = uPaint*(amb + uSunCol*ndl*sh + headlights(vP, n));
    // two-layer paint: tinted metallic base under a clear coat
    vec3 metal = uPaint*env*0.55*(1.0-ndv*0.5);
    col = (base + metal)*(1.0-F) + env*(F + 0.07) + sunSpec;
    col = mix(col, env*mix(vec3(1.0),uPaint,0.75)*1.05 + sunSpec, uChrome);
  } else if(m==2){
    col = vec3(0.012,0.016,0.02)*amb + env*(0.22+0.78*F) + sunSpec;
  } else if(m==3){
    col = vC*2.1 + env*0.3;
  } else if(m==4){
    col = vC*(0.25*(amb+uSunCol*ndl*sh) + 0.25 + uNight*1.2 + uBrake*2.6);
  } else if(m==5){
    col = vC*(amb + uSunCol*ndl*sh);
  } else {
    col = vC*(amb + uSunCol*ndl*sh)*(1.0-F*0.4) + env*F*0.4 + sunSpec*0.3;
  }
  finish(applyFog(col, vP), 1.0);
}
)";

static const char* FS_WATER = R"(
#ifndef FCF_WATER_TEST
#define FCF_WATER_TEST 0
#endif
in vec3 vP, vN, vC; in vec2 vUV; in float vMat;
uniform sampler2D uHeight;
float wh(vec2 p){ float t = uTime;
  return fbm(p*0.09 + vec2(t*0.11, t*0.07))*1.4 + vnoise(p*0.55 + vec2(-t*0.5, t*0.33))*0.35 + vnoise(p*1.7 + vec2(t*0.9, -t*0.6))*0.10; }
void main(){
  vec2 huv = (vP.xz+768.0)/1536.0;
#if FCF_WATER_TEST == 1
  vec4 ht = texture(uHeight, clamp(huv, 0.0, 1.0));
  float th = (ht.r*65280.0 + ht.g*255.0)/65535.0*192.0 - 96.0;
#else
  float th = texture(uHeight, clamp(huv, 0.0, 1.0)).r;
#endif
  if(huv.x<0.0||huv.x>1.0||huv.y<0.0||huv.y>1.0) th = -30.0;
  float depth = -th;
#if FCF_WATER_TEST != 2
  if(depth < -0.02) discard;
#endif
  float dist = length(uCamPos-vP);
  float e = 0.35;
#if FCF_WATER_TEST == 3
  float h0 = 0.0, hx = 0.0, hz = 0.0;
#else
  float h0 = wh(vP.xz), hx = wh(vP.xz+vec2(e,0.0)), hz = wh(vP.xz+vec2(0.0,e));
#endif
  float k = 0.9/(1.0+dist*0.012);
  vec3 n = normalize(vec3(-(hx-h0)/e*k, 1.0, -(hz-h0)/e*k));
  vec3 V = normalize(uCamPos-vP);
  vec3 R = reflect(-V, n); R.y = abs(R.y)+0.02; R = normalize(R);
  float F = 0.03+0.97*pow(1.0-max(dot(n,V),0.0), 5.0);
  float sh = shadow(vP, vec3(0.0,1.0,0.0));
  vec3 sky = skyColor(R);
  vec3 deep = vec3(0.012,0.07,0.11), shallow = vec3(0.07,0.30,0.30);
  vec3 body = mix(shallow, deep, smoothstep(0.0, 5.0, depth))*(ambient(n)*1.4 + uSunCol*0.35*sh);
  vec3 col = mix(body, sky*mix(0.55,1.0,sh), F);
  // shoreline foam, animated
  float fo = 1.0-smoothstep(0.0, 0.45, depth);
  float foam = fo*smoothstep(0.42, 0.75, vnoise(vP.xz*1.6 + vec2(uTime*0.4,0.0)) + fo*0.5 + 0.15*sin(depth*9.0 - uTime*2.5));
  col = mix(col, vec3(0.9)*(ambient(n)+uSunCol*0.6*sh), clamp(foam,0.0,1.0)*0.85);
  float a = clamp(depth*1.1+0.25, 0.0, 1.0);
#if FCF_WATER_TEST == 2
  if(depth < -0.02) a = 0.0;
#endif
  finish(applyFog(col, vP), a);
}
)";

static const char* FS_PART = R"(
in vec3 vP; in vec2 vUV; in vec4 vC; in vec2 vMisc;
void main(){
  float r = length(vUV); if(r > 1.0) discard;
  float nz = fbm(vUV*1.7 + vMisc.x*31.0);
  float a = (1.0-smoothstep(0.25, 1.0, r + (nz-0.5)*0.7))*vC.a*smoothstep(-1.0, -0.3, vUV.y);
  vec3 c = vC.rgb;
  if(vMisc.y < 0.5) c *= (ambient(vec3(0.0,1.0,0.0)) + uSunCol*0.45)*(0.75+0.5*nz);
  float d = length(vP-uCamPos); float f = 1.0-exp(-d*uFogDen);
  c = mix(c, uSkyHor, f*(1.0-vMisc.y));
  if(uHDR < 0.5) c = tonemap(c);
  oCol = vec4(c*a, a*(1.0-vMisc.y));
}
)";

static const char* FS_DEPTH = R"(
void main(){ oCol = vec4(1.0); }
)";

static const char* FS_UI_FULL = R"(#version 330 core
in vec2 vUV; in vec4 vC; uniform sampler2D uFont; out vec4 oCol;
void main(){ float a = vUV.x < -0.5 ? 1.0 : texture(uFont, vUV).r; oCol = vec4(vC.rgb, vC.a*a); }
)";

static const char* FS_MARK = R"(
in vec3 vP; in vec2 vUV; in vec4 vC; in vec2 vMisc;
void main(){
  float e = vMisc.y > 0.5 ? 1.0 - smoothstep(0.25, 1.0, length(vUV)) : 1.0 - smoothstep(0.5, 1.0, abs(vUV.x));   // misc.y = 1: a soft round patch (the shadow where a car meets the ground)
  vec3 c = vC.rgb*(ambient(vec3(0.0,1.0,0.0)) + uSunCol*0.5); finish(applyFog(c, vP), vC.a*e);
}
)";

// the showdown's ring: a wall of light. vUV.x runs round it in metres, vUV.y from the ground (0) to its top (1); vMisc.y = 1 while it is closing in
static const char* FS_RING = R"(
in vec3 vP; in vec2 vUV; in vec4 vC; in vec2 vMisc;
void main(){
  float u = vUV.x, v = vUV.y;
  float bars = smoothstep(0.30, 0.5, abs(fract(u*0.35 - uTime*0.5 + v*1.6) - 0.5));       // slanted bands sliding round the wall
  float lines = smoothstep(0.90, 1.0, fract(v*5.0 - uTime*0.3));                           // thin lines climbing it
  float foot = exp(-v*11.0), fade = pow(clamp(1.0 - v, 0.0, 1.0), 1.5);
  float d = length(vP - uCamPos), near = 1.0 - smoothstep(6.0, 45.0, d);                   // it thickens as you come up to it
  float a = (0.05 + 0.16*bars + 0.30*lines + 1.5*foot)*fade*vC.a*(0.45 + 0.55*near);
  vec3 c = vC.rgb*(1.0 + 2.0*foot)*(1.0 + vMisc.y*0.35*sin(uTime*10.0));
  c = mix(c, uSkyHor, (1.0 - exp(-d*uFogDen))*0.5);
  if(uHDR < 0.5) c = tonemap(c);
  oCol = vec4(c*a, 0.0);
}
)";

// post chain. uMode 0: bright pass + downsample, 1: gaussian blur, 2: final composite
static const char* FS_POST = R"(#version 330 core
in vec2 vNdc; out vec4 oCol;
uniform sampler2D uTex, uBloom; uniform int uMode; uniform vec2 uDir; uniform float uBlur, uCA, uFlash, uVig, uSat, uRain, uTime; uniform vec3 uFlashCol;
vec3 tonemap(vec3 c){ c *= 0.64; c = (c*(2.51*c+0.03))/(c*(2.43*c+0.59)+0.14); return pow(clamp(c,0.0,1.0), vec3(1.0/2.2)); }
void main(){
  vec2 uv = vNdc*0.5+0.5;
  if(uMode==0){
    vec3 c = (texture(uTex, uv+uDir*vec2(1,1)).rgb + texture(uTex, uv+uDir*vec2(-1,1)).rgb + texture(uTex, uv+uDir*vec2(1,-1)).rgb + texture(uTex, uv+uDir*vec2(-1,-1)).rgb)*0.25;
    float l = max(c.r, max(c.g, c.b)); oCol = vec4(min(c*smoothstep(2.0, 5.0, l), vec3(10.0)), 1.0);
  } else if(uMode==1){
    vec3 c = texture(uTex, uv).rgb*0.227;
    c += (texture(uTex, uv+uDir*1.385).rgb + texture(uTex, uv-uDir*1.385).rgb)*0.316;
    c += (texture(uTex, uv+uDir*3.231).rgb + texture(uTex, uv-uDir*3.231).rgb)*0.070;
    oCol = vec4(c, 1.0);
  } else {
    float wetm = 0.0;
    if(uRain > 0.01){                                                        // drops on the lens: each bends the picture behind it
      for(int L = 0; L < 2; L++){
        float sc = L == 0 ? 6.0 : 11.0; vec2 q = uv*vec2(sc*1.78, sc) + vec2(0.0, uTime*(L == 0 ? 0.035 : 0.08)); vec2 id = floor(q), f = fract(q) - 0.5;
        float h = fract(sin(dot(id + float(L)*7.0, vec2(127.1, 311.7)))*43758.5453), life = fract(uTime*(0.10 + 0.18*h) + h*7.0);
        vec2 dd = f - (vec2(fract(h*17.0), fract(h*31.0)) - 0.5)*0.6; float r = length(dd), rad = (0.07 + 0.10*h)*(1.0 - life*0.7)*step(h, 0.30*uRain);
        float k = rad > 0.0 ? 1.0 - smoothstep(rad*0.35, rad, r) : 0.0;
        uv -= dd*k*0.55/sc; wetm += k*(1.0 - life);
      }
    }
    vec2 d = uv - vec2(0.5, 0.54); float m = smoothstep(0.10, 0.70, length(d)); vec3 c = vec3(0.0);
    const int N = 10;
    for(int i=0;i<N;i++){ vec2 o = d*uBlur*m*(float(i)/float(N-1));
      c.r += texture(uTex, uv-o-d*uCA).r; c.g += texture(uTex, uv-o).g; c.b += texture(uTex, uv-o+d*uCA).b; }
    c /= float(N);
    c += texture(uBloom, uv).rgb*0.38; c *= 1.0 + 0.10*wetm;
    c = tonemap(c);
    c *= 1.0 - uVig*dot(d,d)*1.5;
    c = mix(vec3(dot(c, vec3(0.3,0.59,0.11))), c, uSat);
    oCol = vec4(mix(c, uFlashCol, uFlash), 1.0);
  }
}
)";
