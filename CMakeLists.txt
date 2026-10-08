cmake_minimum_required(VERSION 3.24)
set(CMAKE_OSX_DEPLOYMENT_TARGET 13.4 CACHE STRING "Oldest supported macOS with portable speech")
project(perikop VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# Perikop uses no C++ modules. Scanning for them slows builds and stops ccache from caching GCC compiles.
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
include(FetchContent)
include(CTest)
include(CheckCXXSourceCompiles)
include(cmake/format.cmake)
include(cmake/tidy.cmake)
check_cxx_source_compiles("#include <expected>
int main() { std::expected<int,int> value{1}; return *value - 1; }" ORTHO_HAS_EXPECTED)
if(NOT ORTHO_HAS_EXPECTED)
    message(FATAL_ERROR "A C++23 standard library with std::expected is required. Use GCC 13+ or a recent Apple Clang.")
endif()
option(ORTHO_BUILD_GUI "Build the native reader" ON)
option(ORTHO_SYSTEM_WX "Use installed wxWidgets for developer builds" OFF)
option(ORTHO_SANITIZERS "Enable address and undefined behavior sanitizers" OFF)
FetchContent_Declare(sqlite
    URL https://sqlite.org/2026/sqlite-amalgamation-3530400.zip
    URL_HASH SHA256=1e71ddf93849c6a6ecf58b827c0692073d2dd7ee40196158068f7b29f422e87d
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(sqlite)
add_library(ortho-sqlite STATIC "${sqlite_SOURCE_DIR}/sqlite3.c")
target_include_directories(ortho-sqlite PUBLIC "${sqlite_SOURCE_DIR}")
target_compile_definitions(ortho-sqlite PRIVATE SQLITE_THREADSAFE=1 SQLITE_OMIT_LOAD_EXTENSION)
find_package(Threads REQUIRED)
target_link_libraries(ortho-sqlite PUBLIC Threads::Threads ${CMAKE_DL_LIBS})
add_library(ortho-core
    src/core/model.cpp
    src/core/lectionary.cpp
    src/core/swedish_titles.cpp
    src/core/canon.cpp
    src/core/reading_display.cpp
    src/core/reading_plan.cpp
    src/storage/sqlite.cpp
    src/storage/corpus_db.cpp
    src/storage/study_db.cpp
    src/storage/user_db.cpp
    src/storage/speech_cache.cpp
    src/storage/pronunciation_review_db.cpp
    src/speech/speech.cpp
    src/speech/reading_speech.cpp
    src/speech/read_aloud.cpp
    src/speech/pronunciation_review.cpp)
target_include_directories(ortho-core PUBLIC src)
target_link_libraries(ortho-core PUBLIC ortho-sqlite)
if(MSVC)
    target_compile_options(ortho-core PUBLIC /W4 /utf-8)
else()
    target_compile_options(ortho-core PUBLIC -Wall -Wextra -Wpedantic)
endif()
if(MINGW)
    add_link_options(-static)
endif()
if(ORTHO_SANITIZERS AND NOT MSVC)
    target_compile_options(ortho-core PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(ortho-core PUBLIC -fsanitize=address,undefined)
endif()
if(BUILD_TESTING)
    add_executable(ortho-reading-speech-tests tests/reading_speech_test.cpp)
    target_link_libraries(ortho-reading-speech-tests PRIVATE ortho-core)
    add_test(NAME reading-speech COMMAND ortho-reading-speech-tests "${CMAKE_SOURCE_DIR}/resources/corpus/corpus.db")
    add_executable(ortho-pronunciation-review-tests tests/pronunciation_review_test.cpp)
    target_link_libraries(ortho-pronunciation-review-tests PRIVATE ortho-core)
    add_test(NAME pronunciation-review COMMAND ortho-pronunciation-review-tests "${CMAKE_SOURCE_DIR}/resources" "${CMAKE_BINARY_DIR}/test-pronunciation")
    add_executable(ortho-pcm-stream-tests tests/pcm_stream_test.cpp)
    target_link_libraries(ortho-pcm-stream-tests PRIVATE ortho-core Threads::Threads)
    add_test(NAME pcm-streaming COMMAND ortho-pcm-stream-tests)
    add_executable(ortho-calendar-probe tests/calendar_probe.cpp)
    target_link_libraries(ortho-calendar-probe PRIVATE ortho-core)
    add_executable(ortho-tests tests/core_test.cpp)
    target_link_libraries(ortho-tests PRIVATE ortho-core)
    add_test(NAME core-storage-speech COMMAND ortho-tests "${CMAKE_SOURCE_DIR}/resources/corpus/corpus.db" "${CMAKE_BINARY_DIR}/test-user.db" "${CMAKE_SOURCE_DIR}/tests/antiochian-2026.tsv")
endif()
if(ORTHO_BUILD_GUI)
    if(APPLE AND CMAKE_OSX_DEPLOYMENT_TARGET VERSION_LESS 13.4)
        message(FATAL_ERROR "Portable speech requires macOS 13.4 or later. Reconfigure with -DCMAKE_OSX_DEPLOYMENT_TARGET=13.4.")
    endif()
    include(cmake/speech.cmake)
    if(ORTHO_SYSTEM_WX)
        find_package(wxWidgets 3.2 REQUIRED COMPONENTS core base adv)
        include(${wxWidgets_USE_FILE})
        set(ortho_wx ${wxWidgets_LIBRARIES})
    else()
        set(wxBUILD_SHARED OFF CACHE BOOL "" FORCE)
        set(wxBUILD_SAMPLES OFF CACHE STRING "" FORCE)
        set(wxBUILD_TESTS OFF CACHE STRING "" FORCE)
        set(wxBUILD_DEMOS OFF CACHE BOOL "" FORCE)
        set(wxBUILD_INSTALL OFF CACHE BOOL "" FORCE)
        # GTK builds leave private fonts off by default; the reader loads its bundled fonts with them.
        set(wxUSE_PRIVATE_FONTS ON CACHE BOOL "" FORCE)
        foreach(library ZLIB LIBPNG LIBJPEG REGEX NANOSVG)
            set(wxUSE_${library} builtin CACHE STRING "" FORCE)
        endforeach()
        foreach(feature LIBTIFF LIBWEBP LIBLZMA WEBVIEW MEDIACTRL OPENGL GLCANVAS XRC
                AUI PROPGRID RIBBON RICHTEXT STC HTML HELP WXHTML_HELP XML SOCKETS PROTOCOL URL
                FS_INET WEBREQUEST SECRETSTORE SPELLCHECK DEBUGREPORT LIBSDL LIBNOTIFY LIBGNOMEVFS)
            set(wxUSE_${feature} OFF CACHE BOOL "" FORCE)
        endforeach()
        FetchContent_Declare(wxWidgets
            URL https://github.com/wxWidgets/wxWidgets/releases/download/v3.3.3/wxWidgets-3.3.3.tar.bz2
            URL_HASH SHA256=81b09d6dd9f1ed9301f8c55a968a488d0491f264dc2bab19a7e407ac67009482
            PATCH_COMMAND ${CMAKE_COMMAND} -P "${CMAKE_SOURCE_DIR}/cmake/patch-wxwidgets.cmake" <SOURCE_DIR>
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
        FetchContent_MakeAvailable(wxWidgets)
        set(ortho_wx wx::adv)
    endif()
    add_executable(perikop WIN32 MACOSX_BUNDLE
        src/app/main.cpp
        src/ui/main_frame.cpp
        src/ui/main_frame_menus.cpp
        src/ui/main_frame_playback.cpp
        src/ui/main_frame_toolbar.cpp
        src/ui/main_frame_plans.cpp
        src/ui/main_frame_smoke.cpp
        src/ui/controls.cpp
        src/ui/about.cpp
        src/ui/toolbar.cpp
        src/ui/bible_picker.cpp
        src/ui/date_picker.cpp
        src/ui/scripture_view.cpp
        src/ui/scripture_layout.cpp
        src/ui/scripture_selection.cpp
        src/ui/scripture_playback.cpp
        src/ui/scripture_render.cpp
        src/ui/pronunciation_review.cpp
        src/ui/study_panel.cpp
        src/typesetting/paragraph_layout.cpp)
    target_link_libraries(perikop PRIVATE ortho-speech ortho-core ${ortho_wx})
    if(WIN32)
        enable_language(RC)
        target_sources(perikop PRIVATE cmake/reader.rc)
        target_include_directories(perikop PRIVATE cmake)
    endif()
    set(perikop_version_header "${CMAKE_BINARY_DIR}/generated/perikop_version.hpp")
    add_custom_target(perikop-version
        COMMAND "${CMAKE_COMMAND}" -DSOURCE_DIR=${CMAKE_SOURCE_DIR} -DVERSION=${PROJECT_VERSION}
            -DOUTPUT=${perikop_version_header} -P "${CMAKE_SOURCE_DIR}/cmake/version.cmake"
        BYPRODUCTS "${perikop_version_header}"
        VERBATIM)
    add_dependencies(perikop perikop-version)
    target_include_directories(perikop PRIVATE "${CMAKE_BINARY_DIR}/generated")
    target_compile_definitions(perikop PRIVATE ORTHO_RESOURCE_INSTALL_PATH="${CMAKE_INSTALL_PREFIX}/share/perikop/resources")
    if(APPLE)
        enable_language(OBJCXX)
        target_sources(perikop PRIVATE src/ui/native_scroll.mm src/ui/native_icon.mm)
        set_source_files_properties(src/ui/native_icon.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
        target_link_libraries(perikop PRIVATE "-framework CoreText" "-framework CoreFoundation")
        set_target_properties(perikop PROPERTIES OUTPUT_NAME "Perikop"
            MACOSX_BUNDLE_GUI_IDENTIFIER "org.perikop.prototype"
            MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/cmake/Info.plist.in"
            MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}")
        set(resource_destination "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Perikop.app/Contents/Resources")
        set(ortho_icon_copy COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_SOURCE_DIR}/resources/icons/OrthodoxReader.icns" "${resource_destination}/OrthodoxReader.icns")
        install(TARGETS perikop BUNDLE DESTINATION .)
    else()
        set(resource_destination "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/resources")
        install(TARGETS perikop RUNTIME DESTINATION bin)
    endif()
    # The Alice and Björn voice pack is bundled, so the installed program never needs a network.
    # Prepare it with tools/speech/prepare_kokoro.py; see docs/kokoro-voices.md.
    set(PERIKOP_VOICE_PACK "${CMAKE_SOURCE_DIR}/build/kokoro-pack" CACHE PATH "Prepared voice pack to bundle")
    if(EXISTS "${PERIKOP_VOICE_PACK}/voice-pack.json")
        file(READ "${PERIKOP_VOICE_PACK}/voice-pack.json" perikop_voice_manifest)
        string(JSON perikop_voice_id GET "${perikop_voice_manifest}" id)
        set(perikop_voice_copy COMMAND ${CMAKE_COMMAND} -E copy_directory "${PERIKOP_VOICE_PACK}"
            "${resource_destination}/voices/${perikop_voice_id}")
    else()
        message(WARNING "No voice pack at ${PERIKOP_VOICE_PACK}. Perikop is built without read-aloud.")
    endif()
    add_custom_target(ortho-resources ALL
        COMMAND ${CMAKE_COMMAND} -E remove_directory "${resource_destination}"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/resources" "${resource_destination}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/THIRD-PARTY-NOTICES.md" "${resource_destination}/THIRD-PARTY-NOTICES.md"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/LICENSE" "${resource_destination}/LICENSE"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${ortho_speech_source}/licenses" "${resource_destination}/licenses"
        ${ortho_icon_copy}
        ${perikop_voice_copy})
    add_dependencies(perikop ortho-resources)
    if(APPLE)
        set_target_properties(perikop PROPERTIES BUILD_RPATH "@executable_path/../Frameworks" INSTALL_RPATH "@executable_path/../Frameworks")
        add_custom_command(TARGET perikop POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_BUNDLE_DIR:perikop>/Contents/Frameworks"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${ortho_ort_library}" "$<TARGET_BUNDLE_DIR:perikop>/Contents/Frameworks/libonnxruntime.${ortho_ort_version}.dylib")
    elseif(WIN32)
        add_custom_command(TARGET perikop POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ortho_ort_SOURCE_DIR}/lib/onnxruntime.dll" "$<TARGET_FILE_DIR:perikop>/onnxruntime.dll")
        install(FILES "${ortho_ort_SOURCE_DIR}/lib/onnxruntime.dll" DESTINATION bin)
        # onnxruntime.dll needs the Visual C++ runtime, which a clean Windows may lack.
        set(PERIKOP_MSVC_RUNTIME "" CACHE PATH "Microsoft.VC14x.CRT folder to install beside onnxruntime.dll")
        if(PERIKOP_MSVC_RUNTIME)
            foreach(library msvcp140 msvcp140_1 vcruntime140 vcruntime140_1)
                install(FILES "${PERIKOP_MSVC_RUNTIME}/${library}.dll" DESTINATION bin)
            endforeach()
        else()
            message(WARNING "PERIKOP_MSVC_RUNTIME is not set. Installed Perikop needs the Visual C++ runtime.")
        endif()
    else()
        set_target_properties(perikop PROPERTIES BUILD_RPATH "$ORIGIN" INSTALL_RPATH "$ORIGIN")
        # The reader refreshes Pango's font list after it registers the bundled fonts.
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(perikop_pango REQUIRED IMPORTED_TARGET pangoft2 pangocairo)
        target_link_libraries(perikop PRIVATE PkgConfig::perikop_pango)
        add_custom_command(TARGET perikop POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ortho_ort_library}" "$<TARGET_FILE_DIR:perikop>/libonnxruntime.so.1")
        install(FILES "${ortho_ort_library}" DESTINATION bin RENAME libonnxruntime.so.1)
    endif()
    # The assembled resources include the generated licenses and the bundled voice pack.
    if(APPLE)
        install(DIRECTORY "${resource_destination}/" DESTINATION "Perikop.app/Contents/Resources")
        # A downloaded bundle whose seal does not cover its contents opens as "damaged".
        install(CODE "execute_process(COMMAND codesign --force --deep --sign -
            \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/Perikop.app\" COMMAND_ERROR_IS_FATAL ANY)")
    elseif(WIN32)
        install(DIRECTORY "${resource_destination}/" DESTINATION bin/resources)
    else()
        install(DIRECTORY "${resource_destination}/" DESTINATION share/perikop/resources)
    endif()
    if(BUILD_TESTING)
        add_test(NAME ui-smoke COMMAND "$<TARGET_FILE:perikop>" --smoke-test --resources "${resource_destination}")
        set_tests_properties(ui-smoke PROPERTIES LABELS "gui" TIMEOUT 30)
    endif()
    # Release packages: a DMG on macOS and an installer on Windows. Linux uses tools/package_appimage.sh.
    if(APPLE OR WIN32)
        set(CPACK_PACKAGE_NAME Perikop)
        set(CPACK_PACKAGE_VENDOR Perikop)
        set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Orthodox lectionary reader")
        set(CPACK_PACKAGE_INSTALL_DIRECTORY Perikop)
        if(APPLE)
            set(CPACK_GENERATOR DragNDrop)
            set(CPACK_SYSTEM_NAME "macOS-${CMAKE_SYSTEM_PROCESSOR}")
            set(CPACK_DMG_VOLUME_NAME Perikop)
        else()
            set(CPACK_GENERATOR NSIS)
            set(CPACK_SYSTEM_NAME windows-x64)
            set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64")
            set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/resources/icons/OrthodoxReader.ico")
            set(CPACK_NSIS_INSTALLED_ICON_NAME bin/perikop.exe)
            set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
            # Nothing needs accepting to install; the license ships in the resources.
            set(CPACK_NSIS_IGNORE_LICENSE_PAGE ON)
            set(CPACK_PACKAGE_EXECUTABLES perikop Perikop)
        endif()
        include(CPack)
    endif()
endif()
