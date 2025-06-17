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

bool Hiz_Culling(uint objectIndex) {
    StructuredBuffer<CullData> sphereCullData = ResourceDescriptorHeap[1015];
    Texture2D<float> hiZTexture = ResourceDescriptorHeap[1021];
    SamplerState linearSampler = ResourceDescriptorHeap[4];
    float3 center =  sphereCullData[objectIndex].center;
    float3 extents = sphereCullData[objectIndex].extents;
    float3 points[8];
    for(uint i = 0; i < 8; i++) {
        points[i] = GetAABBPoint(i, center, extents);
    }

    float4 viewPoints[8];
    float minZ = 1.0f;
    float maxZ = 0.0f;
    float2 minXY = float2(1.0f, 1.0f);
    float2 maxXY = float2(-1.0f, -1.0f);

    float4x4 VPMat = mul(projectMat, viewMat);

    for(uint i = 0; i < 8; i++) {
        viewPoints[i] = mul(VPMat, float4(points[i], 1.0f));
        viewPoints[i].xyz /= viewPoints[i].w;
        
        minZ = min(minZ, viewPoints[i].z);
        maxZ = max(maxZ, viewPoints[i].z);
        minXY = min(minXY, viewPoints[i].xy);
        maxXY = max(maxXY, viewPoints[i].xy);
    }
    minXY = minXY * 0.5f + 0.5f;
    maxXY = maxXY * 0.5f + 0.5f;

    float2 size = maxXY - minXY;
    float maxSize = max(size.x, size.y);
    float mipLevel = ceil(log2(maxSize));
    
    float2 uv = (minXY + maxXY) * 0.5f;
    float depth = hiZTexture.SampleLevel(linearSampler, uv, mipLevel).r;

    return maxZ >= depth;
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


float2 ProjectSphere( float x, float z, float r, float ResultScale )
{
	float t = sqrt( x*x + z*z - r*r );

	float A = ( t*z + r*x );
	float B = ( t*z - r*x );
	ResultScale /= ( A * B );	// Divide by common denominator instead of dividing twice

	float Min = ( t*x - r*z ) * B;
	float Max = ( t*x + r*z ) * A;

	return float2( Min, Max ) * ResultScale;
}

// [ Mara & Morgan 2013, "2D Polyhedral Bounds of a Clipped, Perspective-Projected 3D Sphere" ]
float4 SphereToScreenRect( float3 Center, float Radius, float4x4 ViewToClip )
{
	float2 ExtentX = ProjectSphere( Center.x, Center.z, Radius, ViewToClip[0][0] ) + ViewToClip[2][0];
	float2 ExtentY = ProjectSphere( Center.y, Center.z, Radius, ViewToClip[1][1] ) + ViewToClip[2][1];

	return float4( ExtentX.x, ExtentY.x, ExtentX.y, ExtentY.y );
}

bool SphereCullFrustum(uint objectIndex) {
    StructuredBuffer<CullData> sphereCullData = ResourceDescriptorHeap[1015];
    float3 center = sphereCullData[objectIndex].center;
    float radius = min(sphereCullData[objectIndex].extents.x, min(sphereCullData[objectIndex].extents.y, sphereCullData[objectIndex].extents.z));
    
    float4 centerVS = mul(g_PushConstants.viewMat, float4(center, 1.0));
    float SphereMinZ = centerVS.z - radius;
    float SphereMaxZ = centerVS.z + radius;

    bool visible = true;
    if (visible)
	{
		visible = (SphereMinZ * projectMat[2][2] + projectMat[3][2] > 0);
	}

    float4 Rect = SphereToScreenRect(centerVS.xyz, radius, projectMat);

    float3 RectMin = float3( Rect.xy, 0 );
	float3 RectMax = float3( Rect.zw, SphereMaxZ );

    if (visible)
	{
		visible = all(RectMin.xy < 1) && all(RectMax.xy > -1);
	}

    return visible;
}

[numthreads(8, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID) {
    uint objectIndex = DTid.x;
    if (objectIndex >= g_PushConstants.totalObjectCount) {
        return;
    }

    StructuredBuffer<DrawData> allObjects = ResourceDescriptorHeap[1016];
    RWStructuredBuffer<DrawData> visibleObjects = ResourceDescriptorHeap[1017];
    RWStructuredBuffer<uint> visibleObjectCounter = ResourceDescriptorHeap[1018];

    // bool visible = FrustumVisible(objectIndex);
    // visible = visible && Hiz_Culling(objectIndex);
    // bool visible = Hiz_Culling(objectIndex);
    bool visible = SphereCullFrustum(objectIndex);
    if (visible) {
        uint writeIndex = 0;
        InterlockedAdd(visibleObjectCounter[0], 1, writeIndex);
        if (writeIndex < g_PushConstants.totalObjectCount) {
            // 暂时规定最大数量为最坏剔除结果
            visibleObjects[writeIndex] = allObjects[objectIndex];
        }
    }
}
