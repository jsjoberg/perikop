# Formatting is optional for builds and never includes fetched dependencies.
find_program(ORTHO_CLANG_FORMAT NAMES clang-format)
if(ORTHO_CLANG_FORMAT)
    file(GLOB_RECURSE ortho_format_sources CONFIGURE_DEPENDS
        "${CMAKE_SOURCE_DIR}/src/*.cpp"
        "${CMAKE_SOURCE_DIR}/src/*.hpp"
        "${CMAKE_SOURCE_DIR}/src/*.mm"
        "${CMAKE_SOURCE_DIR}/tests/*.cpp"
        "${CMAKE_SOURCE_DIR}/tools/*.cpp")
    add_custom_target(format
        COMMAND "${ORTHO_CLANG_FORMAT}" -i ${ortho_format_sources}
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Format native sources"
        VERBATIM)
    add_custom_target(format-check
        COMMAND "${ORTHO_CLANG_FORMAT}" --dry-run --Werror ${ortho_format_sources}
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Check native source formatting"
        VERBATIM)
endif()
