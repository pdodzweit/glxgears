#version 330
#define DFDX_SIGN 1.0
#define DFDY_SIGN 1.0
#define OS_UNIX
#define GPU_FRAGMENT_SHADER
#define IN_OUT in
#define blender_srgb_to_framebuffer_space(a) a
#define V3D_LIGHTING_STUDIO
#define WORKBENCH_ENCODE_NORMALS
#define OPAQUE_MATERIAL
#define COMMON_VIEW_LIB
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

#define ViewNear (ViewVecs[0].w)
#define ViewFar (ViewVecs[1].w)

#define cameraForward ViewMatrixInverse[2].xyz
#define cameraPos ViewMatrixInverse[3].xyz
#define cameraVec(P) ((ProjectionMatrix[3][3] == 0.0) ? normalize(cameraPos - P) : cameraForward)
#define viewCameraVec(vP) ((ProjectionMatrix[3][3] == 0.0) ? normalize(-vP) : vec3(0.0, 0.0, 1.0))

#ifdef world_clip_planes_calc_clip_distance
#  undef world_clip_planes_calc_clip_distance
#  define world_clip_planes_calc_clip_distance(p) \
    _world_clip_planes_calc_clip_distance(p, clipPlanes)
#endif

#ifdef COMMON_GLOBALS_LIB
float mul_project_m4_v3_zfac(in vec3 co)
{
  return pixelFac * ((ViewProjectionMatrix[0][3] * co.x) + (ViewProjectionMatrix[1][3] * co.y) +
                     (ViewProjectionMatrix[2][3] * co.z) + ViewProjectionMatrix[3][3]);
}
#endif

/* Not the right place but need to be common to all overlay's.
 * TODO: Split to an overlay lib. */
mat4 extract_matrix_packed_data(mat4 mat, out vec4 dataA, out vec4 dataB)
{
  const float div = 1.0 / 255.0;
  int a = int(mat[0][3]);
  int b = int(mat[1][3]);
  int c = int(mat[2][3]);
  int d = int(mat[3][3]);
  dataA = vec4(a & 0xFF, a >> 8, b & 0xFF, b >> 8) * div;
  dataB = vec4(c & 0xFF, c >> 8, d & 0xFF, d >> 8) * div;
  mat[0][3] = mat[1][3] = mat[2][3] = 0.0;
  mat[3][3] = 1.0;
  return mat;
}

/* Same here, Not the right place but need to be common to all overlay's.
 * TODO: Split to an overlay lib. */
/* edge_start and edge_pos needs to be in the range [0..sizeViewport]. */
vec4 pack_line_data(vec2 frag_co, vec2 edge_start, vec2 edge_pos)
{
  vec2 edge = edge_start - edge_pos;
  float len = length(edge);
  if (len > 0.0) {
    edge /= len;
    vec2 perp = vec2(-edge.y, edge.x);
    float dist = dot(perp, frag_co - edge_start);
    /* Add 0.1 to diffenrentiate with cleared pixels. */
    return vec4(perp * 0.5 + 0.5, dist * 0.25 + 0.5 + 0.1, 1.0);
  }
  else {
    /* Default line if the origin is perfectly aligned with a pixel. */
    return vec4(1.0, 0.0, 0.5 + 0.1, 1.0);
  }
}

uniform int resourceChunk;

#ifdef GPU_VERTEX_SHADER
#  ifdef GPU_ARB_shader_draw_parameters
#    define baseInstance gl_BaseInstanceARB
#  else /* no ARB_shader_draw_parameters */
uniform int baseInstance;
#  endif

#  if defined(IN_PLACE_INSTANCES) || defined(INSTANCED_ATTR)
/* When drawing instances of an object at the same position. */
#    define instanceId 0
#  elif defined(GPU_DEPRECATED_AMD_DRIVER)
/* A driver bug make it so that when using an attribute with GL_INT_2_10_10_10_REV as format,
 * the gl_InstanceID is incremented by the 2 bit component of the attribute.
 * Ignore gl_InstanceID then. */
