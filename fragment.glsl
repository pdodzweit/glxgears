#version 330

in ShaderStageInterface
{
  vec3 normal_interp;
};

vec2 workbench_normal_encode(bool front_face, vec3 n)
{
  n = normalize(front_face ? n : -n);
  float p = sqrt(n.z * 8.0 + 8.0);
  n.xy = clamp(n.xy / p + 0.5, 0.0, 1.0);
  return n.xy;
}

layout(location = 0) out vec2 normalData;

void main()
{
  normalData = workbench_normal_encode(gl_FrontFacing, normal_interp);
}
