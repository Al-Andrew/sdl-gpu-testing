#pragma once


#include <cmath>


namespace FK::LinAlg {

struct Vector3 {
    float m_x = 0;
    float m_y = 0;
    float m_z = 0;
};

constexpr Vector3 operator+(Vector3 a, Vector3 b) {
    return {a.m_x + b.m_x, a.m_y + b.m_y, a.m_z + b.m_z};
}

constexpr Vector3 operator*(Vector3 value, float scalar) {
    return {value.m_x * scalar, value.m_y * scalar, value.m_z * scalar};
}

constexpr float dot(Vector3 a, Vector3 b) {
    return a.m_x * b.m_x + a.m_y * b.m_y + a.m_z * b.m_z;
}

inline float length(Vector3 value) {
    return std::sqrt(dot(value, value));
}

/** Zero vectors remain zero. */
inline Vector3 normalized(Vector3 value) {
    const float magnitude = length(value);
    if (magnitude == 0) {
        return {};
    }

    return value * (1.0F / magnitude);
}

} // namespace FK::LinAlg
