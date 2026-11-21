cmake_minimum_required(VERSION 3.25)
project(Aisdar VERSION 0.1.0 LANGUAGES C CXX)

# Keep language requirements scoped to our targets, not third-party code.
set(CMAKE_CXX_EXTENSIONS OFF)

include(cmake/OutputDirectories.cmake)
include(cmake/BuildVersion.cmake)
add_subdirectory(src)
