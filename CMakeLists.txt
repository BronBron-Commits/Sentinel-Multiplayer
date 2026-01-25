cmake_minimum_required(VERSION 3.20)
project(sentinel_multiplayer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================================
# SIM LIBRARY
# ============================================================

add_library(sentinel_sim STATIC
    src/sim/sim.cpp
    src/sim/sim_update.cpp
)

target_include_directories(sentinel_sim PUBLIC
    include
)

# ============================================================
# NET LIBRARY
# ============================================================
add_library(sentinel_net
    src/net/net_api.cpp

    # replication
    src/net/replication/replication_client.cpp
    src/net/replication/snapshot_buffer.cpp

    # transport
    src/net/transport/udp_win.cpp
)

# THIS LINE IS THE FIX
target_include_directories(sentinel_net
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if (WIN32)
    target_link_libraries(sentinel_net
        PUBLIC
            ws2_32
    )
endif()

# ============================================================
# SDL2 (WINDOWS-SAFE)
# ============================================================

find_package(SDL2 CONFIG QUIET)

if (TARGET SDL2::SDL2)
    set(SDL2_TARGET SDL2::SDL2)
else()
    find_package(SDL2 REQUIRED)
    set(SDL2_TARGET ${SDL2_LIBRARIES})
endif()

# ============================================================
# CLIENT
# ============================================================
add_executable(client
    src/client/client_main.cpp
)

target_include_directories(client PRIVATE
    include
    ${SDL2_INCLUDE_DIRS}
)

target_link_libraries(client
    sentinel_net
    ${SDL2_TARGET}
    opengl32
    glu32
)


# ============================================================
# SERVER
# ============================================================

add_executable(server
    src/server/server_main.cpp
)

target_link_libraries(server
    sentinel_net
    sentinel_sim
)
