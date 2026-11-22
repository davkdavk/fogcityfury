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
float uOvercast;
};

layout(location=0) in vec3 aP; layout(location=1) in vec3 aN; layout(location=2) in vec3 aC;
layout(location=3) in vec2 aUV; layout(location=4) in float aM;

 
layout(location=0) out vec3 vP; layout(location=1) out vec3 vL; layout(location=2) out vec3 vC; layout(location=3) out vec2 vUV; layout(location=4) out float vMat; layout(location=5) out vec2 vFx; layout(location=6) out vec4 vFog;
void main(){ vec4 w = uModel*vec4(aP,1.0); vP = w.xyz; vec3 n = normalize(mat3(uModel)*aN);
  float clear = 1.0 - 0.92*uOvercast, up = max(uSunDir.y, 0.0);
  
  vL = (mix(uSkyHor*0.55 + vec3(0.05,0.045,0.04), uSkyTop*0.6 + uSkyHor*0.35, n.y*0.5+0.5) + uSunCol*max(dot(n, uSunDir), 0.0)
        + uSunCol*vec3(0.10,0.085,0.06)*((0.5 - 0.5*n.y)*up))*(1.0 - 0.3*uWet);
  float wall = step(1.5, aM)*step(aM, 2.5);
  vL *= mix(1.0, clamp(0.66 + aUV.y*0.04, 0.66, 1.0), wall);                    
  vec3 toCam = uCamPos - w.xyz; float d = length(toCam); vec3 V = toCam/max(d, 0.001);
  vFx = vec2(exp(-d*uFogDen), clamp(1.0 - d*0.007, 0.0, 1.0));
  
  float sd = max(-dot(V, uSunDir), 0.0), s4 = sd*sd; s4 *= s4;
  float nh = max(dot(n, normalize(V + uSunDir)), 0.0), g = nh*nh; g *= g; g *= g;
  vFog = vec4(uSkyHor + uSunCol*(s4*0.10*clear), g*wall*clear*up*dot(uSunCol, vec3(0.3)));
  vC = aC; vUV = aUV; vMat = aM; gl_Position = uVP*w; }
