#version 330
#define IN_OUT in
#define DRW_RESOURCE_CHUNK_LEN 512



/* ---- Opengl Depth conversion ---- */


IN_OUT ShaderStageInterface
{
  vec3 normal_interp;
  vec3 color_interp;
};

#  define WB_Normal vec2

/* From http://aras-p.info/texts/CompactNormalStorage.html
 * Using Method #4: Spheremap Transform */
WB_Normal workbench_normal_encode(bool front_face, vec3 n)
{
  n = normalize(front_face ? n : -n);
  float p = sqrt(n.z * 8.0 + 8.0);
  n.xy = clamp(n.xy / p + 0.5, 0.0, 1.0);
  return n.xy;
}

layout(location = 0) out vec4 materialData;
layout(location = 1) out WB_Normal normalData;
layout(location = 2) out uint objectId;

uniform bool useMatcap = false;

void main()
{
  normalData = workbench_normal_encode(gl_FrontFacing, normal_interp);
}
