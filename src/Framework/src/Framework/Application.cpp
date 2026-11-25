#include <algorithm>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <FieldKit/Logging.hpp>
#include <FieldKit/Profiling.hpp>
#include <Framework/Application.hpp>
#include <Renderer/Renderer.hpp>


namespace aisdar {

namespace {

struct SdlLifetime {
    ~SdlLifetime() {
        SDL_Quit();
    }
};

constexpr SDL_Scancode SCANCODES[] = {SDL_SCANCODE_W,      SDL_SCANCODE_A,     SDL_SCANCODE_S,
                                      SDL_SCANCODE_D,      SDL_SCANCODE_Q,     SDL_SCANCODE_E,
                                      SDL_SCANCODE_LSHIFT, SDL_SCANCODE_ESCAPE};

void clearTransitions(Input& input) {
    input.m_pressed.fill(false);
    input.m_released.fill(false);
    input.m_mouse_x = 0;
    input.m_mouse_y = 0;
}

} // namespace

FK::ErrorCodeOr<void> Application::run(Game& game, const ApplicationSettings& settings) {
    SDL_SetMainReady();
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        FK_LOG_ERROR("SDL initialization: {}", SDL_GetError());

        return FK::ErrorCode::UNSPECIFIED;
    }

    SdlLifetime lifetime;
    Window window;
    auto opened = window.open(settings.m_window);
    if (!opened) {
        return opened.releaseError();
    }

    Renderer renderer;
    auto initialized = renderer.initialize(window, settings.m_vsync);
    if (!initialized) {
        return initialized.releaseError();
    }

    auto game_initialized = game.initialize(renderer);
    if (!game_initialized) {
        return game_initialized.releaseError();
    }

    FK::LibCppAllocator frame_allocator;
    Frame frame(frame_allocator);
    auto frame_initialized = frame.initialize(settings.m_draw_capacity);
    if (!frame_initialized) {
        return frame_initialized.releaseError();
    }

    Input input;
    input.m_focused = (SDL_GetWindowFlags(window.m_native) & SDL_WINDOW_INPUT_FOCUS) != 0;
    Uint64 previous = SDL_GetTicksNS();
    double accumulator = 0;
    unsigned int frame_count = 0;
    constexpr double STEP = 1.0 / 120.0;

    while (true) {
        FK_PROFILE_ZONE("Application frame");
        const Uint64 frame_start = SDL_GetTicksNS();

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED ||
                (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE)) {
                return {};
            }

            if (event.type == SDL_EVENT_WINDOW_FOCUS_GAINED) {
                input.m_focused = true;
            }

            if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
                input.m_focused = false;
                input.m_pressed.fill(false);
                input.m_right_mouse = false;
                input.m_mouse_x = 0;
                input.m_mouse_y = 0;
                for (std::size_t i = 0; i < input.m_held.size(); ++i) {
                    input.m_released[i] = input.m_released[i] || input.m_held[i];
                    input.m_held[i] = false;
                }

                auto released = window.setMouseCaptured(false);
                if (!released) {
                    return released.releaseError();
                }
            }

            if (!input.m_focused) {
                continue;
            }

            if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) {
                for (std::size_t i = 0; i < input.m_held.size(); ++i) {
                    if (event.key.scancode == SCANCODES[i]) {
                        const bool down = event.type == SDL_EVENT_KEY_DOWN;
                        input.m_pressed[i] = input.m_pressed[i] || (down && !input.m_held[i]);
                        input.m_released[i] = input.m_released[i] || (!down && input.m_held[i]);
                        input.m_held[i] = down;
                    }
                }
            }

            if ((event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
                 event.type == SDL_EVENT_MOUSE_BUTTON_UP) &&
                event.button.button == SDL_BUTTON_RIGHT) {
                input.m_right_mouse = event.type == SDL_EVENT_MOUSE_BUTTON_DOWN;
            }

            if (event.type == SDL_EVENT_MOUSE_MOTION &&
                SDL_GetWindowRelativeMouseMode(window.m_native)) {
                input.m_mouse_x += event.motion.xrel;
                input.m_mouse_y += event.motion.yrel;
            }
        }

        const Uint64 now = SDL_GetTicksNS();
        accumulator += std::min(static_cast<double>(now - previous) * 1e-9, 0.1);
        previous = now;

        while (accumulator >= STEP) {
            FK_PROFILE_ZONE("Fixed simulation tick");
            auto updated = game.update(input, static_cast<float>(STEP), window);
            if (!updated) {
                return updated.releaseError();
            }

            clearTransitions(input);
            accumulator -= STEP;
        }

        frame.reset();
        auto drawn = game.draw(frame);
        if (!drawn) {
            return drawn.releaseError();
        }

        auto presented = renderer.render(frame);
        if (!presented) {
            return presented.releaseError();
        }

        const double seconds = static_cast<double>(SDL_GetTicksNS() - frame_start) * 1e-9;
        game.frameCompleted({seconds, renderer.statistics()});
        FK_PROFILE_FRAME();

        if (renderer.statistics().m_presented && settings.m_frame_limit != 0 &&
            ++frame_count >= settings.m_frame_limit) {
            return {};
        }

        if (window.isMinimized()) {
            SDL_Delay(20);
        }
    }
}

} // namespace aisdar
