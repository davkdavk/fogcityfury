#version 450
layout(binding=0) uniform Globals {
mat4 uVP;
mat4 uModel;
vec3 uSunDir;
vec3 uSunCol;
vec3 uSkyTop;
vec3 uSkyHor;
vec3 uCamPos;
float uFogDen;
float uWet;
};

layout(location=0) in vec3 aP; layout(location=1) in vec3 aN; layout(location=2) in vec3 aC;
layout(location=3) in vec2 aUV; layout(location=4) in float aM;

 
layout(location=0) out vec3 vP; layout(location=1) out vec3 vL; layout(location=2) out vec3 vC; layout(location=3) out vec2 vUV; layout(location=4) out float vMat; layout(location=5) out vec2 vFx;
void main(){ vec4 w = uModel*vec4(aP,1.0); vP = w.xyz; vec3 n = normalize(mat3(uModel)*aN);
  vL = (mix(uSkyHor*0.55 + vec3(0.05,0.045,0.04), uSkyTop*0.6 + uSkyHor*0.35, n.y*0.5+0.5) + uSunCol*max(dot(n, uSunDir), 0.0))*(1.0 - 0.3*uWet);
  float d = length(uCamPos - w.xyz); vFx = vec2(exp(-d*uFogDen), clamp(1.0 - d*0.007, 0.0, 1.0));
  vC = aC; vUV = aUV; vMat = aM; gl_Position = uVP*w; }
