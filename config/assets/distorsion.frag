#version 330 core
out vec3 color;
in vec2 fUV;
uniform sampler2D iUserTextures[32];
uniform float iTime;
uniform float iX;
uniform vec3 iResolution;

uniform float factor;

void main() { 
  vec2 uv = fUV;
  vec3 a = texture2D(iUserTextures[0], uv).xyz;
  vec3 b = texture2D(iUserTextures[1], uv).xyz;

  float f = sin(fUV.x + iTime) * 0.5 + 0.5;
  float r = (fUV.x - iX / iResolution.x);

  r*= factor;
  color = mix(a, b, smoothstep(-0.3,0.3,r));
} 
