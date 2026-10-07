# Writes the build's version into a header: the project version and the commit,
# such as 0.1.0-bffaaa0, with "-dirty" for uncommitted changes. It runs on every
# build and rewrites the header only when the text changes.
execute_process(COMMAND git rev-parse --short=7 HEAD
    WORKING_DIRECTORY "${SOURCE_DIR}" OUTPUT_VARIABLE commit
    OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET RESULT_VARIABLE failed)
set(version "${VERSION}")
if(NOT failed AND commit)
    string(APPEND version "-${commit}")
    execute_process(COMMAND git diff-index --quiet HEAD --
        WORKING_DIRECTORY "${SOURCE_DIR}" RESULT_VARIABLE changed ERROR_QUIET)
    if(changed)
        string(APPEND version "-dirty")
    endif()
endif()
file(CONFIGURE OUTPUT "${OUTPUT}" CONTENT "#pragma once\n#define PERIKOP_VERSION \"@version@\"\n" @ONLY)
