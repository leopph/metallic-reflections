// ReSharper disable CppEnforceCVQualifiersPlacement

#ifndef BRDF_HLSLI
#define BRDF_HLSLI

float3 FresnelSchlick(const float v_dot_h, const float3 f0) {
  return f0 + (1.0 - f0) * pow(saturate(1.0 - v_dot_h), 5.0);
}

#endif
