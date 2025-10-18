#ifndef SHADER_INTEROP_H
#define SHADER_INTEROP_H

#if defined(__cplusplus)
#define row_major
#include <array>
using float3 = std::array<float, 3>;
using float4x4 = std::array<std::array<float, 4>, 4>;
#else
#define BOOL bool
#endif

#define TONEMAPPING_HDR_TEX_SRV_SLOT 0
#define TONEMAPPING_SAMPLER_SLOT 0

#define MATERIAL_BASE_COLOR_MAP_SLOT 0
#define MATERIAL_ROUGHNESS_MAP_SLOT 1
#define MATERIAL_NORMAL_MAP_SLOT 2
#define MATERIAL_SAMPLER_SLOT 0
#define OBJECT_CB_SLOT 0
#define CAMERA_CB_SLOT 1
#define MATERIAL_CB_SLOT 2

struct Material {
  float3 base_color;
  BOOL has_base_color_map;
  float roughness;
  BOOL has_roughness_map;
  BOOL has_normal_map;
  float pad;
};

struct ObjectConstants {
  row_major float4x4 world_mtx;
  row_major float4x4 normal_mtx;
};

struct CameraConstants {
  row_major float4x4 view_mtx;
  row_major float4x4 proj_mtx;
  row_major float4x4 view_proj_mtx;
  float3 position;
  float pad;
};

#endif
