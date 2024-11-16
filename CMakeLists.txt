cmake_minimum_required(VERSION 3.29)
project(testyy)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBINPUT REQUIRED libinput)

include_directories(${LIBINPUT_INCLUDE_DIRS})
link_libraries(${LIBINPUT_LIBRARIES})

set(CMAKE_CXX_STANDARD 17)

add_executable(testyy main.cpp)
