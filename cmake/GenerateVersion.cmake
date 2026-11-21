set(branch "unknown")
set(commit "uncommitted")
set(dirty "")
if(GIT_EXECUTABLE)
    execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${SOURCE_DIR}" symbolic-ref --short HEAD
        OUTPUT_VARIABLE git_branch OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${SOURCE_DIR}" rev-parse --short HEAD
        OUTPUT_VARIABLE git_commit OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    if(git_branch)
        set(branch "${git_branch}")
    elseif(git_commit)
        set(branch "detached")
    endif()
    if(git_commit)
        set(commit "${git_commit}")
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" -C "${SOURCE_DIR}" status --porcelain
                --untracked-files=normal --ignore-submodules=none
            OUTPUT_VARIABLE git_status OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE status_result ERROR_QUIET)
        if(status_result EQUAL 0 AND NOT git_status STREQUAL "")
            set(dirty "(dirty)")
        endif()
    endif()
endif()
set(AISDAR_BUILD_VERSION "${branch}-${commit}${dirty}")
# Escape branch names before embedding them in a C++ string literal.
string(REPLACE "\\" "\\\\" AISDAR_BUILD_VERSION "${AISDAR_BUILD_VERSION}")
string(REPLACE "\"" "\\\"" AISDAR_BUILD_VERSION "${AISDAR_BUILD_VERSION}")
configure_file("${SOURCE_DIR}/cmake/version.cpp.in" "${OUTPUT_FILE}" @ONLY)
