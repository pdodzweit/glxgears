#version 330
#define IN_OUT out
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


#define cameraForward ViewMatrixInverse[2].xyz
#define cameraPos ViewMatrixInverse[3].xyz


uniform int resourceChunk;
uniform int baseInstance;

#define instanceId gl_InstanceID
#define resource_id (baseInstance + instanceId)


/* clang-format on */
struct ObjectMatrices {
  mat4 drw_modelMatrix;
  mat4 drw_modelMatrixInverse;
};

layout(std140) uniform modelBlock
{
  ObjectMatrices drw_matrices[DRW_RESOURCE_CHUNK_LEN];
};

#  define ModelMatrix (drw_matrices[resource_id].drw_modelMatrix)
#  define ModelMatrixInverse (drw_matrices[resource_id].drw_modelMatrixInverse)

#define resource_handle (resourceChunk * DRW_RESOURCE_CHUNK_LEN + resource_id)

#define NormalMatrix transpose(mat3(ModelMatrixInverse))
#define NormalMatrixInverse transpose(mat3(ModelMatrix))

#define normal_object_to_view(n) (mat3(ViewMatrix) * (NormalMatrix * n))


#define point_object_to_world(p) ((ModelMatrix * vec4(p, 1.0)).xyz)
#define point_world_to_ndc(p) (ViewProjectionMatrix * vec4(p, 1.0))


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



/* Encoding into the alpha of a RGBA16F texture. (10bit mantissa) */
#define TARGET_BITCOUNT 8u
#define METALLIC_BITS 3u /* Metallic channel is less important. */
#define ROUGHNESS_BITS (TARGET_BITCOUNT - METALLIC_BITS)

/* Encode 2 float into 1 with the desired precision. */
float workbench_float_pair_encode(float v1, float v2)
{
  // const uint v1_mask = ~(0xFFFFFFFFu << ROUGHNESS_BITS);
  // const uint v2_mask = ~(0xFFFFFFFFu << METALLIC_BITS);
  /* Same as above because some compiler are very dumb and think we use medium int. */
  const int v1_mask = 0x1F;
  const int v2_mask = 0x7;
  int iv1 = int(v1 * float(v1_mask));
  int iv2 = int(v2 * float(v2_mask)) << int(ROUGHNESS_BITS);
  return float(iv1 | iv2);
}


layout(std140) uniform material_block
{
  vec4 mat_data[4096];
};

/* If set to -1, the resource handle is used instead. */
uniform int materialIndex;

void workbench_material_data_get(
    int handle, out vec3 color, out float alpha, out float roughness, out float metallic)
{
  handle = (materialIndex != -1) ? materialIndex : handle;
  vec4 data = mat_data[uint(handle) & 0xFFFu];
  color = data.rgb;

  uint encoded_data = floatBitsToUint(data.w);
  alpha = float((encoded_data >> 16u) & 0xFFu) * (1.0 / 255.0);
  roughness = float((encoded_data >> 8u) & 0xFFu) * (1.0 / 255.0);
  metallic = float(encoded_data & 0xFFu) * (1.0 / 255.0);
}


in vec3 pos;
in vec3 nor;
in vec4 ac; /* active color */
in vec2 au; /* active texture layer */

void main()
{
  vec3 world_pos = point_object_to_world(pos);
  gl_Position = point_world_to_ndc(world_pos);

  uv_interp = au;

  normal_interp = normalize(normal_object_to_view(nor));

  float metallic, roughness;
  workbench_material_data_get(resource_handle, color_interp, alpha_interp, roughness, metallic);

  if (materialIndex == 0) {
    color_interp = ac.rgb;
  }

  packed_rough_metal = workbench_float_pair_encode(roughness, metallic);

  object_id = int(uint(resource_handle) & 0xFFFFu) + 1;
}
