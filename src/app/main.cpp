#include <iostream>
#include <string_view>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <FieldKit/Logging.hpp>
#include <aisdar/application.hpp>

int main(int argc, char* argv[]) {



    if (argc > 1 && std::string_view{argv[1]} == "--version") {
        std::cout << aisdar::build_version() << '\n';
        return 0;
    }
    const bool smoke_test = argc > 1 && std::string_view{argv[1]} == "--smoke-test";

    FK_LOG_INFO("Starting {} {}", aisdar::application_name(), aisdar::build_version());

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        FK_LOG_ERROR("SDL initialization failed: {}", SDL_GetError());
        return 1;
    }

    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    if (!SDL_CreateWindowAndRenderer(aisdar::application_name().data(), 1280, 720,
                                     SDL_WINDOW_RESIZABLE, &window, &renderer)) {
        FK_LOG_ERROR("Window creation failed: {}", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    bool running = true;
    int result = 0;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT ||
                (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE)) {
                running = false;
            }
        }
        if (!SDL_SetRenderDrawColor(renderer, 24, 32, 48, 255) || !SDL_RenderClear(renderer) ||
            !SDL_RenderPresent(renderer)) {
            FK_LOG_ERROR("Rendering failed: {}", SDL_GetError());
            result = 1;
            break;
        }
        if (smoke_test) {
            break;
        }
        SDL_Delay(16);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    FK_LOG_INFO("{} exited with code {}", aisdar::application_name(), result);

    return result;
}
