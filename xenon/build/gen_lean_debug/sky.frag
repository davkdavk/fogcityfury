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
vec3 uCamR;
vec3 uCamU;
vec3 uCamF;
vec2 uTan;
};
layout(binding=0) uniform sampler2D uNoise;




layout(location=0) out vec4 oCol;

vec3 tone(vec3 c){ c = min(c, vec3(2.5)); return sqrt(max(c*(1.3246 + c*(c*0.1284 - 0.6999)) - 0.0257, 0.0)); }
vec3 ambient(float ny){ return mix(uSkyHor*0.55 + vec3(0.05,0.045,0.04), uSkyTop*0.6 + uSkyHor*0.35, ny*0.5+0.5); }

layout(location=0) in vec2 vNdc;  
void main(){
  vec3 d = normalize(uCamF + uCamR*vNdc.x*uTan.x + uCamU*vNdc.y*uTan.y);
  vec3 c = mix(uSkyHor, uSkyTop, sqrt(clamp(d.y, 0.0, 1.0)));
  float sd = max(dot(d, uSunDir), 0.0), s4 = sd*sd; s4 *= s4; float s16 = s4*s4; s16 *= s16;
  c += uSunCol*((clamp(sd*4000.0 - 3997.6, 0.0, 1.0)*30.0 + s16*s16*0.3 + s4*0.08)*(1.0 - 0.92*uOvercast));
  float cn = texture(uNoise, d.xz/(abs(d.y) + 0.14)*0.175 + uTime*vec2(0.00075, 0.00025)).r;                 
  float cl = clamp((cn - 0.48 + 0.36*uOvercast)*3.3, 0.0, 1.0)*clamp(d.y*10.0, 0.0, 1.0)*(0.85 + 0.13*uOvercast);
  c = mix(c, mix(uSkyHor, vec3(1.0 - 0.5*uNight), 0.55 - 0.41*uOvercast), cl);
  c = mix(c, uSkyHor*0.8, clamp(-d.y*4.0, 0.0, 1.0));                                                         
  oCol = vec4(tone(c), 1.0);
}
