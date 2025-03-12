// © 2021 NVIDIA Corporation

#include "NRIFramework.h"

void Camera::Initialize(const vec3 &position, const vec3 &lookAt,
                        bool isRelative) {
  vec3 dir = normalize(lookAt - position);

  vec3 rot;
  rot.x = atan2(dir.y, dir.x);
  rot.y = asin(dir.z);
  rot.z = 0.0f;

  state.globalPosition = vec3(position);
  state.rotation = degrees(rot);
  m_IsRelative = isRelative;
}

void Camera::InitializeWithRotation(const vec3 &position, const vec3 &rotation,
                                    bool isRelative) {
  state.globalPosition = vec3(position);
  state.rotation = rotation;
  m_IsRelative = isRelative;
}

void Camera::Update(const CameraDesc &desc, uint32_t frameIndex) {
  uint32_t projFlags = 0;
  projFlags |= 0;

  // Position
  const vec3 vRight = vec3(state.mWorldToView[0].x, state.mWorldToView[0].y,
                           state.mWorldToView[0].z);
  const vec3 vUp = vec3(state.mWorldToView[1].x, state.mWorldToView[1].y,
                        state.mWorldToView[1].z);
  const vec3 vForward = vec3(state.mWorldToView[2].x, state.mWorldToView[2].y,
                             state.mWorldToView[2].z);

  vec3 delta = desc.dLocal * desc.timeScale;
  delta.z *= desc.isPositiveZ ? 1.0f : -1.0f;

  state.globalPosition += vec3(vRight * delta.x);
  state.globalPosition += vec3(vUp * delta.y);
  state.globalPosition += vec3(vForward * delta.z);
  state.globalPosition += vec3(desc.dUser);

  //   if (desc.limits.IsValid())
  //     state.globalPosition = clamp(state.globalPosition,
  //     vec3(desc.limits.vMin),
  //                                  vec3(desc.limits.vMax));

  if (desc.isCustomMatrixSet) {
    const vec3 vCustomRight = vec3(
        desc.customMatrix[3].x, desc.customMatrix[3].y, desc.customMatrix[3].z);
    state.globalPosition = vec3(vCustomRight);
  }

  if (m_IsRelative) {
    state.position = vec3(0.0);
    statePrev.position = vec3(statePrev.globalPosition - state.globalPosition);
    // statePrev.mWorldToView.PreTranslation(-statePrev.position);
  } else {
    state.position = vec3(state.globalPosition);
    statePrev.position = vec3(statePrev.globalPosition);
  }

  // Rotation
  float angularSpeed = 0.03f * clamp(desc.horizontalFov * 0.5f / 90.0f, 0.0f, 1.0f);

  state.rotation.x += desc.dYaw * angularSpeed;
  state.rotation.y += desc.dPitch * angularSpeed;

  state.rotation.x = fmodf(state.rotation.x, 360.0f);
  state.rotation.y = clamp(state.rotation.y, -90.0f, 90.0f);

  if (desc.isCustomMatrixSet) {
    state.mViewToWorld = desc.customMatrix;

    // state.rotation = degrees(state.mViewToWorld.GetRotationYPR());
    state.rotation.z = 0.0f;
  } else
    // state.mViewToWorld.SetupByRotationYPR(radians(state.rotation.x),
    //                                       radians(state.rotation.y),
    //                                       radians(state.rotation.z));

  state.mWorldToView = state.mViewToWorld;
//   state.mWorldToView.PreTranslation(vec3(state.mWorldToView.GetRow2().xyz) *
//                                     desc.backwardOffset);
//   state.mWorldToView.WorldToView(projFlags);
//   state.mWorldToView.PreTranslation(-state.position);

  // Projection
  if (desc.orthoRange > 0.0f) {
    float x = desc.orthoRange;
    float y = desc.orthoRange / desc.aspectRatio;
    // state.mViewToClip.SetupByOrthoProjection(-x, x, -y, y, desc.nearZ,
                                            //  desc.farZ, projFlags);
  } else {
    // if (desc.farZ == 0.0f)
    //   state.mViewToClip.SetupByHalfFovxInf(0.5f * radians(desc.horizontalFov),
    //                                        desc.aspectRatio, desc.nearZ,
    //                                        projFlags);
    // else
    //   state.mViewToClip.SetupByHalfFovx(0.5f * radians(desc.horizontalFov),
    //                                     desc.aspectRatio, desc.nearZ, desc.farZ,
    //                                     projFlags);
  }

  // Other
  state.mWorldToClip = state.mViewToClip * state.mWorldToView;

  state.mViewToWorld = state.mWorldToView;
//   state.mViewToWorld.InvertOrtho();

  state.mClipToView = state.mViewToClip;
//   state.mClipToView.Invert();

  state.mClipToWorld = state.mWorldToClip;
//   state.mClipToWorld.Invert();

//   state.viewportJitter = Sequence::Halton2D(frameIndex) - 0.5f;

  // Previous other
  statePrev.mWorldToClip = statePrev.mViewToClip * statePrev.mWorldToView;

  statePrev.mViewToWorld = statePrev.mWorldToView;
//   statePrev.mViewToWorld.InvertOrtho();

  statePrev.mClipToWorld = statePrev.mWorldToClip;
//   statePrev.mClipToWorld.Invert();
}
