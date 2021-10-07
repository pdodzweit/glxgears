#version 330
#define IN_OUT in
#define DRW_RESOURCE_CHUNK_LEN 512

/* keep in sync with DRWManager.view_data */
layout(std140) uniform viewBlock
{
  /* Same order as DRWViewportMatrixType */
  mat4 ViewProjectionMatrix;
  mat4 ViewProjectionMatrixInverse;
  mat4 ViewMatrix;
  mat4 ViewMatrixInverse;
  mat4 ProjectionMatrix;
  mat4 ProjectionMatrixInverse;

  vec4 clipPlanes[6];

  /* View frustum corners [NDC(-1.0, -1.0, -1.0) & NDC(1.0, 1.0, 1.0)].
   * Fourth components are near and far values. */
  vec4 ViewVecs[2];

  /* TODO: move it elsewhere. */
  vec4 CameraTexCoFactors;
};

uniform int resourceChunk;


/* clang-format on */
struct ObjectMatrices {
  mat4 drw_modelMatrix;
  mat4 drw_modelMatrixInverse;
};

layout(std140) uniform modelBlock
{
  ObjectMatrices drw_matrices[DRW_RESOURCE_CHUNK_LEN];
};

/* ---- Opengl Depth conversion ---- */


IN_OUT ShaderStageInterface
{
  vec3 normal_interp;
  vec3 color_interp;
  float alpha_interp;
  vec2 uv_interp;
#ifdef TRANSPARENT_MATERIAL
  flat float roughness;
  flat float metallic;
#else
  flat float packed_rough_metal;
#endif
  flat int object_id;
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


uniform sampler2DArray imageTileArray;
uniform sampler1DArray imageTileData;
uniform sampler2D imageTexture;

uniform float imageTransparencyCutoff = 0.1;
uniform bool imagePremult;



layout(location = 0) out vec4 materialData;
layout(location = 1) out WB_Normal normalData;
layout(location = 2) out uint objectId;

uniform bool useMatcap = false;

void main()
{
  normalData = workbench_normal_encode(gl_FrontFacing, normal_interp);

  materialData = vec4(color_interp, packed_rough_metal);

  objectId = uint(object_id);

  if (useMatcap) {
    /* For matcaps, save front facing in alpha channel. */
    materialData.a = float(gl_FrontFacing);
  }

}
