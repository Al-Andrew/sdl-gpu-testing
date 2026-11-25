#pragma once


#include <algorithm>
#include <cmath>

#include <FieldKit/ErrorOr.hpp>
#include <FieldKit/LinAlg/Matrix4.hpp>


namespace aisdar {

class Camera {
public:
    void setPosition(FK::LinAlg::Vector3 position) {
        m_position = position;
    }

    /** Vertical field of view is in radians; near/far distances must be positive. */
    FK::ErrorCodeOr<void> setPerspective(float vertical_fov, float near_plane, float far_plane) {
        if (!std::isfinite(vertical_fov) || !std::isfinite(near_plane) ||
            !std::isfinite(far_plane) || vertical_fov <= 0 || vertical_fov >= 3.14159265F ||
            near_plane <= 0 || far_plane <= near_plane) {
            return FK::ErrorCode::INVALID_ARGUMENT;
        }

        m_vertical_fov = vertical_fov;
        m_near_plane = near_plane;
        m_far_plane = far_plane;

        return {};
    }

    void turn(float yaw_delta, float pitch_delta) {
        m_yaw += yaw_delta;
        m_pitch = std::clamp(m_pitch + pitch_delta, -1.5F, 1.5F);
    }

    /** Movement follows yaw on the ground plane; Y remains world up. */
    void moveLocal(FK::LinAlg::Vector3 displacement) {
        const float cos_yaw = std::cos(m_yaw);
        const float sin_yaw = std::sin(m_yaw);

        m_position = m_position +
                     FK::LinAlg::Vector3{displacement.m_x * cos_yaw + displacement.m_z * sin_yaw,
                                         displacement.m_y,
                                         -displacement.m_x * sin_yaw + displacement.m_z * cos_yaw};
    }

    FK::LinAlg::Vector3 position() const {
        return m_position;
    }

    FK::LinAlg::Matrix4 viewProjection(float aspect) const {
        const float cos_yaw = std::cos(m_yaw);
        const float sin_yaw = std::sin(m_yaw);
        const float cos_pitch = std::cos(m_pitch);
        const float sin_pitch = std::sin(m_pitch);

        const FK::LinAlg::Vector3 right{cos_yaw, 0, -sin_yaw};
        const FK::LinAlg::Vector3 up{-sin_yaw * sin_pitch, cos_pitch, -cos_yaw * sin_pitch};
        const FK::LinAlg::Vector3 forward{sin_yaw * cos_pitch, sin_pitch, cos_yaw * cos_pitch};
        const auto view = FK::LinAlg::viewFromBasis(m_position, right, up, forward);

        return FK::LinAlg::perspective(m_vertical_fov, aspect, m_near_plane, m_far_plane) * view;
    }


private:
    FK::LinAlg::Vector3 m_position;
    float m_yaw = 0;
    float m_pitch = 0;
    float m_vertical_fov = 1.2F;
    float m_near_plane = 0.1F;
    float m_far_plane = 200.0F;
};

} // namespace aisdar
