cmake_minimum_required(VERSION 3.16)
project(sentinel_multiplayer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -------------------------------------------------
# Dependencies
# -------------------------------------------------
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)
pkg_check_modules(SDL2_TTF REQUIRED SDL2_ttf)

# -------------------------------------------------
# Options
# -------------------------------------------------
option(ENABLE_MULTIPLAYER "Enable multiplayer networking" ON)

if(ENABLE_MULTIPLAYER)
    add_compile_definitions(ENABLE_MULTIPLAYER)
endif()

# -------------------------------------------------
# Client (SDL + OpenGL)
# -------------------------------------------------
add_executable(drone-client
    src/client_main.cpp
    src/net/net_stub.cpp
    src/firework.cpp          # 🔥 REQUIRED
)

target_include_directories(drone-client PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${SDL2_INCLUDE_DIRS}
    ${SDL2_TTF_INCLUDE_DIRS}
)

target_link_libraries(drone-client
    ${SDL2_LIBRARIES}
    ${SDL2_TTF_LIBRARIES}
    GL
)

# -------------------------------------------------
# Server (headless authoritative)
# -------------------------------------------------
add_executable(drone-server
    src/server_main.cpp
    src/net/net_stub.cpp
)

target_include_directories(drone-server PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(drone-server
    pthread
)

