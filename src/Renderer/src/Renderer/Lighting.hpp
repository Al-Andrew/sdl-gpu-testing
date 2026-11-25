#pragma once


#include <cstddef>
#include <array>
#include <cmath>

#include <FieldKit/ErrorOr.hpp>
#include <FieldKit/LinAlg/Vector3.hpp>


namespace aisdar {

/** Direction points from the surface toward the light. Colors/radiance are linear. */
struct DirectionalLight {
    FK::LinAlg::Vector3 m_direction{0, 1, 0};
    std::array<float, 3> m_color{1, 1, 1};
    float m_intensity = 0;
};

/** Smooth inverse-square attenuation, with zero contribution beyond radius. */
struct PointLight {
    FK::LinAlg::Vector3 m_position;
    float m_radius = 10;
    std::array<float, 3> m_color{1, 1, 1};
    float m_intensity = 10;
};

/** A small forward-lighting list; no allocations or shadows. */
struct Lighting {
    static constexpr std::size_t MAX_POINT_LIGHTS = 8;

    std::array<float, 3> m_ambient{0.03F, 0.03F, 0.03F};
    DirectionalLight m_directional;
    std::array<PointLight, MAX_POINT_LIGHTS> m_points{};
    std::size_t m_point_count = 0;

    bool isValid() const {
        auto colorValid = [](const auto& color) {
            for (float value : color) {
                if (!std::isfinite(value) || value < 0) {
                    return false;
                }
            }

            return true;
        };
        auto vectorValid = [](FK::LinAlg::Vector3 value) {
            return std::isfinite(value.m_x) && std::isfinite(value.m_y) && std::isfinite(value.m_z);
        };

        if (m_point_count > MAX_POINT_LIGHTS || !colorValid(m_ambient) ||
            !colorValid(m_directional.m_color) || !vectorValid(m_directional.m_direction) ||
            !std::isfinite(m_directional.m_intensity) || m_directional.m_intensity < 0 ||
            (m_directional.m_intensity > 0 &&
             FK::LinAlg::length(m_directional.m_direction) < 0.0001F)) {
            return false;
        }

        for (std::size_t i = 0; i < m_point_count; ++i) {
            const auto& light = m_points[i];
            if (!vectorValid(light.m_position) || !colorValid(light.m_color) ||
                !std::isfinite(light.m_radius) || light.m_radius <= 0 ||
                !std::isfinite(light.m_intensity) || light.m_intensity < 0) {
                return false;
            }
        }

        return true;
    }
};

} // namespace aisdar
