// ReSharper disable once CppInconsistentNaming
#define _CRT_SECURE_NO_WARNINGS

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <d3d11_4.h>
#include <dxgi1_6.h>
#include <stb_image.h>
#include <Windows.h>
#include <wrl/client.h>

#include "OrbitingCamera.hpp"
#include "scene.hpp"
#include "shader_collection.hpp"
#include "winapi_helpers.hpp"
#include "window.hpp"
#include "shaders/shader_interop.h"

import std;

auto wmain(int const argc, wchar_t** const argv) -> int {
  if (argc <= 2) {
    std::cerr << "Usage: metallic-reflections <path-to-model-file> <path-to-environment-map>\n";
    return -1;
  }

  auto wnd{refl::Window::New()};

  if (!wnd) {
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

  SetWindowLongW(wnd->GetHwnd(), GWL_STYLE, WS_POPUP);
  SetWindowPos(wnd->GetHwnd(), nullptr, output_desc.DesktopCoordinates.left, output_desc.DesktopCoordinates.top,
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
  ThrowIfFailed(dxgi_factory->CreateSwapChainForHwnd(dev.Get(), wnd->GetHwnd(), &swap_chain_desc, nullptr, nullptr,
                                                     &tmp_swap_chain));

  ComPtr<IDXGISwapChain2> swap_chain;
  ThrowIfFailed(tmp_swap_chain.As(&swap_chain));

  ComPtr<ID3D11Texture2D> swap_chain_tex;
  ThrowIfFailed(swap_chain->GetBuffer(0, IID_PPV_ARGS(&swap_chain_tex)));

  D3D11_TEXTURE2D_DESC const ibl_tex_desc{
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

  ComPtr<ID3D11Texture2D> ibl_tex;
  ThrowIfFailed(dev->CreateTexture2D(&ibl_tex_desc, nullptr, &ibl_tex));

  D3D11_RENDER_TARGET_VIEW_DESC const ibl_rtv_desc{
    .Format = ibl_tex_desc.Format,
    .ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D,
    .Texture2D = {.MipSlice = 0}
  };

  ComPtr<ID3D11RenderTargetView> ibl_rtv;
  ThrowIfFailed(dev->CreateRenderTargetView(ibl_tex.Get(), &ibl_rtv_desc, &ibl_rtv));

  D3D11_SHADER_RESOURCE_VIEW_DESC const ibl_srv_desc{
    .Format = ibl_tex_desc.Format,
    .ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D,
    .Texture2D = {.MostDetailedMip = 0, .MipLevels = 1}
  };

  ComPtr<ID3D11ShaderResourceView> ibl_srv;
  ThrowIfFailed(dev->CreateShaderResourceView(ibl_tex.Get(), &ibl_srv_desc, &ibl_srv));

  D3D11_TEXTURE2D_DESC const ssr_tex_desc{
    .Width = output_width,
    .Height = output_height,
    .MipLevels = 1,
    .ArraySize = 1,
    .Format = DXGI_FORMAT_R32G32B32A32_FLOAT,
    .SampleDesc = {.Count = 1, .Quality = 0},
    .Usage = D3D11_USAGE_DEFAULT,
    .BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS,
    .CPUAccessFlags = 0,
    .MiscFlags = 0,
  };

  ComPtr<ID3D11Texture2D> ssr_tex;
  ThrowIfFailed(dev->CreateTexture2D(&ssr_tex_desc, nullptr, &ssr_tex));

  D3D11_UNORDERED_ACCESS_VIEW_DESC const ssr_uav_tex{
    .Format = ssr_tex_desc.Format,
    .ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D,
    .Texture2D = {.MipSlice = 0}
  };

  ComPtr<ID3D11UnorderedAccessView> ssr_uav;
  ThrowIfFailed(dev->CreateUnorderedAccessView(ssr_tex.Get(), &ssr_uav_tex, &ssr_uav));

  D3D11_SHADER_RESOURCE_VIEW_DESC const ssr_srv_desc{
    .Format = ssr_tex_desc.Format,
    .ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D,
    .Texture2D = {.MostDetailedMip = 0, .MipLevels = 1}
  };

  ComPtr<ID3D11ShaderResourceView> ssr_srv;
  ThrowIfFailed(dev->CreateShaderResourceView(ssr_tex.Get(), &ssr_srv_desc, &ssr_srv));

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
    .Format = DXGI_FORMAT_R32_TYPELESS,
    .SampleDesc = {.Count = 1, .Quality = 0},
    .Usage = D3D11_USAGE_DEFAULT,
    .BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE,
    .CPUAccessFlags = 0,
    .MiscFlags = 0
  };

  ComPtr<ID3D11Texture2D> depth_tex;
  ThrowIfFailed(dev->CreateTexture2D(&depth_tex_desc, nullptr, &depth_tex));

  D3D11_DEPTH_STENCIL_VIEW_DESC constexpr depth_tex_dsv_desc{
    .Format = DXGI_FORMAT_D32_FLOAT,
    .ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D,
    .Flags = 0,
    .Texture2D = {.MipSlice = 0}
  };

  ComPtr<ID3D11DepthStencilView> depth_dsv;
  ThrowIfFailed(dev->CreateDepthStencilView(depth_tex.Get(), &depth_tex_dsv_desc, &depth_dsv));

  D3D11_SHADER_RESOURCE_VIEW_DESC constexpr depth_srv_desc{
    .Format = DXGI_FORMAT_R32_FLOAT,
    .ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D,
    .Texture2D = {.MostDetailedMip = 0, .MipLevels = 1}
  };

  ComPtr<ID3D11ShaderResourceView> depth_srv;
  ThrowIfFailed(dev->CreateShaderResourceView(depth_tex.Get(), &depth_srv_desc, &depth_srv));

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

  D3D11_SAMPLER_DESC constexpr sampler_trilinear_clamp_desc{
    .Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR,
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

  ComPtr<ID3D11SamplerState> sampler_trilinear_clamp;
  ThrowIfFailed(dev->CreateSamplerState(&sampler_trilinear_clamp_desc, &sampler_trilinear_clamp));

  D3D11_BUFFER_DESC constexpr cam_cbuf_desc{
    .ByteWidth = sizeof(CameraConstants),
    .Usage = D3D11_USAGE_DYNAMIC,
    .BindFlags = D3D11_BIND_CONSTANT_BUFFER,
    .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
    .MiscFlags = 0,
    .StructureByteStride = 0
  };

  ComPtr<ID3D11Buffer> cam_cbuf;
  ThrowIfFailed(dev->CreateBuffer(&cam_cbuf_desc, nullptr, &cam_cbuf));

  ShowWindow(wnd->GetHwnd(), SW_SHOW);

  // Load scene from disk

  auto const cpu_scene{refl::LoadCpuScene(argv[1])};

  if (!cpu_scene) {
    return -1;
  }

  // Create scene gpu data

  auto const gpu_scene{refl::CreateGpuScene(*cpu_scene, *dev.Get())};

  if (!gpu_scene) {
    return -1;
  }

  // Load environment map from disk

  struct Image {
    int width;
    int height;
    int channel_count;
    float* data;
  };

  Image env_map_info;
  env_map_info.data = stbi_loadf(reinterpret_cast<char const*>(std::filesystem::path{argv[2]}.u8string().data()),
                                 &env_map_info.width, &env_map_info.height, &env_map_info.channel_count, 4);

  if (!env_map_info.data) {
    std::cerr << "Failed to load environment map image.\n";
    return -1;
  }

  // Create 2D texture and SRV for equirectangular environment map

  D3D11_TEXTURE2D_DESC const equi_env_map_tex_desc{
    .Width = static_cast<UINT>(env_map_info.width),
    .Height = static_cast<UINT>(env_map_info.height),
    .MipLevels = 1,
    .ArraySize = 1,
    .Format = DXGI_FORMAT_R32G32B32A32_FLOAT,
    .SampleDesc = {.Count = 1, .Quality = 0},
    .Usage = D3D11_USAGE_IMMUTABLE,
    .BindFlags = D3D11_BIND_SHADER_RESOURCE,
    .CPUAccessFlags = 0,
    .MiscFlags = 0
  };

  D3D11_SUBRESOURCE_DATA const equi_env_map_tex_data{
    .pSysMem = env_map_info.data,
    .SysMemPitch = static_cast<UINT>(env_map_info.width * 4 * sizeof(float)),
    .SysMemSlicePitch = 0
  };

  ComPtr<ID3D11Texture2D> equi_env_map_tex;
  ThrowIfFailed(dev->CreateTexture2D(&equi_env_map_tex_desc, &equi_env_map_tex_data, &equi_env_map_tex));

  stbi_image_free(env_map_info.data);
  env_map_info.data = nullptr;

  D3D11_SHADER_RESOURCE_VIEW_DESC const equi_env_map_srv_desc{
    .Format = equi_env_map_tex_desc.Format,
    .ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D,
    .Texture2D = {.MostDetailedMip = 0, .MipLevels = 1}
  };

  ComPtr<ID3D11ShaderResourceView> equi_env_map_srv;
  ThrowIfFailed(dev->CreateShaderResourceView(equi_env_map_tex.Get(), &equi_env_map_srv_desc, &equi_env_map_srv));

  // Create cubemap texture for environment map

  auto constexpr env_cube_size{1024u};
  auto const env_cube_mip_count{
    static_cast<UINT>(std::floor(std::log2(static_cast<float>(env_cube_size)))) + 1U
  };

  D3D11_TEXTURE2D_DESC const env_cube_tex_desc{
    .Width = env_cube_size,
    .Height = env_cube_size,
    .MipLevels = env_cube_mip_count,
    .ArraySize = 6,
    .Format = DXGI_FORMAT_R32G32B32A32_FLOAT,
    .SampleDesc = {.Count = 1, .Quality = 0},
    .Usage = D3D11_USAGE_DEFAULT,
    .BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET,
    .CPUAccessFlags = 0,
    .MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE | D3D11_RESOURCE_MISC_GENERATE_MIPS
  };

  ComPtr<ID3D11Texture2D> env_cube_tex;
  ThrowIfFailed(dev->CreateTexture2D(&env_cube_tex_desc, nullptr, &env_cube_tex));

  // Render equirectangular environment map to cubemap mip0

  D3D11_UNORDERED_ACCESS_VIEW_DESC const env_cube_mip0_uav_desc{
    .Format = env_cube_tex_desc.Format,
    .ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY,
    .Texture2DArray = {.MipSlice = 0, .FirstArraySlice = 0, .ArraySize = 6}
  };

  ComPtr<ID3D11UnorderedAccessView> env_cube_mip0_uav;
  ThrowIfFailed(dev->CreateUnorderedAccessView(env_cube_tex.Get(), &env_cube_mip0_uav_desc, &env_cube_mip0_uav));

  ctx->CSSetShader(shaders->equirect_to_cubemap_cs.Get(), nullptr, 0);
  ctx->CSSetUnorderedAccessViews(ENV_CUBE_UAV_SLOT, 1, env_cube_mip0_uav.GetAddressOf(), nullptr);
  ctx->CSSetShaderResources(EQUIRECT_ENV_MAP_SRV_SLOT, 1, equi_env_map_srv.GetAddressOf());
  ctx->CSSetSamplers(EQUIRECT_ENV_MAP_SAMPLER_SLOT, 1, sampler_trilinear_clamp.GetAddressOf());

  auto constexpr equirect_to_cube_cs_group_size_x{
    (env_cube_size + EQUIRECT_TO_CUBE_THREADS_X - 1) / EQUIRECT_TO_CUBE_THREADS_X
  };
  auto constexpr equirect_to_cube_cs_group_size_y{
    (env_cube_size + EQUIRECT_TO_CUBE_THREADS_Y - 1) / EQUIRECT_TO_CUBE_THREADS_Y
  };
  ctx->Dispatch(equirect_to_cube_cs_group_size_x, equirect_to_cube_cs_group_size_y, 6);

  ComPtr<ID3D11UnorderedAccessView> const null_uav{nullptr};
  ctx->CSSetUnorderedAccessViews(ENV_CUBE_UAV_SLOT, 1, null_uav.GetAddressOf(), nullptr);

  // Generate mips for environment cubemap

  D3D11_SHADER_RESOURCE_VIEW_DESC const env_cube_srv_desc{
    .Format = env_cube_tex_desc.Format,
    .ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE,
    .TextureCube = {.MostDetailedMip = 0, .MipLevels = env_cube_mip_count}
  };

  ComPtr<ID3D11ShaderResourceView> env_cube_srv;
  ThrowIfFailed(dev->CreateShaderResourceView(env_cube_tex.Get(), &env_cube_srv_desc, &env_cube_srv));

  ctx->GenerateMips(env_cube_srv.Get());

  // Create cubemap for prefiltered environment map

  D3D11_TEXTURE2D_DESC const prefiltered_env_cube_tex_desc{
    .Width = env_cube_size,
    .Height = env_cube_size,
    .MipLevels = env_cube_mip_count,
    .ArraySize = 6,
    .Format = DXGI_FORMAT_R32G32B32A32_FLOAT,
    .SampleDesc = {.Count = 1, .Quality = 0},
    .Usage = D3D11_USAGE_DEFAULT,
    .BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE,
    .CPUAccessFlags = 0,
    .MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE
  };

  ComPtr<ID3D11Texture2D> prefiltered_env_cube_tex;
  ThrowIfFailed(dev->CreateTexture2D(&prefiltered_env_cube_tex_desc, nullptr, &prefiltered_env_cube_tex));

  // Copy env_cube mip0 to prefiltered_env_cube mip0

  for (unsigned face{0}; face < 6; face++) {
    auto const subresource{D3D11CalcSubresource(0, face, env_cube_mip_count)};
    ctx->CopySubresourceRegion(prefiltered_env_cube_tex.Get(), subresource, 0, 0, 0, env_cube_tex.Get(), subresource,
                               nullptr);
  }

  // Prefilter environment cubemap into mip1...N

  D3D11_BUFFER_DESC constexpr env_prefilter_cbv_desc{
    .ByteWidth = sizeof(EnvPrefilterConstants),
    .Usage = D3D11_USAGE_DYNAMIC,
    .BindFlags = D3D11_BIND_CONSTANT_BUFFER,
    .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
    .MiscFlags = 0,
    .StructureByteStride = 0
  };

  ComPtr<ID3D11Buffer> env_prefilter_cbv;
  ThrowIfFailed(dev->CreateBuffer(&env_prefilter_cbv_desc, nullptr, &env_prefilter_cbv));

  for (auto mip{1u}; mip < env_cube_mip_count; ++mip) {
    D3D11_UNORDERED_ACCESS_VIEW_DESC const env_cube_mip_uav_desc{
      .Format = prefiltered_env_cube_tex_desc.Format,
      .ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY,
      .Texture2DArray = {.MipSlice = mip, .FirstArraySlice = 0, .ArraySize = 6}
    };

    ComPtr<ID3D11UnorderedAccessView> prefiltered_env_cube_mip_uav;
    ThrowIfFailed(dev->CreateUnorderedAccessView(prefiltered_env_cube_tex.Get(), &env_cube_mip_uav_desc,
                                                 &prefiltered_env_cube_mip_uav));

    D3D11_MAPPED_SUBRESOURCE mapped_prefilter_cbv;
    ThrowIfFailed(ctx->Map(env_prefilter_cbv.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_prefilter_cbv));

    *static_cast<EnvPrefilterConstants*>(mapped_prefilter_cbv.pData) = {
      .cur_mip = mip,
      .num_mips = env_cube_mip_count,
      .face_base_size = env_cube_size,
      .sample_count = 1024
    };

    ctx->Unmap(env_prefilter_cbv.Get(), 0);

    ctx->CSSetShader(shaders->env_prefilter_cs.Get(), nullptr, 0);
    ctx->CSSetUnorderedAccessViews(ENV_PREFILTER_ENV_CUBE_UAV_SLOT, 1, prefiltered_env_cube_mip_uav.GetAddressOf(),
                                   nullptr);
    ctx->CSSetShaderResources(ENV_PREFILTER_CUBE_SRV_SLOT, 1, env_cube_srv.GetAddressOf());
    ctx->CSSetConstantBuffers(ENV_PREFILTER_CB_SLOT, 1, env_prefilter_cbv.GetAddressOf());
    ctx->CSSetSamplers(ENV_PREFILTER_SAMPLER_SLOT, 1, sampler_trilinear_clamp.GetAddressOf());

    auto const mip_size{std::max(env_cube_size >> mip, 1u)};
    auto const group_count_x{std::ceil(mip_size / static_cast<float>(ENV_PREFILTER_THREADS_X))};
    auto const group_count_y{std::ceil(mip_size / static_cast<float>(ENV_PREFILTER_THREADS_Y))};
    ctx->Dispatch(static_cast<UINT>(group_count_x), static_cast<UINT>(group_count_y), 6);
  }
  ctx->CSSetUnorderedAccessViews(ENV_PREFILTER_ENV_CUBE_UAV_SLOT, 1, null_uav.GetAddressOf(), nullptr);

  // Create srv for all mips of the prefiltered environment cubemap

  D3D11_SHADER_RESOURCE_VIEW_DESC const prefiltered_env_cube_srv_desc{
    .Format = prefiltered_env_cube_tex_desc.Format,
    .ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE,
    .TextureCube = {.MostDetailedMip = 0, .MipLevels = env_cube_mip_count}
  };

  ComPtr<ID3D11ShaderResourceView> prefiltered_env_cube_srv;
  ThrowIfFailed(dev->CreateShaderResourceView(prefiltered_env_cube_tex.Get(), &prefiltered_env_cube_srv_desc,
                                              &prefiltered_env_cube_srv));

  D3D11_VIEWPORT const viewport{
    .TopLeftX = 0.0F, .TopLeftY = 0.0F,
    .Width = static_cast<FLOAT>(output_width),
    .Height = static_cast<FLOAT>(output_height),
    .MinDepth = 0.0F, .MaxDepth = 1.0F
  };

  std::array constexpr black_color{0.0F, 0.0F, 0.0F, 1.0F};

  constexpr auto cam_near{0.1F};
  constexpr auto cam_far{5.F};
  refl::OrbitingCamera cam{{0, 0, 0}, 2.5F, cam_near, cam_far, 90.0F};

  constexpr auto cam_zoom_speed{1.0f};
  //cam.Rotate(20);

  int ret;

  auto begin{std::chrono::steady_clock::now()};
  auto end{begin};

  while (true) {
    auto const delta_time{std::chrono::duration_cast<std::chrono::duration<float>>(end - begin).count()};

    MSG msg;

    if (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
      if (msg.message == WM_QUIT) {
        ret = static_cast<int>(msg.wParam);
        break;
      }

      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }

    // W
    if (wnd->IsKeyPressed(0x57)) {
      cam.Zoom(-cam_zoom_speed * delta_time);
    }

    // S
    if (wnd->IsKeyPressed(0x53)) {
      cam.Zoom(cam_zoom_speed * delta_time);
    }

    cam.Rotate(15 * delta_time);

    auto const view_mtx{cam.ComputeViewMatrix()};
    auto const proj_mtx{cam.ComputeProjMatrix(static_cast<float>(output_width) / static_cast<float>(output_height))};

    auto const xm_view_mtx{DirectX::XMLoadFloat4x4(&view_mtx)};
    auto const xm_view_inv_mtx{DirectX::XMMatrixInverse(nullptr, xm_view_mtx)};
    auto const xm_proj_mtx{DirectX::XMLoadFloat4x4(&proj_mtx)};
    auto const xm_proj_inv_mtx{DirectX::XMMatrixInverse(nullptr, xm_proj_mtx)};
    auto const xm_view_proj_mtx{DirectX::XMMatrixMultiply(xm_view_mtx, xm_proj_mtx)};
    auto const xm_view_proj_inv_mtx{DirectX::XMMatrixInverse(nullptr, xm_view_proj_mtx)};

    DirectX::XMFLOAT4X4 view_inv_mtx;
    DirectX::XMStoreFloat4x4(&view_inv_mtx, xm_view_inv_mtx);
    DirectX::XMFLOAT4X4 proj_inv_mtx;
    DirectX::XMStoreFloat4x4(&proj_inv_mtx, xm_proj_inv_mtx);
    DirectX::XMFLOAT4X4 view_proj_mtx;
    DirectX::XMStoreFloat4x4(&view_proj_mtx, xm_view_proj_mtx);
    DirectX::XMFLOAT4X4 view_proj_inv_mtx;
    DirectX::XMStoreFloat4x4(&view_proj_inv_mtx, xm_view_proj_inv_mtx);

    D3D11_MAPPED_SUBRESOURCE mapped_cam_cbuf;
    ThrowIfFailed(ctx->Map(cam_cbuf.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_cam_cbuf));

    *static_cast<CameraConstants*>(mapped_cam_cbuf.pData) = {
      .view_mtx = view_mtx,
      .view_inv_mtx = view_inv_mtx,
      .proj_mtx = proj_mtx,
      .proj_inv_mtx = proj_inv_mtx,
      .view_proj_mtx = view_proj_mtx,
      .view_proj_inv_mtx = view_proj_inv_mtx,
      .pos_ws = cam.ComputePosition(),
      .near_clip = cam_near,
      .far_clip = cam_far,
      .pad = {}
    };

    ctx->Unmap(cam_cbuf.Get(), 0);

    // GBuffer pass

    std::array const gbuffer_rtvs{gbuffer0_rtv.Get(), gbuffer1_rtv.Get()};
    ctx->OMSetRenderTargets(static_cast<UINT>(gbuffer_rtvs.size()), gbuffer_rtvs.data(), depth_dsv.Get());

    for (auto const rtv : gbuffer_rtvs) {
      ctx->ClearRenderTargetView(rtv, black_color.data());
    }

    ctx->ClearDepthStencilView(depth_dsv.Get(), D3D11_CLEAR_DEPTH, 1.0F, 0);

    ctx->VSSetShader(shaders->gbuffer_vs.Get(), nullptr, 0);
    ctx->PSSetShader(shaders->gbuffer_ps.Get(), nullptr, 0);

    ctx->RSSetViewports(1, &viewport);

    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->IASetInputLayout(shaders->mesh_il.Get());

    ctx->VSSetConstantBuffers(CAMERA_CB_SLOT, 1, cam_cbuf.GetAddressOf());
    ctx->PSSetSamplers(MATERIAL_SAMPLER_SLOT, 1, sampler_trilinear_clamp.GetAddressOf());

    for (auto const& mesh : gpu_scene->meshes) {
      std::array const vertex_buffers{
        mesh.pos_buf.Get(), mesh.norm_buf.Get(), mesh.uv_buf.Get(), mesh.tan_buf.Get()
      };
      std::array constexpr strides{
        16u, 16u, 8u, 16u
      };
      std::array constexpr offsets{
        0u, 0u, 0u, 0u
      };
      ctx->IASetVertexBuffers(0, 4, vertex_buffers.data(), strides.data(), offsets.data());
      ctx->IASetIndexBuffer(mesh.idx_buf.Get(), DXGI_FORMAT_R32_UINT, 0);
      ctx->VSSetConstantBuffers(OBJECT_CB_SLOT, 1, mesh.transform_buf.GetAddressOf());
      ctx->PSSetConstantBuffers(MATERIAL_CB_SLOT, 1, mesh.mtl_buf.GetAddressOf());
      ctx->DrawIndexed(mesh.idx_count, 0, 0);
    }

    // Lighting pass

    ctx->ClearRenderTargetView(ibl_rtv.Get(), black_color.data());
    ctx->OMSetRenderTargets(1, ibl_rtv.GetAddressOf(), nullptr);

    ctx->VSSetShader(shaders->lighting_vs.Get(), nullptr, 0);
    ctx->PSSetShader(shaders->lighting_ps.Get(), nullptr, 0);

    ctx->PSSetConstantBuffers(LIGHTING_CAM_CB_SLOT, 1, cam_cbuf.GetAddressOf());
    ctx->PSSetShaderResources(LIGHTING_GBUFFER0_SRV_SLOT, 1, gbuffer0_srv.GetAddressOf());
    ctx->PSSetShaderResources(LIGHTING_GBUFFER1_SRV_SLOT, 1, gbuffer1_srv.GetAddressOf());
    ctx->PSSetShaderResources(LIGHTING_DEPTH_SRV_SLOT, 1, depth_srv.GetAddressOf());
    ctx->PSSetShaderResources(LIGHTING_ENV_MAP_SRV_SLOT, 1, prefiltered_env_cube_srv.GetAddressOf());
    ctx->PSSetSamplers(LIGHTING_GBUFFER_SAMPLER_SLOT, 1, sampler_point_clamp.GetAddressOf());
    ctx->PSSetSamplers(LIGHTING_ENV_SAMPLER_SLOT, 1, sampler_trilinear_clamp.GetAddressOf());

    ctx->Draw(3, 0);

    ComPtr<ID3D11RenderTargetView> const null_rtv{nullptr};
    ctx->OMSetRenderTargets(1, null_rtv.GetAddressOf(), nullptr);

    // SSR pass

    ctx->CSSetShader(shaders->ssr_cs.Get(), nullptr, 0);

    ctx->CSSetShaderResources(SSR_DEPTH_SRV_SLOT, 1, depth_srv.GetAddressOf());
    ctx->CSSetShaderResources(SSR_GBUFFER0_SRV_SLOT, 1, gbuffer0_srv.GetAddressOf());
    ctx->CSSetShaderResources(SSR_GBUFFER1_SRV_SLOT, 1, gbuffer1_srv.GetAddressOf());
    ctx->CSSetShaderResources(SSR_IBL_SRV_SLOT, 1, ibl_srv.GetAddressOf());
    ctx->CSSetUnorderedAccessViews(SSR_SSR_UAV_SLOT, 1, ssr_uav.GetAddressOf(), nullptr);
    ctx->CSSetConstantBuffers(SSR_CAM_CB_SLOT, 1, cam_cbuf.GetAddressOf());

    auto const ssr_group_count_x{(output_width + SSR_THREADS_X - 1) / SSR_THREADS_X};
    auto const ssr_group_count_y{(output_height + SSR_THREADS_Y - 1) / SSR_THREADS_Y};
    ctx->Dispatch(ssr_group_count_x, ssr_group_count_y, 1);

    ctx->CSSetUnorderedAccessViews(SSR_SSR_UAV_SLOT, 1, null_uav.GetAddressOf(), nullptr);

    // Tonemapping pass

    ctx->OMSetRenderTargets(1, sdr_rtv.GetAddressOf(), nullptr);

    ctx->VSSetShader(shaders->tonemapping_vs.Get(), nullptr, 0);
    ctx->PSSetShader(shaders->tonemapping_ps.Get(), nullptr, 0);

    ctx->PSSetShaderResources(TONEMAPPING_HDR_TEX_SRV_SLOT, 1, ssr_srv.GetAddressOf());
    ctx->PSSetSamplers(TONEMAPPING_SAMPLER_SLOT, 1, sampler_point_clamp.GetAddressOf());

    ctx->Draw(3, 0);

    // Copy to swapchain

    ctx->CopyResource(swap_chain_tex.Get(), sdr_tex.Get());

    // Present

    ThrowIfFailed(swap_chain->Present(0, present_flags));

    begin = end;
    end = std::chrono::steady_clock::now();
  }

  return ret;
}
