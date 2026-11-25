#include <cstddef>
#include <cstring>
#include <limits>
#include <algorithm>
#include <cmath>

#include <SDL3/SDL.h>

#include <FieldKit/BuildMode.hpp>
#include <FieldKit/Logging.hpp>
#include <FieldKit/Profiling.hpp>
#include <Renderer/Renderer.hpp>


namespace aisdar {

namespace {

using FK::LinAlg::Vector3;
using Matrix = FK::LinAlg::Matrix4;

struct Instance {
    Vector3 m_position;
    Vector3 m_scale;
    std::array<float, 4> m_color;
};

static_assert(sizeof(Instance) == 40);

struct Uniforms {
    Matrix m_transform;
    std::array<float, 4> m_color;
};

static_assert(sizeof(Vertex) == 24);
static_assert(sizeof(Uniforms) == 80);

FK::ErrorCode failure() {
    FK_LOG_ERROR("SDL GPU: {}", SDL_GetError());

    return FK::ErrorCode::UNSPECIFIED;
}

FK::ErrorCodeOr<SDL_GPUShader*> loadShader(SDL_GPUDevice* device, const char* path,
                                           SDL_GPUShaderStage stage) {
    std::size_t size = 0;
    std::array<char, 4096> full_path{};
    const auto written =
        SDL_snprintf(full_path.data(), full_path.size(), "%s%s", SDL_GetBasePath(), path);
    if (written < 0 || static_cast<std::size_t>(written) >= full_path.size()) {
        return FK::ErrorCode::OUT_OF_RANGE;
    }

    void* code = SDL_LoadFile(full_path.data(), &size);
    if (!code) {
        return failure();
    }

    SDL_GPUShaderCreateInfo info{};
    info.code = static_cast<const Uint8*>(code);
    info.code_size = size;
    info.entrypoint = "main";
    info.format = SDL_GPU_SHADERFORMAT_SPIRV;
    info.stage = stage;
    info.num_uniform_buffers = stage == SDL_GPU_SHADERSTAGE_VERTEX ? 1 : 2;

    auto* shader = SDL_CreateGPUShader(device, &info);
    SDL_free(code);
    if (!shader) {
        return failure();
    }

    return shader;
}

} // namespace

Renderer::~Renderer() {
    if (!m_device) {
        return;
    }

    SDL_WaitForGPUIdle(m_device);
    if (m_static_instances) {
        SDL_ReleaseGPUBuffer(m_device, m_static_instances);
    }

    if (m_upload) {
        SDL_ReleaseGPUTransferBuffer(m_device, m_upload);
    }

    if (m_instances) {
        SDL_ReleaseGPUBuffer(m_device, m_instances);
    }

    if (m_depth) {
        SDL_ReleaseGPUTexture(m_device, m_depth);
    }

    for (std::size_t i = 0; i < m_mesh_count; ++i) {
        SDL_ReleaseGPUBuffer(m_device, m_meshes[i].m_vertices);
        SDL_ReleaseGPUBuffer(m_device, m_meshes[i].m_indices);
    }

    for (std::size_t i = 0; i < m_material_count; ++i) {
        SDL_ReleaseGPUGraphicsPipeline(m_device, m_materials[i]);
    }

    if (m_window) {
        SDL_ReleaseWindowFromGPUDevice(m_device, m_window);
    }

    SDL_DestroyGPUDevice(m_device);
}

FK::ErrorCodeOr<void> Renderer::initialize(Window& owner, bool vsync) {
    auto* window = owner.m_native;
    FK_ASSERT(window != nullptr && m_device == nullptr);

    m_device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV,
                                   FK::BUILD_MODE == FK::BuildMode::DEBUG, nullptr);
    if (!m_device) {
        return failure();
    }

    if (!SDL_ClaimWindowForGPUDevice(m_device, window)) {
        return failure();
    }

    m_window = window;

    const auto present_mode = vsync ? SDL_GPU_PRESENTMODE_VSYNC : SDL_GPU_PRESENTMODE_IMMEDIATE;
    if (!SDL_WindowSupportsGPUPresentMode(m_device, window, present_mode) ||
        !SDL_SetGPUSwapchainParameters(m_device, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
                                       present_mode)) {
        SDL_SetError("Requested presentation mode is unsupported");

        return failure();
    }

