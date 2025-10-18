// ReSharper disable once CppInconsistentNaming
#define _CRT_SECURE_NO_WARNINGS

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <d3d11_4.h>
#include <dxgi1_6.h>
#include <Windows.h>
#include <wrl/client.h>

#include "shader_collection.hpp"
#include "winapi_helpers.hpp"
#include "window.hpp"
#include "shaders/shader_interop.h"

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

  ComPtr<IDXGIOutput> output;
  ThrowIfFailed(hp_adapter->EnumOutputs(0, &output));

  ComPtr<IDXGIOutput6> output6;
  ThrowIfFailed(output.As(&output6));

  DXGI_OUTPUT_DESC1 output_desc;
  ThrowIfFailed(output6->GetDesc1(&output_desc));

  auto const output_width{
    static_cast<unsigned>(output_desc.DesktopCoordinates.right - output_desc.DesktopCoordinates.left)
  };
  auto const output_height{
    static_cast<unsigned>(output_desc.DesktopCoordinates.bottom - output_desc.DesktopCoordinates.top)
  };

  SetWindowLongW(hwnd.get(), GWL_STYLE, WS_POPUP);
  SetWindowPos(hwnd.get(), nullptr, output_desc.DesktopCoordinates.left, output_desc.DesktopCoordinates.top,
               static_cast<LONG>(output_width), static_cast<LONG>(output_height), SWP_FRAMECHANGED);

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

  auto const shaders{refl::LoadShaders(*dev.Get())};

  if (!shaders) {
    return -1;
  }

  auto tearing_supported{FALSE};
  ThrowIfFailed(dxgi_factory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &tearing_supported,
                                                  sizeof tearing_supported));

  UINT swap_chain_flags{0};
  UINT present_flags{0};

  if (tearing_supported) {
    swap_chain_flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
    present_flags |= DXGI_PRESENT_ALLOW_TEARING;
  }

  DXGI_SWAP_CHAIN_DESC1 const swap_chain_desc{
    .Width = output_width, .Height = output_height,
    .Format = DXGI_FORMAT_R8G8B8A8_UNORM, .Stereo = FALSE, .SampleDesc{.Count = 1, .Quality = 0},
    .BufferUsage = 0, .BufferCount = 2, .Scaling = DXGI_SCALING_STRETCH,
    .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD, .AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED, .Flags = swap_chain_flags
  };

  ComPtr<IDXGISwapChain1> tmp_swap_chain;
  ThrowIfFailed(dxgi_factory->CreateSwapChainForHwnd(dev.Get(), hwnd.get(), &swap_chain_desc, nullptr, nullptr,
                                                     &tmp_swap_chain));

  ComPtr<IDXGISwapChain2> swap_chain;
  ThrowIfFailed(tmp_swap_chain.As(&swap_chain));

  ComPtr<ID3D11Texture2D> swap_chain_tex;
  ThrowIfFailed(swap_chain->GetBuffer(0, IID_PPV_ARGS(&swap_chain_tex)));

  D3D11_TEXTURE2D_DESC const hdr_tex_desc{
    .Width = output_width,
    .Height = output_height,
    .MipLevels = 1,
    .ArraySize = 1,
    .Format = DXGI_FORMAT_R32G32B32A32_FLOAT,
    .SampleDesc = {.Count = 1, .Quality = 0},
    .Usage = D3D11_USAGE_DEFAULT,
    .BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,
    .CPUAccessFlags = 0,
    .MiscFlags = 0,
  };

  ComPtr<ID3D11Texture2D> hdr_tex;
  ThrowIfFailed(dev->CreateTexture2D(&hdr_tex_desc, nullptr, &hdr_tex));

  D3D11_RENDER_TARGET_VIEW_DESC hdr_rtv_desc{
    .Format = hdr_tex_desc.Format,
    .ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D,
    .Texture2D = {.MipSlice = 0}
  };

  ComPtr<ID3D11RenderTargetView> hdr_rtv;
  ThrowIfFailed(dev->CreateRenderTargetView(hdr_tex.Get(), &hdr_rtv_desc, &hdr_rtv));

  D3D11_SHADER_RESOURCE_VIEW_DESC const hdr_srv_desc{
    .Format = hdr_tex_desc.Format,
    .ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D,
    .Texture2D = {.MostDetailedMip = 0, .MipLevels = 1}
  };

  ComPtr<ID3D11ShaderResourceView> hdr_srv;
  ThrowIfFailed(dev->CreateShaderResourceView(hdr_tex.Get(), &hdr_srv_desc, &hdr_srv));

  D3D11_TEXTURE2D_DESC const sdr_tex_desc{
    .Width = output_width,
    .Height = output_height,
    .MipLevels = 1,
    .ArraySize = 1,
    .Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
    .SampleDesc = {.Count = 1, .Quality = 0},
    .Usage = D3D11_USAGE_DEFAULT,
    .BindFlags = D3D11_BIND_RENDER_TARGET,
    .CPUAccessFlags = 0,
    .MiscFlags = 0,
  };

  ComPtr<ID3D11Texture2D> sdr_tex;
  ThrowIfFailed(dev->CreateTexture2D(&sdr_tex_desc, nullptr, &sdr_tex));

  D3D11_RENDER_TARGET_VIEW_DESC const sdr_rtv_desc{
    .Format = sdr_tex_desc.Format,
    .ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D,
    .Texture2D = {.MipSlice = 0}
  };

  ComPtr<ID3D11RenderTargetView> sdr_rtv;
  ThrowIfFailed(dev->CreateRenderTargetView(sdr_tex.Get(), &sdr_rtv_desc, &sdr_rtv));

  D3D11_TEXTURE2D_DESC const gbuffer0_tex_desc{
    .Width = output_width,
    .Height = output_height,
    .MipLevels = 1,
    .ArraySize = 1,
    .Format = DXGI_FORMAT_R32G32B32A32_FLOAT,
    .SampleDesc = {.Count = 1, .Quality = 0},
    .Usage = D3D11_USAGE_DEFAULT,
    .BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,
    .CPUAccessFlags = 0,
    .MiscFlags = 0
  };

  ComPtr<ID3D11Texture2D> gbuffer0_tex;
  ThrowIfFailed(dev->CreateTexture2D(&gbuffer0_tex_desc, nullptr, &gbuffer0_tex));

  D3D11_RENDER_TARGET_VIEW_DESC const gbuffer0_rtv_desc{
    .Format = gbuffer0_tex_desc.Format,
    .ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D,
    .Texture2D = {.MipSlice = 0}
  };

  ComPtr<ID3D11RenderTargetView> gbuffer0_rtv;
  ThrowIfFailed(dev->CreateRenderTargetView(gbuffer0_tex.Get(), &gbuffer0_rtv_desc, &gbuffer0_rtv));

  D3D11_SHADER_RESOURCE_VIEW_DESC const gbuffer0_srv_desc{
    .Format = gbuffer0_tex_desc.Format,
    .ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D,
    .Texture2D = {.MostDetailedMip = 0, .MipLevels = 1}
  };

  ComPtr<ID3D11ShaderResourceView> gbuffer0_srv;
  ThrowIfFailed(dev->CreateShaderResourceView(gbuffer0_tex.Get(), &gbuffer0_srv_desc, &gbuffer0_srv));

  D3D11_TEXTURE2D_DESC const gbuffer1_tex_desc{
    .Width = output_width,
    .Height = output_height,
    .MipLevels = 1,
    .ArraySize = 1,
    .Format = DXGI_FORMAT_R32G32B32A32_FLOAT,
    .SampleDesc = {.Count = 1, .Quality = 0},
    .Usage = D3D11_USAGE_DEFAULT,
    .BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,
    .CPUAccessFlags = 0,
    .MiscFlags = 0
  };

  ComPtr<ID3D11Texture2D> gbuffer1_tex;
  ThrowIfFailed(dev->CreateTexture2D(&gbuffer1_tex_desc, nullptr, &gbuffer1_tex));

  D3D11_RENDER_TARGET_VIEW_DESC const gbuffer1_rtv_desc{
    .Format = gbuffer1_tex_desc.Format,
    .ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D,
    .Texture2D = {.MipSlice = 0}
  };

  ComPtr<ID3D11RenderTargetView> gbuffer1_rtv;
  ThrowIfFailed(dev->CreateRenderTargetView(gbuffer1_tex.Get(), &gbuffer1_rtv_desc, &gbuffer1_rtv));

  D3D11_SHADER_RESOURCE_VIEW_DESC const gbuffer1_srv_desc{
    .Format = gbuffer1_tex_desc.Format,
    .ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D,
    .Texture2D = {.MostDetailedMip = 0, .MipLevels = 1}
  };

  ComPtr<ID3D11ShaderResourceView> gbuffer1_srv;
  ThrowIfFailed(dev->CreateShaderResourceView(gbuffer1_tex.Get(), &gbuffer1_srv_desc, &gbuffer1_srv));

  D3D11_TEXTURE2D_DESC const depth_tex_desc{
    .Width = output_width,
    .Height = output_height,
    .MipLevels = 1,
    .ArraySize = 1,
    .Format = DXGI_FORMAT_D32_FLOAT,
    .SampleDesc = {.Count = 1, .Quality = 0},
    .Usage = D3D11_USAGE_DEFAULT,
    .BindFlags = D3D11_BIND_DEPTH_STENCIL,
    .CPUAccessFlags = 0,
    .MiscFlags = 0
  };

  ComPtr<ID3D11Texture2D> depth_tex;
  ThrowIfFailed(dev->CreateTexture2D(&depth_tex_desc, nullptr, &depth_tex));

  D3D11_DEPTH_STENCIL_VIEW_DESC const depth_tex_dsv_desc{
    .Format = depth_tex_desc.Format,
    .ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D,
    .Flags = 0,
    .Texture2D = {.MipSlice = 0}
  };

  ComPtr<ID3D11DepthStencilView> depth_dsv;
  ThrowIfFailed(dev->CreateDepthStencilView(depth_tex.Get(), &depth_tex_dsv_desc, &depth_dsv));

  D3D11_SAMPLER_DESC constexpr sampler_point_clamp_desc{
    .Filter = D3D11_FILTER_MIN_MAG_MIP_POINT,
    .AddressU = D3D11_TEXTURE_ADDRESS_CLAMP,
    .AddressV = D3D11_TEXTURE_ADDRESS_CLAMP,
    .AddressW = D3D11_TEXTURE_ADDRESS_CLAMP,
    .MipLODBias = 0,
    .MaxAnisotropy = 1,
    .ComparisonFunc = D3D11_COMPARISON_ALWAYS,
    .BorderColor = {0, 0, 0, 1},
    .MinLOD = 0,
    .MaxLOD = D3D11_FLOAT32_MAX
  };

  ComPtr<ID3D11SamplerState> sampler_point_clamp;
  ThrowIfFailed(dev->CreateSamplerState(&sampler_point_clamp_desc, &sampler_point_clamp));

  ShowWindow(hwnd.get(), SW_SHOW);

  D3D11_VIEWPORT const viewport{
    .TopLeftX = 0.0F, .TopLeftY = 0.0F,
    .Width = static_cast<FLOAT>(output_width),
    .Height = static_cast<FLOAT>(output_height),
    .MinDepth = 0.0F, .MaxDepth = 1.0F
  };

  std::array constexpr black_color{0.0F, 0.0F, 0.0F, 1.0F};

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

    ctx->ClearRenderTargetView(hdr_rtv.Get(), black_color.data());
    ctx->OMSetRenderTargets(1, hdr_rtv.GetAddressOf(), nullptr);

    ctx->VSSetShader(shaders->lighting_vs.Get(), nullptr, 0);
    ctx->PSSetShader(shaders->lighting_ps.Get(), nullptr, 0);

    ctx->RSSetViewports(1, &viewport);

    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->Draw(3, 0);

    ctx->OMSetRenderTargets(1, sdr_rtv.GetAddressOf(), nullptr);

    ctx->VSSetShader(shaders->tonemapping_vs.Get(), nullptr, 0);
    ctx->PSSetShader(shaders->tonemapping_ps.Get(), nullptr, 0);

    ctx->PSSetShaderResources(TONEMAPPING_HDR_TEX_SRV_SLOT, 1, hdr_srv.GetAddressOf());
    ctx->PSSetSamplers(TONEMAPPING_SAMPLER_SLOT, 1, sampler_point_clamp.GetAddressOf());

    ctx->Draw(3, 0);

    ctx->CopyResource(swap_chain_tex.Get(), sdr_tex.Get());

    ThrowIfFailed(swap_chain->Present(0, present_flags));
  }

  return ret;
}
