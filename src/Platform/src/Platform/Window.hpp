#pragma once


#include <FieldKit/ErrorOr.hpp>


struct SDL_Window;

namespace aisdar {

class Renderer;
class Application;

struct WindowSettings {
    const char* m_title = "Aisdar";
    int m_width = 1280;
    int m_height = 720;
};

/** Owns the native window. SDL must outlive it; native allocations belong to SDL. */
class Window {
public:
    Window() = default;
    ~Window();
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;


    FK::ErrorCodeOr<void> open(const WindowSettings& settings);
    FK::ErrorCodeOr<void> setMouseCaptured(bool captured);
    FK::ErrorCodeOr<void> setTitle(const char* title);
    bool isMinimized() const;


private:
    friend class Renderer;
    friend class Application;

    SDL_Window* m_native = nullptr;
};

} // namespace aisdar
