add_library(Renderer STATIC src/Renderer/Renderer.cpp)
add_library(Aisdar::Renderer ALIAS Renderer)

target_compile_features(Renderer PUBLIC cxx_std_23)
target_include_directories(Renderer PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(Renderer PUBLIC FK::FieldKit Aisdar::Platform PRIVATE SDL3::SDL3)
