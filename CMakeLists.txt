cmake_minimum_required(VERSION 3.20)

# ============================================================
# REQUIRED: Enable vcpkg MANIFEST MODE
# MUST be before project()
# ============================================================
set(VCPKG_FEATURE_FLAGS "manifests")

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
# GLAD (OpenGL loader)
# ============================================================

add_library(glad STATIC
    external/glad/src/glad.c
)

target_include_directories(glad PUBLIC
    external/glad/include
)

# ============================================================
# SDL2 + SDL2_ttf (vcpkg)
# ============================================================

find_package(SDL2 CONFIG REQUIRED)
find_package(SDL2_ttf CONFIG REQUIRED)

# ============================================================
# SDL2_mixer (MANUAL — NO CMAKE PACKAGE PROVIDED)
# ============================================================

if (WIN32)
    find_library(SDL2_MIXER_LIB
        NAMES SDL2_mixer
        PATHS "${VCPKG_INSTALLED_DIR}/x64-windows/lib"
        REQUIRED
    )
endif()

# ============================================================
# CLIENT
# ============================================================

if (WIN32)
    add_executable(client
        src/client/client_main.cpp
        src/client/render_sky.cpp
        src/client/render_terrain.cpp
        src/client/render_drone_shader.cpp
        src/client/render_drone_mesh.cpp
    )

    target_include_directories(client PRIVATE
        include
        external/glad/include
    )

    target_link_libraries(client PRIVATE
        sentinel_net
        glad
        SDL2::SDL2
        SDL2::SDL2main
        SDL2_ttf::SDL2_ttf
        ${SDL2_MIXER_LIB}
        opengl32
        glu32
    )

    # --------------------------------------------------------
    # Copy SDL DLLs next to client.exe
    # --------------------------------------------------------
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

    # --------------------------------------------------------
    # Copy SDL2_mixer.dll explicitly
    # --------------------------------------------------------
    add_custom_command(TARGET client POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${VCPKG_INSTALLED_DIR}/x64-windows/bin/SDL2_mixer.dll"
            $<TARGET_FILE_DIR:client>
    )

    # --------------------------------------------------------
    # Copy shaders
    # --------------------------------------------------------
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
endif()
