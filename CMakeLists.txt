cmake_minimum_required(VERSION 3.20)

# IMPORTANT: GLAD IS C CODE
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
    ${CMAKE_CURRENT_SOURCE_DIR}/external/glad/src/glad.c
)

target_include_directories(glad PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/external/glad/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/glad/include/glad
)

# ============================================================
# SDL2
# ============================================================

find_package(SDL2 CONFIG QUIET)

if (TARGET SDL2::SDL2)
    set(SDL2_TARGET SDL2::SDL2)
elseif (TARGET SDL2::SDL2main)
    set(SDL2_TARGET SDL2::SDL2)
else()
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
    src/client/render_drone_shader.cpp
)


    target_include_directories(client PRIVATE
        include
        ${CMAKE_CURRENT_SOURCE_DIR}/external/glad/include
        ${SDL2_INCLUDE_DIRS}
    )

    target_link_libraries(client PRIVATE
        sentinel_net
        glad
        ${SDL2_TARGET}
        opengl32
        glu32
    )

    # --------------------------------------------------------
    # Copy SDL2.dll next to client.exe
    # --------------------------------------------------------
    if (TARGET SDL2::SDL2)
        add_custom_command(TARGET client POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:SDL2::SDL2>
                $<TARGET_FILE_DIR:client>
        )
    endif()

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
