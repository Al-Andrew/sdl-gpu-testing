#include <iostream>
#include <string_view>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <aisdar/application.hpp>


int main(int argc, char* argv[]) {
    if (argc > 1 && std::string_view{argv[1]} == "--version") {
        std::cout << aisdar::build_version() << '\n';
        return 0;
    }
    const bool smoke_test = argc > 1 && std::string_view{argv[1]} == "--smoke-test";
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL initialization failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    if (!SDL_CreateWindowAndRenderer(aisdar::application_name().data(), 1280, 720,
                                     SDL_WINDOW_RESIZABLE, &window, &renderer)) {
        SDL_Log("Window creation failed: %s", SDL_GetError());
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
            SDL_Log("Rendering failed: %s", SDL_GetError());
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
    return result;
}