#    define instanceId 0
#  else
#    define instanceId gl_InstanceID
#  endif

#  ifdef UNIFORM_RESOURCE_ID
/* This is in the case we want to do a special instance drawcall but still want to have the
 * right resourceId and all the correct ubo datas. */
uniform int resourceId;
#    define resource_id resourceId
#  else
#    define resource_id (baseInstance + instanceId)
#  endif

/* Use this to declare and pass the value if
 * the fragment shader uses the resource_id. */
#  ifdef USE_GEOMETRY_SHADER
#    define RESOURCE_ID_VARYING flat out int resourceIDGeom;
#    define PASS_RESOURCE_ID resourceIDGeom = resource_id;
#  else
#    define RESOURCE_ID_VARYING flat out int resourceIDFrag;
#    define PASS_RESOURCE_ID resourceIDFrag = resource_id;
#  endif
#endif

/* If used in a fragment / geometry shader, we pass
 * resource_id as varying. */
#ifdef GPU_GEOMETRY_SHADER
#  define RESOURCE_ID_VARYING \
    flat out int resourceIDFrag; \
    flat in int resourceIDGeom[];

#  define resource_id resourceIDGeom
#  define PASS_RESOURCE_ID resourceIDFrag = resource_id[0];
#endif

#ifdef GPU_FRAGMENT_SHADER
flat in int resourceIDFrag;
#  define resource_id resourceIDFrag
#endif

/* Breaking this across multiple lines causes issues for some older GLSL compilers. */
/* clang-format off */
#if !defined(GPU_INTEL) && !defined(GPU_DEPRECATED_AMD_DRIVER) && !defined(OS_MAC) && !defined(INSTANCED_ATTR)
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

#else /* GPU_INTEL */
/* Intel GPU seems to suffer performance impact when the model matrix is in UBO storage.
 * So for now we just force using the legacy path. */
/* Note that this is also a workaround of a problem on osx (amd or nvidia)
 * and older amd driver on windows. */
uniform mat4 ModelMatrix;
uniform mat4 ModelMatrixInverse;
#endif

#define resource_handle (resourceChunk * DRW_RESOURCE_CHUNK_LEN + resource_id)

/** Transform shortcuts. */
/* Rule of thumb: Try to reuse world positions and normals because converting through viewspace
 * will always be decomposed in at least 2 matrix operation. */

/**
 * Some clarification:
 * Usually Normal matrix is transpose(inverse(ViewMatrix * ModelMatrix))
 *
 * But since it is slow to multiply matrices we decompose it. Decomposing
 * inversion and transposition both invert the product order leaving us with
 * the same original order:
 * transpose(ViewMatrixInverse) * transpose(ModelMatrixInverse)
 *
 * Knowing that the view matrix is orthogonal, the transpose is also the inverse.
 * Note: This is only valid because we are only using the mat3 of the ViewMatrixInverse.
 * ViewMatrix * transpose(ModelMatrixInverse)
 */
#define NormalMatrix transpose(mat3(ModelMatrixInverse))
#define NormalMatrixInverse transpose(mat3(ModelMatrix))

#define normal_object_to_view(n) (mat3(ViewMatrix) * (NormalMatrix * n))
#define normal_object_to_world(n) (NormalMatrix * n)
#define normal_world_to_object(n) (NormalMatrixInverse * n)
#define normal_world_to_view(n) (mat3(ViewMatrix) * n)
#define normal_view_to_world(n) (mat3(ViewMatrixInverse) * n)

