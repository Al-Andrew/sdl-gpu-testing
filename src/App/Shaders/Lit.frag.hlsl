struct PointLight {
    float4 m_position_radius;
    float4 m_color_intensity;
};

cbuffer Lighting : register(b0, space3) {
    float4 camera_position_count;
    float4 ambient;
    float4 sun_direction_intensity;
    float4 sun_color;
    PointLight point_lights[8];
};

cbuffer Material : register(b1, space3) {
    float4 base_color;
    float4 emissive_color_intensity;
    float4 surface;
};

static const float PI = 3.14159265;

float3 safeNormalize(float3 value) {
    return value * rsqrt(max(dot(value, value), 0.000001));
}

// GGX direct specular, Schlick Fresnel, and Lambert diffuse. No environment reflections.
float3 evaluateLight(float3 normal, float3 view, float3 light, float3 radiance, float3 albedo) {
    float n_dot_l = saturate(dot(normal, light));
    float n_dot_v = saturate(dot(normal, view));
    if (n_dot_l <= 0 || n_dot_v <= 0) {
        return 0;
    }

    float roughness = max(surface.x, 0.045);
    float metallic = surface.y;
    float3 half_vector = safeNormalize(view + light);
    float n_dot_h = saturate(dot(normal, half_vector));
    float v_dot_h = saturate(dot(view, half_vector));
    float alpha = roughness * roughness;
    float alpha_squared = alpha * alpha;
    float denominator = n_dot_h * n_dot_h * (alpha_squared - 1) + 1;
    float distribution = alpha_squared / max(PI * denominator * denominator, 0.000000000001);

    float k = (roughness + 1) * (roughness + 1) / 8;
    float geometry_view = n_dot_v / (n_dot_v * (1 - k) + k);
    float geometry_light = n_dot_l / (n_dot_l * (1 - k) + k);
    float3 f0 = lerp(surface.zzz, albedo, metallic);
    float3 fresnel = f0 + (1 - f0) * pow(1 - v_dot_h, 5);
    float3 specular = distribution * geometry_view * geometry_light * fresnel /
                      max(4 * n_dot_v * n_dot_l, 0.0001);
    float3 diffuse = (1 - fresnel) * (1 - metallic) * albedo / PI;

    return (diffuse + specular) * radiance * n_dot_l;
}

float4 main(float3 tint : TEXCOORD0, float3 world_position : TEXCOORD1,
            float3 world_normal : TEXCOORD2) : SV_Target0 {
    float3 normal = safeNormalize(world_normal);
    float3 view = safeNormalize(camera_position_count.xyz - world_position);
    float3 albedo = base_color.rgb * tint;
    float3 radiance = ambient.rgb * albedo;

    radiance += evaluateLight(normal, view, sun_direction_intensity.xyz,
                              sun_color.rgb * sun_direction_intensity.w, albedo);
    for (int i = 0; i < (int)camera_position_count.w; ++i) {
        float3 offset = point_lights[i].m_position_radius.xyz - world_position;
        float distance_squared = dot(offset, offset);
        float radius = point_lights[i].m_position_radius.w;
        float cutoff = saturate(1 - distance_squared / (radius * radius));
        float attenuation = cutoff * cutoff / max(distance_squared, 0.01);
        float3 light_radiance = point_lights[i].m_color_intensity.rgb *
                                point_lights[i].m_color_intensity.w * attenuation;
        radiance += evaluateLight(normal, view, safeNormalize(offset), light_radiance, albedo);
    }

    radiance += emissive_color_intensity.rgb * emissive_color_intensity.w * tint;

    // Simple display transform for the SDR UNORM swapchain.
    float3 mapped = max(radiance, 0) / (1 + max(radiance, 0));
    float3 srgb = lerp(12.92 * mapped, 1.055 * pow(mapped, 1.0 / 2.4) - 0.055,
                      step(0.0031308, mapped));

    return float4(srgb, 1);
}