    FK_LOG_INFO("GPU: {}, presentation: {}", SDL_GetGPUDeviceDriver(m_device),
                vsync ? "vsync" : "immediate");

    return {};
}

FK::ErrorCodeOr<MaterialHandle> Renderer::createMaterial(const MaterialSettings& settings) {
    FK_ASSERT(m_device && m_window);

    if (!settings.m_vertex_shader || !settings.m_fragment_shader ||
        !settings.m_parameters.isValid()) {
        return FK::ErrorCode::INVALID_ARGUMENT;
    }

    if (m_material_count == MAX_MATERIALS) {
        return FK::ErrorCode::OVERFLOW;
    }

    auto vertex_result = loadShader(m_device, settings.m_vertex_shader, SDL_GPU_SHADERSTAGE_VERTEX);
    if (!vertex_result) {
        return vertex_result.releaseError();
    }

    auto* vertex = vertex_result.value();
    auto fragment_result =
        loadShader(m_device, settings.m_fragment_shader, SDL_GPU_SHADERSTAGE_FRAGMENT);
    if (!fragment_result) {
        SDL_ReleaseGPUShader(m_device, vertex);

        return fragment_result.releaseError();
    }

    auto* fragment = fragment_result.value();

    SDL_GPUVertexBufferDescription buffers[] = {
        {0, sizeof(Vertex), SDL_GPU_VERTEXINPUTRATE_VERTEX, 0},
        {1, sizeof(Instance), SDL_GPU_VERTEXINPUTRATE_INSTANCE, 0}};
    SDL_GPUVertexAttribute attributes[] = {
        {0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, 0},
        {1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, sizeof(Vector3)},
        {2, 1, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, offsetof(Instance, m_position)},
        {3, 1, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, offsetof(Instance, m_scale)},
        {4, 1, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, offsetof(Instance, m_color)}};

    SDL_GPUColorTargetDescription color{};
    color.format = SDL_GetGPUSwapchainTextureFormat(m_device, m_window);

    SDL_GPUGraphicsPipelineCreateInfo pipeline{};
    pipeline.vertex_shader = vertex;
    pipeline.fragment_shader = fragment;
    pipeline.vertex_input_state = {buffers, 2, attributes, 5};
    pipeline.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    pipeline.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
    pipeline.rasterizer_state.front_face = SDL_GPU_FRONTFACE_CLOCKWISE;
    pipeline.depth_stencil_state.enable_depth_test = true;
    pipeline.depth_stencil_state.enable_depth_write = true;
    pipeline.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
    pipeline.target_info.color_target_descriptions = &color;
    pipeline.target_info.num_color_targets = 1;
    pipeline.target_info.has_depth_stencil_target = true;
    pipeline.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;

    auto* created_pipeline = SDL_CreateGPUGraphicsPipeline(m_device, &pipeline);
    SDL_ReleaseGPUShader(m_device, vertex);
    SDL_ReleaseGPUShader(m_device, fragment);
    if (!created_pipeline) {
        return failure();
    }

    m_material_parameters[m_material_count] = settings.m_parameters;
    m_materials[m_material_count++] = created_pipeline;

    return MaterialHandle{this, static_cast<std::uint32_t>(m_material_count)};
}

FK::ErrorCodeOr<void> Renderer::setMaterialParameters(MaterialHandle material,
                                                      const MaterialParameters& parameters) {
    if (material.m_owner != this || material.m_index == 0 || material.m_index > m_material_count ||
        !parameters.isValid()) {
        return FK::ErrorCode::INVALID_ARGUMENT;
    }

    m_material_parameters[material.m_index - 1] = parameters;

    return {};
}

