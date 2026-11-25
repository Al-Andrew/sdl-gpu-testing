#pragma once


#include <cstdint>
#include <array>
#include <span>

#include <FieldKit/ErrorOr.hpp>
#include <Platform/Window.hpp>
#include <Renderer/Frame.hpp>
#include <Renderer/Material.hpp>


/*
 * SDL GPU renderer for opaque indexed meshes and app-provided shader pairs. The application
 * supplies camera and draw data; this module owns GPU resources and submission.
 */
struct SDL_GPUDevice;
struct SDL_GPUGraphicsPipeline;
struct SDL_GPUBuffer;
struct SDL_GPUTexture;
struct SDL_GPUTransferBuffer;

namespace aisdar {

/** Shader ABI: SPIR-V, entry point main, position/normal at vertex locations 0/1. Instance
 * position/scale/color at 2/3/4. Vertex uniform slot 0 holds a column-major view-projection matrix
 * then reserved float4 (80 bytes). Vertex outputs: color at 0, world position at 1, normal at 2.
 * Fragment slot 0 / space3 b0: camera/count, ambient, directional direction/intensity,
 * directional RGB, then 8 point lights (position/radius, RGB/intensity), all float4 (320 bytes).
 * Fragment slot 1 / space3 b1: base RGBA, emissive RGB/intensity, roughness/metallic/F0/pad
 * (48 bytes). No textures yet. Paths are relative to the executable directory. Source shaders
 * belong to the app.
 */
struct MaterialSettings {
    const char* m_vertex_shader;
    const char* m_fragment_shader;
    MaterialParameters m_parameters;
};

struct RenderStatistics {
    std::uint32_t m_draw_calls = 0;
    std::uint32_t m_pipeline_binds = 0;
    std::uint64_t m_triangles = 0;
    std::uint32_t m_static_instances = 0;
    std::uint32_t m_dynamic_instances = 0;
    std::uint32_t m_instance_upload_bytes = 0;
    bool m_presented = false;
    double m_prepare_ms = 0;
    double m_acquire_ms = 0;
    double m_encode_ms = 0;
    double m_submit_ms = 0;
};

/** Owns meshes and materials until destruction. The borrowed window must outlive it.
 * SDL owns native allocations. Resource tables have limits of 64 meshes/32 materials;
 * creation returns OVERFLOW when full. CPU mesh spans are borrowed only during upload.
 */
class Renderer {
public:
    Renderer() = default;
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;


    /** Call once with a live window, on the SDL main thread. */
    FK::ErrorCodeOr<void> initialize(Window& window, bool vsync = true);
    /** Requires successful initialization. Batches opaque draws by material/mesh without modifying
     * submission order.
     */
    FK::ErrorCodeOr<void> render(Frame& frame);

    /** Replaces the retained opaque world; uploads once and draws it until replaced.
     * Empty input clears it. Failure preserves the previous world. Input is borrowed for this call.
     */
    FK::ErrorCodeOr<void> setStaticDraws(std::span<const MeshDraw> draws);

    FK::ErrorCodeOr<MeshHandle> createMesh(std::span<const Vertex> vertices,
                                           std::span<const std::uint32_t> indices);
    FK::ErrorCodeOr<MeshHandle> createCube();
    FK::ErrorCodeOr<MeshHandle> createPlane();
    FK::ErrorCodeOr<MaterialHandle> createMaterial(const MaterialSettings& settings);
    /** Changes uniforms only; existing static/dynamic instances need no re-upload. */
    FK::ErrorCodeOr<void> setMaterialParameters(MaterialHandle material,
                                                const MaterialParameters& parameters);

    const RenderStatistics& statistics() const {
        return m_statistics;
    }


private:
    struct Mesh {
        SDL_GPUBuffer* m_vertices = nullptr;
        SDL_GPUBuffer* m_indices = nullptr;
        std::uint32_t m_index_count = 0;
    };

    static constexpr std::size_t MAX_MESHES = 64;
    static constexpr std::size_t MAX_MATERIALS = 32;
    std::array<Mesh, MAX_MESHES> m_meshes{};
    std::array<SDL_GPUGraphicsPipeline*, MAX_MATERIALS> m_materials{};
    std::size_t m_mesh_count = 0;
    std::size_t m_material_count = 0;
    std::array<MaterialParameters, MAX_MATERIALS> m_material_parameters{};
    RenderStatistics m_statistics;

    SDL_Window* m_window = nullptr;
    SDL_GPUDevice* m_device = nullptr;
    SDL_GPUTexture* m_depth = nullptr;
    static constexpr auto BATCH_COUNT = MAX_MESHES * MAX_MATERIALS;
    std::array<std::uint32_t, BATCH_COUNT> m_static_counts{};
    std::array<std::uint32_t, BATCH_COUNT> m_static_starts{};
    std::array<std::uint32_t, BATCH_COUNT> m_dynamic_counts{};
    std::array<std::uint32_t, BATCH_COUNT> m_dynamic_starts{};
    std::optional<std::uint64_t> m_uploaded_revision;
    SDL_GPUBuffer* m_static_instances = nullptr;
    std::uint32_t m_static_count = 0;
    SDL_GPUBuffer* m_instances = nullptr;
    SDL_GPUTransferBuffer* m_upload = nullptr;
    std::uint32_t m_instance_capacity = 0;
    std::uint32_t m_width = 0;
    std::uint32_t m_height = 0;
};

} // namespace aisdar
