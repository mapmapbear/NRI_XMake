// © 2021 NVIDIA Corporation

#pragma once
#include "glm/glm.hpp"
#include "glm/gtc/type_ptr.hpp"
using namespace glm;

struct CameraDesc {
    // cBoxf limits = {};
    vec3 dLocal = vec3(0.0f);
    vec3 dUser = vec3(0.0f);
    float dYaw = 0.0f;   // deg
    float dPitch = 0.0f; // deg
    float aspectRatio = 1.0f;
    float horizontalFov = 90.0f; // deg
    float nearZ = 0.1f;
    float farZ = 10000.0f;
    float orthoRange = 0.0f;
    float timeScale = 0.5f;
    float backwardOffset = 0.0f;
    bool isReversedZ = false;
    bool isPositiveZ = true;
    bool isCustomMatrixSet = false;
    mat4 customMatrix = mat4(1.0);
};

struct CameraState {
    vec3 globalPosition = vec3(0.0);
    mat4 mViewToClip = mat4(1.0);
    mat4 mClipToView = mat4(1.0);
    mat4 mWorldToView = mat4(1.0);
    mat4 mViewToWorld = mat4(1.0);
    mat4 mWorldToClip = mat4(1.0);
    mat4 mClipToWorld = mat4(1.0);
    vec3 position = {};
    vec3 rotation = {};
    vec2 viewportJitter = {};
    float motionScale = 0.015f;
};

class Camera {
public:
    void Update(const CameraDesc& desc, uint32_t frameIndex);
    void Initialize(const vec3& position, const vec3& lookAt, bool isRelative = false);
    void InitializeWithRotation(const vec3& position, const vec3& rotationDegrees, bool isRelative);

    inline void SavePreviousState() {
        statePrev = state;
    }

    inline const vec3 GetRelative(const vec3& origin) const {
        vec3 position = m_IsRelative ? state.globalPosition : vec3(0.0);

        return vec3(origin - position);
    }

    inline void* GetState() {
        return &state;
    }

    static inline uint32_t GetStateSize() {
        return sizeof(CameraState);
    }

public:
    CameraState state = {};
    CameraState statePrev = {};

private:
    bool m_IsRelative = false;
};