#define point_object_to_ndc(p) (ViewProjectionMatrix * vec4((ModelMatrix * vec4(p, 1.0)).xyz, 1.0))
#define point_object_to_view(p) ((ViewMatrix * vec4((ModelMatrix * vec4(p, 1.0)).xyz, 1.0)).xyz)
#define point_object_to_world(p) ((ModelMatrix * vec4(p, 1.0)).xyz)
#define point_view_to_ndc(p) (ProjectionMatrix * vec4(p, 1.0))
#define point_view_to_object(p) ((ModelMatrixInverse * (ViewMatrixInverse * vec4(p, 1.0))).xyz)
#define point_view_to_world(p) ((ViewMatrixInverse * vec4(p, 1.0)).xyz)
#define point_world_to_ndc(p) (ViewProjectionMatrix * vec4(p, 1.0))
#define point_world_to_object(p) ((ModelMatrixInverse * vec4(p, 1.0)).xyz)
#define point_world_to_view(p) ((ViewMatrix * vec4(p, 1.0)).xyz)

/* Due to some shader compiler bug, we somewhat need to access gl_VertexID
 * to make vertex shaders work. even if it's actually dead code. */
#ifdef GPU_INTEL
#  define GPU_INTEL_VERTEX_SHADER_WORKAROUND gl_Position.x = float(gl_VertexID);
#else
#  define GPU_INTEL_VERTEX_SHADER_WORKAROUND
#endif

#define DRW_BASE_SELECTED (1 << 1)
#define DRW_BASE_FROM_DUPLI (1 << 2)
#define DRW_BASE_FROM_SET (1 << 3)
#define DRW_BASE_ACTIVE (1 << 4)

/* ---- Opengl Depth conversion ---- */

float linear_depth(bool is_persp, float z, float zf, float zn)
{
  if (is_persp) {
    return (zn * zf) / (z * (zn - zf) + zf);
  }
  else {
    return (z * 2.0 - 1.0) * zf;
  }
}

float buffer_depth(bool is_persp, float z, float zf, float zn)
{
  if (is_persp) {
    return (zf * (zn - z)) / (z * (zn - zf));
  }
  else {
    return (z / (zf * 2.0)) + 0.5;
  }
}

float get_view_z_from_depth(float depth)
{
  if (ProjectionMatrix[3][3] == 0.0) {
    float d = 2.0 * depth - 1.0;
    return -ProjectionMatrix[3][2] / (d + ProjectionMatrix[2][2]);
  }
  else {
    return ViewVecs[0].z + depth * ViewVecs[1].z;
  }
}

float get_depth_from_view_z(float z)
{
  if (ProjectionMatrix[3][3] == 0.0) {
    float d = (-ProjectionMatrix[3][2] / z) - ProjectionMatrix[2][2];
    return d * 0.5 + 0.5;
  }
  else {
    return (z - ViewVecs[0].z) / ViewVecs[1].z;
  }
}

vec2 get_uvs_from_view(vec3 view)
{
  vec4 ndc = ProjectionMatrix * vec4(view, 1.0);
  return (ndc.xy / ndc.w) * 0.5 + 0.5;
}

vec3 get_view_space_from_depth(vec2 uvcoords, float depth)
{
  if (ProjectionMatrix[3][3] == 0.0) {
    return vec3(ViewVecs[0].xy + uvcoords * ViewVecs[1].xy, 1.0) * get_view_z_from_depth(depth);
  }
  else {
    return ViewVecs[0].xyz + vec3(uvcoords, depth) * ViewVecs[1].xyz;
  }
}

vec3 get_world_space_from_depth(vec2 uvcoords, float depth)
{
  return (ViewMatrixInverse * vec4(get_view_space_from_depth(uvcoords, depth), 1.0)).xyz;
}

vec3 get_view_vector_from_screen_uv(vec2 uv)
{
  if (ProjectionMatrix[3][3] == 0.0) {
    return normalize(vec3(ViewVecs[0].xy + uv * ViewVecs[1].xy, 1.0));
  }
  else {
    return vec3(0.0, 0.0, 1.0);
  }
}

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

#define EPSILON 0.00001

#define CAVITY_BUFFER_RANGE 4.0

#ifdef WORKBENCH_ENCODE_NORMALS

#  define WB_Normal vec2

/* From http://aras-p.info/texts/CompactNormalStorage.html
 * Using Method #4: Spheremap Transform */
