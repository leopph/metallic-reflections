#include "shader_collection.hpp"

#ifndef NDEBUG
#include "shaders/generated/Debug/env_prefilter_cs.h"
#include "shaders/generated/Debug/equirect_to_cube.h"
#include "shaders/generated/Debug/gbuffer_ps.h"
#include "shaders/generated/Debug/gbuffer_vs.h"
#include "shaders/generated/Debug/lighting_ps.h"
#include "shaders/generated/Debug/lighting_vs.h"
#include "shaders/generated/Debug/ssr_cs.h"
#include "shaders/generated/Debug/tonemapping_ps.h"
#include "shaders/generated/Debug/tonemapping_vs.h"
#else
#include "shaders/generated/Release/env_prefilter_cs.h"
#include "shaders/generated/Release/equirect_to_cube.h"
#include "shaders/generated/Release/gbuffer_ps.h"
#include "shaders/generated/Release/gbuffer_vs.h"
#include "shaders/generated/Release/lighting_ps.h"
#include "shaders/generated/Release/lighting_vs.h"
#include "shaders/generated/Release/ssr_cs.h"
#include "shaders/generated/Release/tonemapping_ps.h"
#include "shaders/generated/Release/tonemapping_vs.h"
#endif

import std;

namespace refl {
auto LoadShaders(ID3D11Device5& dev) -> std::optional<ShaderCollection> {
  ShaderCollection shaders;

  if (FAILED(dev.CreateVertexShader(
    g_lighting_vs_bytes, ARRAYSIZE(g_lighting_vs_bytes), nullptr,
    &shaders.lighting_vs))) {
    return std::nullopt;
  }

  if (FAILED(dev.CreatePixelShader(
    g_lighting_ps_bytes, ARRAYSIZE(g_lighting_ps_bytes), nullptr,
    &shaders.lighting_ps))) {
    return std::nullopt;
  }

  if (FAILED(dev.CreateVertexShader(
    g_tonemapping_vs_bytes, ARRAYSIZE(g_tonemapping_vs_bytes), nullptr,
    &shaders.tonemapping_vs))) {
    return std::nullopt;
  }

  if (FAILED(dev.CreatePixelShader(
    g_tonemapping_ps_bytes, ARRAYSIZE(g_tonemapping_ps_bytes), nullptr,
    &shaders.tonemapping_ps))) {
    return std::nullopt;
  }

  if (FAILED(dev.CreateVertexShader(
    g_gbuffer_vs_bytes, ARRAYSIZE(g_gbuffer_vs_bytes), nullptr,
    &shaders.gbuffer_vs))) {
    return std::nullopt;
  }

  if (FAILED(dev.CreatePixelShader(
    g_gbuffer_ps_bytes, ARRAYSIZE(g_gbuffer_ps_bytes), nullptr,
    &shaders.gbuffer_ps))) {
    return std::nullopt;
  }

  if (FAILED(dev.CreateComputeShader(
    g_equirect_to_cube_bytes, ARRAYSIZE(g_equirect_to_cube_bytes), nullptr,
    &shaders.equirect_to_cubemap_cs))) {
    return std::nullopt;
  }

  if (FAILED(dev.CreateComputeShader(
    g_env_prefilter_cs_bytes, ARRAYSIZE(g_env_prefilter_cs_bytes), nullptr,
    &shaders.env_prefilter_cs))) {
    return std::nullopt;
  }

  if (FAILED(dev.CreateComputeShader(
    g_ssr_cs_bytes, ARRAYSIZE(g_ssr_cs_bytes), nullptr,
    &shaders.ssr_cs))) {
    return std::nullopt;
  }

  std::array constexpr input_elements{
    D3D11_INPUT_ELEMENT_DESC{
      .SemanticName = "POSITION",
      .SemanticIndex = 0,
      .Format = DXGI_FORMAT_R32G32B32A32_FLOAT,
      .InputSlot = 0,
      .AlignedByteOffset = 0,
      .InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA,
      .InstanceDataStepRate = 0
    },
    D3D11_INPUT_ELEMENT_DESC{
      .SemanticName = "NORMAL",
      .SemanticIndex = 0,
      .Format = DXGI_FORMAT_R32G32B32A32_FLOAT,
      .InputSlot = 1,
      .AlignedByteOffset = 0,
      .InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA,
      .InstanceDataStepRate = 0
    },
    D3D11_INPUT_ELEMENT_DESC{
      .SemanticName = "TEXCOORD",
      .SemanticIndex = 0,
      .Format = DXGI_FORMAT_R32G32_FLOAT,
      .InputSlot = 2,
      .AlignedByteOffset = 0,
      .InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA,
      .InstanceDataStepRate = 0
    },
    D3D11_INPUT_ELEMENT_DESC{
      .SemanticName = "TANGENT",
      .SemanticIndex = 0,
      .Format = DXGI_FORMAT_R32G32B32A32_FLOAT,
      .InputSlot = 3,
      .AlignedByteOffset = 0,
      .InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA,
      .InstanceDataStepRate = 0
    },
  };

  if (FAILED(dev.CreateInputLayout(
    input_elements.data(), static_cast<UINT>(input_elements.size()),
    g_gbuffer_vs_bytes, ARRAYSIZE(g_gbuffer_vs_bytes),
    &shaders.mesh_il))) {
    return std::nullopt;
  }

  return shaders;
}
}
