// ReSharper disable CppEnforceCVQualifiersPlacement

#ifndef ENV_PREFILTER_HLSLI
#define ENV_PREFILTER_HLSLI

#include "brdf.hlsli"
#include "constants.hlsli"
#include "resource_binding_helpers.hlsli"
#include "shader_interop.h"

cbuffer Constants : register(MAKE_REGISTER(b, ENV_PREFILTER_CB_SLOT)) {
  EnvPrefilterConstants g_constants;
}

TextureCube g_env_map : register(MAKE_REGISTER(t, ENV_PREFILTER_CUBE_SRV_SLOT)); // Sample only at mip 0
RWTexture2DArray<float4> g_prefiltered : register(MAKE_REGISTER(u, ENV_PREFILTER_ENV_CUBE_UAV_SLOT));
// Cubemap, 6 slices
SamplerState g_sampler : register(MAKE_REGISTER(s, ENV_PREFILTER_SAMPLER_SLOT));


// Build orthonormal basis from normal N
void BuildBasis(const float3 normal, out float3 tangent, out float3 bitangent) {
  const float3 up = abs(normal.z) < 0.999 ? float3(0, 0, 1) : float3(1, 0, 0);
  tangent = normalize(cross(up, normal));
  bitangent = cross(normal, tangent);
}


float RadicalInverse_VdC(uint bits) {
  bits = (bits << uint(16U)) | (bits >> uint(16U));
  bits = ((bits & uint(0x55555555U)) << uint(1U)) | ((bits & uint(0xAAAAAAAAU)) >> uint(1U));
  bits = ((bits & uint(0x33333333U)) << uint(2U)) | ((bits & uint(0xCCCCCCCCU)) >> uint(2U));
  bits = ((bits & uint(0x0F0F0F0FU)) << uint(4U)) | ((bits & uint(0xF0F0F0F0U)) >> uint(4U));
  bits = ((bits & uint(0x00FF00FFU)) << uint(8U)) | ((bits & uint(0xFF00FF00U)) >> uint(8U));
  return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}


float2 Hammersley(const uint i, const uint N) {
  return float2(float(i) / float(N), RadicalInverse_VdC(i));
}


// GGX NDF (classic form) returning microfacet normal m in tangent space
float3 SampleGgxNdf(const float2 xi, const float alpha) {
  // Invert CDF for theta (Heitz)
  const float phi = 2.0 * kPi * xi.x;
  const float cos_theta = sqrt((1.0 - xi.y) / (1.0 + (alpha * alpha - 1.0) * xi.y));
  const float sin_theta = sqrt(max(0, 1.0 - cos_theta * cos_theta));
  return float3(sin_theta * cos(phi), sin_theta * sin(phi), cos_theta);
}


// Map face + (u, v) to cubemap direction
// (u, v) are in [0, face_size-1]
float3 CubemapDirection(const uint face_index, const uint2 texel_coord, const uint face_size) {
  // Convert texel center to NDC [-1, 1]
  const float2 st = (float2(texel_coord) + 0.5) / float(face_size);
  const float2 uv = 2 * st - 1;

  float3 direction = float3(1, 0, 0);

  switch (face_index) {
  case 0:
    direction = float3(1.0, -uv.y, -uv.x);
    break;
  case 1:
    direction = float3(-1.0, -uv.y, uv.x);
    break;
  case 2:
    direction = float3(uv.x, 1.0, uv.y);
    break;
  case 3:
    direction = float3(uv.x, -1.0, -uv.y);
    break;
  case 4:
    direction = float3(uv.x, -uv.y, 1.0);
    break;
  case 5:
    direction = float3(-uv.x, -uv.y, -1.0);
    break;
  }

  return normalize(direction);
}


[numthreads(ENV_PREFILTER_THREADS_X, ENV_PREFILTER_THREADS_Y, 1)]
void CsMain(const uint3 dtid : SV_DispatchThreadID) {
  const uint face = dtid.z;
  const uint this_mip_face_size = max(g_constants.face_base_size >> g_constants.cur_mip, 1);

  if (dtid.x >= this_mip_face_size || dtid.y >= this_mip_face_size || face >= 6) {
    return;
  }

  const float roughness = (g_constants.num_mips > 1)
                            ? float(g_constants.cur_mip) / float(g_constants.num_mips - 1)
                            : 0.0;
  const float alpha = roughness * roughness;

  // Per texel direction (assume V = N)
  const float3 N = CubemapDirection(face, dtid.xy, this_mip_face_size);
  const float3 V = N;

  float3 T;
  float3 B;
  BuildBasis(N, T, B);

  float3 accum_color = 0;
  float3 accum_weight = 0;

  // Importance sampling loop
  [loop]
  for (uint i = 0; i < g_constants.sample_count; i++) {
    const float2 xi = Hammersley(i, g_constants.sample_count);
    const float3 H_tan = SampleGgxNdf(xi, alpha);

    // Microfacet normal in world space
    const float3 H = normalize(H_tan.x * T + H_tan.y * B + H_tan.z * N);

    // Reflect view about microfacet
    const float3 L = reflect(-V, H);
    const float n_dot_l = saturate(dot(N, L));

    if (n_dot_l <= 0) {
      continue;
    }

    // Sample from the environment's mip level based on roughness/pdf

    const float n_dot_h = saturate(dot(N, H));
    const float D = DistributionTrowbridgeReitz(n_dot_h, roughness);
    const float v_dot_h = saturate(dot(V, H));
    const float pdf = D * n_dot_h / (4.0 * v_dot_h);

    const float omega_s = 1.0 / (float(g_constants.sample_count) * pdf);
    const float omega_p = 4.0 * kPi / (6.0 * float(g_constants.face_base_size) * float(g_constants.face_base_size));

    const float mip = 0.5 * log2(omega_s / omega_p);

    // Sample the environment map at mip 0
    const float4 env = g_env_map.SampleLevel(g_sampler, L, mip);

    // Accumulate weighted cosie term
    accum_color += env.rgb * n_dot_l;
    accum_weight += n_dot_l;
  }

  const float3 prefiltered = (accum_weight > 0) ? (accum_color / accum_weight) : 0;
  g_prefiltered[dtid] = float4(prefiltered, 1);
}


#endif
