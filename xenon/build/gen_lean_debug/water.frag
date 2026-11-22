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
layout(binding=1) uniform sampler2D uHeight;




layout(location=0) out vec4 oCol;

vec3 tone(vec3 c){ c = min(c, vec3(2.5)); return sqrt(max(c*(1.3246 + c*(c*0.1284 - 0.6999)) - 0.0257, 0.0)); }
vec3 ambient(float ny){ return mix(uSkyHor*0.55 + vec3(0.05,0.045,0.04), uSkyTop*0.6 + uSkyHor*0.35, ny*0.5+0.5); }

layout(location=0) in vec3 vP; layout(location=1) in vec3 vN; layout(location=2) in vec3 vC; layout(location=3) in vec2 vUV; layout(location=4) in float vMat;

void main(){
  vec2 huv = (vP.xz + 768.0)/1536.0;
  float depth = -texture(uHeight, clamp(huv, 0.0, 1.0)).r;
  vec2 o = abs(huv - 0.5); if(max(o.x, o.y) > 0.5) depth = 30.0;
  if(depth < -0.02) discard;
  vec3 toCam = uCamPos - vP; float dist = length(toCam); vec3 V = toCam/dist;
  vec2 w = texture(uNoise, vP.xz*0.011 + vec2(uTime*0.014, uTime*0.009)).rg + texture(uNoise, vP.xz*0.07 + vec2(-uTime*0.06, uTime*0.04)).gr - 1.0;
  vec3 n = normalize(vec3(w.x, 1.4 + dist*0.02, w.y));
  float f1 = 1.0 - max(dot(n, V), 0.0), f2 = f1*f1, F = 0.03 + 0.97*f2*f2*f1;
  vec3 R = reflect(-V, n);
  vec3 sky = mix(uSkyHor, uSkyTop, sqrt(clamp(abs(R.y), 0.0, 1.0)));
  float sd = max(dot(R, uSunDir), 0.0), s = sd*sd; s *= s; s *= s; s *= s; s *= s; s *= s;
  vec3 body = mix(vec3(0.07,0.30,0.30), vec3(0.012,0.07,0.11), clamp(depth*0.2, 0.0, 1.0))*(ambient(n.y)*1.4 + uSunCol*0.35);
  vec3 col = mix(body, sky, F) + uSunCol*s*(1.0 - uOvercast);
  float foam = clamp(1.0 - depth*2.2, 0.0, 1.0)*step(0.45, w.x + 0.6);
  col = mix(col, vec3(0.9)*(ambient(1.0) + uSunCol*0.6), foam*0.8);
  col = mix(col, uSkyHor, 1.0 - exp(-dist*uFogDen));
  oCol = vec4(tone(col), clamp(depth*1.1 + 0.25, 0.0, 1.0));
}
