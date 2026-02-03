#pragma once

// Initializes SDL_mixer, loads sounds, starts music
bool audio_init();

// Called every frame to manage drone movement audio
void audio_on_drone_move(bool moving, bool boost);

// Shuts down audio system cleanly
void audio_shutdown();
