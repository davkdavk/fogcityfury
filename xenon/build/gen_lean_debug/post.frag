#version 450
layout(binding=0) uniform Globals {
float uMode;
vec2 uDir;
float uBlur;
float uCA;
float uFlash;
float uVig;
float uSat;
float uRain;
float uTime;
vec3 uFlashCol;
};
layout(binding=0) uniform sampler2D uTex;
layout(binding=1) uniform sampler2D uBloom;

layout(location=0) in vec2 vNdc; layout(location=0) out vec4 oCol;
    
vec3 tonemap(vec3 c){ c *= 0.64; c = (c*(2.51*c+0.03))/(c*(2.43*c+0.59)+0.14); return pow(clamp(c,0.0,1.0), vec3(1.0/2.2)); }
void main(){
  vec2 uv = vNdc*0.5+0.5;
  if(uMode<0.5){
    vec3 c = (texture(uTex, uv+uDir*vec2(1,1)).rgb + texture(uTex, uv+uDir*vec2(-1,1)).rgb + texture(uTex, uv+uDir*vec2(1,-1)).rgb + texture(uTex, uv+uDir*vec2(-1,-1)).rgb)*0.25;
    float l = max(c.r, max(c.g, c.b)); oCol = vec4(min(c*smoothstep(2.0, 5.0, l), vec3(10.0)), 1.0);
  } else if(uMode<1.5){
    vec3 c = texture(uTex, uv).rgb*0.227;
    c += (texture(uTex, uv+uDir*1.385).rgb + texture(uTex, uv-uDir*1.385).rgb)*0.316;
    c += (texture(uTex, uv+uDir*3.231).rgb + texture(uTex, uv-uDir*3.231).rgb)*0.070;
    oCol = vec4(c, 1.0);
  } else {
    float wetm = 0.0;
    if(uRain > 0.01){                                                        
      for(int L = 0; L < 2; L++){
        float sc = float(L) < 0.5 ? 6.0 : 11.0; vec2 q = uv*vec2(sc*1.78, sc) + vec2(0.0, uTime*(float(L) < 0.5 ? 0.035 : 0.08)); vec2 id = floor(q), f = fract(q) - 0.5;
        float h = fract(sin(dot(id + float(L)*7.0, vec2(127.1, 311.7)))*43758.5453), life = fract(uTime*(0.10 + 0.18*h) + h*7.0);
        vec2 dd = f - (vec2(fract(h*17.0), fract(h*31.0)) - 0.5)*0.6; float r = length(dd), rad = (0.07 + 0.10*h)*(1.0 - life*0.7)*step(h, 0.30*uRain);
        float k = rad > 0.0 ? 1.0 - smoothstep(rad*0.35, rad, r) : 0.0;
        uv -= dd*k*0.55/sc; wetm += k*(1.0 - life);
      }
    }
    vec2 d = uv - vec2(0.5, 0.54); float m = smoothstep(0.10, 0.70, length(d)); vec3 c = vec3(0.0);
    const int N = 10;
    for(int i=0;i<N;i++){ vec2 o = d*uBlur*m*(float(i)/float(N-1));
      c.r += texture(uTex, uv-o-d*uCA).r; c.g += texture(uTex, uv-o).g; c.b += texture(uTex, uv-o+d*uCA).b; }
    c /= float(N);
    c += texture(uBloom, uv).rgb*0.38; c *= 1.0 + 0.10*wetm;
    c = tonemap(c);
    c *= 1.0 - uVig*dot(d,d)*1.5;
    c = mix(vec3(dot(c, vec3(0.3,0.59,0.11))), c, uSat);
    oCol = vec4(mix(c, uFlashCol, uFlash), 1.0);
  }
}
