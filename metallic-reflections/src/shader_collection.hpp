#pragma once

#include <optional>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <d3d11_4.h>
#include <wrl/client.h>


namespace refl {
struct ShaderCollection {
  Microsoft::WRL::ComPtr<ID3D11VertexShader> lighting_vs;
  Microsoft::WRL::ComPtr<ID3D11PixelShader> lighting_ps;
  Microsoft::WRL::ComPtr<ID3D11VertexShader> tonemapping_vs;
  Microsoft::WRL::ComPtr<ID3D11PixelShader> tonemapping_ps;
};


[[nodiscard]] auto LoadShaders(ID3D11Device5& dev) -> std::optional<ShaderCollection>;
}
