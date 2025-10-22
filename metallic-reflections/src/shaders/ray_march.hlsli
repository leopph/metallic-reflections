// ReSharper disable CppEnforceCVQualifiersPlacement

// Based on McGuire and Mara, Efficient GPU Screen-Space Ray Tracing, Journal of Computer Graphics Techniques, 2014

#ifndef RAY_MARCH_HLSLI
#define RAY_MARCH_HLSLI

inline void Swap(inout float a, inout float b) {
  float t = a;
  a = b;
  b = t;
}


inline float DistanceSquared(float2 A, float2 B) {
  float2 D = A - B;
  return dot(D, D);
}

/**
  Native left-handed view-space ray trace (single layer, row-major, minimized).

  Returns true if the ray intersects any pixel depth voxel.

  Parameters:
    vsRayOrigin        View-space origin (+Z forward).
    vsRayDirection     Unit view-space direction.
    viewToPixelMatrix  Row-major matrix mapping view space to pixel space.
    vsZBuffer          Depth texture storing either linear positive z or hyperbolic depth.
    vsDepthThickness   Voxel thickness along +Z for each pixel.
    zBufferIsHyperbolic If true, call ReconstructCSZ for positive z.
    nearPlaneZ         > 0 near plane (z = nearPlaneZ).
    farPlaneZ          > 0 far plane.
    pixelStride        Pixel step size (>= 1).
    jitterFraction     Fractional stride offset (0..1) for banding reduction.
    maxSteps           Max pixel iterations.
    maxTraceDistance   Max linear distance along the ray.
  Outputs:
    hitPixel           First intersected pixel or (-1,-1) on miss.
    vsHitPoint         View-space hit position (undefined if miss).
*/
bool MarchRay(
  float3 vsRayOrigin,
  float3 vsRayDirection,
  row_major float4x4 viewToPixelMatrix,
  Texture2D<float> vsZBuffer,
  float vsDepthThickness,
  bool zBufferIsHyperbolic,
  float nearPlaneZ,
  float farPlaneZ,
  float pixelStride,
  float jitterFraction,
  float maxSteps,
  float maxTraceDistance,
  out float2 hitPixel,
  out float3 vsHitPoint) {
  // Clip backward ray against near plane
  float endZCandidate = vsRayOrigin.z + vsRayDirection.z * maxTraceDistance;
  float rayLength = maxTraceDistance;
  if ((vsRayDirection.z < 0.0) && (endZCandidate < nearPlaneZ)) {
    rayLength = (nearPlaneZ - vsRayOrigin.z) / vsRayDirection.z;
  }

  float3 vsRayEndPoint = vsRayOrigin + vsRayDirection * rayLength;

  float4 H0 = mul(float4(vsRayOrigin, 1.0), viewToPixelMatrix);
  float4 H1 = mul(float4(vsRayEndPoint, 1.0), viewToPixelMatrix);

  float invW0 = 1.0 / H0.w;
  float invW1 = 1.0 / H1.w;

  float3 vsHomogRayStart = vsRayOrigin * invW0;
  float3 vsHomogRayEnd = vsRayEndPoint * invW1;

  float2 pixelStart = H0.xy * invW0;
  float2 pixelEnd = H1.xy * invW1;

  hitPixel = float2(-1.0, -1.0);

  // Ensure non-degenerate extent
  if (DistanceSquared(pixelStart, pixelEnd) < 0.0001) {
    pixelEnd.x += 0.01;
  }

  float2 pixelDelta = pixelEnd - pixelStart;
  bool permuteXY = false;
  if (abs(pixelDelta.x) < abs(pixelDelta.y)) {
    permuteXY = true;
    pixelDelta = pixelDelta.yx;
    pixelStart = pixelStart.yx;
    pixelEnd = pixelEnd.yx;
  }

  float stepDir = (pixelDelta.x >= 0.0) ? 1.0 : -1.0;
  float invdx = stepDir / pixelDelta.x;

  float2 dPixel = float2(stepDir, invdx * pixelDelta.y);
  float3 dHomogRay = (vsHomogRayEnd - vsHomogRayStart) * invdx;
  float dInvW = (invW1 - invW0) * invdx;

  dPixel *= pixelStride;
  dHomogRay *= pixelStride;
  dInvW *= pixelStride;

  pixelStart += dPixel * jitterFraction;
  vsHomogRayStart += dHomogRay * jitterFraction;
  invW0 += dInvW * jitterFraction;

  float3 vsHomogRayPoint = vsHomogRayStart;
  float invW = invW0;

  float prevRayZMaxEstimate = vsRayOrigin.z;
  float stepCount = 0.0;
  float vsRayZMax = prevRayZMaxEstimate;
  float vsRayZMin = prevRayZMaxEstimate;

  float vsSceneDepth = 1e30; // Large initial depth
  float end = pixelEnd.x * stepDir;

  for (float2 pixelPos = pixelStart;
       (pixelPos.x * stepDir) <= end &&
       (stepCount < maxSteps) &&
       (vsSceneDepth != 0.0);
       pixelPos += dPixel, vsHomogRayPoint.z += dHomogRay.z, invW += dInvW, stepCount += 1.0) {
    hitPixel = permuteXY ? pixelPos.yx : pixelPos;

    vsRayZMin = prevRayZMaxEstimate;
    vsRayZMax = (dHomogRay.z * 0.5 + vsHomogRayPoint.z) / (dInvW * 0.5 + invW);
    prevRayZMaxEstimate = vsRayZMax;
    if (vsRayZMin > vsRayZMax) {
      Swap(vsRayZMin, vsRayZMax);
    }

    int2 ipixel = int2(hitPixel);
    vsSceneDepth = vsZBuffer.Load(int3(ipixel, 0));

    if (zBufferIsHyperbolic) {
      vsSceneDepth = NdcToViewDepth(vsSceneDepth, nearPlaneZ, farPlaneZ); // Expect positive z
      // If ReconstructCSZ returns negative z, uncomment:
      // vsSceneDepth = -vsSceneDepth;
    }

    float vsVoxelZMin = vsSceneDepth - vsDepthThickness;
    if ((vsRayZMax >= vsVoxelZMin) && (vsRayZMin <= vsSceneDepth)) {
      break;
    }
  }

  vsHomogRayPoint.xy += dHomogRay.xy * stepCount;
  vsHitPoint = vsHomogRayPoint * (1.0 / invW);

  float vsVoxelZMinFinal = vsSceneDepth - vsDepthThickness;
  bool hit = (vsSceneDepth != 0.0) &&
    (vsRayZMax >= vsVoxelZMinFinal) &&
    (vsRayZMin <= vsSceneDepth);

  return hit;
}

#endif
