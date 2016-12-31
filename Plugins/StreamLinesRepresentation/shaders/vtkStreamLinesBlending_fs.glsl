//VTK::System::Dec
//VTK::Output::Dec

varying vec2 tcoordVC;

uniform sampler2D prev;
uniform sampler2D current;
uniform float alpha;

void main(void)
{
  vec4 pc = texture2D(prev, tcoordVC);
  vec4 cc = texture2D(current, tcoordVC);
  if (pc.a < 0.1)
    gl_FragData[0] = cc;
  else
    gl_FragData[0] = pc * alpha + cc;
}
