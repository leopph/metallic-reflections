#include "scene.hpp"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

import std;

namespace refl {
auto LoadCpuScene(std::filesystem::path const& scene_file_path) -> std::optional<CpuScene> {
  namespace dx = DirectX;

  Assimp::Importer importer;

  // We don't need these scene objects
  importer.SetPropertyInteger(AI_CONFIG_PP_RVC_FLAGS, aiComponent_CAMERAS | aiComponent_LIGHTS | aiComponent_COLORS);
  // We don't want to bother with non-triangle primitives
  importer.SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_POINT | aiPrimitiveType_LINE);
  // Smoothing angle for smooth normal generation
  importer.SetPropertyFloat(AI_CONFIG_PP_GSN_MAX_SMOOTHING_ANGLE, 80.0f);

  auto const ai_scene{
    importer.ReadFile(reinterpret_cast<char const*>(scene_file_path.u8string().data()),
                      aiProcessPreset_TargetRealtime_MaxQuality | aiProcess_ConvertToLeftHanded |
                      aiProcess_TransformUVCoords |
                      aiProcess_RemoveComponent)
  };

  if (!ai_scene || ai_scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !ai_scene->mRootNode) {
    std::cerr << "Error loading scene: " << importer.GetErrorString() << "\n";
    return std::nullopt;
  }

  struct NodeTransformData {
    aiNode const* node;
    dx::XMFLOAT4X4 parent_world_mtx;
  };

  std::queue<NodeTransformData> node_queue;
  node_queue.emplace(ai_scene->mRootNode, dx::XMFLOAT4X4{
                       1.0F, 0.0F, 0.0F, 0.0F,
                       0.0F, 1.0F, 0.0F, 0.0F,
                       0.0F, 0.0F, 1.0F, 0.0F,
                       0.0F, 0.0F, 0.0F, 1.0F
                     });

  CpuScene scene;

  while (!node_queue.empty()) {
    auto const [node, parent_world_mtx]{node_queue.front()};
    node_queue.pop();

    dx::XMFLOAT4X4 const local_mtx{
      node->mTransformation.a1, node->mTransformation.b1, node->mTransformation.c1, node->mTransformation.d1,
      node->mTransformation.a2, node->mTransformation.b2, node->mTransformation.c2, node->mTransformation.d2,
      node->mTransformation.a3, node->mTransformation.b3, node->mTransformation.c3, node->mTransformation.d3,
      node->mTransformation.a4, node->mTransformation.b4, node->mTransformation.c4, node->mTransformation.d4
    };

    dx::XMFLOAT4X4 world_mtx;
    dx::XMStoreFloat4x4(
      &world_mtx, dx::XMMatrixMultiply(dx::XMLoadFloat4x4(&local_mtx), dx::XMLoadFloat4x4(&parent_world_mtx)));

    for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
      auto const ai_mesh{ai_scene->mMeshes[node->mMeshes[i]]};
      auto& mesh{scene.meshes.emplace_back()};

      mesh.positions.resize(ai_mesh->mNumVertices);
      mesh.normals.resize(ai_mesh->mNumVertices);
      mesh.texcoords.resize(ai_mesh->mNumVertices);
      mesh.tangents.resize(ai_mesh->mNumVertices);
      mesh.indices.reserve(ai_mesh->mNumFaces * 3);

      std::ranges::transform(ai_mesh->mVertices, ai_mesh->mVertices + ai_mesh->mNumVertices, mesh.positions.begin(),
                             [](auto const& ai_vec) {
                               return Vector4{ai_vec.x, ai_vec.y, ai_vec.z, 1};
                             });

      std::ranges::transform(ai_mesh->mNormals, ai_mesh->mNormals + ai_mesh->mNumVertices, mesh.normals.begin(),
                             [](auto const& ai_vec) {
                               return Vector4{ai_vec.x, ai_vec.y, ai_vec.z, 0};
                             });

      if (ai_mesh->HasTextureCoords(0)) {
        std::ranges::transform(ai_mesh->mTextureCoords[0],
                               ai_mesh->mTextureCoords[0] + ai_mesh->mNumVertices, mesh.texcoords.begin(),
                               [](auto const& ai_vec) {
                                 return Vector2{ai_vec.x, ai_vec.y};
                               });
      }

      if (ai_mesh->HasTangentsAndBitangents()) {
        std::ranges::transform(ai_mesh->mTangents, ai_mesh->mTangents + ai_mesh->mNumVertices, mesh.tangents.begin(),
                               [](auto const& ai_vec) {
                                 return Vector4{ai_vec.x, ai_vec.y, ai_vec.z, 0};
                               });
      }

      for (unsigned int j = 0; j < ai_mesh->mNumFaces; ++j) {
        auto const& face{ai_mesh->mFaces[j]};
        mesh.indices.insert(mesh.indices.end(), face.mIndices, face.mIndices + face.mNumIndices);
      }

      mesh.transform.world_mtx = world_mtx;
      dx::XMStoreFloat4x4(&mesh.transform.normal_mtx,
                          dx::XMMatrixTranspose(dx::XMMatrixInverse(nullptr, dx::XMLoadFloat4x4(&world_mtx))));

      auto const ai_mtl{ai_scene->mMaterials[ai_mesh->mMaterialIndex]};

      if (aiColor3D base_color; ai_mtl->Get(AI_MATKEY_BASE_COLOR, base_color) == aiReturn_SUCCESS) {
        mesh.mtl.base_color = dx::XMFLOAT3{base_color.r, base_color.g, base_color.b};
      }

      if (float roughness; ai_mtl->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness) == aiReturn_SUCCESS) {
        mesh.mtl.roughness = roughness;
      }
    }

    for (unsigned int j = 0; j < node->mNumChildren; ++j) {
      node_queue.emplace(node->mChildren[j], world_mtx);
    }
  }

  return scene;
}

