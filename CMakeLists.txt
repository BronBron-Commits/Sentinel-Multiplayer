cmake_minimum_required(VERSION 3.20)

project(sentinel_multiplayer LANGUAGES C CXX)

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
# GLAD (vcpkg)
# ============================================================

find_package(glad CONFIG REQUIRED)

# ============================================================
# SDL2 (vcpkg)
# ============================================================

find_package(SDL2 CONFIG REQUIRED)
find_package(SDL2_ttf CONFIG REQUIRED)
find_package(SDL2_mixer CONFIG REQUIRED)

# ============================================================
# CLIENT EXECUTABLE
# ============================================================

add_executable(client
    src/client/client_main.cpp
    src/client/vfx/combat_fx.cpp

    src/client/render/render_drone_mesh.cpp
    src/client/render/render_drone_shader.cpp
    src/client/render/render_sky.cpp
    src/client/render/render_terrain.cpp

    src/client/input/control_system.cpp
    src/client/audio/audio_system.cpp
    src/client/core/camera_helpers.cpp
    src/client/util/math_util.cpp
    src/client/world/npc_system.cpp
    src/client/world/combat_system.cpp
    src/client/ui/chat_ui.cpp
    src/client/ui/name_entry.cpp
    src/client/vehicles/drone/drone_controller.cpp
    src/client/vehicles/warthog/warthog_controller.cpp
    src/client/vehicles/warthog/render_warthog.cpp
    src/client/render/warthog_shader.cpp
    src/client/vehicles/walker/walker_controller.cpp
    src/client/vehicles/walker/render_walker.cpp
)

target_include_directories(client PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/client
)

target_link_libraries(client PRIVATE
    sentinel_net
    glad::glad

    SDL2::SDL2
    SDL2_ttf::SDL2_ttf
    SDL2_mixer::SDL2_mixer

    opengl32
    glu32
)

if (WIN32)
    target_link_libraries(client PRIVATE SDL2::SDL2main)
endif()

# ============================================================
# POST-BUILD: COPY RUNTIME DLLs (Windows)
# ============================================================

if (WIN32)
    add_custom_command(TARGET client POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL2::SDL2>
            $<TARGET_FILE_DIR:client>
    )

    add_custom_command(TARGET client POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL2_ttf::SDL2_ttf>
            $<TARGET_FILE_DIR:client>
    )

    add_custom_command(TARGET client POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL2_mixer::SDL2_mixer>
            $<TARGET_FILE_DIR:client>
    )
endif()

# ============================================================
# POST-BUILD: COPY SHADERS
# ============================================================

file(GLOB_RECURSE SHADER_FILES
    ${CMAKE_SOURCE_DIR}/assets/shaders/*.vert
    ${CMAKE_SOURCE_DIR}/assets/shaders/*.frag
)

foreach(SHADER ${SHADER_FILES})
    add_custom_command(TARGET client POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${SHADER}
            $<TARGET_FILE_DIR:client>/assets/shaders
    )
endforeach()
