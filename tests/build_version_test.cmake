cmake_minimum_required(VERSION 3.24)
include("${SOURCE_DIR}/cmake/build-version.cmake")
# Nightly CI sets this for every build step; the computed versions are tested without it.
unset(ENV{PERIKOP_BUILD_VERSION})
file(MAKE_DIRECTORY "${WORK_DIR}")
# A fresh fixture per run, without changing the application's tags or working tree.
string(RANDOM LENGTH 12 fixture_id)
set(fixture "${WORK_DIR}/${fixture_id}")
file(MAKE_DIRECTORY "${fixture}")
function(git_fixture)
    execute_process(COMMAND git ${ARGN} WORKING_DIRECTORY "${fixture}"
        OUTPUT_QUIET ERROR_VARIABLE error RESULT_VARIABLE failed)
    if(failed)
        message(FATAL_ERROR "Git fixture failed: ${error}")
    endif()
endfunction()
git_fixture(init --quiet)
file(WRITE "${fixture}/reader.txt" "Perikop\n")
git_fixture(add reader.txt)
git_fixture(-c user.name=Perikop -c user.email=perikop@example.invalid
    -c commit.gpgsign=false -c core.hooksPath=${fixture}/no-hooks commit --quiet -m fixture)
execute_process(COMMAND git rev-parse --short=7 HEAD WORKING_DIRECTORY "${fixture}"
    OUTPUT_VARIABLE commit OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
perikop_get_version(actual "${fixture}" "0.1.0")
if(NOT actual STREQUAL "0.1.0-${commit}")
    message(FATAL_ERROR "Development version lost its commit: ${actual}")
endif()
git_fixture(tag v0.1.0-alpha.2)
perikop_get_version(actual "${fixture}" "0.1.0")
if(NOT actual STREQUAL "0.1.0-alpha.2")
    message(FATAL_ERROR "Release version lost its suffix: ${actual}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" -DSOURCE_DIR=${fixture} -DVERSION=0.1.0
    -DOUTPUT=${fixture}/perikop_version.hpp -P "${SOURCE_DIR}/cmake/version.cmake"
    COMMAND_ERROR_IS_FATAL ANY)
file(READ "${fixture}/perikop_version.hpp" header)
if(NOT header MATCHES "#define PERIKOP_VERSION \"0.1.0-alpha.2\""
        OR NOT header MATCHES "#define PERIKOP_VERSION_NUMBERS 0,1,0,0")
    message(FATAL_ERROR "Windows needs both the full string and four numeric fields: ${header}")
endif()
# A nightly's ordered version replaces the computed one, and the header still names the commit.
set(ENV{PERIKOP_BUILD_VERSION} "0.1.1-nightly.20261018.412.1")
perikop_get_version(actual "${fixture}" "0.1.0")
execute_process(COMMAND "${CMAKE_COMMAND}" -DSOURCE_DIR=${fixture} -DVERSION=0.1.0
    -DOUTPUT=${fixture}/perikop_version.hpp -P "${SOURCE_DIR}/cmake/version.cmake"
    COMMAND_ERROR_IS_FATAL ANY)
file(READ "${fixture}/perikop_version.hpp" header)
if(NOT actual STREQUAL "0.1.1-nightly.20261018.412.1"
        OR NOT header MATCHES "#define PERIKOP_VERSION \"0.1.1-nightly.20261018.412.1\""
        OR NOT header MATCHES "#define PERIKOP_COMMIT \"${commit}\"")
    message(FATAL_ERROR "Nightly version or its commit is missing: ${actual}, ${header}")
endif()
set(ENV{PERIKOP_BUILD_VERSION} "nightly-2026-10-18")
execute_process(COMMAND "${CMAKE_COMMAND}" -DSOURCE_DIR=${fixture} -DVERSION=0.1.0
    -DOUTPUT=${fixture}/rejected.hpp -P "${SOURCE_DIR}/cmake/version.cmake"
    RESULT_VARIABLE rejected OUTPUT_QUIET ERROR_QUIET)
if(NOT rejected)
    message(FATAL_ERROR "A version that is not SemVer was accepted")
endif()
unset(ENV{PERIKOP_BUILD_VERSION})
file(APPEND "${fixture}/reader.txt" "Changed\n")
perikop_get_version(actual "${fixture}" "0.1.0")
if(NOT actual STREQUAL "0.1.0-alpha.2-dirty")
    message(FATAL_ERROR "Modified release lost its dirty marker: ${actual}")
endif()
perikop_get_version(actual "${WORK_DIR}" "0.1.0")
if(NOT actual STREQUAL "0.1.0")
    message(FATAL_ERROR "Source without Git metadata lost its base version: ${actual}")
endif()
