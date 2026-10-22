# A complete, verified source snapshot is mandatory when this option is set.
# Source overrides suppress FetchContent's download, update, and patch steps.
set(PERIKOP_DEPENDENCY_DIR "" CACHE PATH "Prepared offline native dependencies")
if(PERIKOP_DEPENDENCY_DIR)
    get_filename_component(PERIKOP_DEPENDENCY_DIR "${PERIKOP_DEPENDENCY_DIR}" ABSOLUTE)
    foreach(entry "sqlite/sqlite3.h" "wxwidgets/CMakeLists.txt" "ortho_audio/miniaudio.h")
        if(NOT EXISTS "${PERIKOP_DEPENDENCY_DIR}/${entry}")
            message(FATAL_ERROR "Offline dependency is missing: ${entry}. Restore the pinned CI snapshot; no upstream download is allowed.")
        endif()
    endforeach()
    # In-app updates; see cmake/updates.cmake.
    if(NOT EXISTS "${PERIKOP_DEPENDENCY_DIR}/ortho_velopack/include/Velopack.hpp")
        message(FATAL_ERROR "Offline update SDK is missing. Restore the pinned CI snapshot; no upstream download is allowed.")
    endif()
    set(FETCHCONTENT_SOURCE_DIR_ORTHO_VELOPACK "${PERIKOP_DEPENDENCY_DIR}/ortho_velopack" CACHE PATH "" FORCE)
    foreach(dependency sqlite wxwidgets ortho_audio)
        string(TOUPPER "${dependency}" key)
        set("FETCHCONTENT_SOURCE_DIR_${key}" "${PERIKOP_DEPENDENCY_DIR}/${dependency}" CACHE PATH "" FORCE)
    endforeach()
    # Also block downloads for any future dependency without a source override.
    set(FETCHCONTENT_FULLY_DISCONNECTED ON CACHE BOOL "Offline CI dependencies" FORCE)
    if(POLICY CMP0170)
        cmake_policy(SET CMP0170 NEW)
    endif()
    # Apply the current patch even when the snapshot predates a patch change.
    execute_process(COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_LIST_DIR}/patch-wxwidgets.cmake"
        "${PERIKOP_DEPENDENCY_DIR}/wxwidgets" COMMAND_ERROR_IS_FATAL ANY)
endif()
