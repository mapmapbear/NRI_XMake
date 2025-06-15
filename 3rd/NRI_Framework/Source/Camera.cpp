// © 2021 NVIDIA Corporation

#include "NRIFramework.h"
#include "glm/ext/matrix_transform.hpp"
#include "glm/fwd.hpp"
#include "glm/geometric.hpp"
#include "glm/trigonometric.hpp"
#include "spdlog/spdlog.h"

// to get a perspective matrix with reversed z, simply swap the near and far plane
glm::mat4 perspectiveFovLH_ZO(float fov, float aspect, float zNear, float zFar) {
	assert(abs(aspect - std::numeric_limits<float>::epsilon()) > static_cast<float>(0));

	float const rad = fov;
	float const h = glm::cos(static_cast<float>(0.5) * rad) / glm::sin(static_cast<float>(0.5) * rad);

	float SinFov = glm::sin(0.5f * fov);
	float CosFov = glm::cos(0.5f * fov);

	float Height = CosFov / SinFov;
	float Width = Height / aspect;

	float fRangle = zFar / (zFar - zNear);

	glm::mat4 Result = 0.0f;
	Result[0][0] = Width;

	Result[1][1] = Height;

	Result[2][2] = fRangle;
	Result[2][3] = static_cast<float>(1);

	Result[3][2] = -fRangle * zNear;
	return Result;
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

glm::mat4 reverseZMat(float fov, float aspect, float zNear, float zFar) {
	glm::mat4 result = glm::zero<glm::mat4>();

	assert(abs(aspect - std::numeric_limits<float>::epsilon()) > static_cast<float>(0));

	const float tanHalfFovy = tan(fov / static_cast<float>(2));

	result[0][0] = 1.0f / (aspect * tanHalfFovy);
	result[1][1] = 1.0f / (tanHalfFovy);
	result[2][2] = -zNear / (zFar - zNear);
	result[2][3] = 1.0f;
	result[3][2] = (zFar * zNear) / (zFar - zNear);
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
	state.mWorldToView = glm::lookAtLH(state.globalPosition, state.globalPosition + vForward, vUp);
	statePrev.mWorldToView = state.mWorldToView;
	m_IsRelative = isRelative;
}

void Camera::InitializeWithRotation(const vec3 &position, const vec3 &rotation,
		bool isRelative) {
	state.globalPosition = vec3(position);
	state.rotation = rotation;
	m_IsRelative = isRelative;
}



void Camera::Update(const CameraDesc &desc, uint32_t frameIndex) {
	m_desc = desc;
	uint32_t projFlags = 0;
	projFlags |= 0;

	vec3 delta = desc.dLocal * desc.timeScale;
	delta.z *= desc.isPositiveZ ? 1.0f : -1.0f;

	state.globalPosition += vec3(vRight * delta.x);
	state.globalPosition += vec3(vUp * delta.y);
	state.globalPosition += vec3(vForward * delta.z);
	state.globalPosition += vec3(desc.dUser);

	state.position = vec3(state.globalPosition);
	statePrev.position = vec3(statePrev.globalPosition);
	state.mWorldToView = glm::lookAtLH(state.globalPosition, state.globalPosition + vForward, vUp);
	float angularSpeed = 10.0f * clamp(desc.horizontalFov * 0.5f / 90.0f, 0.0f, 1.0f);

	state.rotation.x += desc.dYaw * angularSpeed;
	state.rotation.y += desc.dPitch * angularSpeed;

	state.rotation.x = fmodf(state.rotation.x, 360.0f);
	state.rotation.y = clamp(state.rotation.y, -90.0f, 90.0f);
#if 1
	// SPDLOG_INFO("state.rotation: {}, {}", state.rotation.x, state.rotation.y);
	// 计算yaw旋转（绕Y轴）
	glm::quat yawRotation = glm::angleAxis(glm::radians(-state.rotation.x), vUp);
	glm::vec3 rotatedForward = glm::normalize(yawRotation * vForward);
	glm::vec3 rotatedRight = glm::normalize(yawRotation * vRight);
	glm::vec3 rotatedUp = glm::normalize(yawRotation * vUp);

	// 计算pitch旋转（绕X轴）
	glm::quat pitchRotation = glm::angleAxis(glm::radians(-state.rotation.y), rotatedRight);
	rotatedForward = glm::normalize(pitchRotation * rotatedForward);
	//rotatedUp = glm::normalize(pitchRotation * rotatedUp);

	// 确保正交性
	// rotatedRight = glm::normalize(glm::cross(rotatedForward, rotatedUp));
	// rotatedUp = glm::normalize(glm::cross(rotatedRight, rotatedForward));

	vForward = rotatedForward;
	vRight = rotatedRight;
	// vUp = rotatedUp;

	// SPDLOG_INFO("vForward: {}, {}, {}", vForward.x, vForward.y, vForward.z);
	// SPDLOG_INFO("vRight: {}, {}, {}", vRight.x, vRight.y, vRight.z);
	// SPDLOG_INFO("vUp: {}, {}, {}", vUp.x, vUp.y, vUp.z);

	// // 构建视图矩阵
	// state.mWorldToView = glm::mat4(
	// 		glm::vec4(rotatedRight, 0.0f),
	// 		glm::vec4(rotatedUp, 0.0f),
	// 		glm::vec4(rotatedForward, 0.0f),
	// 		glm::vec4(state.position, 1.0f));
#endif

	// Projection
	if (desc.orthoRange > 0.0f) {
		float x = desc.orthoRange;
		float y = desc.orthoRange / desc.aspectRatio;
		state.mViewToClip = glm::orthoLH_ZO(-x, x, -y, y, desc.nearZ,
				desc.farZ);
	} else {
		glm::mat4 projMat = glm::mat4(1.0);
		if (desc.isReversedZ) {
			// projMat = reverseZMat(desc.horizontalFov, desc.aspectRatio, desc.nearZ, desc.farZ);
			projMat = glm::perspectiveLH_ZO(desc.horizontalFov, desc.aspectRatio, desc.farZ, desc.nearZ);
			// projMat = infinitePerspectiveFovReverseZLH_ZO(desc.horizontalFov, 900, 600, desc.nearZ);
		} else {
			projMat = perspectiveFovLH_ZO(desc.horizontalFov, desc.aspectRatio, desc.nearZ, desc.farZ);
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

	statePrev.globalPosition = state.globalPosition;
	statePrev.mViewToClip = state.mViewToClip;
	state.rotation.x = 0;
	state.rotation.y = 0;
	state.rotation.z = 0;
}
