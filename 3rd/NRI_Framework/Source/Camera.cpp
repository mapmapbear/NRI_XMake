// © 2021 NVIDIA Corporation

#include "NRIFramework.h"
#include "spdlog/spdlog.h"

// to get a perspective matrix with reversed z, simply swap the near and far plane
glm::mat4 perspectiveFovReverseZLH_ZO(float fov, float width, float height, float zNear, float zFar) {
	return glm::perspectiveFovLH_ZO(fov, width, height, zFar, zNear);
}

// now let zFar go towards infinity
glm::mat4 infinitePerspectiveFovReverseZLH_ZO(float fov, float width, float height, float zNear) {
	const float h = 1.0f / glm::tan(0.5f * fov);
	const float w = h * height / width;
	glm::mat4 result = glm::zero<glm::mat4>();
	result[0][0] = w;
	result[1][1] = h;
	result[2][2] = 0.0f;
	result[2][3] = 1.0f;
	result[3][2] = zNear;
	return result;
}

void Camera::Initialize(const vec3 &position, const vec3 &lookAt,
		bool isRelative) {
	vec3 dir = normalize(lookAt - position);

	vec3 rot;
	rot.x = 0.0f;
	rot.y = 0.0f;
	rot.z = 0.0f;

	state.globalPosition = vec3(position);
	state.rotation = glm::degrees(rot);
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
	const vec3 vRight = vec3(state.mWorldToView[0][0], state.mWorldToView[1][0],
			state.mWorldToView[2][0]);
	const vec3 vUp = vec3(state.mWorldToView[0][1], state.mWorldToView[1][1],
			state.mWorldToView[2][1]);
	const vec3 vForward = vec3(state.mWorldToView[0][2], state.mWorldToView[1][2],
			state.mWorldToView[2][2]);

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
		statePrev.mViewToWorld = glm::translate(statePrev.mViewToWorld, -statePrev.position);
	} else {
		state.position = vec3(state.globalPosition);
		statePrev.position = vec3(statePrev.globalPosition);

		state.mWorldToView = glm::lookAtLH(state.globalPosition, state.globalPosition + vForward, glm::vec3(0.0, 1.0, 0.0));
	}

	// Rotation
	float angularSpeed = 10.0f * clamp(desc.horizontalFov * 0.5f / 90.0f, 0.0f, 1.0f);

	state.rotation.x += desc.dYaw * angularSpeed;
	state.rotation.y += desc.dPitch * angularSpeed;

	state.rotation.x = fmodf(state.rotation.x, 360.0f);
	state.rotation.y = clamp(state.rotation.y, -90.0f, 90.0f);
	SPDLOG_INFO("camera Rot: X={}, Y={}", state.rotation.x, state.rotation.y);
	if (desc.isCustomMatrixSet) {
		state.mViewToWorld = desc.customMatrix;
		state.rotation.z = 0.0f;
	} else {
#if 1
		state.mViewToWorld = glm::rotate(glm::mat4(1.0), glm::radians(state.rotation.x), vUp);
		state.mViewToWorld = glm::rotate(state.mViewToWorld, glm::radians(state.rotation.y), vRight);
#endif
		state.mWorldToView = state.mWorldToView * state.mViewToWorld;
	}

	// Projection
	if (desc.orthoRange > 0.0f) {
		float x = desc.orthoRange;
		float y = desc.orthoRange / desc.aspectRatio;
		state.mViewToClip = glm::orthoLH_ZO(-x, x, -y, y, desc.nearZ,
				desc.farZ);
	} else {
		glm::mat4 projMat = glm::mat4(1.0);
		if (desc.isReversedZ) {
			// projMat = glm::perspectiveLH_ZO(glm::radians(desc.horizontalFov), desc.aspectRatio, desc.farZ, desc.nearZ);
			projMat = infinitePerspectiveFovReverseZLH_ZO(desc.horizontalFov, 900, 600, desc.nearZ);
		} else {
			projMat = glm::perspectiveLH_ZO(desc.horizontalFov, desc.aspectRatio, desc.nearZ, desc.farZ);
		}

		state.mViewToClip = projMat;
	}

	// Other
	state.mWorldToClip = state.mViewToClip * state.mWorldToView;

	// state.mViewToWorld = state.mWorldToView;
	//   state.mViewToWorld.InvertOrtho();

	state.mClipToView = state.mViewToClip;
	//   state.mClipToView.Invert();

	state.mClipToWorld = state.mWorldToClip;
	state.mClipToWorld = glm::inverse(state.mClipToWorld);

	//   state.viewportJitter = Sequence::Halton2D(frameIndex) - 0.5f;

	// Previous other
	statePrev.mWorldToClip = statePrev.mViewToClip * statePrev.mWorldToView;
	statePrev.mViewToWorld = state.mViewToWorld;

	statePrev.mClipToWorld = statePrev.mWorldToClip;

	statePrev.rotation.x = state.rotation.x;
	statePrev.rotation.y = state.rotation.y;
	statePrev.rotation.z = state.rotation.z;
	state.rotation.x = 0;
	state.rotation.y = 0;
	state.rotation.z = 0;
}
