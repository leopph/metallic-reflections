#include "OrbitingCamera.hpp"

#include <algorithm>

namespace refl {
OrbitingCamera::OrbitingCamera(DirectX::XMFLOAT3 const& orbit_center, float const orbit_dist, float const near_clip,
                               float const far_clip, float const vertical_fov_degrees) :
  orbit_center_{orbit_center}, orbit_dist_{orbit_dist}, near_clip_{near_clip}, far_clip_{far_clip},
  vertical_fov_degrees_{vertical_fov_degrees} {
}

auto OrbitingCamera::Rotate(float const yaw_degrees) -> void {
  using namespace DirectX;
  XMVECTOR const current_rotation_quat{XMLoadFloat4(&rotation_)};
  float const yaw_radians{XMConvertToRadians(yaw_degrees)};
  XMVECTOR const yaw_rotation_quat{XMQuaternionRotationAxis(XMVectorSet(0.0F, 1.0F, 0.0F, 0.0F), yaw_radians)};
  XMVECTOR const new_rotation_quat{XMQuaternionMultiply(yaw_rotation_quat, current_rotation_quat)};
  XMStoreFloat4(&rotation_, new_rotation_quat);
}

auto OrbitingCamera::Zoom(float const amount) -> void {
  orbit_dist_ += amount;
  orbit_dist_ = std::max(orbit_dist_, 0.1F);
}

auto OrbitingCamera::ComputePosition() const -> DirectX::XMFLOAT3 {
  using namespace DirectX;
  XMVECTOR const rotation_quat{XMLoadFloat4(&rotation_)};
  XMVECTOR const forward_vec{XMVector3Rotate(XMVectorSet(0.0F, 0.0F, 1.0F, 0.0F), rotation_quat)};
  XMVECTOR const camera_pos{XMVectorAdd(XMLoadFloat3(&orbit_center_), XMVectorScale(forward_vec, -orbit_dist_))};
  XMFLOAT3 camera_pos_float3;
  XMStoreFloat3(&camera_pos_float3, camera_pos);
  return camera_pos_float3;
}

auto OrbitingCamera::ComputeViewMatrix() const -> DirectX::XMFLOAT4X4 {
  using namespace DirectX;

  XMVECTOR const rotation_quat{XMLoadFloat4(&rotation_)};
  XMVECTOR const forward_vec{XMVector3Rotate(XMVectorSet(0.0F, 0.0F, 1.0F, 0.0F), rotation_quat)};
  XMVECTOR const camera_pos{XMVectorAdd(XMLoadFloat3(&orbit_center_), XMVectorScale(forward_vec, -orbit_dist_))};
  XMVECTOR const up_vec{XMVector3Rotate(XMVectorSet(0.0F, 1.0F, 0.0F, 0.0F), rotation_quat)};

  XMFLOAT4X4 view_mtx;
  XMStoreFloat4x4(&view_mtx, XMMatrixLookAtLH(camera_pos, XMLoadFloat3(&orbit_center_), up_vec));

  return view_mtx;
}

auto OrbitingCamera::ComputeProjMatrix(float const aspect_ratio) const -> DirectX::XMFLOAT4X4 {
  using namespace DirectX;

  XMFLOAT4X4 proj_mtx;
  XMStoreFloat4x4(&proj_mtx, XMMatrixPerspectiveFovLH(XMConvertToRadians(vertical_fov_degrees_), aspect_ratio,
                                                      near_clip_, far_clip_));

  return proj_mtx;
}
}
