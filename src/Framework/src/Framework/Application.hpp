#pragma once


#include <cstddef>
#include <array>

#include <FieldKit/ErrorOr.hpp>
#include <Platform/Window.hpp>
#include <Renderer/Frame.hpp>
#include <Renderer/Renderer.hpp>


/* The framework owns SDL startup, event polling, fixed updates, presentation,
 * and shutdown. Game callbacks run on the calling (main) thread.
 */
namespace aisdar {

enum class Key { W, A, S, D, Q, E, SHIFT, ESCAPE, COUNT };

/** Transitions and mouse motion are delivered once, to the next fixed update.
 * Held state persists across updates. Focus loss releases held keys.
 */
struct Input {
    std::array<bool, static_cast<std::size_t>(Key::COUNT)> m_held{};
    std::array<bool, static_cast<std::size_t>(Key::COUNT)> m_pressed{};
    std::array<bool, static_cast<std::size_t>(Key::COUNT)> m_released{};
    float m_mouse_x = 0;
    float m_mouse_y = 0;
    bool m_right_mouse = false;
    bool m_focused = true;

    bool held(Key key) const {
        FK_ASSERT(static_cast<std::size_t>(key) < m_held.size());

        return m_held[static_cast<std::size_t>(key)];
    }
};

struct ApplicationSettings {
    WindowSettings m_window;
    std::size_t m_draw_capacity = 16384;
    bool m_vsync = true;
    /** Zero runs until quit. A nonzero limit is useful for smoke tests. */
    unsigned int m_frame_limit = 0;
};

/** The application borrows the game for run(). Callbacks must not retain the
 * window or frame references. All recoverable callback errors stop the loop.
 */
struct FrameReport {
    /** CPU wall time for polling, update, draw submission, and presentation waits.
     * This is not GPU execution time. Callback/reporting time is excluded.
     */
    double m_seconds = 0;
    RenderStatistics m_renderer;
};

class Game {
public:
    virtual ~Game() = default;

    virtual FK::ErrorCodeOr<void> initialize(Renderer&) {
        return {};
    }
    virtual void frameCompleted(const FrameReport&) {
    }

    virtual FK::ErrorCodeOr<void> update(const Input& input, float seconds, Window& window) = 0;
    virtual FK::ErrorCodeOr<void> draw(Frame& frame) = 0;
};

class Application {
public:
    /** Call on the main thread, with no other SDL application running.
     * Owns all runtime state for the duration; SDL performs native allocations.
     */
    FK::ErrorCodeOr<void> run(Game& game, const ApplicationSettings& settings = {});
};

} // namespace aisdar
