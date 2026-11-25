#include <SDL3/SDL.h>

#include <FieldKit/Logging.hpp>
#include <Platform/Window.hpp>


namespace aisdar {

Window::~Window() {
    if (m_native) {
        SDL_DestroyWindow(m_native);
    }
}

FK::ErrorCodeOr<void> Window::open(const WindowSettings& settings) {
    FK_ASSERT(m_native == nullptr);

    if (!settings.m_title || settings.m_width <= 0 || settings.m_height <= 0) {
        return FK::ErrorCode::INVALID_ARGUMENT;
    }

    m_native = SDL_CreateWindow(settings.m_title, settings.m_width, settings.m_height,
                                SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!m_native) {
        FK_LOG_ERROR("Window creation: {}", SDL_GetError());

        return FK::ErrorCode::UNSPECIFIED;
    }

    return {};
}

FK::ErrorCodeOr<void> Window::setMouseCaptured(bool captured) {
    FK_ASSERT(m_native != nullptr);

    if (!SDL_SetWindowRelativeMouseMode(m_native, captured)) {
        FK_LOG_ERROR("Mouse capture: {}", SDL_GetError());

        return FK::ErrorCode::UNSPECIFIED;
    }

    return {};
}

FK::ErrorCodeOr<void> Window::setTitle(const char* title) {
    FK_ASSERT(m_native != nullptr && title != nullptr);

    if (!SDL_SetWindowTitle(m_native, title)) {
        return FK::ErrorCode::UNSPECIFIED;
    }

    return {};
}

bool Window::isMinimized() const {
    FK_ASSERT(m_native != nullptr);

    return (SDL_GetWindowFlags(m_native) & SDL_WINDOW_MINIMIZED) != 0;
}

} // namespace aisdar
