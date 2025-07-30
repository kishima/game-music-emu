#include "gme/gme.h"
#include "SDL.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Music_Emu* emu = NULL;
static bool playing = true;
static int sample_rate = 44100;
static int buf_size = 16384;

void audio_callback(void* userdata, Uint8* stream, int len)
{
    if (!emu || !playing) {
        memset(stream, 0, len);
        return;
    }
    
    gme_play(emu, len / 2, (short*)stream);
}

int main(int argc, char* argv[])
{
    if (argc < 2) {
        printf("Usage: %s <music_file> [track_number]\n", argv[0]);
        return 1;
    }

    const char* filename = argv[1];
    int track = (argc > 2) ? atoi(argv[2]) - 1 : 0;

    // Initialize SDL audio only
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        printf("SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    // Load the music file
    gme_err_t err = gme_open_file(filename, &emu, sample_rate);
    if (err) {
        printf("Error loading file: %s\n", err);
        SDL_Quit();
        return 1;
    }

    // Start the track
    err = gme_start_track(emu, track);
    if (err) {
        printf("Error starting track: %s\n", err);
        gme_delete(emu);
        SDL_Quit();
        return 1;
    }

    // Setup audio
    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = sample_rate;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = buf_size;
    want.callback = audio_callback;

    SDL_AudioDeviceID dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (dev == 0) {
        printf("Failed to open audio: %s\n", SDL_GetError());
        gme_delete(emu);
        SDL_Quit();
        return 1;
    }

    printf("Playing %s, track %d\n", filename, track + 1);
    printf("Sample rate: %d Hz, Buffer: %d samples\n", have.freq, have.samples);
    printf("Press Enter to stop...\n");

    SDL_PauseAudioDevice(dev, 0);  // Start playing

    // Wait for user input
    getchar();

    // Cleanup
    SDL_CloseAudioDevice(dev);
    gme_delete(emu);
    SDL_Quit();

    return 0;
}