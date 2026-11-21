function(aisdar_add_build_version target)
    find_package(Git QUIET)
    set(version_source "${CMAKE_CURRENT_BINARY_DIR}/generated/version.cpp")
    # Check Git on every build; configure_file changes the source only when needed.
    add_custom_target(${target}_version
      COMMAND "${CMAKE_COMMAND}"
        "-DGIT_EXECUTABLE=${GIT_EXECUTABLE}"
        "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}"
        "-DOUTPUT_FILE=${version_source}"
        -P "${PROJECT_SOURCE_DIR}/cmake/GenerateVersion.cmake"
      BYPRODUCTS "${version_source}"
      VERBATIM
    )
    add_dependencies(${target} ${target}_version)
    target_sources(${target} PRIVATE "${version_source}")
endfunction()
