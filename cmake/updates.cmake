# In-app updates use Velopack's C/C++ SDK: linked statically on Linux and macOS, and as velopack_libc.dll on
# Windows. macOS builds target Apple Silicon only.
set(perikop_updates FALSE)
if(WIN32)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(ARM64|aarch64)$")
        set(perikop_velopack_library win_arm64_msvc)
    else()
        set(perikop_velopack_library win_x64_msvc)
    endif()
    set(perikop_updates TRUE)
elseif(APPLE)
    set(perikop_velopack_library osx_arm64_gnu)
    set(perikop_updates TRUE)
elseif(UNIX AND NOT APPLE)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
        set(perikop_velopack_library linux_arm64_gnu)
    else()
        set(perikop_velopack_library linux_x64_gnu)
    endif()
    set(perikop_updates TRUE)
endif()
if(perikop_updates)
    FetchContent_Declare(ortho_velopack
        URL https://github.com/velopack/velopack/releases/download/1.2.161/velopack_libc_1.2.161.zip
        URL_HASH SHA256=31de8f2cf05a47bd5328a9f98b0c41aec1582156f6075f1058b26941f7f17c20
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(ortho_velopack)
    if(WIN32)
        # MinGW cannot link the SDK's static MSVC library, so the program loads its DLL through the C API.
        # The import library names it velopack_libc.dll, which is the name it ships under.
        set(perikop_velopack_dll "${ortho_velopack_SOURCE_DIR}/lib/velopack_libc_${perikop_velopack_library}.dll")
        add_library(ortho-velopack SHARED IMPORTED)
        set_target_properties(ortho-velopack PROPERTIES
            IMPORTED_IMPLIB "${perikop_velopack_dll}.lib"
            IMPORTED_LOCATION "${perikop_velopack_dll}"
            INTERFACE_INCLUDE_DIRECTORIES "${ortho_velopack_SOURCE_DIR}/include")
    else()
        add_library(ortho-velopack STATIC IMPORTED)
        set_target_properties(ortho-velopack PROPERTIES
            IMPORTED_LOCATION "${ortho_velopack_SOURCE_DIR}/lib-static/velopack_libc_${perikop_velopack_library}.a"
            INTERFACE_INCLUDE_DIRECTORIES "${ortho_velopack_SOURCE_DIR}/include")
    endif()
    if(APPLE)
        target_link_libraries(ortho-velopack INTERFACE
            "-framework CoreFoundation" "-framework Security" "-framework SystemConfiguration")
    elseif(NOT WIN32)
        target_link_libraries(ortho-velopack INTERFACE Threads::Threads ${CMAKE_DL_LIBS} m)
    endif()
endif()
