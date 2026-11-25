if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/SDL/CMakeLists.txt")
    message(FATAL_ERROR "SDL is missing. Run: git submodule update --init --recursive")
endif()

set(SDL_SHARED OFF CACHE BOOL "Build shared SDL")
set(SDL_STATIC ON CACHE BOOL "Build static SDL")
set(SDL_TEST_LIBRARY OFF CACHE BOOL "Build SDL test library")
set(SDL_TESTS OFF CACHE BOOL "Build SDL tests")
set(SDL_EXAMPLES OFF CACHE BOOL "Build SDL examples")
add_subdirectory(SDL EXCLUDE_FROM_ALL SYSTEM)

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/spdlog/CMakeLists.txt")
    message(FATAL_ERROR "spdlog is missing. Run: git submodule update --init --recursive")
endif()

set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "Build shared spdlog")
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "Build spdlog examples")
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "Build spdlog tests")
set(SPDLOG_BUILD_BENCH OFF CACHE BOOL "Build spdlog benchmarks")
set(SPDLOG_INSTALL OFF CACHE BOOL "Install spdlog")
add_subdirectory(spdlog EXCLUDE_FROM_ALL SYSTEM)

if(AISDAR_ENABLE_TRACY)
  include(FetchContent)
  set(TRACY_ENABLE ON CACHE BOOL "Enable Tracy" FORCE)
  set(TRACY_ON_DEMAND ON CACHE BOOL "Record while connected" FORCE)
  set(TRACY_ONLY_LOCALHOST ON CACHE BOOL "Local profiler connection" FORCE)
  set(TRACY_NO_SYSTEM_TRACING ON CACHE BOOL "Use explicit instrumentation" FORCE)
  set(TRACY_NO_SAMPLING ON CACHE BOOL "Do not require perf sampling permissions" FORCE)
  FetchContent_Declare(tracy
    GIT_REPOSITORY https://github.com/wolfpld/tracy.git
    GIT_TAG c556831ddc6fe26d2fce01c14c97205a9dad46d5
  )
  FetchContent_MakeAvailable(tracy)
endif()
