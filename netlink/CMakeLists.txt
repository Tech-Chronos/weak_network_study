cmake_minimum_required(VERSION 3.18)

project(NETLINK)

add_executable(test test.cc)

set(CMAKE_BUILD_TYPE Debug)

set_target_properties(
    test PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)