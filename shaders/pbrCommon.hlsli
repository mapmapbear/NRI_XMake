
static const float c_pi = 3.14159265359;
static const float c_F0 = 0.04f;

float DistributionGGX(float3 N, float3 H, float roughness)
{
    float a      = roughness*roughness;
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0f);
    float NdotH2 = NdotH*NdotH;
  
    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0f) + 1.0f);
    denom = c_pi * denom * denom;
  
    return nom / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0f);
    float k = (r*r) / 8.0f;

    float nom   = NdotV;
    float denom = NdotV * (1.0f - k) + k;
  
    return nom / denom;
}
float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0f);
    float NdotL = max(dot(N, L), 0.0f);
    float ggx2  = GeometrySchlickGGX(NdotV, roughness);
    float ggx1  = GeometrySchlickGGX(NdotL, roughness);
  
    return ggx1 * ggx2;
}

float3 fresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
{
    return F0 + (max(float3((1.0 - roughness).xxx), F0) - F0) * pow(1.0f - cosTheta, 5.0f);
}  

float3 fresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0f - F0) * pow(1.0 - cosTheta, 5.0f);
}

float3 PositionalLight (float3 worldPos, float3 N, float3 V, float3 lightPos, float3 lightColor, float3 albedo, float metallic, float roughness, float scalarF0)
{
    roughness = clamp(roughness, 0.01f, 0.99f);

    float distance    = length(lightPos - worldPos);
    float attenuation = 1.0 / (distance * distance);
    float3 L = normalize(lightPos - worldPos);

    float3 H = normalize(V + L);

    float3 radiance     = lightColor * attenuation;

    float3 F0 = float3(scalarF0, scalarF0, scalarF0); 
    F0      = lerp(F0, albedo, metallic);
    float3 F  = fresnelSchlick(max(dot(H, V), 0.0f), F0);

    float NDF = DistributionGGX(N, H, roughness);
    float G   = GeometrySmith(N, V, L, roughness);

    float3 nominator    = NDF * G * F;
    float denominator = 4.0f * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.001f;
    float3 specular     = nominator / denominator;

    float3 kS = F;
    float3 kD = float3(1.0f, 1.0f, 1.0f) - kS;

    kD *= 1.0f - metallic;

    float NdotL = max(dot(N, L), 0.0f);
    return (kD * albedo / c_pi + specular) * radiance * NdotL;
}

float3 DirectionalLight (float3 worldPos, float3 N, float3 V, float3 lightDir, float3 lightColor, float3 albedo, float metallic, float roughness, float scalarF0)
{
    roughness = clamp(roughness, 0.01f, 0.99f);

    float3 L = normalize(lightDir);

    float3 H = normalize(V + L);

    float3 radiance     = lightColor;

    float3 F0 = float3(scalarF0, scalarF0, scalarF0);
    F0      = lerp(F0, albedo, metallic);
    float3 F  = fresnelSchlick(max(dot(H, V), 0.0f), F0);

    float NDF = DistributionGGX(N, H, roughness);
    float G   = GeometrySmith(N, V, L, roughness);

    float3 nominator    = NDF * G * F;
    float denominator = 4.0f * max(dot(N, V), 0.0) * max(dot(N, L), 0.0f) + 0.001f;
    float3 specular     = nominator / denominator;

    float3 kS = F;
    float3 kD = float3(1.0f, 1.0f, 1.0f) - kS;

    kD *= 1.0 - metallic;

    float NdotL = max(dot(N, L), 0.0f);
    return (kD * albedo / c_pi + specular) * radiance * NdotL;
}


float3 IBL (float3 N, float3 V, float3 R, float3 albedo, float metallic, float roughness, float scalarF0, in Texture2D<float4> SplitSum, in TextureCube<float4> DiffuseIBL, in TextureCube<float4> SpecularIBL, in SamplerState LinearWrap, in SamplerState LinearClamp)
{
    roughness = clamp(roughness, 0.01f, 0.99f);
    
    float3 ambient = 0.0;
    float3 F0 = float3(scalarF0.xxx);
    F0 = lerp(F0, albedo, metallic);
    
    float3 irradiance = DiffuseIBL.SampleLevel(LinearWrap, N, 0).rgb;
    irradiance = pow(irradiance, 1.0 / 2.2);
    float2 f_ab = SplitSum.Sample(LinearClamp, float2(max(dot(N, V), 0.0), roughness)).rg;
    f_ab = pow(f_ab, 1.0 / 2.2);

    uint width, height, specularTextureLevels;
	SpecularIBL.GetDimensions(0, width, height, specularTextureLevels);

    float3 radiance = SpecularIBL.SampleLevel(LinearWrap, R, round(roughness * specularTextureLevels)).rgb;
    radiance = pow(radiance, 1.0/2.2);
    float3 kS = F0;
    float3 FssEss = kS * f_ab.x + f_ab.y;
    
    float3 c_diff = albedo;
    // Multiple scattering, from Fdez-Aguera
    float Ems = (1.0 - (f_ab.x + f_ab.y));
    float3 F_avg = F0 + (1.0 - F0) / 21.0;
    float3 FmsEms = Ems * FssEss * F_avg / (1.0 - F_avg * Ems);
    float3 k_D = c_diff * (1.0 - FssEss - FmsEms);
    ambient = FssEss * radiance + (FmsEms + k_D) * irradiance;

    return ambient;
}