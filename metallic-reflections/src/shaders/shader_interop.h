#ifndef SHADER_INTEROP_H
#define SHADER_INTEROP_H

#if defined(__cplusplus)
#define row_major
#include <cstdint>
#include <DirectXMath.h>
using float3 = DirectX::XMFLOAT3;
using float4x4 = DirectX::XMFLOAT4X4;
using uint = std::uint32_t;
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

#define EQUIRECT_ENV_MAP_SRV_SLOT 0
#define ENV_CUBE_UAV_SLOT 0
#define EQUIRECT_ENV_MAP_SAMPLER_SLOT 0
#define EQUIRECT_TO_CUBE_THREADS_X 8
#define EQUIRECT_TO_CUBE_THREADS_Y 8

#define ENV_PREFILTER_CUBE_SRV_SLOT 0
#define ENV_PREFILTER_ENV_CUBE_UAV_SLOT 0
#define ENV_PREFILTER_SAMPLER_SLOT 0
#define ENV_PREFILTER_CB_SLOT 0
#define ENV_PREFILTER_THREADS_X 8
#define ENV_PREFILTER_THREADS_Y 8

#define LIGHTING_GBUFFER0_SRV_SLOT 0
#define LIGHTING_GBUFFER1_SRV_SLOT 1
#define LIGHTING_DEPTH_SRV_SLOT 2
#define LIGHTING_ENV_MAP_SRV_SLOT 3
#define LIGHTING_GBUFFER_SAMPLER_SLOT 0
#define LIGHTING_ENV_SAMPLER_SLOT 1
#define LIGHTING_CAM_CB_SLOT 0

#define SSR_DEPTH_SRV_SLOT 0
#define SSR_GBUFFER0_SRV_SLOT 1
#define SSR_GBUFFER1_SRV_SLOT 2
#define SSR_IBL_SRV_SLOT 3
#define SSR_SSR_UAV_SLOT 0
#define SSR_CAM_CB_SLOT 0
#define SSR_THREADS_X 8
#define SSR_THREADS_Y 8


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
  row_major float4x4 view_inv_mtx;
  row_major float4x4 proj_mtx;
  row_major float4x4 proj_inv_mtx;
  row_major float4x4 view_proj_mtx;
  row_major float4x4 view_proj_inv_mtx;
  float3 pos_ws;
  float near_clip;
	float far_clip;
  float3 pad;
};

struct EnvPrefilterConstants {
  uint cur_mip; // 0 is original env map
  uint num_mips;
  uint face_base_size; // Size of face at mip 0
  uint sample_count; // Number of GGX samples to use for this prefiltering pass
};

#endif
