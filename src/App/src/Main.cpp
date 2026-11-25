#include <iostream>
#include <limits>
#include <string_view>
#include <array>
#include <charconv>
#include <cmath>
#include <iomanip>

#include <FieldKit/LinAlg/Vector3.hpp>
#include <FieldKit/Profiling.hpp>
#include <Framework/Application.hpp>
#include <aisdar/application.hpp>


namespace {

struct Options {
    unsigned int m_objects = 10000;
    unsigned int m_moving_percent = 10;
    unsigned int m_frames = 0;
    unsigned int m_warmup = 60;
    bool m_vsync = false;
    bool m_mixed = true;
};

class Benchmark final : public aisdar::Game {
public:
    explicit Benchmark(Options options) : m_options(options), m_moving(m_allocator) {
        m_camera.setPosition({0, 7, -22});
        m_camera.turn(0, -0.16F);
    }

    ~Benchmark() override {
        if (m_samples != 0) {
            std::cout << "Mean phase ms: prepare=" << m_prepare_ms / m_samples
                      << " acquire=" << m_acquire_ms / m_samples
                      << " encode=" << m_encode_ms / m_samples
                      << " submit=" << m_submit_ms / m_samples << '\n';
        }

        if (m_samples == 0) {
            std::cout << "No measured frames (warmup or unavailable swapchain).\n";
            return;
        }

        std::cout << "Instance bytes uploaded during measured frames: " << m_uploaded_bytes << '\n';

        std::cout << std::fixed << std::setprecision(3) << "Measured " << m_samples
                  << " frames: mean " << m_total_seconds * 1000 / m_samples << " ms, min "
                  << m_min_seconds * 1000 << " ms, max " << m_max_seconds * 1000
                  << " ms, throughput " << m_samples / m_total_seconds << " frames/s\n"
                  << "CPU wall time includes presentation waits; not GPU execution time.\n";
    }


