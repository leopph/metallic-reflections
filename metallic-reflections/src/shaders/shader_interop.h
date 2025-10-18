#ifndef SHADER_INTEROP_H
#define SHADER_INTEROP_H

#if defined(__cplusplus)
#include <array>
using float3 = std::array<float, 3>;
#endif

#define TONEMAPPING_HDR_TEX_SRV_SLOT 0
#define TONEMAPPING_SAMPLER_SLOT 0

struct SphereGeometry {
  float3 center_ws;
  float radius;
};

struct Material {
  float3 albedo;
  float roughness;
  float metallic;
  float emissive;
};

struct SphereInfo {
  SphereGeometry geometry;
  Material material;
};

#endif
