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
