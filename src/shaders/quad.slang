struct UniformBuffer {
    float4x4 projection;
};
ConstantBuffer<UniformBuffer> ubo;
Sampler2D atlas_sampler;

struct VIn {
    [[vk::location(0)]]
    float2 vPosition;
    [[vk::location(1)]]
    float2 uv;
    
    [[vk::location(2)]]
    float2 iPosition;
    [[vk::location(3)]]
    float2 size;
    [[vk::location(4)]]
    float2 uvMin;
    [[vk::location(5)]]
    float2 uvMax;
    [[vk::location(6)]]
    float4 color;
}

struct VOut {
    float4 position : SV_Position;
    float4 color;
    float2 uv;
};

[shader("vertex")]
VOut vertMain(VIn input) {
    VOut output;
    float2 position = input.iPosition + input.vPosition * input.size;
    output.position = mul(ubo.projection, float4(position, 0., 1.));
    output.color = input.color;
    output.uv = lerp(input.uvMin, input.uvMax, input.uv);
    return output;
}

[shader("fragment")]
float4 fragMain(VOut input) : SV_Target
{
    float bitmap = atlas_sampler.Sample(input.uv).r;
    return float4(input.color.rgb, bitmap * input.color.a);
}
