cmake_minimum_required(VERSION 3.21)
project(topolang_compiler C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

include(FetchContent)

# raylib for raymath.h
FetchContent_Declare(
        raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG 5.0
)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(raylib)

add_library(topolang
        src/arena.c
        src/lexer.c
        src/parser.c
        src/mesh.c
        src/intrinsics.c
        src/eval.c
        src/gltf.c
        src/topolang.c
        src/obj.c
)

target_include_directories(topolang PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${raylib_SOURCE_DIR}/src
)


add_executable(demo examples/demo.c)
target_link_libraries(demo PRIVATE topolang m)
target_link_libraries(topolang PRIVATE raylib_static)
