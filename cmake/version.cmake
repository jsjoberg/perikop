# Writes the build's version into a header. A commit with a version tag uses the
# tag without its "v", such as 0.1.0-alpha.2. Other commits use the project version
# and the commit, such as 0.1.0-bffaaa0. Uncommitted changes add "-dirty". It runs
# on every build and rewrites the header only when the text changes. PERIKOP_COMMIT
# keeps the commit visible when a nightly's version does not name it.
cmake_minimum_required(VERSION 3.24)
include("${CMAKE_CURRENT_LIST_DIR}/build-version.cmake")
perikop_get_version(version "${SOURCE_DIR}" "${VERSION}")
string(REPLACE "." "," version_numbers "${VERSION}")
execute_process(COMMAND git rev-parse --short=7 HEAD WORKING_DIRECTORY "${SOURCE_DIR}"
    OUTPUT_VARIABLE commit OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
file(CONFIGURE OUTPUT "${OUTPUT}" CONTENT
    "#pragma once\n#define PERIKOP_VERSION \"@version@\"\n#define PERIKOP_VERSION_NUMBERS @version_numbers@,0\n#define PERIKOP_COMMIT \"@commit@\"\n" @ONLY)
