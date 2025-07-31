#version 330 core
out vec3 fragColor;
in vec2 fUV;
uniform vec3 iResolution;
uniform float iTime;
uniform vec2 iMouse;

float r(float i) { return sin(i) * 0.5+ 0.5; }

void main()
{
    vec2 uv = fUV;
    vec3 color = vec3(0.0f);
    for(float i = 1.0f; i < 15.0f; i+= 1.0f) {
        float c = r(i + iTime) * (r(iTime + i) * 0.2 + 0.5);
        vec3 basecolor = vec3(r(i * 1.3 + iTime) * 0.1f, r(i * 1.3 - iTime) * 0.4f, r(i * 3.0 + 0.4 - iTime * 2.0) * 0.8f);
        float a = (sin((uv.x + 0.1f * (iMouse.x / iResolution.x) * i)* (10.0 + c) + iTime * r(i * 3.0) - r(i * 3.0)) * 0.5) * 0.5 * (0.5 + r(iTime + i * 1.3)) * r(i * 2.0) + 0.5;
        float t = 1.0 / pow(abs(a - uv.y)* 50.0f, 1.3f);
        color += basecolor * t * c * (3.0 + r(iTime + uv.x + r(i * 3.0)));
    } 

    fragColor = color;
}
