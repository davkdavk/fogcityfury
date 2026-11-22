#version 450
layout(binding=0) uniform Globals {
vec3 uSkyHor;
float uNight;
float uWet;
};
layout(binding=0) uniform sampler2D uNoise;

 

layout(location=0) in vec3 vP; layout(location=1) in vec3 vL; layout(location=2) in vec3 vC; layout(location=3) in vec2 vUV; layout(location=4) in float vMat; layout(location=5) in vec2 vFx; layout(location=6) in vec4 vFog;
layout(location=0) out vec4 oCol;
vec3 tone(vec3 c){ c = min(c, vec3(2.5)); return sqrt(max(c*(1.3246 + c*(c*0.1284 - 0.6999)) - 0.0257, 0.0)); }
void main(){
  
  
  float det = mix(0.5, texture(uNoise, vP.xz*0.05).b, vFx.y);
  float nz = texture(uNoise, vP.xz*0.026).r;
  vec3 alb = vC, lit = vL; vec3 glow = vec3(0.0);
  vec3 base = alb, g0 = glow;
  { float u = abs(vUV.x);                                        
    vec3 k = clamp(1.0 - abs(vec3(u) - vec3(0.17, 4.0, 7.5))*12.0, 0.0, 1.0);
    vec3 r = vec3(0.105,0.105,0.115)*(0.62 + 0.76*det)*(0.8 + 0.4*nz);
    r = mix(r, vec3(0.85,0.62,0.05), k.x*0.9);
    r = mix(r, vec3(0.82), max(k.y*step(fract(vUV.y/9.0), 0.42), k.z)*0.85);
    alb = mix(r, mix(vec3(0.75,0.08,0.06), vec3(0.85), step(0.5, fract(vUV.y*0.25))), step(8.0, u));
    lit = uSkyHor*0.55 + vec3(0.05,0.045,0.04); glow = uSkyHor*(uWet*0.22*vC.b); }   
  vec3 aRoad = alb, lRoad = lit; alb = base; lit = vL;
  { vec2 q = vUV*vec2(0.2941, 0.3030), f = abs(fract(q) - vec2(0.5, 0.55));   
    float win = step(f.x, 0.3)*step(f.y, 0.27)*step(0.0, vUV.y);
    alb = mix(alb*(0.8 + 0.4*nz), vec3(0.06,0.08,0.11) + uSkyHor*0.25, win);
    glow = vec3(2.0,1.56,0.9)*(uNight*win*step(0.55, fract(dot(floor(q), vec2(0.37, 0.71))))) + vec3(1.0,0.9,0.72)*(vFog.w*win); }
  vec3 aWall = alb, gWall = glow; alb = base; glow = g0;
  { vec2 c = floor(vUV*1.4286);                                  
    if(vMat > 3.5) alb = mix(vec3(0.03), vec3(0.95), fract((c.x + c.y)*0.5)*2.0);
    if(vMat > 4.5) alb = mix(vec3(0.95,0.72,0.02), vec3(0.04), step(0.5, fract(vUV.x*3.0 + vUV.y*6.0)));
    if(vMat > 5.5) glow = vC*3.0; }
  alb = vMat < 1.5 ? aRoad : vMat < 2.5 ? aWall : alb; lit = vMat < 1.5 ? lRoad : vL; glow = vMat < 1.5 ? g0 : vMat < 2.5 ? gWall : glow;
  oCol = vec4(tone(mix(vFog.rgb, alb*lit + glow, vFx.x)), 1.0);
}
