#version 330 core
out vec4 color;
in vec2 fUV;
uniform float iTime;

void main() {
  color = vec4(fUV.x + abs(sin(iTime)),fUV.y,0.4,1.0);
}
