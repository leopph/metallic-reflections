#pragma once

#include <DirectXMath.h>

namespace refl {
class OrbitingCamera {
public:
  OrbitingCamera(DirectX::XMFLOAT3 const& orbit_center, float orbit_dist, float near_clip, float far_clip,
                 float vertical_fov_degrees);

  [[nodiscard]] auto ComputeViewMatrix() const -> DirectX::XMFLOAT4X4;
  [[nodiscard]] auto ComputeProjMatrix(float aspect_ratio) const -> DirectX::XMFLOAT4X4;

private:
  DirectX::XMFLOAT3 orbit_center_;
  float orbit_dist_;
  DirectX::XMFLOAT4 rotation_{0.0F, 0.0F, 0.0F, 1.0F};
  float near_clip_;
  float far_clip_;
  float vertical_fov_degrees_;
};
}
