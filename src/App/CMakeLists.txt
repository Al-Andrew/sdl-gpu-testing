find_program(AISDAR_GLSLANG NAMES glslangValidator glslang REQUIRED)
set(shader_outputs)
foreach(shader Lit.vert Lit.frag Swizzle.frag)
  string(REGEX REPLACE ".*\\." "" stage "${shader}")
  set(shader_source "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/${shader}.hlsl")
  set(shader_output "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Shaders/${shader}.spv")
  add_custom_command(OUTPUT "${shader_output}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Shaders"
    COMMAND "${AISDAR_GLSLANG}" -D -V -S ${stage} -e main "${shader_source}" -o "${shader_output}"
    DEPENDS "${shader_source}" VERBATIM)
  list(APPEND shader_outputs "${shader_output}")
endforeach()
add_custom_target(aisdar_shaders DEPENDS ${shader_outputs})

add_executable(aisdar src/Main.cpp)
target_compile_features(aisdar PRIVATE cxx_std_23)
target_link_libraries(aisdar PRIVATE Aisdar::Core Aisdar::Framework)

add_dependencies(aisdar aisdar_shaders)
