# Shared package metadata for native builds and the prebuilt Windows installer.
if(NOT perikop_source_dir)
    set(perikop_source_dir "${CMAKE_SOURCE_DIR}")
endif()
set(CPACK_PACKAGE_NAME Perikop)
set(CPACK_STRIP_FILES TRUE)
set(CPACK_VERBATIM_VARIABLES TRUE)
set(CPACK_PACKAGE_VENDOR Perikop)
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Orthodox lectionary reader")
set(CPACK_PACKAGE_INSTALL_DIRECTORY Perikop)
include("${CMAKE_CURRENT_LIST_DIR}/build-version.cmake")
perikop_get_version(CPACK_PACKAGE_VERSION "${perikop_source_dir}" "${PROJECT_VERSION}")
if(DEFINED perikop_prebuilt_version AND NOT perikop_prebuilt_version STREQUAL CPACK_PACKAGE_VERSION)
    message(FATAL_ERROR "Prebuilt version ${perikop_prebuilt_version} does not match source version ${CPACK_PACKAGE_VERSION}")
endif()
if(APPLE)
    set(CPACK_GENERATOR DragNDrop)
    if(perikop_macos_universal)
        set(CPACK_SYSTEM_NAME macOS-universal)
    else()
        set(CPACK_SYSTEM_NAME "macOS-${perikop_macos_architectures}")
    endif()
    set(CPACK_DMG_VOLUME_NAME Perikop)
else()
    set(CPACK_GENERATOR NSIS)
    include("${CMAKE_CURRENT_LIST_DIR}/nsis-swedish.cmake")
    set(CPACK_SYSTEM_NAME windows-x64)
    set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64")
    set(CPACK_NSIS_MUI_ICON "${perikop_source_dir}/resources/icons/OrthodoxReader.ico")
    set(CPACK_NSIS_INSTALLED_ICON_NAME bin/perikop.exe)
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    # Nothing needs accepting to install; the license ships in the resources.
    set(CPACK_NSIS_IGNORE_LICENSE_PAGE ON)
    set(CPACK_PACKAGE_EXECUTABLES perikop Perikop)
    # Windows' fixed version is numeric; its displayed version preserves the release suffix.
    set(CPACK_NSIS_DEFINES "VIProductVersion \"${PROJECT_VERSION}.0\"
VIAddVersionKey /LANG=1033 \"ProductName\" \"Perikop\"
VIAddVersionKey /LANG=1033 \"ProductVersion\" \"${CPACK_PACKAGE_VERSION}\"
VIAddVersionKey /LANG=1033 \"FileVersion\" \"${CPACK_PACKAGE_VERSION}\"
VIAddVersionKey /LANG=1033 \"FileDescription\" \"Perikop installer\"
VIAddVersionKey /LANG=1033 \"LegalCopyright\" \"Perikop contributors. MIT license.\"")
endif()
set(CPACK_PACKAGE_FILE_NAME "Perikop-${PROJECT_VERSION}-${CPACK_SYSTEM_NAME}")
include(CPack)