FK::ErrorCodeOr<MeshHandle> Renderer::createCube() {
    // Each face has independent vertices so its normal stays flat.
    const Vector3 normals[] = {{0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0}, {0, -1, 0}, {0, 1, 0}};
    const Vector3 corners[][4] = {{{-1, -1, -1}, {-1, 1, -1}, {1, 1, -1}, {1, -1, -1}},
                                  {{1, -1, 1}, {1, 1, 1}, {-1, 1, 1}, {-1, -1, 1}},
                                  {{-1, -1, 1}, {-1, 1, 1}, {-1, 1, -1}, {-1, -1, -1}},
                                  {{1, -1, -1}, {1, 1, -1}, {1, 1, 1}, {1, -1, 1}},
                                  {{-1, -1, 1}, {-1, -1, -1}, {1, -1, -1}, {1, -1, 1}},
                                  {{-1, 1, -1}, {-1, 1, 1}, {1, 1, 1}, {1, 1, -1}}};

    std::array<Vertex, 24> vertices{};
    std::array<std::uint32_t, 36> indices{};
    for (int face = 0; face < 6; ++face) {
        for (int i = 0; i < 4; ++i) {
            vertices[face * 4 + i] = {corners[face][i], normals[face]};
        }

        const int order[] = {0, 1, 2, 0, 2, 3};
        for (int i = 0; i < 6; ++i) {
            indices[face * 6 + i] = static_cast<std::uint32_t>(face * 4 + order[i]);
        }
    }

    return createMesh(vertices, indices);
}

FK::ErrorCodeOr<MeshHandle> Renderer::createPlane() {
    const Vertex vertices[] = {{{-1, 0, -1}, {0, 1, 0}},
                               {{-1, 0, 1}, {0, 1, 0}},
                               {{1, 0, 1}, {0, 1, 0}},
                               {{1, 0, -1}, {0, 1, 0}}};
    const std::uint32_t indices[] = {0, 1, 2, 0, 2, 3};

    return createMesh(vertices, indices);
}

FK::ErrorCodeOr<MeshHandle> Renderer::createMesh(std::span<const Vertex> vertices,
                                                 std::span<const std::uint32_t> indices) {
    FK_ASSERT(m_device != nullptr);

    if (vertices.empty() || indices.empty() || indices.size() % 3 != 0 ||
        vertices.size_bytes() > std::numeric_limits<Uint32>::max() ||
        indices.size_bytes() > std::numeric_limits<Uint32>::max() - vertices.size_bytes()) {
        return FK::ErrorCode::INVALID_ARGUMENT;
    }

    for (auto index : indices) {
        if (index >= vertices.size()) {
            return FK::ErrorCode::OUT_OF_RANGE;
        }
    }

    if (m_mesh_count == MAX_MESHES) {
        return FK::ErrorCode::OVERFLOW;
    }

    const auto vertex_bytes = static_cast<Uint32>(vertices.size_bytes());
    const auto index_bytes = static_cast<Uint32>(indices.size_bytes());
    SDL_GPUBufferCreateInfo vertex_info{SDL_GPU_BUFFERUSAGE_VERTEX, vertex_bytes, 0};
    SDL_GPUBufferCreateInfo index_info{SDL_GPU_BUFFERUSAGE_INDEX, index_bytes, 0};
    auto* vertex_buffer = SDL_CreateGPUBuffer(m_device, &vertex_info);
    auto* index_buffer = SDL_CreateGPUBuffer(m_device, &index_info);
    SDL_GPUTransferBufferCreateInfo transfer_info{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
                                                  vertex_bytes + index_bytes, 0};
    auto* transfer = SDL_CreateGPUTransferBuffer(m_device, &transfer_info);
    auto cleanup = [&] {
        if (transfer) {
            SDL_ReleaseGPUTransferBuffer(m_device, transfer);
        }

        if (vertex_buffer) {
            SDL_ReleaseGPUBuffer(m_device, vertex_buffer);
        }

        if (index_buffer) {
            SDL_ReleaseGPUBuffer(m_device, index_buffer);
        }
    };

    if (!vertex_buffer || !index_buffer || !transfer) {
        const auto error = failure();
        cleanup();

        return error;
    }

    auto* memory = static_cast<Uint8*>(SDL_MapGPUTransferBuffer(m_device, transfer, false));
    if (!memory) {
        const auto error = failure();
        cleanup();

        return error;
    }

    std::memcpy(memory, vertices.data(), vertex_bytes);
    std::memcpy(memory + vertex_bytes, indices.data(), index_bytes);
    SDL_UnmapGPUTransferBuffer(m_device, transfer);

    auto* command = SDL_AcquireGPUCommandBuffer(m_device);
    if (!command) {
        const auto error = failure();
        cleanup();

        return error;
    }

    auto* copy = SDL_BeginGPUCopyPass(command);
    SDL_GPUTransferBufferLocation source{transfer, 0};
    SDL_GPUBufferRegion destination{vertex_buffer, 0, vertex_bytes};
    SDL_UploadToGPUBuffer(copy, &source, &destination, false);

    source.offset = vertex_bytes;
    destination = {index_buffer, 0, index_bytes};
    SDL_UploadToGPUBuffer(copy, &source, &destination, false);
    SDL_EndGPUCopyPass(copy);

    if (!SDL_SubmitGPUCommandBuffer(command)) {
        const auto error = failure();
        cleanup();

        return error;
    }

    SDL_ReleaseGPUTransferBuffer(m_device, transfer);
    m_meshes[m_mesh_count++] = {vertex_buffer, index_buffer,
                                static_cast<std::uint32_t>(indices.size())};

    return MeshHandle{this, static_cast<std::uint32_t>(m_mesh_count)};
}

