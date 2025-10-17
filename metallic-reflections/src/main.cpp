// ReSharper disable once CppInconsistentNaming
#define _CRT_SECURE_NO_WARNINGS

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <d3d11_4.h>
#include <dxgi1_6.h>
#include <Windows.h>
#include <wrl/client.h>

#include "winapi_helpers.hpp"
#include "window.hpp"
#include "shaders/shader_interop.h"

#ifndef NDEBUG
#include "shaders/generated/Debug/ps.h"
#include "shaders/generated/Debug/vs.h"
#else
#include "shaders/generated/Release/ps.h"
#include "shaders/generated/Release/vs.h"
#endif

import std;

auto main() -> int {
  auto hwnd{refl::MakeWindow()};

  if (!hwnd) {
    return -1;
  }

  using Microsoft::WRL::ComPtr;
  using refl::ThrowIfFailed;

  UINT dxgi_factory_flags{0};
#ifndef NDEBUG
  dxgi_factory_flags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

  ComPtr<IDXGIFactory7> dxgi_factory;
  ThrowIfFailed(CreateDXGIFactory2(dxgi_factory_flags, IID_PPV_ARGS(&dxgi_factory)));

  ComPtr<IDXGIAdapter4> hp_adapter;
  ThrowIfFailed(
    dxgi_factory->EnumAdapterByGpuPreference(0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&hp_adapter)));

  UINT d3d_device_flags{0};
#ifndef NDEBUG
  d3d_device_flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

  ComPtr<ID3D11Device> tmp_dev;
  ComPtr<ID3D11DeviceContext> ctx;
  ThrowIfFailed(D3D11CreateDevice(hp_adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, d3d_device_flags,
                                  std::array{D3D_FEATURE_LEVEL_12_1}.data(), 1, D3D11_SDK_VERSION, &tmp_dev, nullptr,
                                  &ctx));

  ComPtr<ID3D11Device5> dev;
  ThrowIfFailed(tmp_dev.As(&dev));

#ifndef NDEBUG
  ComPtr<ID3D11Debug> debug;
  ThrowIfFailed(tmp_dev.As<ID3D11Debug>(&debug));
  ComPtr<ID3D11InfoQueue> info_queue;
  ThrowIfFailed(debug.As<ID3D11InfoQueue>(&info_queue));
  ThrowIfFailed(info_queue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, TRUE));
  ThrowIfFailed(info_queue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, TRUE));
