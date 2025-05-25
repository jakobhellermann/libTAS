cmake_minimum_required(VERSION 3.16)

project(libTAS)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

find_package(PkgConfig)
pkg_check_modules(X11 REQUIRED IMPORTED_TARGET x11)
pkg_check_modules(XCB REQUIRED IMPORTED_TARGET xcb)
pkg_check_modules(XCB_XKB REQUIRED IMPORTED_TARGET xcb-xkb)
pkg_check_modules(XCB_KEYSYMS REQUIRED IMPORTED_TARGET xcb-keysyms)
pkg_check_modules(LIBCAP REQUIRED IMPORTED_TARGET libcap)
pkg_check_modules(LUA REQUIRED IMPORTED_TARGET lua54)

add_compile_options(-O2 -g -Wall -pedantic)

add_subdirectory(./src/library)
add_subdirectory(./src/program)

install(TARGETS libTAS libTAS_program DESTINATION bin)
install(FILES libTAS.desktop DESTINATION share/applications)
install(FILES libTAS.svg DESTINATION share/icons/hicolor/scalable/apps)

install(CODE "execute_process(COMMAND setcap cap_checkpoint_restore+eip ${CMAKE_BINARY_DIR}/libTAS)")
