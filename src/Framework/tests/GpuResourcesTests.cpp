#include <FieldKit/Assertions.hpp>
#include <Framework/Application.hpp>


namespace {

class ResourceTest final : public aisdar::Game {
public:
    FK::ErrorCodeOr<void> initialize(aisdar::Renderer& renderer) override {
        const aisdar::Vertex vertices[] = {{{0, 0, 0}, {0, 1, 0}}};
        const std::uint32_t indices[] = {0, 1, 0};
        auto invalid = renderer.createMesh(vertices, indices);
        FK_VERIFY(invalid.isError());
        FK_VERIFY(invalid.releaseError() == FK::ErrorCode::OUT_OF_RANGE);

        auto missing =
            renderer.createMaterial({"Shaders/DoesNotExist.spv", "Shaders/Lit.frag.spv"});
        FK_VERIFY(missing.isError());

        auto mesh = renderer.createCube();
        if (!mesh) {
            return mesh.releaseError();
        }

        auto material = renderer.createMaterial({"Shaders/Lit.vert.spv", "Shaders/Lit.frag.spv"});
        if (!material) {
            return material.releaseError();
        }

        aisdar::MaterialParameters parameters;
        parameters.m_roughness = 0.2F;
        parameters.m_metallic = 0.8F;
        auto edited = renderer.setMaterialParameters(material.value(), parameters);
        FK_VERIFY(edited);
        parameters.m_emissive_intensity = -1;
        auto rejected_material = renderer.setMaterialParameters(material.value(), parameters);
        FK_VERIFY(rejected_material.isError());
        auto rejected_handle = renderer.setMaterialParameters({}, {});
        FK_VERIFY(rejected_handle.isError());

        m_draw = {mesh.value(), material.value(), {0, 0, 5}};
        m_run_frame = 0;
        auto retained = renderer.setStaticDraws({&m_draw, 1});
        FK_VERIFY(retained);
        auto cleared = renderer.setStaticDraws({});
        FK_VERIFY(cleared);
        auto replaced = renderer.setStaticDraws({&m_draw, 1});
        FK_VERIFY(replaced);

        auto invalid_draw = m_draw;
        invalid_draw.m_mesh.m_index = 999;
        auto rejected = renderer.setStaticDraws({&invalid_draw, 1});
        FK_VERIFY(rejected.isError());

        return {};
    }

    FK::ErrorCodeOr<void> update(const aisdar::Input&, float, aisdar::Window&) override {
        return {};
    }

    FK::ErrorCodeOr<void> draw(aisdar::Frame& frame) override {
        if (m_fail_draw) {
            return FK::ErrorCode::INVALID_ARGUMENT;
        }

        // Light changes are independent of cached instance revisions.
        aisdar::Lighting lighting;
        lighting.m_directional.m_intensity = 2;
        lighting.m_point_count = 1;
        lighting.m_points[0].m_position = {static_cast<float>(m_run_frame), 2, 3};
        auto lighting_set = frame.setLighting(lighting);
        FK_VERIFY(lighting_set);

        frame.setRevision(m_run_frame / 2);
        if (m_run_frame >= 4) {
            return {};
        }

        auto draw = m_draw;
        draw.m_position.m_y = static_cast<float>(m_run_frame / 2);
        auto first = frame.drawMesh(draw);
        if (!first) {
            return first.releaseError();
        }

        return frame.drawMesh(draw);
    }

    void frameCompleted(const aisdar::FrameReport& report) override {
        if (report.m_renderer.m_presented) {
            FK_VERIFY(report.m_renderer.m_draw_calls == (m_run_frame < 4 ? 2 : 1));
            FK_VERIFY(report.m_renderer.m_static_instances == 1);
            FK_VERIFY(report.m_renderer.m_dynamic_instances == (m_run_frame < 4 ? 2 : 0));
            FK_VERIFY(report.m_renderer.m_instance_upload_bytes ==
                      (m_run_frame < 4 && m_run_frame % 2 == 0 ? 80 : 0));
            FK_VERIFY(report.m_renderer.m_pipeline_binds == 1);
            FK_VERIFY(report.m_renderer.m_triangles == (m_run_frame < 4 ? 36 : 12));
            ++m_frames;
            ++m_run_frame;
        }
    }


    bool m_fail_draw = false;
    unsigned int m_frames = 0;


private:
    aisdar::MeshDraw m_draw;
    unsigned int m_run_frame = 0;
};

} // namespace

int main() {
    aisdar::Application application;
    aisdar::ApplicationSettings settings;
    settings.m_frame_limit = 6;
    settings.m_draw_capacity = 2;
    ResourceTest game;

    auto first = application.run(game, settings);
    FK_VERIFY(first);
    FK_VERIFY(game.m_frames == 6);

    game.m_fail_draw = true;
    auto failed = application.run(game, settings);
    FK_VERIFY(failed.isError());
    FK_VERIFY(failed.releaseError() == FK::ErrorCode::INVALID_ARGUMENT);

    game.m_fail_draw = false;
    auto recovered = application.run(game, settings);
    FK_VERIFY(recovered);
    FK_VERIFY(game.m_frames == 12);

    return 0;
}
