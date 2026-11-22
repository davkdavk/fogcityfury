#version 450
layout(location=0) in vec3 vP; layout(location=1) in vec3 vL; layout(location=2) in vec3 vC; layout(location=3) in vec2 vUV; layout(location=4) in float vMat; layout(location=5) in vec2 vFx;
layout(location=0) out vec4 oCol;
void main(){ oCol = vec4(1.0, 1.0, 0.0, 1.0); }
