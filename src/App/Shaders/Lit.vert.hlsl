cbuffer Frame : register(b0, space1) {
    column_major float4x4 transform;
    float4 reserved;
};

struct Input {
    float3 m_position : TEXCOORD0;
    float3 m_normal : TEXCOORD1;
    float3 m_translation : TEXCOORD2;
    float3 m_scale : TEXCOORD3;
    float4 m_color : TEXCOORD4;
};

struct Output {
    float4 m_position : SV_Position;
    float3 m_color : TEXCOORD0;
    float3 m_world_position : TEXCOORD1;
    float3 m_normal : TEXCOORD2;
};

Output main(Input input) {
    Output output;
    output.m_world_position = input.m_position * input.m_scale + input.m_translation;
    output.m_position = mul(transform, float4(output.m_world_position, 1));
    output.m_normal = normalize(input.m_normal / input.m_scale);
    output.m_color = input.m_color.rgb;

    return output;
}
