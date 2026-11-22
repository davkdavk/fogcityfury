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
