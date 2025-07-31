#version 330
in vec2 aPosition;
out vec2 fUV;

void main() {
  gl_Position = vec4(aPosition.x, aPosition.y, 0.0,1.0);
  fUV = aPosition.xy * 0.5 + 0.5;
} 
