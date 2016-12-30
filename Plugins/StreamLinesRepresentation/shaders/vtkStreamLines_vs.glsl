#version 120

//VTK::System::Dec
attribute vec4 vertexMC;

uniform mat4 MCDCMatrix;

void main(void)
{
  gl_Position = MCDCMatrix * vertexMC;
}
