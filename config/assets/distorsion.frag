#version 330 core
out vec3 color;
in vec2 fUV;
uniform sampler2D iUserTextures[32];
uniform float iTime;

void main() { 
  vec2 uv = fUV + vec2(sin(iTime * 0.1 + fUV.x * 32.0) * 0.2, cos(iTime * 0.4 + fUV.y * 12.04 + fUV.x * 32.04) * 0.4);
  color = texture2D(iUserTextures[0], uv).xyz;
} 
