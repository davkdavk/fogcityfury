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
  { vec2 c = floor(vUV*1.4286);                                  
    if(vMat > 3.5) alb = mix(vec3(0.03), vec3(0.95), fract((c.x + c.y)*0.5)*2.0);
    if(vMat > 4.5) alb = mix(vec3(0.95,0.72,0.02), vec3(0.04), step(0.5, fract(vUV.x*3.0 + vUV.y*6.0)));
    if(vMat > 5.5) glow = vC*3.0; }
  oCol = vec4(tone(mix(vFog.rgb, alb*lit + glow, vFx.x)), 1.0);
}
