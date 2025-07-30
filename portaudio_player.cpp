#include "gme/gme.h"
#include <portaudio.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

typedef struct {
    Music_Emu* emu;
} AudioData;

static int audioCallback(const void* inputBuffer, void* outputBuffer,
                        unsigned long framesPerBuffer,
                        const PaStreamCallbackTimeInfo* timeInfo,
                        PaStreamCallbackFlags statusFlags,
                        void* userData)
{
    AudioData* data = (AudioData*)userData;
    short* out = (short*)outputBuffer;
    
    if (data->emu) {
        gme_play(data->emu, framesPerBuffer * 2, out);
    } else {
        // Silence
        for (unsigned long i = 0; i < framesPerBuffer * 2; i++) {
            out[i] = 0;
        }
    }
    
    return paContinue;
}

int main(int argc, char* argv[])
{
    if (argc < 2) {
        printf("Usage: %s <music_file> [track_number]\n", argv[0]);
        return 1;
    }

    const char* filename = argv[1];
    int track = (argc > 2) ? atoi(argv[2]) - 1 : 0;
    
    // Initialize PortAudio
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        printf("PortAudio error: %s\n", Pa_GetErrorText(err));
        return 1;
    }

    // Load music file
    Music_Emu* emu = NULL;
    gme_err_t gme_err = gme_open_file(filename, &emu, 44100);
    if (gme_err) {
        printf("Error loading file: %s\n", gme_err);
        Pa_Terminate();
        return 1;
    }

    // Start track
    gme_err = gme_start_track(emu, track);
    if (gme_err) {
        printf("Error starting track: %s\n", gme_err);
        gme_delete(emu);
        Pa_Terminate();
        return 1;
    }

    AudioData audioData = { emu };

    // Setup audio stream
    PaStream* stream;
    err = Pa_OpenDefaultStream(&stream,
                               0,          // no input channels
                               2,          // stereo output
                               paInt16,    // 16-bit samples
                               44100,      // sample rate
                               4096,       // frames per buffer
                               audioCallback,
                               &audioData);

    if (err != paNoError) {
        printf("PortAudio error: %s\n", Pa_GetErrorText(err));
        gme_delete(emu);
        Pa_Terminate();
        return 1;
    }

    err = Pa_StartStream(stream);
    if (err != paNoError) {
        printf("PortAudio error: %s\n", Pa_GetErrorText(err));
        Pa_CloseStream(stream);
        gme_delete(emu);
        Pa_Terminate();
        return 1;
    }

    printf("Playing %s, track %d with PortAudio\n", filename, track + 1);
    printf("Press Enter to stop...\n");

    getchar();

    // Cleanup
    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    gme_delete(emu);
    Pa_Terminate();

    return 0;
}