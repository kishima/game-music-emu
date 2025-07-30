#include "gme/gme.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

static bool keep_playing = true;

void signal_handler(int sig) {
    keep_playing = false;
}

int main(int argc, char* argv[])
{
    if (argc < 2) {
        printf("Usage: %s <music_file> [track]\n", argv[0]);
        printf("Output: raw PCM to stdout (pipe to aplay/paplay)\n");
        return 1;
    }

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    const char* filename = argv[1];
    int track = (argc > 2) ? atoi(argv[2]) - 1 : 0;

    Music_Emu* emu;
    gme_err_t err = gme_open_file(filename, &emu, 44100);
    if (err) {
        fprintf(stderr, "Error: %s\n", err);
        return 1;
    }

    err = gme_start_track(emu, track);
    if (err) {
        fprintf(stderr, "Error: %s\n", err);
        gme_delete(emu);
        return 1;
    }

    fprintf(stderr, "Streaming track %d from %s...\n", track + 1, filename);
    fprintf(stderr, "Use: %s file.nsf | aplay -f S16_LE -c 2 -r 44100\n", argv[0]);

    short buffer[4410];  // 0.1 second chunks
    
    while (keep_playing && !gme_track_ended(emu)) {
        gme_play(emu, 4410, buffer);
        
        // Write raw PCM to stdout
        if (fwrite(buffer, sizeof(short), 4410, stdout) != 4410) {
            break;  // Broken pipe
        }
        
        fflush(stdout);
    }

    gme_delete(emu);
    return 0;
}