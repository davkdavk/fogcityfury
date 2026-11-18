#version 450
layout(binding=0) uniform Globals {
vec3 uSunDir;
vec3 uSunCol;
vec3 uSkyTop;
vec3 uSkyHor;
vec3 uCamPos;
float uTime;
float uFogDen;
float uHDR;
float uNight;
float uHeadOn;
float uWet;
float uRain;
float uOvercast;
float uFogNoise;
float uBoltI;
float uUseEnv;
vec3 uHeadPos;
vec3 uHeadDir;
vec3 uBoltDir;
vec3 uEnvPos;
mat4 uLightVP;
vec3 uPaint;
float uBrake;
float uChrome;
float uBurnt;
float uGone;
};
layout(binding=1) uniform samplerCube uEnv;
layout(binding=2) uniform sampler2DShadow uShadow;


 
   
 
layout(location=0) out vec4 oCol;
float hash(vec2 p){ return fract(sin(dot(p, vec2(127.1,311.7)))*43758.5453); }
float vnoise(vec2 p){ vec2 i = floor(p), f = fract(p); f = f*f*(3.0-2.0*f);
  return mix(mix(hash(i),hash(i+vec2(1,0)),f.x), mix(hash(i+vec2(0,1)),hash(i+vec2(1,1)),f.x), f.y); }
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
    cc *= 1.0 - ov*(0.25 + 0.55*smoothstep(0.35, 0.75, fbm(cp*2.1+9.0)));     
    if(uBoltI > 0.01 && d.y > 0.02){                                          
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
float shadow(vec3 wp, vec3 n){
  vec4 lp = uLightVP*vec4(wp + n*0.12, 1.0); vec3 s = lp.xyz/lp.w*0.5+0.5;
  float r = 1.0;
  if(!(s.x<0.0||s.x>1.0||s.y<0.0||s.y>1.0||s.z>1.0)){
    r = 0.0; vec2 t = vec2(1.0/2048.0);
    for(int x=-1;x<=1;x++) for(int y=-1;y<=1;y++) r += texture(uShadow, vec3(s.xy + vec2(x,y)*t, s.z-0.0012));
    float edge = 1.0-smoothstep(0.42, 0.5, max(abs(s.x-0.5), abs(s.y-0.5)));
    r = mix(1.0, r/9.0, edge);
  }
  return r;
}
vec3 applyFog(vec3 c, vec3 wp){
  float d = length(wp-uCamPos);
  float bank = uFogNoise > 0.0 ? 1.0 + uFogNoise*(fbm(wp.xz*0.011 + vec2(uTime*0.035, uTime*0.02)) - 0.45)*1.5 : 1.0;
  float f = 1.0-exp(-d*uFogDen*bank);
  vec3 fc = uSkyHor + uSunCol*0.12*pow(max(dot(normalize(wp-uCamPos),uSunDir),0.0), 6.0);
  return mix(c, fc, f);
}
vec3 tonemap(vec3 c){ c *= 0.64; c = (c*(2.51*c+0.03))/(c*(2.43*c+0.59)+0.14); return pow(clamp(c,0.0,1.0), vec3(1.0/2.2)); }
float roadGrain(vec2 p){ return vnoise(p*6.5) + 0.5*vnoise(p*15.0); }

vec2 ripple(vec2 p, float sc, float seed){
  vec2 q = p*sc, id = floor(q), f = fract(q) - 0.5;
  float h1 = hash(id + seed), h2 = hash(id + seed + 17.3);
  vec2 d = f - (vec2(h1, h2) - 0.5)*0.5; float r = length(d), t = fract(uTime*(0.8 + 0.7*h2) + h1), rr = t*0.42;
  float ring = sin((r - rr)*52.0)*(1.0 - smoothstep(0.0, 0.07, abs(r - rr)))*(1.0 - t);
  return r > 0.001 ? d/r*ring : vec2(0.0);
}

vec3 headlights(vec3 wp, vec3 n){
  vec3 L = wp - uHeadPos; float d = length(L); L /= max(d, 0.001);
  float cone = smoothstep(0.78, 0.97, dot(L, uHeadDir));
  return vec3(1.0,0.93,0.78)*uNight*uHeadOn*cone*max(dot(n,-L), 0.15)*22.0/(1.0+0.012*d*d);
}
void finish(vec3 c, float a){ if(uHDR > 0.5) oCol = vec4(c, a); else oCol = vec4(tonemap(c), a); }

layout(location=0) in vec3 vP; layout(location=1) in vec3 vN; layout(location=2) in vec3 vC; layout(location=3) in vec2 vUV; layout(location=4) in float vMat;
    
void main(){
  vec3 n = normalize(vN); if(!gl_FrontFacing) n = -n;
  vec3 V = normalize(uCamPos-vP); float ndv = max(dot(n,V),0.0);
  vec3 R = reflect(-V, n);
  vec3 env = textureLod(uEnv, R, int(vMat+0.5)==1 ? 1.3 : 0.4).rgb;
  float F = 0.05+0.95*pow(1.0-ndv, 5.0);
  float ndl = max(dot(n,uSunDir),0.0); float sh = shadow(vP, n);
  vec3 H = normalize(V+uSunDir); float ndh = max(dot(n,H),0.0);
  vec3 amb = ambient(n); vec3 col; int m = int(vMat+0.5);
  F = mix(F, min(1.0, F*1.5 + 0.04), uWet);                                   
  if(uGone > 0.001 && fract(52.9829189*fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715)))) < uGone) discard;
  if(uBurnt > 0.001){                                                        
    float soot = fbm(vP.xz*2.3 + vP.y*3.1), em = smoothstep(0.80, 0.93, vnoise(vP.xz*9.0 + vP.y*7.0 + uTime*0.25));
    vec3 base = mix(vec3(0.012,0.011,0.010), vec3(0.10,0.075,0.055), smoothstep(0.45, 0.8, soot));
    col = base*(amb + uSunCol*ndl*sh + headlights(vP, n)) + env*F*0.05 + vec3(1.6,0.45,0.06)*em*clamp(uBurnt - 1.0, 0.0, 1.0);
    finish(applyFog(col, vP), 1.0); return;
  }
  vec3 sunSpec = uSunCol*sh*(pow(ndh,500.0)*5.0 + pow(ndh,40.0)*0.22);
  if(m==1){
    vec3 base = uPaint*(amb + uSunCol*ndl*sh + headlights(vP, n));
    
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
