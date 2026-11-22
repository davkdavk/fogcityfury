#version 450
layout(binding=0) uniform sampler2D uFont;

layout(location=0) in vec2 vUV; layout(location=1) in vec4 vC;  layout(location=0) out vec4 oCol;
void main(){ float a = vUV.x < -0.5 ? 1.0 : texture(uFont, vUV).r; oCol = vec4(vC.rgb, vC.a*a); }
