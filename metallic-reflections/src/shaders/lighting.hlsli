// ReSharper disable CppEnforceCVQualifiersPlacement

#include "fullscreen_tri.hlsli"
#include "shader_interop.h"

StructuredBuffer<SphereInfo> g_spheres : register(t0);

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
  return float4(1, 0, 1, 1);
}
