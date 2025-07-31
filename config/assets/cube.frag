#version 330 core
out vec3 o;
in vec2 fUV;
uniform vec3 iResolution;
uniform float iTime;

mat2 R;
float d=1., z, G=9., M=1e-3;

float D(vec3 p) {
  p.xy *= R;
  p.xz *= R;
  vec3 S = sin(123.*p);
  G = min(
      G
    , max(
        abs(length(p)-.6)
      , d = pow(dot(p*=p*p*p,p),.125) - .5
        - pow(1.+S.x*S.y*S.z,8.)/1e5
      )
    );
  return d;
}

void main() {
  vec2 C = fUV * iResolution.xy;
  vec3 p, O, r=vec3(iResolution.xy, 0.0), I=normalize(vec3(C-.5*r.xy, r.y)), B=vec3(1,2,9)*M;

  for(R=mat2(cos(.3*iTime+vec4(0,11,33,0))); z<9. && d > M; z += D(p))
    p = z*I, p.z -= 2.;

  if (z < 9.) {
    for (int i; i < 3; O[i++] = D(p+r) - D(p-r))
      r -= r, r[i] = M;

    z = 1.+dot(O = normalize(O),I);
    r = reflect(I,O);
    C = (p+r*(5.-p.y)/abs(r.y)).xz;

    O =
        z*z *
        (
            r.y>0.
          ? 5e2*smoothstep(5., 4., d = sqrt(length(C*C))+1.)*d*B
          : exp(-2.*length(C))*(B/M-1.)
        )
      + pow(1.+O.y,5.)*B;
  }

  o = sqrt(O+B/G).xyz;
}
