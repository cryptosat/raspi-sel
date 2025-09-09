cmake_minimum_required(VERSION 3.16)
project(raspi-sel)

set (CMAKE_CXX_STANDARD 17)

# Turn on common warnings
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wcast-qual -Wold-style-cast -Wnon-virtual-dtor)
    # Promote all warnings to errors
    add_compile_options(-Werror)
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-Werror)
endif()

add_subdirectory(seelie)
add_subdirectory(record)

