# Keep CPack's current installer template, with only the Swedish UI translation.
file(READ "${CMAKE_ROOT}/Modules/Internal/CPack/NSIS.template.in" perikop_nsis_template)
string(REGEX MATCHALL "[ \t]*!insertmacro MUI_LANGUAGE \"[^\"]+\"[^\r\n]*"
    perikop_nsis_languages "${perikop_nsis_template}")
foreach(language IN LISTS perikop_nsis_languages)
    if(NOT language MATCHES "!insertmacro MUI_LANGUAGE \"Swedish\"")
        string(REPLACE "${language}" "" perikop_nsis_template "${perikop_nsis_template}")
    endif()
endforeach()
string(REGEX MATCHALL "!insertmacro MUI_LANGUAGE \"[^\"]+\""
    perikop_nsis_languages "${perikop_nsis_template}")
if(NOT perikop_nsis_languages STREQUAL "!insertmacro MUI_LANGUAGE \"Swedish\"")
    message(FATAL_ERROR "CPack's NSIS template must provide the Swedish installer language")
endif()
set(perikop_nsis_directory "${CMAKE_BINARY_DIR}/generated/nsis")
file(MAKE_DIRECTORY "${perikop_nsis_directory}")
file(WRITE "${perikop_nsis_directory}/NSIS.template.in" "${perikop_nsis_template}")
set(CPACK_MODULE_PATH "${perikop_nsis_directory};${CMAKE_MODULE_PATH}")
