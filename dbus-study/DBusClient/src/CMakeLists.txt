add_executable(dbus_demo main.cc DBusClient.cc)
set_target_properties(
    dbus_demo PROPERTIES 
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    INCLUDE_DIRECTORIES ${CMAKE_SOURCE_DIR}/include
)

find_package(PkgConfig)
pkg_check_modules(DBUS REQUIRED IMPORTED_TARGET dbus-1)
target_link_libraries(dbus_demo PRIVATE PkgConfig::DBUS)