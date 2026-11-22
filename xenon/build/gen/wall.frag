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
  { vec2 q = vUV*vec2(0.2941, 0.3030), f = abs(fract(q) - vec2(0.5, 0.55));   
    float win = step(f.x, 0.3)*step(f.y, 0.27)*step(0.0, vUV.y);
    alb = mix(alb*(0.8 + 0.4*nz), vec3(0.06,0.08,0.11) + uSkyHor*0.25, win);
    glow = vec3(2.0,1.56,0.9)*(uNight*win*step(0.55, fract(dot(floor(q), vec2(0.37, 0.71))))) + vec3(1.0,0.9,0.72)*(vFog.w*win); }
  oCol = vec4(tone(mix(vFog.rgb, alb*lit + glow, vFx.x)), 1.0);
}
