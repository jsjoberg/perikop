# Static analysis with clang-tidy and the Clang Static Analyzer, using .clang-tidy.
# It is optional for builds, never includes fetched dependencies, and needs a built tree.
find_program(ORTHO_RUN_CLANG_TIDY NAMES run-clang-tidy run-clang-tidy-18)
find_program(ORTHO_CLANG_TIDY NAMES clang-tidy clang-tidy-18)
if(ORTHO_RUN_CLANG_TIDY AND ORTHO_CLANG_TIDY)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
    set(ortho_tidy_extra)
    if(APPLE)
        # A non-Apple clang-tidy needs to be told where the macOS SDK is.
        execute_process(COMMAND xcrun --show-sdk-path OUTPUT_VARIABLE ortho_sdk OUTPUT_STRIP_TRAILING_WHITESPACE)
        set(ortho_tidy_extra "-extra-arg=-isysroot${ortho_sdk}")
    endif()
    add_custom_target(tidy
        COMMAND "${ORTHO_RUN_CLANG_TIDY}" -p "${CMAKE_BINARY_DIR}" -quiet
            -clang-tidy-binary "${ORTHO_CLANG_TIDY}" ${ortho_tidy_extra} "${CMAKE_SOURCE_DIR}/(src|tests|tools)/"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Analyze native sources"
        VERBATIM)
endif()