FK::ErrorCodeOr<void> Renderer::setStaticDraws(std::span<const MeshDraw> draws) {
    FK_PROFILE_ZONE("Static world upload");
    FK_ASSERT(m_device != nullptr);

    if (draws.size() > 1000000) {
        return FK::ErrorCode::OUT_OF_RANGE;
    }

    std::array<Uint32, BATCH_COUNT> counts{};
    std::array<Uint32, BATCH_COUNT> starts{};
    auto batchIndex = [](const MeshDraw& draw) {
        return (draw.m_material.m_index - 1) * MAX_MESHES + draw.m_mesh.m_index - 1;
    };
    for (const auto& draw : draws) {
        if (draw.m_mesh.m_owner != this || draw.m_material.m_owner != this ||
            draw.m_mesh.m_index == 0 || draw.m_mesh.m_index > m_mesh_count ||
            draw.m_material.m_index == 0 || draw.m_material.m_index > m_material_count ||
            !(draw.m_scale.m_x > 0 && draw.m_scale.m_y > 0 && draw.m_scale.m_z > 0)) {
            return FK::ErrorCode::INVALID_ARGUMENT;
        }

        ++counts[batchIndex(draw)];
    }

    Uint32 offset = 0;
    for (std::size_t batch = 0; batch < BATCH_COUNT; ++batch) {
        starts[batch] = offset;
        offset += counts[batch];
    }

    SDL_GPUBuffer* buffer = nullptr;
    if (!draws.empty()) {
        const auto bytes = static_cast<Uint32>(draws.size() * sizeof(Instance));
        SDL_GPUBufferCreateInfo buffer_info{SDL_GPU_BUFFERUSAGE_VERTEX, bytes, 0};
        SDL_GPUTransferBufferCreateInfo upload_info{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, bytes, 0};
        buffer = SDL_CreateGPUBuffer(m_device, &buffer_info);
        auto* upload = SDL_CreateGPUTransferBuffer(m_device, &upload_info);
        auto cleanup = [&] {
            if (buffer) {
                SDL_ReleaseGPUBuffer(m_device, buffer);
            }

            if (upload) {
                SDL_ReleaseGPUTransferBuffer(m_device, upload);
            }
        };
        if (!buffer || !upload) {
            const auto error = failure();
            cleanup();

            return error;
        }

        auto* instances = static_cast<Instance*>(SDL_MapGPUTransferBuffer(m_device, upload, false));
        if (!instances) {
            const auto error = failure();
            cleanup();

            return error;
        }

        auto positions = starts;
        for (const auto& draw : draws) {
            instances[positions[batchIndex(draw)]++] = {draw.m_position, draw.m_scale,
                                                        draw.m_color};
        }

        SDL_UnmapGPUTransferBuffer(m_device, upload);

        auto* command = SDL_AcquireGPUCommandBuffer(m_device);
        if (!command) {
            const auto error = failure();
            cleanup();

            return error;
        }

        auto* copy = SDL_BeginGPUCopyPass(command);
        SDL_GPUTransferBufferLocation source{upload, 0};
        SDL_GPUBufferRegion destination{buffer, 0, bytes};
        SDL_UploadToGPUBuffer(copy, &source, &destination, false);
        SDL_EndGPUCopyPass(copy);
        if (!SDL_SubmitGPUCommandBuffer(command)) {
            const auto error = failure();
            cleanup();

            return error;
        }

        SDL_ReleaseGPUTransferBuffer(m_device, upload);
        FK_PROFILE_PLOT("Static upload bytes", static_cast<std::int64_t>(bytes));
    }

    if (m_static_instances) {
        SDL_ReleaseGPUBuffer(m_device, m_static_instances);
    }

    m_static_instances = buffer;
    m_static_counts = counts;
    m_static_starts = starts;
    m_static_count = static_cast<Uint32>(draws.size());

    return {};
}

