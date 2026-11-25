#include <limits>
#include <array>
#include <cmath>

#include <FieldKit/Assertions.hpp>
#include <FieldKit/LinAlg/Matrix4.hpp>
#include <Renderer/Frame.hpp>
#include <Renderer/Renderer.hpp>


namespace {

using FK::LinAlg::Matrix4;
using FK::LinAlg::Vector3;

std::array<float, 4> transform(const Matrix4& matrix, std::array<float, 4> point) {
    std::array<float, 4> result{};

    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            result[row] += matrix.m_values[column * 4 + row] * point[column];
        }
    }

    return result;
}

bool near(float a, float b) {
    return std::abs(a - b) < 0.0001F;
}

} // namespace

int main() {
    const auto direction = FK::LinAlg::normalized(Vector3{3, 0, 4});
    FK_VERIFY(near(FK::LinAlg::length(direction), 1));
    FK_VERIFY(FK::LinAlg::length(FK::LinAlg::normalized({})) == 0);

    const auto projection = FK::LinAlg::perspective(1.2F, 2, 0.1F, 200);
    const auto near_point = transform(projection, {0, 0, 0.1F, 1});
    const auto far_point = transform(projection, {0, 0, 200, 1});
    FK_VERIFY(near(near_point[2] / near_point[3], 0));
    FK_VERIFY(near(far_point[2] / far_point[3], 1));
    FK_VERIFY(near(projection.m_values[5] / projection.m_values[0], 2));

    const auto model = FK::LinAlg::translationScale({2, 3, 4}, {2, 3, 4});
    const auto view = FK::LinAlg::viewFromBasis({2, 3, 4}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1});
    const auto point = transform(view * model, {1, 1, 1, 1});
    FK_VERIFY(near(point[0], 2) && near(point[1], 3) && near(point[2], 4));

    aisdar::Camera camera;
    camera.turn(1.57079633F, 0);
    camera.moveLocal({0, 0, 2});
    const auto ahead = transform(camera.viewProjection(1), {3, 0, 0, 1});
    FK_VERIFY(near(ahead[0], 0) && near(ahead[1], 0) && near(ahead[3], 1));

    FK::LibCppAllocator allocator;
    aisdar::Renderer owner;
    aisdar::Frame frame(allocator);
    auto initialized = frame.initialize(32);
    FK_VERIFY(initialized);
    aisdar::MeshDraw cube{{&owner, 1}, {&owner, 1}};
    for (std::size_t i = 0; i < 32; ++i) {
        const auto submitted = frame.drawMesh(cube);
        FK_VERIFY(submitted);
    }

    auto overflow = frame.drawMesh(cube);
    FK_VERIFY(overflow.isError());
    FK_VERIFY(overflow.releaseError() == FK::ErrorCode::OVERFLOW);
    FK_VERIFY(frame.draws().size() == 32);

    frame.reset();
    FK_VERIFY(frame.draws().empty());
    cube.m_scale.m_x = -1;
    auto invalid = frame.drawMesh(cube);
    FK_VERIFY(invalid.isError());
    FK_VERIFY(frame.draws().empty());

    aisdar::MaterialParameters material;
    FK_VERIFY(material.isValid());
    material.m_roughness = -0.1F;
    FK_VERIFY(!material.isValid());
    material.m_roughness = 0;
    material.m_reflectivity = std::numeric_limits<float>::quiet_NaN();
    FK_VERIFY(!material.isValid());

    aisdar::Lighting lighting;
    lighting.m_point_count = 1;
    auto lighting_set = frame.setLighting(lighting);
    FK_VERIFY(lighting_set);
    lighting.m_points[0].m_radius = 0;
    auto invalid_light = frame.setLighting(lighting);
    FK_VERIFY(invalid_light.isError());
    FK_VERIFY(frame.lighting().m_points[0].m_radius == 10);
    lighting.m_point_count = aisdar::Lighting::MAX_POINT_LIGHTS + 1;
    FK_VERIFY(!lighting.isValid());
    lighting = {};
    lighting.m_directional.m_direction = {};
    lighting.m_directional.m_intensity = 1;
    FK_VERIFY(!lighting.isValid());

    frame.reset();
    FK_VERIFY(frame.lighting().m_point_count == 0);

    return 0;
}