auto CreateGpuScene(CpuScene const& cpu_scene, ID3D11Device& dev) -> std::optional<GpuScene> {
  GpuScene gpu_scene;

  for (auto const& cpu_mesh : cpu_scene.meshes) {
    auto& gpu_mesh{gpu_scene.meshes.emplace_back()};

    {
      D3D11_BUFFER_DESC const pos_buf_desc{
        .ByteWidth = static_cast<UINT>(cpu_mesh.positions.size() * sizeof(Vector4)),
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_VERTEX_BUFFER,
        .CPUAccessFlags = 0,
        .MiscFlags = 0,
        .StructureByteStride = 0
      };

      D3D11_SUBRESOURCE_DATA const pos_buf_data{
        .pSysMem = cpu_mesh.positions.data(),
        .SysMemPitch = 0,
        .SysMemSlicePitch = 0
      };

      if (FAILED(dev.CreateBuffer(&pos_buf_desc, &pos_buf_data, &gpu_mesh.pos_buf))) {
        std::cerr << "Failed to create position buffer\n";
        return std::nullopt;
      }
    }

    {
      D3D11_BUFFER_DESC const norm_buf_desc{
        .ByteWidth = static_cast<UINT>(cpu_mesh.normals.size() * sizeof(Vector4)),
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_VERTEX_BUFFER,
        .CPUAccessFlags = 0,
        .MiscFlags = 0,
        .StructureByteStride = 0
      };

      D3D11_SUBRESOURCE_DATA const norm_buf_data{
        .pSysMem = cpu_mesh.normals.data(),
        .SysMemPitch = 0,
        .SysMemSlicePitch = 0
      };

      if (FAILED(dev.CreateBuffer(&norm_buf_desc, &norm_buf_data, &gpu_mesh.norm_buf))) {
        std::cerr << "Failed to create normal buffer\n";
        return std::nullopt;
      }
    }

    {
      D3D11_BUFFER_DESC const uv_buf_desc{
        .ByteWidth = static_cast<UINT>(cpu_mesh.texcoords.size() * sizeof(Vector2)),
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_VERTEX_BUFFER,
        .CPUAccessFlags = 0,
        .MiscFlags = 0,
        .StructureByteStride = 0
      };

      D3D11_SUBRESOURCE_DATA const uv_buf_data{
        .pSysMem = cpu_mesh.texcoords.data(),
        .SysMemPitch = 0,
        .SysMemSlicePitch = 0
      };

      if (FAILED(dev.CreateBuffer(&uv_buf_desc, &uv_buf_data, &gpu_mesh.uv_buf))) {
        std::cerr << "Failed to create uv buffer\n";
        return std::nullopt;
      }
    }

    {
      D3D11_BUFFER_DESC const tan_buf_desc{
        .ByteWidth = static_cast<UINT>(cpu_mesh.tangents.size() * sizeof(Vector4)),
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_VERTEX_BUFFER,
        .CPUAccessFlags = 0,
        .MiscFlags = 0,
        .StructureByteStride = 0
      };

      D3D11_SUBRESOURCE_DATA const tan_buf_data{
        .pSysMem = cpu_mesh.tangents.data(),
        .SysMemPitch = 0,
        .SysMemSlicePitch = 0
      };

      if (FAILED(dev.CreateBuffer(&tan_buf_desc, &tan_buf_data, &gpu_mesh.tan_buf))) {
        std::cerr << "Failed to create tangent buffer\n";
        return std::nullopt;
      }
    }

    {
      D3D11_BUFFER_DESC const idx_buf_desc{
        .ByteWidth = static_cast<UINT>(cpu_mesh.indices.size() * sizeof(std::uint32_t)),
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_INDEX_BUFFER,
        .CPUAccessFlags = 0,
        .MiscFlags = 0,
        .StructureByteStride = 0
      };

      D3D11_SUBRESOURCE_DATA const idx_buf_data{
        .pSysMem = cpu_mesh.indices.data(),
        .SysMemPitch = 0,
        .SysMemSlicePitch = 0
      };

      if (FAILED(dev.CreateBuffer(&idx_buf_desc, &idx_buf_data, &gpu_mesh.idx_buf))) {
        std::cerr << "Failed to create index buffer\n";
        return std::nullopt;
      }
    }

    {
      D3D11_BUFFER_DESC constexpr transform_buf_desc{
        .ByteWidth = static_cast<UINT>(sizeof(GpuMeshTransform)),
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_CONSTANT_BUFFER,
        .CPUAccessFlags = 0,
        .MiscFlags = 0,
        .StructureByteStride = 0
      };

      D3D11_SUBRESOURCE_DATA const transform_buf_data{
        .pSysMem = &cpu_mesh.transform,
        .SysMemPitch = 0,
        .SysMemSlicePitch = 0
      };

      if (FAILED(dev.CreateBuffer(&transform_buf_desc, &transform_buf_data, &gpu_mesh.transform_buf))) {
        std::cerr << "Failed to create transform buffer\n";
        return std::nullopt;
      }
    }

    {
      D3D11_BUFFER_DESC constexpr mtl_buf_desc{
        .ByteWidth = static_cast<UINT>(sizeof(GpuMaterial)),
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_CONSTANT_BUFFER,
        .CPUAccessFlags = 0,
        .MiscFlags = 0,
        .StructureByteStride = 0
      };

      GpuMaterial const gpu_mtl{
        .base_color = cpu_mesh.mtl.base_color,
        .roughness = cpu_mesh.mtl.roughness,
        .has_base_color_map = FALSE,
        .has_roughness_map = FALSE,
        .has_normal_map = FALSE,
        .pad = {}
      };

      D3D11_SUBRESOURCE_DATA const mtl_buf_data{
        .pSysMem = &gpu_mtl,
        .SysMemPitch = 0,
        .SysMemSlicePitch = 0
      };

      if (FAILED(dev.CreateBuffer(&mtl_buf_desc, &mtl_buf_data, &gpu_mesh.mtl_buf))) {
        std::cerr << "Failed to create material buffer\n";
        return std::nullopt;
      }
    }

    gpu_mesh.idx_count = static_cast<UINT>(cpu_mesh.indices.size());
  }

  return gpu_scene;
}
}
