cmake_minimum_required(VERSION 3.18)

project(dbus_demo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

add_executable(dbus_demo main.cc)

set_target_properties(dbus_demo PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

find_package(PkgConfig)

pkg_check_modules(DBUS REQUIRED IMPORTED_TARGET dbus-1)

target_link_libraries(dbus_demo PRIVATE PkgConfig::DBUS)