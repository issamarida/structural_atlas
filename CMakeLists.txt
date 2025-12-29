cmake_minimum_required(VERSION 3.16)
project(pk_rk4 C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)

add_executable(pk_rk4 src/main.c)
target_include_directories(pk_rk4 PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(pk_rk4 PRIVATE ${SDL2_LIBRARIES} m)
target_compile_options(pk_rk4 PRIVATE ${SDL2_CFLAGS_OTHER})

enable_testing()
add_subdirectory(gtests)
