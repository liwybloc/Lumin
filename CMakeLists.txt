cmake_minimum_required(VERSION 3.16)
project(Lumin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

file(GLOB_RECURSE SOURCES
    src/*.cpp
)

add_executable(lumin ${SOURCES})

target_link_libraries(lumin PRIVATE zstd)
