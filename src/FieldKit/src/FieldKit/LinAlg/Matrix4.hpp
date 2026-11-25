#pragma once


#include <array>
#include <cmath>

#include <FieldKit/Assertions.hpp>
#include <FieldKit/LinAlg/Vector3.hpp>


/* Column-major matrices acting on column vectors. World/view space is left-handed,
 * +Y is up, and perspective projection maps depth to [0, 1]. Angles are radians.
 */
namespace FK::LinAlg {

struct Matrix4 {
    std::array<float, 16> m_values{};
};

inline Matrix4 operator*(const Matrix4& a, const Matrix4& b) {
    Matrix4 result;

    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            for (int k = 0; k < 4; ++k) {
                result.m_values[column * 4 + row] +=
                    a.m_values[k * 4 + row] * b.m_values[column * 4 + k];
            }
        }
    }

    return result;
}

inline Matrix4 translationScale(Vector3 position, Vector3 scale) {
    Matrix4 result;
    result.m_values[0] = scale.m_x;
    result.m_values[5] = scale.m_y;
    result.m_values[10] = scale.m_z;
    result.m_values[12] = position.m_x;
    result.m_values[13] = position.m_y;
    result.m_values[14] = position.m_z;
    result.m_values[15] = 1;

    return result;
}

/** The basis must be orthonormal. */
inline Matrix4 viewFromBasis(Vector3 position, Vector3 right, Vector3 up, Vector3 forward) {
    Matrix4 result;
    result.m_values = {right.m_x,
                       up.m_x,
                       forward.m_x,
                       0,
                       right.m_y,
                       up.m_y,
                       forward.m_y,
                       0,
                       right.m_z,
                       up.m_z,
                       forward.m_z,
                       0,
                       -dot(right, position),
                       -dot(up, position),
                       -dot(forward, position),
                       1};

    return result;
}

inline Matrix4 perspective(float vertical_fov, float aspect, float near_plane, float far_plane) {
    FK_ASSERT(vertical_fov > 0 && vertical_fov < 3.14159265F);
    FK_ASSERT(aspect > 0 && near_plane > 0 && far_plane > near_plane);

    const float focal_length = 1.0F / std::tan(vertical_fov * 0.5F);
    Matrix4 result;
    result.m_values[0] = focal_length / aspect;
    result.m_values[5] = focal_length;
    result.m_values[10] = far_plane / (far_plane - near_plane);
    result.m_values[11] = 1;
    result.m_values[14] = -near_plane * far_plane / (far_plane - near_plane);

    return result;
}

} // namespace FK::LinAlg
