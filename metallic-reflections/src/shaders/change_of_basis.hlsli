// ReSharper disable CppEnforceCVQualifiersPlacement

#ifndef CHANGE_OF_BASIS_HLSLI
#define CHANGE_OF_BASIS_HLSLI

float2 UvToNdc(const float2 uv) {
  return uv * float2(2, -2) + float2(-1, 1);
}

float2 NdcToUv(const float3 ndc) {
  return ndc.xy * float2(0.5, -0.5) + float2(0.5, 0.5);
}

float NdcToViewDepth(const float ndc_depth, const float near_clip, const float far_clip) {
  return (near_clip * far_clip) / (far_clip - ndc_depth * (far_clip - near_clip));
}

#endif
