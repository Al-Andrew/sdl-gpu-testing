add_executable(aisdar main.cpp)
target_compile_features(aisdar PRIVATE cxx_std_23)
target_link_libraries(aisdar PRIVATE Aisdar::Core SDL3::SDL3 FK::FieldKit)
