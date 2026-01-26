cmake_minimum_required(VERSION 3.20)
project(sentinel_multiplayer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================================
# NET LIBRARY
# ============================================================

add_library(sentinel_net STATIC
    src/net/net_api.cpp
    src/net/replication/replication_client.cpp
    src/net/replication/snapshot_buffer.cpp
)

target_include_directories(sentinel_net PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if (WIN32)
    target_sources(sentinel_net PRIVATE
        src/net/transport/udp_win.cpp
    )
    target_link_libraries(sentinel_net PUBLIC ws2_32)
else()
    target_sources(sentinel_net PRIVATE
        src/net/transport/udp_linux.cpp
    )
endif()

# ============================================================
# SDL2
# ============================================================

# Prefer CONFIG mode (vcpkg / official)
find_package(SDL2 CONFIG QUIET)

if (TARGET SDL2::SDL2)
    set(SDL2_TARGET SDL2::SDL2)
elseif(TARGET SDL2::SDL2main)
    set(SDL2_TARGET SDL2::SDL2)
else()
    # Fallback (older FindSDL2.cmake)
    find_package(SDL2 REQUIRED)
    set(SDL2_TARGET ${SDL2_LIBRARIES})
endif()

# ============================================================
# CLIENT
# ============================================================

if (WIN32)
    add_executable(client
        src/client/client_main.cpp
        src/client/render_sky.cpp
        src/client/render_terrain.cpp
    )

    target_include_directories(client PRIVATE
        include
        ${SDL2_INCLUDE_DIRS}
    )

    target_link_libraries(client PRIVATE
        sentinel_net
        ${SDL2_TARGET}
        opengl32
        glu32
    )

    # ========================================================
    # COPY SDL2.dll NEXT TO client.exe (CRITICAL FIX)
    # ========================================================

    if (TARGET SDL2::SDL2)
        add_custom_command(TARGET client POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:SDL2::SDL2>
                $<TARGET_FILE_DIR:client>
        )
    endif()
endif()
