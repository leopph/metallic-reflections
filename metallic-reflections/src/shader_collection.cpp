#include "shader_collection.hpp"

#ifndef NDEBUG
#include "shaders/generated/Debug/lighting_ps.h"
#include "shaders/generated/Debug/lighting_vs.h"
#include "shaders/generated/Release/tonemapping_ps.h"
#include "shaders/generated/Release/tonemapping_vs.h"
#else
#include "shaders/generated/Release/lighting_ps.h"
#include "shaders/generated/Release/lighting_vs.h"
#include "shaders/generated/Release/tonemapping_ps.h"
#include "shaders/generated/Release/tonemapping_vs.h"
#endif

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

  return shaders;
}
}
