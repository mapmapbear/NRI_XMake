RWTexture2D<float4> Output : register(u0);
Texture2D Input : register(t0);

[numthreads(16, 16, 1)]
void main(uint3 DTid : SV_DispatchThreadID) {
    float4 color = Input[DTid.xy];
    Output[DTid.xy] = color;
}