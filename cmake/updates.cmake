# In-app updates use Velopack's C/C++ SDK, linked statically, so no extra library ships beside the program.
# Windows and universal macOS builds follow later; until then they report that updates are unavailable.
set(perikop_updates FALSE)
if(APPLE AND NOT perikop_macos_universal)
    set(perikop_velopack_library "osx_${perikop_macos_architectures}_gnu")
    string(REPLACE "x86_64" "x64" perikop_velopack_library "${perikop_velopack_library}")
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
    add_library(ortho-velopack STATIC IMPORTED)
    set_target_properties(ortho-velopack PROPERTIES
        IMPORTED_LOCATION "${ortho_velopack_SOURCE_DIR}/lib-static/velopack_libc_${perikop_velopack_library}.a"
        INTERFACE_INCLUDE_DIRECTORIES "${ortho_velopack_SOURCE_DIR}/include")
    if(APPLE)
        target_link_libraries(ortho-velopack INTERFACE
            "-framework CoreFoundation" "-framework Security" "-framework SystemConfiguration")
    else()
        target_link_libraries(ortho-velopack INTERFACE Threads::Threads ${CMAKE_DL_LIBS} m)
    endif()
endif()
