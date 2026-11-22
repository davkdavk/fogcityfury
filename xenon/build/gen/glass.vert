#version 450
layout(binding=0) uniform Globals {
mat4 uVP;
};

layout(location=0) in vec3 aP; layout(location=1) in vec2 aUV; layout(location=2) in vec4 aC; layout(location=3) in vec2 aMisc;

layout(location=0) out vec3 vP; layout(location=1) out vec2 vUV; layout(location=2) out vec4 vC; layout(location=3) out vec2 vMisc;
void main(){ vP = aP; vUV = aUV; vC = aC; vMisc = aMisc; gl_Position = uVP*vec4(aP,1.0); }
