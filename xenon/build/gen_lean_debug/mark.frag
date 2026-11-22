#version 450
layout(binding=0) uniform Globals {
vec3 uSunDir;
vec3 uSunCol;
vec3 uSkyTop;
vec3 uSkyHor;
vec3 uCamPos;
float uTime;
float uFogDen;
float uNight;
float uOvercast;
float uWet;
};
layout(binding=0) uniform sampler2D uNoise;




layout(location=0) out vec4 oCol;

vec3 tone(vec3 c){ c = min(c, vec3(2.5)); return sqrt(max(c*(1.3246 + c*(c*0.1284 - 0.6999)) - 0.0257, 0.0)); }
vec3 ambient(float ny){ return mix(uSkyHor*0.55 + vec3(0.05,0.045,0.04), uSkyTop*0.6 + uSkyHor*0.35, ny*0.5+0.5); }

layout(location=0) in vec3 vP; layout(location=1) in vec2 vUV; layout(location=2) in vec4 vC; layout(location=3) in vec2 vMisc;
void main(){
  float e = mix(clamp((1.0 - abs(vUV.x))*2.0, 0.0, 1.0), clamp((1.0 - length(vUV))*1.33, 0.0, 1.0), vMisc.y);
  vec3 c = vC.rgb*(ambient(1.0) + uSunCol*0.5);
  c = mix(c, uSkyHor, 1.0 - exp(-length(vP - uCamPos)*uFogDen));
  oCol = vec4(tone(c), vC.a*e);
}
