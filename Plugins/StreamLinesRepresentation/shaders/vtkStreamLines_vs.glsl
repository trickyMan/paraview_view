//VTK::System::Dec
//VTK::Output::Dec

attribute vec4 vertexMC;

uniform mat4 MCDCMatrix;

void main(void)
{
  gl_Position = MCDCMatrix * vertexMC;
}
