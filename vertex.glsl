#version 330

out ShaderStageInterface
{
  vec3 normal_interp;
};

in vec3 pos;
in vec3 nor;

void main()
{
  gl_Position = vec4(pos,1.0);

  normal_interp = normalize(nor);


}
