# Keep the minimum CPU independent of the machine that builds a package.
if(CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$")
    if(APPLE)
        add_link_options("$<$<CONFIG:Release,MinSizeRel>:-Wl,-dead_strip>")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64)$" AND CMAKE_SIZEOF_VOID_P EQUAL 8)
        # SSE4.2 and POPCNT fit X220-era CPUs without requiring AVX or AVX2.
        add_compile_options(-march=x86-64-v2 -mtune=generic)
    endif()
    if(MINGW OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
        add_compile_options("$<$<CONFIG:Release,MinSizeRel>:-ffunction-sections>"
            "$<$<CONFIG:Release,MinSizeRel>:-fdata-sections>")
        add_link_options("$<$<CONFIG:Release,MinSizeRel>:-Wl,--gc-sections>")
    endif()
endif()

option(PERIKOP_LTO "Use link-time optimization in Release and MinSizeRel builds" ON)
if(PERIKOP_LTO AND NOT ORTHO_SANITIZERS)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT perikop_ipo_supported OUTPUT perikop_ipo_error LANGUAGES C CXX)
    if(perikop_ipo_supported)
        # Set these before creating targets so static dependencies participate too.
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL TRUE)
    else()
        message(WARNING "Link-time optimization is unavailable: ${perikop_ipo_error}")
    endif()
endif()
