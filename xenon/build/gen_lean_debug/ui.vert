#version 450
layout(binding=0) uniform Globals {
vec2 uScr;
};

layout(location=0) in vec2 aP; layout(location=1) in vec2 aUV; layout(location=2) in vec4 aC;
 layout(location=0) out vec2 vUV; layout(location=1) out vec4 vC;
void main(){ vUV = aUV; vC = aC; gl_Position = vec4(aP.x/uScr.x*2.0-1.0, 1.0-aP.y/uScr.y*2.0, 0.0, 1.0); }
