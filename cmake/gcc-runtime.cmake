# Resolve libraries from the selected compiler, rather than the system compiler.
set(perikop_gcc_runtimes libstdc++.so.6 libgcc_s.so.1)
foreach(runtime IN LISTS perikop_gcc_runtimes)
    execute_process(COMMAND "${CMAKE_CXX_COMPILER}" "-print-file-name=${runtime}"
        OUTPUT_VARIABLE runtime_path OUTPUT_STRIP_TRAILING_WHITESPACE
        COMMAND_ERROR_IS_FATAL ANY)
    if(NOT IS_ABSOLUTE "${runtime_path}" OR NOT EXISTS "${runtime_path}")
        message(FATAL_ERROR "The selected compiler cannot locate ${runtime}")
    endif()
    file(REAL_PATH "${runtime_path}" runtime_path)
    set("perikop_runtime_${runtime}" "${runtime_path}")
    get_filename_component(runtime_directory "${runtime_path}" DIRECTORY)
    # Test executables must also work with a compiler installed outside /usr.
    list(APPEND CMAKE_BUILD_RPATH "${runtime_directory}")
endforeach()
list(REMOVE_DUPLICATES CMAKE_BUILD_RPATH)
