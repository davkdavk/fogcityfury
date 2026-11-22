#version 450
layout(binding=0) uniform sampler2D uFont;

layout(location=0) in vec2 vUV; layout(location=1) in vec4 vC;  layout(location=0) out vec4 oCol;
void main(){
  float k = floor(vUV.y + 0.25);
  float d = (texture(uFont, vec2(vUV.x, (vUV.y - k)*2.0)).r - 0.5)*2.0;
  float a = vUV.x < -0.5 ? 1.0 : clamp(d*k*0.125 + 0.5, 0.0, 1.0);
  oCol = vec4(vC.rgb, vC.a*a);
}
