cmake_minimum_required(VERSION 3.16)
# ✅ REQUIRED for SDL2_ttf 2.22.x with CMake 4.x
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
project(sentinel_multiplayer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -------------------------------------------------
# Options
# -------------------------------------------------
option(ENABLE_MULTIPLAYER "Enable multiplayer networking" ON)
option(BUILD_SERVER "Build server binary" ON)

if (ENABLE_MULTIPLAYER)
    add_compile_definitions(ENABLE_MULTIPLAYER)
endif()

# -------------------------------------------------
# Dependencies
# -------------------------------------------------
if (WIN32)
    # Windows: use CMake config packages (NO pkg-config)
    find_package(SDL2 CONFIG REQUIRED)
    find_package(SDL2_ttf CONFIG REQUIRED)
else()
    # Linux: pkg-config
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED sdl2)
    pkg_check_modules(SDL2_TTF REQUIRED SDL2_ttf)
endif()

# -------------------------------------------------
# OpenGL
# -------------------------------------------------
if (WIN32)
    set(OPENGL_LIB opengl32)
else()
    find_package(OpenGL REQUIRED)
    set(OPENGL_LIB OpenGL::GL)
endif()

# -------------------------------------------------
# Client (SDL + OpenGL)
# -------------------------------------------------
add_executable(drone-client
    src/client_main.cpp
    src/firework.cpp
)

if (ENABLE_MULTIPLAYER)
    if (WIN32)
        target_sources(drone-client PRIVATE
            src/net/net_udp_win.cpp
        )
        target_link_libraries(drone-client ws2_32)
    else()
        target_sources(drone-client PRIVATE
            src/net/net_udp_linux.cpp
        )
    endif()
else()
    target_sources(drone-client PRIVATE
        src/net/net_stub.cpp
    )
endif()


target_include_directories(drone-client PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

if (WIN32)
    target_link_libraries(drone-client
        SDL2::SDL2
        SDL2::SDL2main
        SDL2_ttf::SDL2_ttf
        ${OPENGL_LIB}
    )
else()
    target_include_directories(drone-client PRIVATE
        ${SDL2_INCLUDE_DIRS}
        ${SDL2_TTF_INCLUDE_DIRS}
    )

    target_link_libraries(drone-client
        ${SDL2_LIBRARIES}
        ${SDL2_TTF_LIBRARIES}
        ${OPENGL_LIB}
        m
    )
endif()

# -------------------------------------------------
# Server (headless authoritative)
# -------------------------------------------------
if (BUILD_SERVER)
    add_executable(drone-server
        src/server_main.cpp
        src/net/net_stub.cpp
    )

    target_include_directories(drone-server PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )

    if (WIN32)
        target_link_libraries(drone-server
            ws2_32
        )
    else()
        target_link_libraries(drone-server
            pthread
        )
    endif()
endif()
