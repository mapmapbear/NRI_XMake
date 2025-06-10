#include "NRICompatibility.hlsli"

struct PushConstants {
    float4x4 viewMat;
    float4 cameraArgs; // znear, zfar, distCull, cullingEnabled
    float4 frustum; // left, right, top, bottom
    uint totalObjectCount;
};
NRI_ROOT_CONSTANTS(PushConstants, g_PushConstants, 1, 0);

struct DrawData {
    uint indexNum;
    uint instanceNum;
    uint baseIndex;
    int baseVertex;
    uint baseInstance;
};

struct CullData {
    float3 center;
    float radians;
};

bool FrustumVisible(uint objectIndex) {
    StructuredBuffer<CullData> sphereCullData = ResourceDescriptorHeap[1014];
    float3 center =  sphereCullData[objectIndex].center;
    float radius = sphereCullData[objectIndex].radians;
    float3 centerVS = mul(g_PushConstants.viewMat, float4(center,1.f)).xyz;
    bool visible = true;

    //frustrum culling
    visible = visible && centerVS.z * g_PushConstants.frustum[1] - abs(centerVS.x) * g_PushConstants.frustum.x > -radius;
    visible = visible && centerVS.z * g_PushConstants.frustum[3] - abs(centerVS.y) * g_PushConstants.frustum[2] > -radius;

    if(g_PushConstants.cameraArgs.z != 0) {
        // the near/far plane culling uses camera space Z directly
        visible = visible && centerVS.z + radius > g_PushConstants.cameraArgs.x && centerVS.z - radius < g_PushConstants.cameraArgs.y;
    }

    visible = visible || g_PushConstants.cameraArgs.w == 0;

    return visible;
}

[numthreads(64, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID) {
    uint objectIndex = DTid.x;
    if (objectIndex >= g_PushConstants.totalObjectCount) {
        return;
    }

    StructuredBuffer<DrawData> allObjects = ResourceDescriptorHeap[1015];
    AppendStructuredBuffer<DrawData> visibleObjects = ResourceDescriptorHeap[1016]; // Index Error

    bool visible = FrustumVisible(objectIndex);
    if (visible) {
        visibleObjects.Append(allObjects[objectIndex]);
    }
}