[earlydepthstencil]
float4 main() : SV_Target
{
    half4 h1 = 0.3h;
    half4 h2 = 0.2h;
    half4 h = h1 + h2;
    return float4(h);
}