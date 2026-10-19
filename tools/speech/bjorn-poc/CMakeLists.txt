cmake_minimum_required(VERSION 3.24)
project(bjorn_copy_synthesis LANGUAGES C)
# Independent experiment: deliberately does not configure or link the application.
add_executable(bjorn-render render.c)
set_target_properties(bjorn-render PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
if(NOT MSVC)
    target_compile_options(bjorn-render PRIVATE -Wall -Wextra -Werror)
    target_link_libraries(bjorn-render PRIVATE m)
endif()
