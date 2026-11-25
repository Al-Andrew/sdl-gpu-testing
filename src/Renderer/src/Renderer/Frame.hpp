#pragma once


#include <cstdint>
#include <memory>
#include <optional>
#include <array>
#include <span>

#include <FieldKit/Allocator.hpp>
#include <FieldKit/NonNullPointer.hpp>
#include <Renderer/Camera.hpp>
#include <Renderer/Lighting.hpp>


namespace aisdar {

class Renderer;

/** Renderer-local resource IDs. Zero is invalid; resources live until renderer destruction. */
struct MeshHandle {
    const Renderer* m_owner = nullptr;
    std::uint32_t m_index = 0;
};

struct MaterialHandle {
    const Renderer* m_owner = nullptr;
    std::uint32_t m_index = 0;
};

struct Vertex {
    FK::LinAlg::Vector3 m_position;
    FK::LinAlg::Vector3 m_normal;
};

/** Opaque, indexed triangle draw. Positive axis-aligned scales only for now. */
struct MeshDraw {
    MeshHandle m_mesh;
    MaterialHandle m_material;
    FK::LinAlg::Vector3 m_position;
    FK::LinAlg::Vector3 m_scale{1, 1, 1};
    std::array<float, 4> m_color{1, 1, 1, 1};
};

/** Owns a bounded draw list. The allocator must outlive this nonmovable frame.
 * initialize allocates once; drawMesh and reset never allocate. Submitted data is copied.
 */
class Frame {
public:
    explicit Frame(FK::Allocator& allocator) : m_allocator(allocator) {
    }
    ~Frame() {
        if (m_draws) {
            std::destroy_n(m_draws, m_capacity);
            m_allocator->deallocate(m_draws, m_capacity * sizeof(MeshDraw), alignof(MeshDraw));
        }
    }
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;


    FK::ErrorCodeOr<void> initialize(std::size_t capacity) {
        FK_ASSERT(m_draws == nullptr);

        if (capacity == 0 || capacity > 1000000) {
            return FK::ErrorCode::INVALID_ARGUMENT;
        }

        auto allocation = m_allocator->allocate(capacity * sizeof(MeshDraw), alignof(MeshDraw));
        if (!allocation) {
            return allocation.releaseError();
        }

        m_draws = static_cast<MeshDraw*>(allocation.value());
        m_capacity = capacity;
        std::uninitialized_value_construct_n(m_draws, capacity);

        return {};
    }

    void reset() {
        m_count = 0;
        m_revision.reset();
        m_camera = Camera{};
        m_lighting = Lighting{};
    }

    /** Opt-in cache key for this entire dynamic list. Change it whenever any draw changes.
     * Camera changes do not require a new revision. Omit to upload every frame.
     */
    void setRevision(std::uint64_t revision) {
        m_revision = revision;
    }
    std::optional<std::uint64_t> revision() const {
        return m_revision;
    }

    FK::ErrorCodeOr<void> setLighting(const Lighting& lighting) {
        if (!lighting.isValid()) {
            return FK::ErrorCode::INVALID_ARGUMENT;
        }

        m_lighting = lighting;

        return {};
    }

    const Lighting& lighting() const {
        return m_lighting;
    }

    void setCamera(const Camera& camera) {
        m_camera = camera;
    }

    FK::ErrorCodeOr<void> drawMesh(const MeshDraw& draw) {
        if (!draw.m_mesh.m_owner || draw.m_mesh.m_index == 0 ||
            draw.m_material.m_owner != draw.m_mesh.m_owner || draw.m_material.m_index == 0 ||
            !(draw.m_scale.m_x > 0 && draw.m_scale.m_y > 0 && draw.m_scale.m_z > 0)) {
            return FK::ErrorCode::INVALID_ARGUMENT;
        }

        if (m_count == m_capacity) {
            return FK::ErrorCode::OVERFLOW;
        }

        m_draws[m_count++] = draw;

        return {};
    }


    const Camera& camera() const {
        return m_camera;
    }
    std::span<MeshDraw> draws() {
        return {m_draws, m_count};
    }


private:
    FK::NonNullPointer<FK::Allocator> m_allocator;
    Camera m_camera;
    Lighting m_lighting;
    std::optional<std::uint64_t> m_revision;
    MeshDraw* m_draws = nullptr;
    std::size_t m_count = 0;
    std::size_t m_capacity = 0;
};

} // namespace aisdar
