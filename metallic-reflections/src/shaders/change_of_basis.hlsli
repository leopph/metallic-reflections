// ReSharper disable CppEnforceCVQualifiersPlacement

#ifndef CHANGE_OF_BASIS_HLSLI
#define CHANGE_OF_BASIS_HLSLI

float2 UvToNdc(const float2 uv) {
  return uv * float2(2, -2) + float2(-1, 1);
}

#endif
