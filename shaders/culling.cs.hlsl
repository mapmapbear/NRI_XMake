#include "NRICompatibility.hlsli"

struct PushConstants {
    float4x4 viewMat;
    float4 cameraArgs; // znear, zfar, distCull, cullingEnabled
    float4 frustum[4]; // left, right, top, bottom
    uint totalObjectCount;
};
NRI_ROOT_CONSTANTS(PushConstants, g_PushConstants, 1, 0);

NRI_RESOURCE(cbuffer, CommonConstants, b, 0, 1) {
    float4x4 modelMat;
    float4x4 viewMat;
    float4x4 projectMat;
    float4x4 lightVP;
};

struct DrawData {
    uint indexNum;
    uint instanceNum;
    uint baseIndex;
    int baseVertex;
    uint baseInstance;
};

struct CullData {
    float3 center;
    float3 extents;
};

float3 GetAABBPoint(uint cornerIndex, float3 center, float3 extents) {
    float3 p;
    p.x = (cornerIndex & 1) ? center.x + extents.x : center.x - extents.x;
    p.y = (cornerIndex & 2) ? center.y + extents.y : center.y - extents.y;
    p.z = (cornerIndex & 4) ? center.z + extents.z : center.z - extents.z;
    return p;
}

bool isSphereInFrustum(float3 center, float radius) {
    for (int i = 0; i < 4; i++) {
        float4 plane = g_PushConstants.frustum[i];
        float distance = dot(plane.xyz, center);
        if (distance < -radius) {
            return false;
        }
    }
    return true;
}

bool FrustumVisible(uint objectIndex) {
    StructuredBuffer<CullData> sphereCullData = ResourceDescriptorHeap[1015];
    float3 center =  sphereCullData[objectIndex].center;
    float radius = min(sphereCullData[objectIndex].extents.x, min(sphereCullData[objectIndex].extents.y, sphereCullData[objectIndex].extents.z));
    float4 centerVS = mul(g_PushConstants.viewMat, float4(center,radius));
    bool visible = true;

    visible = isSphereInFrustum(centerVS.xyz, radius);
    return visible;
}

float2 ProjectSphere(float x, float z, float r, float ResultScale) {
    float t = sqrt(x*x + z*z - r*r);

    float A = (t*z + r*x);
    float B = (t*z - r*x);
    ResultScale /= (A * B);	// Divide by common denominator instead of dividing twice

    float Min = (t*x - r*z) * B;
    float Max = (t*x + r*z) * A;

    return float2(Min, Max) * ResultScale;
}

// [ Mara & Morgan 2013, "2D Polyhedral Bounds of a Clipped, Perspective-Projected 3D Sphere" ]
float4 SphereToScreenRect(float3 Center, float Radius, float4x4 ViewToClip) {
    float2 ExtentX = ProjectSphere(Center.x, Center.z, Radius, ViewToClip[0][0]) + ViewToClip[2][0];
    float2 ExtentY = ProjectSphere(Center.y, Center.z, Radius, ViewToClip[1][1]) + ViewToClip[2][1];

    return float4(ExtentX.x, ExtentY.x, ExtentX.y, ExtentY.y);
}

bool SphereCullFrustum(uint objectIndex) {
    StructuredBuffer<CullData> sphereCullData = ResourceDescriptorHeap[1015];
    float3 center = sphereCullData[objectIndex].center;
    float radius = min(sphereCullData[objectIndex].extents.x, min(sphereCullData[objectIndex].extents.y, sphereCullData[objectIndex].extents.z));

    float4 centerVS = mul(g_PushConstants.viewMat, float4(center, 1.0));
    float SphereMinZ = centerVS.z - radius;
    float SphereMaxZ = centerVS.z + radius;

    bool visible = true;
    if (visible) {
        visible = (SphereMinZ * projectMat[2][2] + projectMat[3][2] > 0);
    }

    float4 Rect = SphereToScreenRect(centerVS.xyz, radius, projectMat);

    float3 RectMin = float3(Rect.xy, 0);
    float3 RectMax = float3(Rect.zw, SphereMaxZ);

    if (visible) {
        visible = all(RectMin.xy < 1) && all(RectMax.xy > -1);
    }

    return visible;
}

