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

add_custom_target(run_tests
    COMMAND ${CMAKE_SOURCE_DIR}/runtests.sh
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Building and running tests"
)
add_custom_target(clean_up
    COMMAND bash -c "cd ${CMAKE_SOURCE_DIR} && rm -rf build && mkdir build && cd build && make run_tests"
)