    FK::ErrorCodeOr<void> initialize(aisdar::Renderer& renderer) override {
        auto material = renderer.createMaterial({"Shaders/Lit.vert.spv", "Shaders/Lit.frag.spv"});
        if (!material) {
            return material.releaseError();
        }

        m_material = material.value();

        auto alternate = renderer.createMaterial({"Shaders/Lit.vert.spv", "Shaders/Lit.frag.spv"});
        if (!alternate) {
            return alternate.releaseError();
        }

        m_alternate = alternate.value();

        aisdar::MaterialParameters matte;
        matte.m_base_color = {0.12F, 0.65F, 0.8F, 1};
        matte.m_roughness = 0.85F;
        auto matte_set = renderer.setMaterialParameters(m_material, matte);
        if (!matte_set) {
            return matte_set.releaseError();
        }

        aisdar::MaterialParameters metal;
        metal.m_base_color = {0.95F, 0.55F, 0.16F, 1};
        metal.m_roughness = 0.18F;
        metal.m_metallic = 0.9F;
        auto metal_set = renderer.setMaterialParameters(m_alternate, metal);
        if (!metal_set) {
            return metal_set.releaseError();
        }

        aisdar::MaterialParameters emissive;
        emissive.m_base_color = {0.1F, 0.1F, 0.1F, 1};
        emissive.m_emissive_color = {1, 1, 1};
        emissive.m_emissive_intensity = 5;
        auto glow =
            renderer.createMaterial({"Shaders/Lit.vert.spv", "Shaders/Lit.frag.spv", emissive});
        if (!glow) {
            return glow.releaseError();
        }
        m_glow = glow.value();


        auto cube = renderer.createCube();
        if (!cube) {
            return cube.releaseError();
        }

        m_cube = cube.value();

        auto plane = renderer.createPlane();
        if (!plane) {
            return plane.releaseError();
        }

        m_plane = plane.value();

        // An app-authored indexed mesh exercises the same API used by imported assets.
        constexpr unsigned int RINGS = 16;
        constexpr unsigned int SEGMENTS = 24;
        constexpr float PI = 3.14159265F;
        std::array<aisdar::Vertex, (RINGS + 1) * (SEGMENTS + 1)> vertices{};
        std::array<std::uint32_t, RINGS * SEGMENTS * 6> indices{};
        for (unsigned int ring = 0; ring <= RINGS; ++ring) {
            const float phi = PI * static_cast<float>(ring) / RINGS;
            for (unsigned int segment = 0; segment <= SEGMENTS; ++segment) {
                const float theta = 2 * PI * static_cast<float>(segment) / SEGMENTS;
                const FK::LinAlg::Vector3 normal{std::sin(phi) * std::cos(theta), std::cos(phi),
                                                 std::sin(phi) * std::sin(theta)};
                vertices[ring * (SEGMENTS + 1) + segment] = {normal, normal};
            }
        }

        std::size_t next_index = 0;
        for (unsigned int ring = 0; ring < RINGS; ++ring) {
            for (unsigned int segment = 0; segment < SEGMENTS; ++segment) {
                const auto a = ring * (SEGMENTS + 1) + segment;
                const auto b = a + SEGMENTS + 1;
                for (auto index : {a, a + 1, b, a + 1, b + 1, b}) {
                    indices[next_index++] = index;
                }
            }
        }

        auto sphere = renderer.createMesh(vertices, indices);
        if (!sphere) {
            return sphere.releaseError();
        }

        m_sphere = sphere.value();

        auto perspective = m_camera.setPerspective(1.2F, 0.1F, 1500);
        if (!perspective) {
            return perspective.releaseError();
        }

        auto scene = buildScene(renderer);
        if (!scene) {
            return scene.releaseError();
        }

        std::cout << "Moving fraction: " << m_options.m_moving_percent << "%\n";
        std::cout << "Benchmark: " << m_options.m_objects
                  << " objects + ground/material samples/light markers, "
                  << (m_options.m_mixed ? "cubes/spheres" : "cubes") << ", three materials, "
                  << (m_options.m_vsync ? "vsync" : "immediate presentation") << ", warmup "
                  << m_options.m_warmup << " frames\n";

        return {};
    }

    FK::ErrorCodeOr<void> update(const aisdar::Input& input, float seconds,
                                 aisdar::Window& window) override {
        FK_PROFILE_ZONE("Benchmark simulation");
        m_time += seconds;
        ++m_revision;

        const bool capture = input.m_focused && input.m_right_mouse;
        if (capture != m_mouse_captured) {
            auto result = window.setMouseCaptured(capture);
            if (!result) {
                return result.releaseError();
            }

            m_mouse_captured = capture;
        }

        if (!input.m_focused) {
            return {};
        }

        if (capture) {
            m_camera.turn(input.m_mouse_x * 0.003F, -input.m_mouse_y * 0.003F);
        }

        using aisdar::Key;
        const FK::LinAlg::Vector3 direction{
            static_cast<float>(input.held(Key::D)) - static_cast<float>(input.held(Key::A)),
            static_cast<float>(input.held(Key::E)) - static_cast<float>(input.held(Key::Q)),
            static_cast<float>(input.held(Key::W)) - static_cast<float>(input.held(Key::S))};
        const float speed = input.held(Key::SHIFT) ? 80.0F : 25.0F;
        m_camera.moveLocal(FK::LinAlg::normalized(direction) * (speed * seconds));

        return {};
    }

