#pragma once

#include <optional>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <d3d11_4.h>
#include <wrl/client.h>


namespace refl {
struct ShaderCollection {
	Microsoft::WRL::ComPtr<ID3D11VertexShader> gbuffer_vs;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> gbuffer_ps;
  Microsoft::WRL::ComPtr<ID3D11VertexShader> lighting_vs;
  Microsoft::WRL::ComPtr<ID3D11PixelShader> lighting_ps;
  Microsoft::WRL::ComPtr<ID3D11VertexShader> tonemapping_vs;
  Microsoft::WRL::ComPtr<ID3D11PixelShader> tonemapping_ps;
	Microsoft::WRL::ComPtr<ID3D11ComputeShader> equirect_to_cubemap_cs;

  Microsoft::WRL::ComPtr<ID3D11InputLayout> mesh_il;
};


[[nodiscard]] auto LoadShaders(ID3D11Device5& dev) -> std::optional<ShaderCollection>;
}
