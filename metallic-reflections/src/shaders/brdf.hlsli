// ReSharper disable CppEnforceCVQualifiersPlacement

#ifndef BRDF_HLSLI
#define BRDF_HLSLI

#include "constants.hlsli"

float DistributionTrowbridgeReitz(const float n_dot_h, const float roughness) {
  const float alpha = roughness * roughness;
  const float alpha2 = alpha * alpha;
  const float n_dot_h2 = n_dot_h * n_dot_h;
  float denom = n_dot_h2 * (alpha2 - 1.0) + 1.0;
  denom = kPi * denom * denom;
  return alpha2 / denom;
}

float3 FresnelSchlick(const float v_dot_h, const float3 f0) {
  return f0 + (1.0 - f0) * pow(saturate(1.0 - v_dot_h), 5.0);
}

#endif
