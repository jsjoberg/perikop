# Static analysis with clang-tidy and the Clang Static Analyzer, using .clang-tidy.
# It is optional for builds, never includes fetched dependencies, and needs a built tree.
find_program(ORTHO_RUN_CLANG_TIDY NAMES run-clang-tidy)
find_program(ORTHO_CLANG_TIDY NAMES clang-tidy)
if(ORTHO_RUN_CLANG_TIDY AND ORTHO_CLANG_TIDY)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
    set(ortho_tidy_extra)
    if(APPLE)
        # A non-Apple clang-tidy needs to be told where the macOS SDK is.
        execute_process(COMMAND xcrun --show-sdk-path OUTPUT_VARIABLE ortho_sdk OUTPUT_STRIP_TRAILING_WHITESPACE)
        set(ortho_tidy_extra "-extra-arg=-isysroot${ortho_sdk}")
        list(LENGTH CMAKE_OSX_ARCHITECTURES ortho_architecture_count)
        if(ortho_architecture_count GREATER 1)
            # clang-tidy accepts one compiler job; analyze the host slice of a universal build.
            list(APPEND ortho_tidy_extra -removed-arg=-arch)
            foreach(architecture IN LISTS CMAKE_OSX_ARCHITECTURES)
                list(APPEND ortho_tidy_extra "-removed-arg=${architecture}")
            endforeach()
            list(APPEND ortho_tidy_extra -extra-arg=-arch "-extra-arg=${CMAKE_HOST_SYSTEM_PROCESSOR}")
        endif()
    endif()
    add_custom_target(tidy
        COMMAND "${ORTHO_RUN_CLANG_TIDY}" -p "${CMAKE_BINARY_DIR}" -quiet
            -clang-tidy-binary "${ORTHO_CLANG_TIDY}" ${ortho_tidy_extra} "${CMAKE_SOURCE_DIR}/(src|tests|tools)/"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Analyze native sources"
        VERBATIM)
endif()
