#version 450
layout(binding=0) uniform Globals {
mat4 uVP;
mat4 uModel;
};

layout(location=0) in vec3 aP; layout(location=1) in vec3 aN; layout(location=2) in vec3 aC;
layout(location=3) in vec2 aUV; layout(location=4) in float aM;

layout(location=0) out vec3 vP; layout(location=1) out vec3 vN; layout(location=2) out vec3 vC; layout(location=3) out vec2 vUV; layout(location=4) out float vMat;
void main(){ vec4 w = uModel*vec4(aP,1.0); vP = w.xyz; vN = mat3(uModel)*aN; vC = aC; vUV = aUV; vMat = aM; gl_Position = uVP*w; }
