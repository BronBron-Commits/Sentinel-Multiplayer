cmake_minimum_required(VERSION 3.16)
project(sentinel_multiplayer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------- SIM ----------------
add_library(sentinel_sim
    src/sim/sim.cpp
    src/sim/sim_update.cpp
)

target_include_directories(sentinel_sim
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
)

# ---------------- NET ----------------
add_library(sentinel_net
    src/net/net_api.cpp
    src/net/replication/replication_client.cpp
    src/net/replication/snapshot_buffer.cpp
    src/net/transport/udp_linux.cpp
)

target_include_directories(sentinel_net
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
)

# ---------------- SERVER ----------------
add_executable(server
    src/server/server_main.cpp
)

target_link_libraries(server
    sentinel_sim
    sentinel_net
)

target_include_directories(server
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

# ---------------- CLIENT ----------------
find_package(SDL2 REQUIRED)

add_executable(client
    src/client/client_main.cpp
)

target_link_libraries(client
    sentinel_net
    SDL2::SDL2
)

target_include_directories(client
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)


# -------- OpenGL --------
find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)

target_link_libraries(client
    sentinel_net
    SDL2::SDL2
    OpenGL::GL
    OpenGL::GLU
    GLEW::GLEW
)


# -------- OpenGL --------
find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)

target_link_libraries(client
    sentinel_net
    SDL2::SDL2
    OpenGL::GL
    OpenGL::GLU
    GLEW::GLEW
)


# -------- OpenGL --------
find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)

target_link_libraries(client
    sentinel_net
    SDL2::SDL2
    OpenGL::GL
    OpenGL::GLU
    GLEW::GLEW
)


target_include_directories(client
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src/client
)

