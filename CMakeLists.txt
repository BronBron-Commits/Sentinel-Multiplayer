cmake_minimum_required(VERSION 3.16)
project(sentinel_multiplayer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------- Sim ----------------
add_library(sentinel_sim
    src/sim/sim.cpp
)
target_include_directories(sentinel_sim PUBLIC include)

# ---------------- Net ----------------
add_library(sentinel_net
    src/net/net_api.cpp
    src/net/replication/snapshot_buffer.cpp
    src/net/replication/replication_client.cpp
    src/net/transport/udp_linux.cpp
)
target_include_directories(sentinel_net PUBLIC include)

# ---------------- Client ----------------
add_executable(client
    src/client/client_main.cpp
)
target_link_libraries(client
    sentinel_net
    sentinel_sim
    SDL2
    GL
)

# ---------------- Server ----------------
add_executable(server
    src/server/server_main.cpp
)
target_link_libraries(server
    sentinel_net
    sentinel_sim
)