bool HizCull(uint objectIndex) {
    StructuredBuffer<CullData> sphereCullData = ResourceDescriptorHeap[1015];
    Texture2D<float> HizBuffer = ResourceDescriptorHeap[1021];
    SamplerState HizSampler = ResourceDescriptorHeap[5];

    float3 center = sphereCullData[objectIndex].center;
    float3 extents = sphereCullData[objectIndex].extents;

    float3 corners[8];
    corners[0] = center + float3(-extents.x, -extents.y, -extents.z);
    corners[1] = center + float3(extents.x, -extents.y, -extents.z);
    corners[2] = center + float3(extents.x,  extents.y, -extents.z);
    corners[3] = center + float3(-extents.x,  extents.y, -extents.z);
    corners[4] = center + float3(-extents.x, -extents.y,  extents.z);
    corners[5] = center + float3(extents.x, -extents.y,  extents.z);
    corners[6] = center + float3(extents.x,  extents.y,  extents.z);
    corners[7] = center + float3(-extents.x,  extents.y,  extents.z);

    float4x4 VPMat = mul(projectMat, viewMat);
    VPMat = g_PushConstants.viewMat;
    float2 minNDC = float2(2.0, 2.0);
    float2 maxNDC = float2(-2.0, -2.0);
    float minDepth = 1.0;
    for (int i = 0; i < 8; ++i) {
        float4 clipPos = mul(VPMat, float4(corners[i], 1.0));

        // 如果所有顶点都在裁剪空间w <= 0后面，则剔除
        // if (clipPos.w > 0) {
        //     return false;
        // }
        float3 ndc = clipPos.xyz / clipPos.w;
        // 转换为UV坐标 (0 to 1)
        float2 uv = ndc.xy * 0.5 + 0.5;

        minNDC = min(minNDC, uv);
        maxNDC = max(maxNDC, uv);
        minDepth = min(minDepth, ndc.z);
    }

    if (minNDC.x < 0.0 || maxNDC.x < 0.0 || minNDC.y > 1.0 || maxNDC.y > 1.0) {
        return false;
    }

    float2 screenSize = float2(1920.0 / 2, 1080.0 / 2);
    float2 boxScreenSize = (maxNDC - minNDC) * screenSize;
    float mipLevel = ceil(log2(max(boxScreenSize.x, boxScreenSize.y)));
    mipLevel = clamp(mipLevel, 0, 9);

    float2 minUV = minNDC;
    float2 maxUV = maxNDC;

    float2 p0 = float2(minNDC.x, minNDC.y);
    float2 p1 = float2(maxNDC.x, minNDC.y);
    float2 p2 = float2(minNDC.x, maxNDC.y);
    float2 p3 = float2(maxNDC.x, maxNDC.y);

    float p0Depth = HizBuffer.SampleLevel(HizSampler, p0, mipLevel).r;
    float p1Depth = HizBuffer.SampleLevel(HizSampler, p1, mipLevel).r;
    float p2Depth = HizBuffer.SampleLevel(HizSampler, p2, mipLevel).r;
    float p3Depth = HizBuffer.SampleLevel(HizSampler, p3, mipLevel).r;
    float HizDepth = max(max(p0Depth, p1Depth), max(p2Depth, p3Depth));

    // 然后用 occluderDepth 进行比较
    return minDepth < HizDepth;
}

groupshared uint s_DrawCount;

[numthreads(8, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID) {
    uint objectIndex = DTid.x;
    if (objectIndex >= g_PushConstants.totalObjectCount) {
        return;
    }

    if (DTid.x == 0) {
        s_DrawCount = 0;
    }

    GroupMemoryBarrierWithGroupSync();

    StructuredBuffer<DrawData> allObjects = ResourceDescriptorHeap[1016];
    RWStructuredBuffer<DrawData> visibleObjects = ResourceDescriptorHeap[1017];
    RWStructuredBuffer<uint> visibleObjectCounter = ResourceDescriptorHeap[1018];

    // bool visible = FrustumVisible(objectIndex);
    // visible = visible && Hiz_Culling(objectIndex);HZB
    // bool visible = Hiz_Culling(objectIndex);
    // bool visible = SphereCullFrustum(objectIndex);
    bool visible = HizCull(objectIndex);
    if (visible) {
        uint writeIndex = 0;
        InterlockedAdd(s_DrawCount, 1, writeIndex);
        if (writeIndex < g_PushConstants.totalObjectCount) {
            // 暂时规定最大数量为最坏剔除结果
            visibleObjects[writeIndex] = allObjects[objectIndex];
        }
    }

    GroupMemoryBarrierWithGroupSync();
    if (DTid.x == 0) {
        visibleObjectCounter[0] = s_DrawCount;
    }
}
