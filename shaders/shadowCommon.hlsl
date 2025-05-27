#define sampler_t SamplerComparisonState
#define texture2d_t Texture2D<float>

float hard_shadow(float3 shadow_pos, sampler_t shadow_sampler, texture2d_t shadow_map, float bias)
{
    float shadow = shadow_map.SampleCmpLevelZero(shadow_sampler, shadow_pos.xy, shadow_pos.z + bias).x;
    return shadow;
}

float pcf_shadow(float3 shadow_pos, sampler_t shadow_sampler, texture2d_t shadow_map, float bias = 0.002, int kernelSize = 3)
{
    float shadow = 0.0;
    float2 texelSize;
    uint width, height;
    shadow_map.GetDimensions(width, height);
    texelSize = float2(1.0 / width, 1.0 / height);
    
    int halfKernel = kernelSize / 2;
    float samples = 0.0;
    
    for (int x = -halfKernel; x <= halfKernel; x++)
    {
        for (int y = -halfKernel; y <= halfKernel; y++)
        {
            float2 offset = float2(x, y) * texelSize;
            shadow += shadow_map.SampleCmpLevelZero(shadow_sampler, shadow_pos.xy + offset, shadow_pos.z - bias).x;
            samples += 1.0;
        }
    }
    
    return shadow / samples;
}

float pcf_shadow_weighted(float3 shadow_pos, sampler_t shadow_sampler, texture2d_t shadow_map, float bias = 0.002)
{
    float shadow = 0.0;
    float2 texelSize;
    uint width, height;
    shadow_map.GetDimensions(width, height);
    texelSize = float2(1.0 / width, 1.0 / height);
    
    float weights[9] = {
        1.0, 2.0, 1.0,
        2.0, 4.0, 2.0,
        1.0, 2.0, 1.0
    };
    float totalWeight = 16.0;
    
    int idx = 0;
    for (int x = -1; x <= 1; x++)
    {
        for (int y = -1; y <= 1; y++)
        {
            float2 offset = float2(x, y) * texelSize;
            shadow += weights[idx] * shadow_map.SampleCmpLevelZero(shadow_sampler, shadow_pos.xy + offset, shadow_pos.z - bias).x;
            idx++;
        }
    }
    
    return shadow / totalWeight;
}

float pcf_shadow_poisson(float3 shadow_pos, sampler_t shadow_sampler, texture2d_t shadow_map, float bias = 0.002, int samples = 16)
{
    float shadow = 0.0;
    float2 texelSize;
    uint width, height;
    shadow_map.GetDimensions(width, height);
    texelSize = float2(1.0 / width, 1.0 / height);
    float2 poissonDisk[16] = {
        float2(-0.94201624, -0.39906216),
        float2(0.94558609, -0.76890725),
        float2(-0.09418410, -0.92938870),
        float2(0.34495938, 0.29387760),
        float2(-0.91588581, 0.45771432),
        float2(-0.81544232, -0.87912464),
        float2(-0.38277543, 0.27676845),
        float2(0.97484398, 0.75648379),
        float2(0.44323325, -0.97511554),
        float2(0.53742981, -0.47373420),
        float2(-0.26496911, -0.41893023),
        float2(0.79197514, 0.19090188),
        float2(-0.24188840, 0.99706507),
        float2(-0.81409955, 0.91437590),
        float2(0.19984126, 0.78641367),
        float2(0.14383161, -0.14100790)
    };

    float randomAngle = frac(sin(dot(shadow_pos.xy, float2(12.9898, 78.233))) * 43758.5453) * 3.14159 * 2.0;
    float s = sin(randomAngle);
    float c = cos(randomAngle);
    
    float radius = 2.5;
    
    for (int i = 0; i < min(samples, 16); i++)
    {
        float2 rotatedOffset = float2(
            poissonDisk[i].x * c - poissonDisk[i].y * s,
            poissonDisk[i].x * s + poissonDisk[i].y * c
        );

        float2 offset = rotatedOffset * texelSize * radius;
        shadow += shadow_map.SampleCmpLevelZero(shadow_sampler, shadow_pos.xy + offset, shadow_pos.z - bias).x;
    }
    
    return shadow / float(min(samples, 16));
}

float pcf_shadow_poisson_weighted(float3 shadow_pos, sampler_t shadow_sampler, texture2d_t shadow_map, float bias = 0.002)
{
    float shadow = 0.0;
    float2 texelSize;
    uint width, height;
    shadow_map.GetDimensions(width, height);
    texelSize = float2(1.0 / width, 1.0 / height);
    float2 poissonDisk[12] = {
        float2(-0.326212, -0.40581),
        float2(-0.840144, -0.07358),
        float2(-0.695914, 0.457137),
        float2(-0.203345, 0.620716),
        float2(0.96234, -0.194983),
        float2(0.473434, -0.480026),
        float2(0.519456, 0.767022),
        float2(0.185461, -0.893124),
        float2(0.507431, 0.064425),
        float2(0.89642, 0.412458),
        float2(-0.32194, -0.932615),
        float2(-0.791559, -0.59771)
    };
    
    float weights[12] = {
        0.2,
        0.1,
        0.1,
        0.15,
        0.1,
        0.15,
        0.1,
        0.1,
        0.15,
        0.1,
        0.1,
        0.1
    };
    float totalWeight = 1.35;
    float randomAngle = frac(sin(dot(shadow_pos.xy, float2(12.9898, 78.233))) * 43758.5453) * 3.14159 * 2.0;
    float s = sin(randomAngle);
    float c = cos(randomAngle);
    float radius = 2.0;
    
    for (int i = 0; i < 12; i++)
    {
        float2 rotatedOffset = float2(
            poissonDisk[i].x * c - poissonDisk[i].y * s,
            poissonDisk[i].x * s + poissonDisk[i].y * c
        );
        float2 offset = rotatedOffset * texelSize * radius;
        shadow += weights[i] * shadow_map.SampleCmpLevelZero(shadow_sampler, shadow_pos.xy + offset, shadow_pos.z - bias).x;
    }
    
    return shadow / totalWeight;
}
