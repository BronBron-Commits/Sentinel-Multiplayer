#include "client/audio/audio_system.hpp"

#include <SDL_mixer.h>
#include <cstdio>
#include <cmath>

// ------------------------------------------------------------
// Internal audio state (PRIVATE TO THIS FILE)
// ------------------------------------------------------------
static Mix_Music* g_music = nullptr;
static Mix_Chunk* g_drone_move_sfx = nullptr;
static Mix_Chunk* g_drone_move_boost_sfx = nullptr;

static int  g_drone_move_channel = -1;
static bool g_boost_active = false;

// Volume tuning
constexpr int DRONE_BASE_VOLUME = MIX_MAX_VOLUME / 5;

// ------------------------------------------------------------
// Init
// ------------------------------------------------------------
bool audio_init()
{
    int flags = MIX_INIT_OGG | MIX_INIT_MP3;
    if ((Mix_Init(flags) & flags) != flags) {
        printf("[audio] Mix_Init failed: %s\n", Mix_GetError());
        return false;
    }

    if (Mix_OpenAudio(48000, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        printf("[audio] Mix_OpenAudio failed: %s\n", Mix_GetError());
        return false;
    }

    Mix_AllocateChannels(16);

    // Load assets
    g_music = Mix_LoadMUS("assets/audio/main_loop.ogg");
    if (!g_music) {
        printf("[audio] Failed to load music: %s\n", Mix_GetError());
    }
    else {
        Mix_VolumeMusic(MIX_MAX_VOLUME / 2);
        Mix_PlayMusic(g_music, -1);
    }

    g_drone_move_sfx = Mix_LoadWAV("assets/audio/drone_move.wav");
    g_drone_move_boost_sfx = Mix_LoadWAV("assets/audio/drone_move_boost.wav");

    if (!g_drone_move_sfx)
        printf("[audio] Failed to load drone_move.wav\n");

    if (!g_drone_move_boost_sfx)
        printf("[audio] Failed to load drone_move_boost.wav\n");

    return true;
}

// ------------------------------------------------------------
// Runtime update
// ------------------------------------------------------------
void audio_on_drone_move(bool moving, bool boost)
{
    if (!g_drone_move_sfx)
        return;

    Mix_Chunk* desired =
        (boost && g_drone_move_boost_sfx)
        ? g_drone_move_boost_sfx
        : g_drone_move_sfx;

    if (moving) {
        if (g_drone_move_channel == -1 || g_boost_active != boost) {

            if (g_drone_move_channel != -1)
                Mix_HaltChannel(g_drone_move_channel);

            g_drone_move_channel = Mix_PlayChannel(-1, desired, -1);
            g_boost_active = boost;
        }

        int vol = boost
            ? MIX_MAX_VOLUME
            : DRONE_BASE_VOLUME;

        Mix_Volume(g_drone_move_channel, vol);
    }
    else {
        if (g_drone_move_channel != -1) {
            Mix_HaltChannel(g_drone_move_channel);
            g_drone_move_channel = -1;
        }
    }
}

// ------------------------------------------------------------
// Shutdown
// ------------------------------------------------------------
void audio_shutdown()
{
    if (g_music) {
        Mix_HaltMusic();
        Mix_FreeMusic(g_music);
        g_music = nullptr;
    }

    if (g_drone_move_sfx) {
        Mix_FreeChunk(g_drone_move_sfx);
        g_drone_move_sfx = nullptr;
    }

    if (g_drone_move_boost_sfx) {
        Mix_FreeChunk(g_drone_move_boost_sfx);
        g_drone_move_boost_sfx = nullptr;
    }

    Mix_CloseAudio();
    Mix_Quit();
}
