add_library(aisdar_core STATIC application.cpp)
add_library(Aisdar::Core ALIAS aisdar_core)
aisdar_add_build_version(aisdar_core)
target_compile_features(aisdar_core PUBLIC cxx_std_23)
target_include_directories(
  aisdar_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
