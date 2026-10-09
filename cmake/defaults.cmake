# Compiler selection must happen before project() enables C and C++.
# Respect explicit compilers, environment variables, and toolchain files.
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux"
        AND (NOT CMAKE_SYSTEM_NAME OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
        AND NOT CMAKE_TOOLCHAIN_FILE
        AND "$ENV{CMAKE_TOOLCHAIN_FILE}" STREQUAL ""
        AND NOT CMAKE_C_COMPILER AND NOT CMAKE_CXX_COMPILER
        AND "$ENV{CC}" STREQUAL "" AND "$ENV{CXX}" STREQUAL "")
    find_program(perikop_gcc NAMES gcc-15 NO_CACHE)
    find_program(perikop_gxx NAMES g++-15 NO_CACHE)
    if(perikop_gcc AND perikop_gxx)
        set(CMAKE_C_COMPILER "${perikop_gcc}" CACHE FILEPATH "C compiler")
        set(CMAKE_CXX_COMPILER "${perikop_gxx}" CACHE FILEPATH "C++ compiler")
    endif()
endif()
