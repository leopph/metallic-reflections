// ReSharper disable CppEnforceCVQualifiersPlacement

#ifndef TONEMAPPING_HLSLI
#define TONEMAPPING_HLSLI

#include "fullscreen_tri.hlsli"
#include "resource_binding_helpers.hlsli"
#include "shader_interop.h"

Texture2D g_hdr_tex : register(MAKE_REGISTER(t, TONEMAPPING_HDR_TEX_SRV_SLOT));
SamplerState g_sampler_point_clamp : register(MAKE_REGISTER(s, TONEMAPPING_SAMPLER_SLOT));

static const float3x3 aces_input_matrix = {
  0.59719f, 0.07600f, 0.02840f,
  0.35458f, 0.90834f, 0.13383f,
  0.04823f, 0.01566f, 0.83777f
};

static const float3x3 aces_output_matrix = {
  1.60475f, -0.10208f, -0.00327f,
  -0.53108f, 1.10813f, -0.07276f,
  -0.07367f, -0.00605f, 1.07602f
};


float3 RrtAndOdtFit(const float3 color) {
  float3 a = color * (color + 0.0245786f) - 0.000090537f;
  float3 b = color * (0.983729f * color + 0.4329510f) + 0.238081f;
  return a / b;
}


float3 TonemapReinhard(const float3 color) {
  return color / (color + 1.0);
}


float3 TonemapAcesFilmic(float3 color) {
  color = mul(color, aces_input_matrix);
  color = RrtAndOdtFit(color);
  return mul(color, aces_output_matrix);
}

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
  const float3 hdr_color = g_hdr_tex.Sample(g_sampler_point_clamp, ps_in.uv).rgb;
  const float3 tonemapped_color = TonemapAcesFilmic(hdr_color);
  return float4(tonemapped_color, 1);
}

#endif
