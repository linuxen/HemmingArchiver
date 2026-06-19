cmake_minimum_required(VERSION 3.20)
project(HamArc)

set(CMAKE_CXX_STANDARD 20)

add_executable(hamarc
    main/main.cpp
    bin/archive.cpp
    bin/parser.cpp
    bin/codec.cpp
)

target_include_directories(hamarc PRIVATE bin)

enable_testing()
add_subdirectory(tests)