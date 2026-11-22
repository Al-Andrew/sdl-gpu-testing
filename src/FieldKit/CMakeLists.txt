add_library(FieldKit STATIC src/FieldKit/Error.cpp src/FieldKit/Assertions.cpp src/FieldKit/Allocator.cpp src/FieldKit/ManagedString.cpp)
add_library(FK::FieldKit ALIAS FieldKit)
target_compile_features(FieldKit PUBLIC cxx_std_23)
target_include_directories(FieldKit PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")

# Publish the mode so inline code and every consumer use the same assertion policy.
# Debug is explicit; all other configurations (including an unset configuration) are release.
target_compile_definitions(FieldKit PUBLIC
  FK_MODE_DEBUG=$<IF:$<CONFIG:Debug>,1,0>
  FK_MODE_RELEASE=$<IF:$<CONFIG:Debug>,0,1>
)

target_link_libraries(FieldKit PUBLIC spdlog::spdlog)
