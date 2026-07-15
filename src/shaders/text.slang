struct UniformBuffer {
    float4x4 projection;
};
ConstantBuffer<UniformBuffer> ubo;
Sampler2D atlas_sampler;

struct VIn {
    float2 position;
    float3 color;
    float2 uv;
}

struct VOut {
    float4 position : SV_Position;
    float3 color;
    float2 uv;
};

[shader("vertex")]
VOut vertMain(VIn input) {
    VOut output;
    output.position = mul(ubo.projection, float4(input.position, 0., 1.));
    output.color = input.color;
    output.uv = input.uv;
    return output;
}

[shader("fragment")]
float4 fragMain(VOut input) : SV_Target
{
    float bitmap = atlas_sampler.Sample(input.uv).r;
    return float4(input.color.rgb, bitmap);
}
