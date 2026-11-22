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
  alb *= (0.72 + 0.56*nz)*(0.8 + 0.4*det);                    
  vec2 a = abs(mod(vP.xz + 660.0, 27.0) - 13.5);
  vec3 cty = mix(vec3(0.27,0.262,0.25), vec3(0.078,0.078,0.088), clamp((3.7 - min(a.x, a.y))*4.0, 0.0, 1.0)*clamp(vUV.y - 16.5, 0.0, 1.0))*(0.6 + 0.8*det);   
  float strip = clamp(vUV.x - 1.0, 0.0, 1.0);                       
  vec3 prom = mix(vec3(0.40,0.20,0.14)*(0.7 + 0.6*det), alb, 1.0 - clamp(vUV.y - 13.8, 0.0, 1.0));
  cty = mix(cty, prom, strip);
  alb = mix(alb, cty, clamp(vUV.x, 0.0, 1.0));
  oCol = vec4(tone(mix(vFog.rgb, alb*lit + glow, vFx.x)), 1.0);
}