vec3 workbench_normal_decode(vec4 enc)
{
  vec2 fenc = enc.xy * 4.0 - 2.0;
  float f = dot(fenc, fenc);
  float g = sqrt(1.0 - f / 4.0);
  vec3 n;
  n.xy = fenc * g;
  n.z = 1 - f / 2;
  return n;
}

/* From http://aras-p.info/texts/CompactNormalStorage.html
 * Using Method #4: Spheremap Transform */
WB_Normal workbench_normal_encode(bool front_face, vec3 n)
{
  n = normalize(front_face ? n : -n);
  float p = sqrt(n.z * 8.0 + 8.0);
  n.xy = clamp(n.xy / p + 0.5, 0.0, 1.0);
  return n.xy;
}

#else
#  define WB_Normal vec3
/* Well just do nothing... */
#  define workbench_normal_encode(f, a) (a)
#  define workbench_normal_decode(a) (a.xyz)
#endif /* WORKBENCH_ENCODE_NORMALS */

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

void workbench_float_pair_decode(float data, out float v1, out float v2)
{
  // const uint v1_mask = ~(0xFFFFFFFFu << ROUGHNESS_BITS);
  // const uint v2_mask = ~(0xFFFFFFFFu << METALLIC_BITS);
  /* Same as above because some compiler are very dumb and think we use medium int. */
  const int v1_mask = 0x1F;
  const int v2_mask = 0x7;
  int idata = int(data);
  v1 = float(idata & v1_mask) * (1.0 / float(v1_mask));
  v2 = float(idata >> int(ROUGHNESS_BITS)) * (1.0 / float(v2_mask));
}

/* TODO(fclem): deduplicate code. */
bool node_tex_tile_lookup(inout vec3 co, sampler2DArray ima, sampler1DArray map)
{
  vec2 tile_pos = floor(co.xy);

  if (tile_pos.x < 0 || tile_pos.y < 0 || tile_pos.x >= 10) {
    return false;
  }

  float tile = 10.0 * tile_pos.y + tile_pos.x;
  if (tile >= textureSize(map, 0).x) {
    return false;
  }

  /* Fetch tile information. */
  float tile_layer = texelFetch(map, ivec2(tile, 0), 0).x;
  if (tile_layer < 0.0) {
    return false;
  }

  vec4 tile_info = texelFetch(map, ivec2(tile, 1), 0);

  co = vec3(((co.xy - tile_pos) * tile_info.zw) + tile_info.xy, tile_layer);
  return true;
}

uniform sampler2DArray imageTileArray;
uniform sampler1DArray imageTileData;
uniform sampler2D imageTexture;

uniform float imageTransparencyCutoff = 0.1;
uniform bool imagePremult;

vec3 workbench_image_color(vec2 uvs)
{
#ifdef V3D_SHADING_TEXTURE_COLOR
  vec4 color;

#  ifdef TEXTURE_IMAGE_ARRAY
  vec3 co = vec3(uvs, 0.0);
  if (node_tex_tile_lookup(co, imageTileArray, imageTileData)) {
    color = texture(imageTileArray, co);
  }
  else {
    color = vec4(1.0, 0.0, 1.0, 1.0);
  }
#  else

  color = texture(imageTexture, uvs);
#  endif

  /* Unpremultiply if stored multiplied, since straight alpha is expected by shaders. */
  if (imagePremult && !(color.a == 0.0 || color.a == 1.0)) {
    color.rgb /= color.a;
  }

#  ifdef GPU_FRAGMENT_SHADER
  if (color.a < imageTransparencyCutoff) {
    discard;
  }
#  endif

  return color.rgb;
#else

  return vec3(1.0);
#endif
}

#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(workbench_shader_interface_lib.glsl)
#pragma BLENDER_REQUIRE(workbench_common_lib.glsl)
#pragma BLENDER_REQUIRE(workbench_image_lib.glsl)

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

#ifdef V3D_SHADING_TEXTURE_COLOR
  materialData.rgb = workbench_image_color(uv_interp);
#endif
}
