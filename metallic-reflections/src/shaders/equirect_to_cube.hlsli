// ReSharper disable CppEnforceCVQualifiersPlacement

#ifndef EQUIRECT_TO_CUBE_HLSLI
#define EQUIRECT_TO_CUBE_HLSLI

#include "change_of_basis.hlsli"
#include "constants.hlsli"
#include "resource_binding_helpers.hlsli"
#include "shader_interop.h"

Texture2D<float3> g_equi_env_map : register(MAKE_REGISTER(t, EQUIRECT_ENV_MAP_SRV_SLOT));
RWTexture2DArray<float4> g_cube_env_map : register(MAKE_REGISTER(u, ENV_CUBE_UAV_SLOT));
SamplerState g_sampler : register(MAKE_REGISTER(s, EQUIRECT_ENV_MAP_SAMPLER_SLOT));

[numthreads(EQUIRECT_TO_CUBE_THREADS_X, EQUIRECT_TO_CUBE_THREADS_Y, 1)]
void CsMain(const uint3 dtid : SV_DispatchThreadID) {
  const uint x = dtid.x;
  const uint y = dtid.y;
  const uint face = dtid.z;

  uint3 cube_map_size;
  g_cube_env_map.GetDimensions(cube_map_size.x, cube_map_size.y, cube_map_size.z);

  if (x >= cube_map_size.x || y >= cube_map_size.y || face >= cube_map_size.z) {
    return;
  }
  
  const float2 ndc = UvToNdc((float2(x, y) + 0.5) / float2(cube_map_size.xy));

  float3 dir = float3(1, 0, 0);

  switch (face) {
  case 0:
    dir = normalize(float3(1, ndc.y, -ndc.x));
    break;
  case 1:
    dir = normalize(float3(-1, ndc.y, ndc.x));
    break;
  case 2:
    dir = normalize(float3(ndc.x, 1, ndc.y));
    break;
  case 3:
    dir = normalize(float3(ndc.x, -1, -ndc.y));
    break;
  case 4:
    dir = normalize(float3(ndc.x, ndc.y, 1));
    break;
  case 5:
    dir = normalize(float3(-ndc.x, ndc.y, -1));
    break;
  }

  float lon = atan2(dir.z, dir.x);
  float lat = acos(clamp(dir.y, -1, 1));
  float2 uv_eq = float2(lon / (2 * kPi) + 0.5, lat / kPi);

  float3 color = g_equi_env_map.SampleLevel(g_sampler, uv_eq, 0);
  g_cube_env_map[uint3(x, y, face)] = float4(color, 1);
}

#endif
