// ReSharper disable CppEnforceCVQualifiersPlacement

#include "brdf.hlsli"
#include "change_of_basis.hlsli"
#include "resource_binding_helpers.hlsli"
#include "shader_interop.h"

cbuffer CameraCbuffer : register(MAKE_REGISTER(b, SSR_CAM_CB_SLOT)) {
  CameraConstants g_cam_constants;
}

Texture2D<float> g_depth_tex : register(MAKE_REGISTER(t, SSR_DEPTH_SRV_SLOT));
Texture2D g_gbuffer0 : register(MAKE_REGISTER(t, SSR_GBUFFER0_SRV_SLOT));
Texture2D g_gbuffer1 : register(MAKE_REGISTER(t, SSR_GBUFFER1_SRV_SLOT));
Texture2D g_ibl_tex : register(MAKE_REGISTER(t, SSR_IBL_SRV_SLOT));
RWTexture2D<float4> g_ssr_tex : register(MAKE_REGISTER(u, SSR_SSR_UAV_SLOT));

[numthreads(SSR_THREADS_X, SSR_THREADS_Y, 1)]
void CsMain(const uint3 dtid : SV_DispatchThreadID) {
  uint2 depth_tex_size;
  g_depth_tex.GetDimensions(depth_tex_size.x, depth_tex_size.y);

  // Check bounds

  if (dtid.x > depth_tex_size.x || dtid.y > depth_tex_size.y) {
    return;
  }

  const float depth = g_depth_tex[dtid.xy];

  // Check background

  if (depth >= 0.9999) {
    g_ssr_tex[dtid.xy] = g_ibl_tex[dtid.xy];
    return;
  }

  const float4 gbuffer0_value = g_gbuffer0[dtid.xy];
  const float roughness = gbuffer0_value.a;

  // Check roughness

  if (roughness >= 0.5) {
    g_ssr_tex[dtid.xy] = g_ibl_tex[dtid.xy];
    return;
  }

  const float4 gbuffer1_value = g_gbuffer1[dtid.xy];
  const float3 normal_ws = gbuffer1_value.rgb;
  const float3 normal_vs = mul(float4(normal_ws, 0.0), g_cam_constants.view_mtx).xyz;

  const float4 pos4_vs = mul(float4(UvToNdc(float2(dtid.xy) / float2(depth_tex_size)), depth, 1.0),
                             g_cam_constants.proj_inv_mtx);
  const float3 pos_vs = pos4_vs.xyz / pos4_vs.w;

  const float3 V = normalize(-pos_vs);
  const float3 R = reflect(-V, normal_vs);

  bool hit = false;
  uint2 hit_pixel;

  const float step_size = 0.001;
  const float thickness = 0.005;

  const float3 ray_start_vs = pos_vs + 0.1 * R;

  for (int i = 0; i <= 10000; i++) {
    const float3 test_pos_vs = ray_start_vs + i * step_size * R;
    const float4 test_pos_cs = mul(float4(test_pos_vs, 1), g_cam_constants.proj_mtx);
    const float3 test_pos_ndc = test_pos_cs.xyz / test_pos_cs.w;
    const float2 test_uv = NdcToUv(test_pos_ndc);
    const uint2 test_px = uint2(test_uv * float2(depth_tex_size));

    if (test_px.x >= depth_tex_size.x || test_px.y >= depth_tex_size.y) {
      break;
    }

    const float test_depth_ndc = g_depth_tex[test_px].r;
    const float test_depth_vs = NdcToViewDepth(test_depth_ndc, g_cam_constants.near_clip, g_cam_constants.far_clip);

    if (abs(test_pos_vs.z - test_depth_vs) < thickness) {
      hit = true;
      hit_pixel = test_px;
      break;
    }
  }

  //const float2 half_depth_tex_size = float2(depth_tex_size) / 2;

  //const float4x4 cs_to_px = {
  //  half_depth_tex_size.x, 0.0, 0.0, 0.0,
  //  0.0, -half_depth_tex_size.y, 0.0, 0.0,
  //  0.0, 0.0, 1.0, 0.0,
  //  half_depth_tex_size.x, half_depth_tex_size.y, 0.0, 1.0
  //};

  //float4x4 proj_to_px = mul(g_cam_constants.proj_mtx, cs_to_px);

  //float2 hit_pixel;
  //float3 hit_point_vs;
  //const bool hit = MarchRay(pos_vs + 0.1 * R, R, proj_to_px, g_depth_tex, 1.0, true, g_cam_constants.near_clip,
  //                          g_cam_constants.far_clip, 1, 0, 1000, 1000, hit_pixel, hit_point_vs);

  if (hit) {
    const float3 hit_color = g_ibl_tex[hit_pixel].rgb;
    const float3 px_color = g_ibl_tex[dtid.xy].rgb;
    const float3 F = FresnelSchlick(saturate(dot(normal_vs, V)), hit_color);
    const float3 weight = pow(1.0 - roughness, 3.0) * F;
    g_ssr_tex[dtid.xy] = float4(lerp(px_color, hit_color, weight), 1);
  } else {
    g_ssr_tex[dtid.xy] = g_ibl_tex[dtid.xy];
  }
}
