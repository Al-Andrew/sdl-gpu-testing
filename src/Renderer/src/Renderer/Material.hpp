#pragma once


#include <array>
#include <cmath>


namespace aisdar {

/** Linear RGB. Instance tint multiplies base color; alpha is reserved (opaque rendering).
 * Reflectivity is dielectric normal-incidence reflectance (F0), not a reflection texture.
 * Metallic blends F0 toward base color; roughness controls highlight spread.
 * Emissive RGB times intensity adds radiance without illuminating other objects.
 */
struct MaterialParameters {
    std::array<float, 4> m_base_color{1, 1, 1, 1};
    std::array<float, 3> m_emissive_color{0, 0, 0};
    float m_emissive_intensity = 0;
    float m_roughness = 0.5F;
    float m_metallic = 0;
    float m_reflectivity = 0.04F;

    bool isValid() const {
        for (float value : m_base_color) {
            if (!std::isfinite(value) || value < 0 || value > 1) {
                return false;
            }
        }

        for (float value : m_emissive_color) {
            if (!std::isfinite(value) || value < 0) {
                return false;
            }
        }

        return std::isfinite(m_emissive_intensity) && m_emissive_intensity >= 0 &&
               std::isfinite(m_roughness) && m_roughness >= 0 && m_roughness <= 1 &&
               std::isfinite(m_metallic) && m_metallic >= 0 && m_metallic <= 1 &&
               std::isfinite(m_reflectivity) && m_reflectivity >= 0 && m_reflectivity <= 1;
    }
};

} // namespace aisdar
