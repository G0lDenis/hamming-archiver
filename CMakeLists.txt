cmake_minimum_required(VERSION 3.12)

project(hamming_archiver)

set(CMAKE_CXX_STANDARD 20)

add_subdirectory(argument_parser/lib)
add_subdirectory(bin)
add_subdirectory(lib)
