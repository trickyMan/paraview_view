//VTK::System::Dec
//VTK::Output::Dec

uniform vec3 color;

void main(void)
{
  gl_FragData[0] = vec4(color.rgb, 1.);
}
