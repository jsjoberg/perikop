# Use the exact release tag when present. Other builds identify their commit.
function(perikop_get_version output source base)
    if(NOT EXISTS "${source}/.git")
        set(${output} "${base}" PARENT_SCOPE)
        return()
    endif()
    execute_process(COMMAND git rev-parse --short=7 HEAD
        WORKING_DIRECTORY "${source}" OUTPUT_VARIABLE commit
        OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET RESULT_VARIABLE failed)
    set(version "${base}")
    if(NOT failed AND commit)
        execute_process(COMMAND git describe --tags --exact-match --match "v[0-9]*" HEAD
            WORKING_DIRECTORY "${source}" OUTPUT_VARIABLE tag
            OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET RESULT_VARIABLE untagged)
        if(NOT untagged AND tag)
            string(SUBSTRING "${tag}" 1 -1 version)
        else()
            string(APPEND version "-${commit}")
        endif()
        execute_process(COMMAND git diff-index --quiet HEAD --
            WORKING_DIRECTORY "${source}" RESULT_VARIABLE changed ERROR_QUIET)
        if(changed)
            string(APPEND version "-dirty")
        endif()
    endif()
    set(${output} "${version}" PARENT_SCOPE)
endfunction()
