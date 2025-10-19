#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <d3d11_4.h>
#include <DirectXMath.h>
#include <wrl/client.h>

namespace refl {
using Vector2 = std::array<float, 2>;
using Vector4 = std::array<float, 4>;

struct CpuMeshTransform {
  DirectX::XMFLOAT4X4 world_mtx;
  DirectX::XMFLOAT4X4 normal_mtx;
};

struct CpuMesh {
  std::vector<Vector4> positions;
  std::vector<Vector4> normals;
  std::vector<Vector2> texcoords;
  std::vector<Vector4> tangents;
  std::vector<std::uint32_t> indices;
  CpuMeshTransform transform;
};

struct CpuScene {
  std::vector<CpuMesh> meshes;
};

using GpuMeshTransform = CpuMeshTransform;

struct GpuMesh {
  Microsoft::WRL::ComPtr<ID3D11Buffer> pos_buf; // Vector4s
  Microsoft::WRL::ComPtr<ID3D11Buffer> norm_buf; // Vector4s
  Microsoft::WRL::ComPtr<ID3D11Buffer> uv_buf; // Vector2s
  Microsoft::WRL::ComPtr<ID3D11Buffer> tan_buf; //Vector4s
  Microsoft::WRL::ComPtr<ID3D11Buffer> idx_buf; // u32s
  Microsoft::WRL::ComPtr<ID3D11Buffer> transform_buf; // GpuMeshTransform
};

struct GpuScene {
  std::vector<GpuMesh> meshes;
};


auto LoadCpuScene(std::filesystem::path const& sceneFilePath) -> std::optional<CpuScene>;
auto CreateGpuScene(CpuScene const& cpuScene, ID3D11Device& dev) -> std::optional<GpuScene>;
}