FK::ErrorCodeOr<void> Renderer::render(Frame& frame) {
    FK_ASSERT(m_device != nullptr);

    FK_PROFILE_ZONE("Renderer frame");
    const auto start = SDL_GetTicksNS();
    m_statistics = {};
    auto draws = frame.draws();
    auto& counts = m_dynamic_counts;
    auto& starts = m_dynamic_starts;
    const bool changed = !frame.revision() || frame.revision() != m_uploaded_revision;
    const auto instance_bytes = static_cast<Uint32>(draws.size() * sizeof(Instance));
    m_statistics.m_static_instances = m_static_count;
    m_statistics.m_dynamic_instances = static_cast<Uint32>(draws.size());

    if (changed) {
        FK_PROFILE_ZONE("Dynamic instance grouping and packing");
        m_uploaded_revision.reset();
        counts.fill(0);
        starts.fill(0);
        auto batchIndex = [](const MeshDraw& draw) {
            return (draw.m_material.m_index - 1) * MAX_MESHES + draw.m_mesh.m_index - 1;
        };
        for (const auto& draw : draws) {
            if (draw.m_mesh.m_owner != this || draw.m_material.m_owner != this ||
                draw.m_mesh.m_index == 0 || draw.m_mesh.m_index > m_mesh_count ||
                draw.m_material.m_index == 0 || draw.m_material.m_index > m_material_count) {
                return FK::ErrorCode::INVALID_ARGUMENT;
            }
        }

        for (const auto& draw : draws) {
            ++counts[batchIndex(draw)];
        }

        Uint32 offset = 0;
        for (std::size_t batch = 0; batch < BATCH_COUNT; ++batch) {
            starts[batch] = offset;
            offset += counts[batch];
        }

        if (instance_bytes > m_instance_capacity) {
            SDL_GPUBufferCreateInfo buffer_info{SDL_GPU_BUFFERUSAGE_VERTEX, instance_bytes, 0};
            SDL_GPUTransferBufferCreateInfo upload_info{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
                                                        instance_bytes, 0};
            auto* buffer = SDL_CreateGPUBuffer(m_device, &buffer_info);
            auto* upload = SDL_CreateGPUTransferBuffer(m_device, &upload_info);
            if (!buffer || !upload) {
                const auto error = failure();
                if (buffer) {
                    SDL_ReleaseGPUBuffer(m_device, buffer);
                }

                if (upload) {
                    SDL_ReleaseGPUTransferBuffer(m_device, upload);
                }

                return error;
            }

            if (m_instances) {
                SDL_ReleaseGPUBuffer(m_device, m_instances);
            }

            if (m_upload) {
                SDL_ReleaseGPUTransferBuffer(m_device, m_upload);
            }

            m_instances = buffer;
            m_upload = upload;
            m_instance_capacity = instance_bytes;
        }

        if (!draws.empty()) {
            auto* instances =
                static_cast<Instance*>(SDL_MapGPUTransferBuffer(m_device, m_upload, true));
            if (!instances) {
                return failure();
            }

            auto positions = starts;
            for (const auto& draw : draws) {
                instances[positions[batchIndex(draw)]++] = {draw.m_position, draw.m_scale,
                                                            draw.m_color};
            }

            SDL_UnmapGPUTransferBuffer(m_device, m_upload);
        }
    }

    const auto prepared = SDL_GetTicksNS();
    m_statistics.m_prepare_ms = static_cast<double>(prepared - start) * 1e-6;

    auto* command = SDL_AcquireGPUCommandBuffer(m_device);
    if (!command) {
        return failure();
    }

    SDL_GPUTexture* swapchain = nullptr;
    Uint32 width = 0;
    Uint32 height = 0;
    {
        FK_PROFILE_ZONE("Swapchain acquire wait");
        if (!SDL_WaitAndAcquireGPUSwapchainTexture(command, m_window, &swapchain, &width,
                                                   &height)) {
            SDL_CancelGPUCommandBuffer(command);

            return failure();
        }
    }

    if (!swapchain) {
        if (!SDL_SubmitGPUCommandBuffer(command)) {
            return failure();
        }

        return {};
    }

    const auto acquired = SDL_GetTicksNS();
    m_statistics.m_acquire_ms = static_cast<double>(acquired - prepared) * 1e-6;

    if (!m_depth || width != m_width || height != m_height) {
        SDL_GPUTextureCreateInfo info{};
        info.type = SDL_GPU_TEXTURETYPE_2D;
        info.format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
        info.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
        info.width = width;
        info.height = height;
        info.layer_count_or_depth = 1;
        info.num_levels = 1;

        auto* depth = SDL_CreateGPUTexture(m_device, &info);
        if (!depth) {
            const auto error = failure();
            SDL_SubmitGPUCommandBuffer(command);

            return error;
        }

        if (m_depth) {
            SDL_ReleaseGPUTexture(m_device, m_depth);
        }

        m_depth = depth;
        m_width = width;
        m_height = height;
    }

    if (changed && !draws.empty()) {
        FK_PROFILE_ZONE("Dynamic instance copy commands");
        m_statistics.m_instance_upload_bytes = instance_bytes;
        auto* copy = SDL_BeginGPUCopyPass(command);
        SDL_GPUTransferBufferLocation source{m_upload, 0};
        SDL_GPUBufferRegion destination{m_instances, 0, instance_bytes};
        SDL_UploadToGPUBuffer(copy, &source, &destination, true);
        SDL_EndGPUCopyPass(copy);
    }

    const auto view_projection =
        frame.camera().viewProjection(static_cast<float>(width) / static_cast<float>(height));

    SDL_GPUColorTargetInfo color{};
    color.texture = swapchain;
    color.clear_color = {0.035F, 0.05F, 0.075F, 1};
    color.load_op = SDL_GPU_LOADOP_CLEAR;
    color.store_op = SDL_GPU_STOREOP_STORE;

    SDL_GPUDepthStencilTargetInfo depth{};
    depth.texture = m_depth;
    depth.clear_depth = 1;
    depth.load_op = SDL_GPU_LOADOP_CLEAR;
    depth.store_op = SDL_GPU_STOREOP_DONT_CARE;
    depth.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
    depth.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
    depth.cycle = true;

    auto* pass = SDL_BeginGPURenderPass(command, &color, 1, &depth);
    std::uint32_t bound_material = 0;
    std::uint32_t bound_mesh = 0;

    // Explicit float4 packing matches the app's HLSL uniform layout.
    std::array<std::array<float, 4>, 20> light_uniforms{};
    const auto camera_position = frame.camera().position();
    const auto& lighting = frame.lighting();
    const auto direction = FK::LinAlg::normalized(lighting.m_directional.m_direction);
    light_uniforms[0] = {camera_position.m_x, camera_position.m_y, camera_position.m_z,
                         static_cast<float>(lighting.m_point_count)};
    light_uniforms[1] = {lighting.m_ambient[0], lighting.m_ambient[1], lighting.m_ambient[2], 0};
    light_uniforms[2] = {direction.m_x, direction.m_y, direction.m_z,
                         lighting.m_directional.m_intensity};
    light_uniforms[3] = {lighting.m_directional.m_color[0], lighting.m_directional.m_color[1],
                         lighting.m_directional.m_color[2], 0};
    for (std::size_t i = 0; i < lighting.m_point_count; ++i) {
        const auto& light = lighting.m_points[i];
        light_uniforms[4 + i * 2] = {light.m_position.m_x, light.m_position.m_y,
                                     light.m_position.m_z, light.m_radius};
        light_uniforms[5 + i * 2] = {light.m_color[0], light.m_color[1], light.m_color[2],
                                     light.m_intensity};
    }
    SDL_PushGPUFragmentUniformData(command, 0, light_uniforms.data(), sizeof(light_uniforms));

    const Uniforms uniforms{view_projection, {1, 1, 1, 1}};
    SDL_PushGPUVertexUniformData(command, 0, &uniforms, sizeof(uniforms));

    auto drawBatches = [&](SDL_GPUBuffer* instance_buffer_handle, const auto& counts,
                           const auto& starts) {
        FK_PROFILE_ZONE("Encode instance batches");
        for (std::size_t batch = 0; batch < BATCH_COUNT; ++batch) {
            if (counts[batch] == 0) {
                continue;
            }

            const auto material_index = static_cast<Uint32>(batch / MAX_MESHES + 1);
            const auto mesh_index = static_cast<Uint32>(batch % MAX_MESHES + 1);
            if (bound_material != material_index) {
                SDL_BindGPUGraphicsPipeline(pass, m_materials[material_index - 1]);
                const auto& material = m_material_parameters[material_index - 1];
                const std::array<std::array<float, 4>, 3> material_uniforms{
                    {material.m_base_color,
                     {material.m_emissive_color[0], material.m_emissive_color[1],
                      material.m_emissive_color[2], material.m_emissive_intensity},
                     {material.m_roughness, material.m_metallic, material.m_reflectivity, 0}}};
                SDL_PushGPUFragmentUniformData(command, 1, material_uniforms.data(),
                                               sizeof(material_uniforms));
                bound_material = material_index;
                ++m_statistics.m_pipeline_binds;
            }

            const auto& mesh = m_meshes[mesh_index - 1];
            if (bound_mesh != mesh_index) {
                SDL_GPUBufferBinding vertex{mesh.m_vertices, 0};
                SDL_GPUBufferBinding index{mesh.m_indices, 0};
                SDL_BindGPUVertexBuffers(pass, 0, &vertex, 1);
                SDL_BindGPUIndexBuffer(pass, &index, SDL_GPU_INDEXELEMENTSIZE_32BIT);
                bound_mesh = mesh_index;
            }

            SDL_GPUBufferBinding instance_buffer{
                instance_buffer_handle, static_cast<Uint32>(starts[batch] * sizeof(Instance))};
            SDL_BindGPUVertexBuffers(pass, 1, &instance_buffer, 1);
            SDL_DrawGPUIndexedPrimitives(pass, mesh.m_index_count, counts[batch], 0, 0, 0);
            ++m_statistics.m_draw_calls;
            m_statistics.m_triangles +=
                static_cast<std::uint64_t>(mesh.m_index_count / 3) * counts[batch];
        }
    };
    drawBatches(m_static_instances, m_static_counts, m_static_starts);
    drawBatches(m_instances, m_dynamic_counts, m_dynamic_starts);

    SDL_EndGPURenderPass(pass);
    const auto encoded = SDL_GetTicksNS();
    m_statistics.m_encode_ms = static_cast<double>(encoded - acquired) * 1e-6;
    {
        FK_PROFILE_ZONE("Submit and present");
        if (!SDL_SubmitGPUCommandBuffer(command)) {
            return failure();
        }
    }

    m_uploaded_revision = frame.revision();
    FK_PROFILE_PLOT("Instance upload bytes/frame",
                    static_cast<std::int64_t>(m_statistics.m_instance_upload_bytes));
    FK_PROFILE_PLOT("Static instances", static_cast<std::int64_t>(m_static_count));
    FK_PROFILE_PLOT("Dynamic instances", static_cast<std::int64_t>(draws.size()));
    FK_PROFILE_PLOT("Draw calls", static_cast<std::int64_t>(m_statistics.m_draw_calls));
    m_statistics.m_submit_ms = static_cast<double>(SDL_GetTicksNS() - encoded) * 1e-6;
    m_statistics.m_presented = true;

    return {};
}

} // namespace aisdar