    FK::ErrorCodeOr<void> draw(aisdar::Frame& frame) override {
        FK_PROFILE_ZONE("Build moving draw list");
        frame.setCamera(m_camera);
        frame.setRevision(m_revision);

        aisdar::Lighting lighting;
        lighting.m_directional = {{-0.4F, 0.8F, -0.5F}, {1, 0.95F, 0.85F}, 3};
        lighting.m_point_count = 2;
        lighting.m_points[0] = {{-5, 4, -4}, 25, {0.1F, 0.5F, 1}, 180};
        lighting.m_points[1] = {{6 * std::sin(m_time), 4, 2}, 25, {1, 0.15F, 0.05F}, 180};
        auto lit = frame.setLighting(lighting);
        if (!lit) {
            return lit.releaseError();
        }

        for (std::size_t i = 0; i < lighting.m_point_count; ++i) {
            const auto& light = lighting.m_points[i];
            auto marker =
                frame.drawMesh({m_sphere,
                                m_glow,
                                light.m_position,
                                {0.25F, 0.25F, 0.25F},
                                {light.m_color[0], light.m_color[1], light.m_color[2], 1}});
            if (!marker) {
                return marker.releaseError();
            }
        }


        std::size_t index = 0;
        for (auto draw : m_moving.draws()) {
            draw.m_position.m_y +=
                1.5F * std::sin(m_time * 1.8F + static_cast<float>(index++) * 0.07F);
            auto submitted = frame.drawMesh(draw);
            if (!submitted) {
                return submitted.releaseError();
            }
        }

        return {};
    }

    void frameCompleted(const aisdar::FrameReport& report) override {
        if (!report.m_renderer.m_presented) {
            return;
        }

        if (m_presented++ < m_options.m_warmup) {
            return;
        }

        ++m_samples;
        m_total_seconds += report.m_seconds;
        m_uploaded_bytes += report.m_renderer.m_instance_upload_bytes;
        m_prepare_ms += report.m_renderer.m_prepare_ms;
        m_acquire_ms += report.m_renderer.m_acquire_ms;
        m_encode_ms += report.m_renderer.m_encode_ms;
        m_submit_ms += report.m_renderer.m_submit_ms;
        m_min_seconds = std::min(m_min_seconds, report.m_seconds);
        m_max_seconds = std::max(m_max_seconds, report.m_seconds);
        if (m_samples == 1 || m_samples % 120 == 0) {
            std::cout << "frame " << m_samples << ": " << report.m_seconds * 1000 << " ms, "
                      << report.m_renderer.m_draw_calls << " draws, "
                      << report.m_renderer.m_triangles << " triangles, "
                      << report.m_renderer.m_pipeline_binds << " pipeline binds, "
                      << report.m_renderer.m_instance_upload_bytes << " instance bytes uploaded\n";
        }
    }


private:
    FK::ErrorCodeOr<void> buildScene(aisdar::Renderer& renderer) {
        FK_PROFILE_ZONE("Build static and moving world");
        aisdar::Frame frame(m_allocator);
        auto allocated = frame.initialize(m_options.m_objects + 4);
        if (!allocated) {
            return allocated.releaseError();
        }
        auto moving_allocated = m_moving.initialize(m_options.m_objects);
        if (!moving_allocated) {
            return moving_allocated.releaseError();
        }

        auto ground = frame.drawMesh(
            {m_plane, m_material, {0, -2, 250}, {600, 1, 600}, {0.15F, 0.18F, 0.22F, 1}});
        if (!ground) {
            return ground.releaseError();
        }

        const auto side = static_cast<unsigned int>(
            std::ceil(std::sqrt(static_cast<double>(m_options.m_objects))));
        for (unsigned int i = 0; i < m_options.m_objects; ++i) {
            const float x = (static_cast<float>(i % side) - static_cast<float>(side) * 0.5F) * 3.5F;
            const float z = static_cast<float>(i / side) * 3.5F;
            const float y = std::sin(static_cast<float>(i) * 0.07F) * 0.5F;
            const bool sphere = m_options.m_mixed && i % 3 == 0;
            auto& destination = (i * 37) % 100 < m_options.m_moving_percent ? m_moving : frame;
            auto submitted =
                destination.drawMesh({sphere ? m_sphere : m_cube,
                                      i % 2 == 0 ? m_material : m_alternate,
                                      {x, y, z},
                                      {1, 1, 1},
                                      {0.35F + static_cast<float>(i % 7) * 0.08F, 0.65F, 0.8F, 1}});
            if (!submitted) {
                return submitted.releaseError();
            }
        }

        const aisdar::MeshDraw samples[] = {
            {m_sphere, m_material, {-5, 1, -3}, {2, 2, 2}, {1, 1, 1, 1}},
            {m_sphere, m_alternate, {0, 1, -3}, {2, 2, 2}, {1, 1, 1, 1}},
            {m_sphere, m_glow, {5, 1, -3}, {2, 2, 2}, {0.2F, 1, 0.4F, 1}}};
        for (const auto& sample : samples) {
            auto submitted = frame.drawMesh(sample);
            if (!submitted) {
                return submitted.releaseError();
            }
        }

        return renderer.setStaticDraws(frame.draws());
    }


