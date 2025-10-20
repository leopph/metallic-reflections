// ReSharper disable CppEnforceCVQualifiersPlacement

#ifndef FULLSCREEN_TRI_HLSLI
#define FULLSCREEN_TRI_HLSLI

#include "change_of_basis.hlsli"

void MakeFullscreenTriangleVertex(const uint vertex_id, out float4 pos_os, out float2 uv) {
  uv = float2((vertex_id << 1) & 2, vertex_id & 2);
  pos_os = float4(UvToNdc(uv), 1, 1);
}

#endif
