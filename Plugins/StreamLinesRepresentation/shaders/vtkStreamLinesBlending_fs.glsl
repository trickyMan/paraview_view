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
  vec4 c = pc * alpha + cc;
  gl_FragData[0] = vec4(c.rgb, 1.);
}