    Options m_options;
    FK::LibCppAllocator m_allocator;
    aisdar::Frame m_moving;
    float m_time = 0;
    std::uint64_t m_revision = 0;
    aisdar::Camera m_camera;
    aisdar::MeshHandle m_cube, m_plane, m_sphere;
    aisdar::MaterialHandle m_material, m_alternate, m_glow;
    bool m_mouse_captured = false;
    unsigned int m_presented = 0;
    unsigned int m_samples = 0;
    double m_prepare_ms = 0;
    double m_acquire_ms = 0;
    double m_encode_ms = 0;
    double m_submit_ms = 0;
    std::uint64_t m_uploaded_bytes = 0;
    double m_total_seconds = 0;
    double m_min_seconds = std::numeric_limits<double>::max();
    double m_max_seconds = 0;
};

} // namespace

int main(int argc, char* argv[]) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string_view argument{argv[i]};
        if (argument == "--version") {
            std::cout << aisdar::build_version() << '\n';

            return 0;
        }

        if (argument == "--help") {
            std::cout << "--objects N (1..100000) --frames N (measured frames; 0 = interactive)\n"
                         "--moving-percent N (0..100; default 10) --warmup N --vsync --cubes-only "
                         "--smoke-test\n"
                         "WASD move, Q/E vertical, Shift faster, RMB look, Esc quit.\n";

            return 0;
        }

        if (argument == "--vsync") {
            options.m_vsync = true;
            continue;
        }

        if (argument == "--cubes-only") {
            options.m_mixed = false;
            continue;
        }

        if (argument == "--smoke-test") {
            options.m_objects = 64;
            options.m_frames = 6;
            options.m_warmup = 2;
            continue;
        }

        unsigned int* destination = nullptr;
        if (argument == "--moving-percent") {
            destination = &options.m_moving_percent;
        }
        if (argument == "--objects") {
            destination = &options.m_objects;
        }

        if (argument == "--frames") {
            destination = &options.m_frames;
        }

        if (argument == "--warmup") {
            destination = &options.m_warmup;
        }

        if (!destination || ++i == argc) {
            std::cerr << "Unknown option or missing value: " << argument << '\n';

            return 1;
        }

        const std::string_view value{argv[i]};
        const auto parsed =
            std::from_chars(value.data(), value.data() + value.size(), *destination);
        if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size()) {
            std::cerr << "Invalid number: " << value << '\n';

            return 1;
        }
    }

    if (options.m_moving_percent > 100 || options.m_objects == 0 || options.m_objects > 100000 ||
        options.m_frames > 1000000 || options.m_warmup > 1000000) {
        std::cerr << "Benchmark option out of range. Use --help.\n";

        return 1;
    }

    aisdar::ApplicationSettings settings;
    settings.m_window.m_title = "Aisdar benchmark | WASD | RMB look | Q/E | Shift | Esc";
    settings.m_vsync = options.m_vsync;
    settings.m_draw_capacity = options.m_objects + 2;
    settings.m_frame_limit = options.m_frames == 0 ? 0 : options.m_warmup + options.m_frames;

    Benchmark benchmark(options);
    aisdar::Application application;
    auto result = application.run(benchmark, settings);
    if (!result) {
        std::cerr << "Benchmark failed.\n";
    }

    return result ? 0 : 1;
}
