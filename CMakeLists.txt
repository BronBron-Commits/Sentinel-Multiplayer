cmake_minimum_required(VERSION 3.16)
project(sentinel_multiplayer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)
pkg_check_modules(SDL2_TTF REQUIRED SDL2_ttf)

add_executable(drone-client
    src/client_main.cpp
)

target_include_directories(drone-client PRIVATE
    ${SDL2_INCLUDE_DIRS}
    ${SDL2_TTF_INCLUDE_DIRS}
)

target_link_libraries(drone-client
    ${SDL2_LIBRARIES}
    ${SDL2_TTF_LIBRARIES}
    GL
)

