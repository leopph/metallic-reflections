// ReSharper disable CppEnforceCVQualifiersPlacement

#ifndef GBUFFER_HLSLI
#define GBUFFER_HLSLI

#include "resource_binding_helpers.hlsli"
#include "shader_interop.h"

cbuffer g_object_cb : register(MAKE_REGISTER(b, OBJECT_CB_SLOT)) {
  ObjectConstants g_object_cb;
}


cbuffer g_camera_cb : register(MAKE_REGISTER(b, CAMERA_CB_SLOT)) {
  CameraConstants g_camera_cb;
}


cbuffer g_material_cb : register(MAKE_REGISTER(b, MATERIAL_CB_SLOT)) {
  Material g_material;
}


Texture2D<float3> g_base_color_map : register(MAKE_REGISTER(t, MATERIAL_BASE_COLOR_MAP_SLOT));
Texture2D<float> g_roughness_map : register(MAKE_REGISTER(t, MATERIAL_ROUGHNESS_MAP_SLOT));
Texture2D<float3> g_normal_map : register(MAKE_REGISTER(t, MATERIAL_NORMAL_MAP_SLOT));
SamplerState g_sampler : register(MAKE_REGISTER(s, MATERIAL_SAMPLER_SLOT));


struct VsIn {
  float3 pos_os : POSITION;
  float3 norm_os : NORMAL;
  float3 tan_os : TANGENT;
  float2 uv : TEXCOORD;
};


struct PsIn {
  float4 pos_cs : SV_Position;
  float3 pos_ws : POSITION;
  float3 norm_ws : NORMAL;
  float4 tan_ws : TANGENT;
  float2 uv : TEXCOORD;
};


PsIn VsMain(const VsIn vs_in) {
  PsIn ret;
  ret.pos_ws = mul(float4(vs_in.pos_os, 1), g_object_cb.world_mtx).xyz;
  ret.norm_ws = mul(float4(vs_in.norm_os, 0), g_object_cb.normal_mtx).xyz;
  ret.tan_ws = float4(mul(float4(vs_in.tan_os, 0), g_object_cb.normal_mtx).xyz, 1); // w is handedness
  ret.pos_cs = mul(float4(ret.pos_ws, 1), g_camera_cb.view_proj_mtx);
  ret.uv = vs_in.uv;
  return ret;
}


void PsMain(const PsIn ps_in, out float4 gbuffer0 : SV_Target0, out float3 gbuffer1 : SV_Target1) {
  gbuffer0.rgb = g_material.has_base_color_map
                   ? g_material.base_color * g_base_color_map.Sample(g_sampler, ps_in.uv).rgb
                   : g_material.base_color;

  gbuffer0.a = g_material.has_roughness_map
                 ? g_material.roughness * g_roughness_map.Sample(g_sampler, ps_in.uv).r
                 : g_material.roughness;

  if (g_material.has_normal_map) {
    const float3 normal_ws = normalize(ps_in.norm_ws);
    const float3 tangent_ws = normalize(ps_in.tan_ws.xyz - normal_ws * dot(normal_ws, ps_in.tan_ws.xyz));
    const float3 bitangent_ws = cross(normal_ws, tangent_ws) * ps_in.tan_ws.w; // w is handedness

    const float3 sampled_normal_ts = g_normal_map.Sample(g_sampler, ps_in.uv).rgb * 2 - 1;
    const float3 sampled_normal_ws = normalize(
      sampled_normal_ts.x * tangent_ws +
      sampled_normal_ts.y * bitangent_ws +
      sampled_normal_ts.z * normal_ws
    );

    gbuffer1.rgb = sampled_normal_ws;
  } else {
    gbuffer1.rgb = normalize(ps_in.norm_ws);
  }
}

#endif
