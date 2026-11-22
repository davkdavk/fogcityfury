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
vec3 uPaint;
float uBrake;
float uChrome;
float uBurnt;
float uGone;
};
layout(binding=0) uniform sampler2D uNoise;




layout(location=0) out vec4 oCol;

vec3 tone(vec3 c){ c = min(c, vec3(2.5)); return sqrt(max(c*(1.3246 + c*(c*0.1284 - 0.6999)) - 0.0257, 0.0)); }
vec3 ambient(float ny){ return mix(uSkyHor*0.55 + vec3(0.05,0.045,0.04), uSkyTop*0.6 + uSkyHor*0.35, ny*0.5+0.5); }

layout(location=0) in vec3 vP; layout(location=1) in vec3 vN; layout(location=2) in vec3 vC; layout(location=3) in vec2 vUV; layout(location=4) in float vMat;
 
void main(){
  vec3 n = normalize(vN)*(gl_FrontFacing ? 1.0 : -1.0);
  vec3 toCam = uCamPos - vP; float dist = length(toCam); vec3 V = toCam/dist;
  float ndv = max(dot(n, V), 0.0), f1 = 1.0 - ndv, F = 0.05 + 0.95*f1*f1*f1*f1;
  
  
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
