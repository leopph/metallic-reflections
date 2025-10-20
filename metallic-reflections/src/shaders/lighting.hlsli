// ReSharper disable CppEnforceCVQualifiersPlacement

#include "brdf.hlsli"
#include "fullscreen_tri.hlsli"
#include "resource_binding_helpers.hlsli"
#include "shader_interop.h"

cbuffer CamCbuffer : register(MAKE_REGISTER(b, LIGHTING_CAM_CB_SLOT)) {
  CameraConstants g_cam_constants;
}

Texture2D g_gbuffer0 : register(MAKE_REGISTER(t, LIGHTING_GBUFFER0_SRV_SLOT));
Texture2D g_gbuffer1 : register(MAKE_REGISTER(t, LIGHTING_GBUFFER1_SRV_SLOT));
Texture2D g_depth_tex : register(MAKE_REGISTER(t, LIGHTING_DEPTH_SRV_SLOT));
TextureCube g_env_map : register(MAKE_REGISTER(t, LIGHTING_ENV_MAP_SRV_SLOT));
SamplerState g_gbuffer_samp : register(MAKE_REGISTER(s, LIGHTING_GBUFFER_SAMPLER_SLOT));
SamplerState g_env_samp : register(MAKE_REGISTER(s, LIGHTING_ENV_SAMPLER_SLOT));


struct PsIn {
  float4 pos_os : SV_Position;
  float2 uv : TEXCOORD;
};


PsIn VsMain(const uint vertex_id : SV_VertexID) {
  PsIn ret;
  MakeFullscreenTriangleVertex(vertex_id, ret.pos_os, ret.uv);
  return ret;
}


float4 PsMain(const PsIn ps_in) : SV_Target {
  const float4 gbuffer0_data = g_gbuffer0.Sample(g_gbuffer_samp, ps_in.uv);
  const float4 gbuffer1_data = g_gbuffer1.Sample(g_gbuffer_samp, ps_in.uv);
  const float depth = g_depth_tex.Sample(g_gbuffer_samp, ps_in.uv).r;

  const float3 base_color = gbuffer0_data.rgb;
  const float roughness = gbuffer0_data.a;
  const float3 normal_ws = gbuffer1_data.rgb;

  float4 pos_ws = mul(float4(ps_in.uv * 2 - 1, depth, 1), g_cam_constants.view_proj_inv_mtx);
  pos_ws /= pos_ws.w;

  const float3 V = normalize(g_cam_constants.pos_ws - pos_ws.xyz);
  const float3 R = reflect(-V, normal_ws);

  float3 env_map_size; // width, height, mips
  g_env_map.GetDimensions(0, env_map_size.x, env_map_size.y, env_map_size.z);

  const float env_mip = roughness * clamp(roughness * env_map_size.z - 1, 0, env_map_size.z - 1);
  const float3 env = g_env_map.SampleLevel(g_env_samp, R, env_mip).rgb;
  const float3 F = FresnelSchlick(dot(normal_ws, V), base_color);

  const float3 final_color = env * F;
  return float4(final_color, 1);
}