#endif

  auto tearing_supported{FALSE};
  ThrowIfFailed(dxgi_factory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &tearing_supported,
                                                  sizeof tearing_supported));

  UINT swap_chain_flags{0};
  UINT present_flags{0};

  if (tearing_supported) {
    swap_chain_flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
    present_flags |= DXGI_PRESENT_ALLOW_TEARING;
  }

  auto constexpr swap_chain_format{DXGI_FORMAT_R8G8B8A8_UNORM};
  auto constexpr swap_chain_buffer_count{2};

  DXGI_SWAP_CHAIN_DESC1 const swap_chain_desc{
    .Width = 0, .Height = 0,
    .Format = swap_chain_format, .Stereo = FALSE, .SampleDesc{.Count = 1, .Quality = 0},
    .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_UNORDERED_ACCESS,
    .BufferCount = swap_chain_buffer_count, .Scaling = DXGI_SCALING_STRETCH,
    .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD, .AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED, .Flags = swap_chain_flags
  };

  ComPtr<IDXGISwapChain1> tmp_swap_chain;
  ThrowIfFailed(dxgi_factory->CreateSwapChainForHwnd(tmp_dev.Get(), hwnd.get(), &swap_chain_desc, nullptr, nullptr,
                                                     &tmp_swap_chain));

  ComPtr<IDXGISwapChain2> swap_chain;
  ThrowIfFailed(tmp_swap_chain.As(&swap_chain));

  ComPtr<ID3D11Texture2D> swap_chain_tex;
  ThrowIfFailed(swap_chain->GetBuffer(0, IID_PPV_ARGS(&swap_chain_tex)));

  D3D11_RENDER_TARGET_VIEW_DESC constexpr swap_chain_rtv_desc{
    .Format = swap_chain_format, .ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D, .Texture2D = {.MipSlice = 0}
  };

  ComPtr<ID3D11VertexShader> vertex_shader;
  ThrowIfFailed(tmp_dev->CreateVertexShader(g_vs_bytes, ARRAYSIZE(g_vs_bytes), nullptr,
                                            &vertex_shader));

  ComPtr<ID3D11PixelShader> pixel_shader;
  ThrowIfFailed(tmp_dev->CreatePixelShader(g_ps_bytes, ARRAYSIZE(g_ps_bytes), nullptr,
                                           &pixel_shader));

  ComPtr<ID3D11RenderTargetView> swap_chain_rtv;
  ThrowIfFailed(tmp_dev->CreateRenderTargetView(swap_chain_tex.Get(), &swap_chain_rtv_desc, &swap_chain_rtv));

  std::array constexpr sphere_infos{
    SphereInfo{
      .geometry = SphereGeometry{.center_ws = {0.0F, 0.0F, 0.0F}, .radius = 1.0F},
      .material = Material{.albedo = {1.0F, 0.0F, 0.0F}, .roughness = 0.01F, .metallic = 0.0F, .emissive = 0.0F}
    },
    SphereInfo{
      .geometry = SphereGeometry{.center_ws = {2.0F, 0.0F, 0.0F}, .radius = 1.0F},
      .material = Material{.albedo = {0.0F, 1.0F, 0.0F}, .roughness = 0.01F, .metallic = 0.0F, .emissive = 0.0F}
    }
  };

  D3D11_BUFFER_DESC constexpr sphere_buf_desc{
    .ByteWidth = static_cast<UINT>(sphere_infos.size() * sizeof(SphereInfo)),
    .Usage = D3D11_USAGE_DYNAMIC,
    .BindFlags = D3D11_BIND_SHADER_RESOURCE,
    .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
    .MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED,
    .StructureByteStride = sizeof(SphereInfo)
  };

  // ReSharper disable once CppVariableCanBeMadeConstexpr
  D3D11_SUBRESOURCE_DATA const sphere_buf_data{
    .pSysMem = sphere_infos.data(),
    .SysMemPitch = 0,
    .SysMemSlicePitch = 0
  };

  ComPtr<ID3D11Buffer> sphere_buf;
  ThrowIfFailed(tmp_dev->CreateBuffer(&sphere_buf_desc, &sphere_buf_data, &sphere_buf));

  ShowWindow(hwnd.get(), SW_SHOW);

  int ret;

  while (true) {
    MSG msg;

    if (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
      if (msg.message == WM_QUIT) {
        ret = static_cast<int>(msg.wParam);
        break;
      }

      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }

    ctx->ClearRenderTargetView(swap_chain_rtv.Get(), std::array{0.0F, 0.0F, 0.0F, 1.0F}.data());
    ctx->OMSetRenderTargets(1, swap_chain_rtv.GetAddressOf(), nullptr);

    ctx->VSSetShader(vertex_shader.Get(), nullptr, 0);
    ctx->PSSetShader(pixel_shader.Get(), nullptr, 0);

    DXGI_SWAP_CHAIN_DESC1 cur_swap_chain_desc;
    ThrowIfFailed(swap_chain->GetDesc1(&cur_swap_chain_desc));

    D3D11_VIEWPORT const viewport{
      .TopLeftX = 0.0F, .TopLeftY = 0.0F,
      .Width = static_cast<FLOAT>(cur_swap_chain_desc.Width),
      .Height = static_cast<FLOAT>(cur_swap_chain_desc.Height),
      .MinDepth = 0.0F, .MaxDepth = 1.0F
    };

    ctx->RSSetViewports(1, &viewport);

    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->Draw(3, 0);

    ThrowIfFailed(swap_chain->Present(0, present_flags));
  }

  return ret;
}
