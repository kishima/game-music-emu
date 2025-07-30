#include "gme/gme.h"
#include <stdio.h>
#include <stdlib.h>
#include <cstdint>

// Simple WAV header
struct WAVHeader {
    char riff[4] = {'R','I','F','F'};
    uint32_t fileSize;
    char wave[4] = {'W','A','V','E'};
    char fmt[4] = {'f','m','t',' '};
    uint32_t fmtSize = 16;
    uint16_t format = 1;
    uint16_t channels = 2;
    uint32_t sampleRate = 44100;
    uint32_t byteRate = 44100 * 2 * 2;
    uint16_t blockAlign = 4;
    uint16_t bitsPerSample = 16;
    char data[4] = {'d','a','t','a'};
    uint32_t dataSize;
};

int main(int argc, char* argv[])
{
    if (argc < 2) {
        printf("Usage: %s <music_file> [track] [seconds]\n", argv[0]);
        return 1;
    }

    const char* filename = argv[1];
    int track = (argc > 2) ? atoi(argv[2]) - 1 : 0;
    int seconds = (argc > 3) ? atoi(argv[3]) : 30;

    Music_Emu* emu;
    gme_err_t err = gme_open_file(filename, &emu, 44100);
    if (err) {
        printf("Error: %s\n", err);
        return 1;
    }

    err = gme_start_track(emu, track);
    if (err) {
        printf("Error: %s\n", err);
        gme_delete(emu);
        return 1;
    }

    char outname[256];
    sprintf(outname, "output_track_%d.wav", track + 1);
    FILE* f = fopen(outname, "wb");
    if (!f) {
        printf("Cannot create %s\n", outname);
        gme_delete(emu);
        return 1;
    }

    int samples = 44100 * 2 * seconds;  // stereo samples for duration
    WAVHeader header;
    header.dataSize = samples * 2;      // bytes
    header.fileSize = sizeof(WAVHeader) - 8 + header.dataSize;

    fwrite(&header, sizeof(header), 1, f);

    short* buffer = new short[8192];
    int remaining = samples;

    printf("Writing %d seconds to %s...\n", seconds, outname);

    while (remaining > 0) {
        int count = (remaining > 8192) ? 8192 : remaining;
        gme_play(emu, count, buffer);
        fwrite(buffer, sizeof(short), count, f);
        remaining -= count;
    }

    delete[] buffer;
    fclose(f);
    gme_delete(emu);

    printf("Done! Play %s with any audio player\n", outname);
    return 0;
}