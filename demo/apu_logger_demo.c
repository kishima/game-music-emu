/* APU Logger Demo - NSF playback with APU register logging
 * 
 * Demonstrates how to use the APU logger to record register writes
 * during NSF playback for later lightweight playback.
 * 
 * Compile with: -DGME_APU_LOGGER
 */

#include "gme/gme.h"

#ifdef GME_APU_LOGGER
#include "gme/Apu_Logger.h"
#endif

#include "Wave_Writer.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

void handle_error( const char* str );

int main(int argc, char *argv[])
{
    const char *filename = "test.nsf"; /* Default file to open */
    if ( argc >= 2 )
        filename = argv[1];

    int sample_rate = 44100;
    int track = argc >= 3 ? atoi(argv[2]) : 0;
    int record_time_sec = argc >= 4 ? atoi(argv[3]) : 10; /* Record time in seconds */

#ifdef GME_APU_LOGGER
    printf("APU Logger enabled - will record register writes\n");
    
    // Initialize APU logger
    apu_logger_init();
    apu_logger_set_enabled(1);
    apu_logger_clear();
    
    printf("APU Logger initialized and enabled\n");
#else
    printf("APU Logger not compiled in (use -DGME_APU_LOGGER)\n");
#endif

    /* Open music file in new emulator */
    Music_Emu* emu;
    handle_error( gme_open_file( filename, &emu, sample_rate ) );

    printf("Opened NSF file: %s\n", filename);
    printf("Track count: %d\n", gme_track_count(emu));

    /* Start track */
    handle_error( gme_start_track( emu, track ) );
    printf("Started track %d\n", track);

#ifdef GME_APU_LOGGER
    // Reset time base at start of track
    if (g_apu_logger) {
        g_apu_logger->set_time_base(gme_tell_samples(emu));
    }
#endif

    /* Begin writing to wave file */
    wave_open( sample_rate, "out.wav" );
    wave_enable_stereo();

    printf("Recording %d seconds to out.wav...\n", record_time_sec);

    /* Record specified time of track */
    while ( gme_tell( emu ) < record_time_sec * 1000L )
    {
        /* Sample buffer */
        #define buf_size 1024
        short buf [buf_size];

        /* Fill sample buffer */
        handle_error( gme_play( emu, buf_size, buf ) );

        /* Write samples to wave file */
        wave_write( buf, buf_size );
        
        /* Print progress every second */
        static int last_second = -1;
        int current_second = gme_tell(emu) / 1000;
        if (current_second != last_second) {
            last_second = current_second;
            printf("Recording: %d/%d seconds\r", current_second, record_time_sec);
            fflush(stdout);
        }
    }
    printf("\n");

#ifdef GME_APU_LOGGER
    // Save APU log
    size_t entry_count = apu_logger_get_entry_count();
    printf("APU Logger recorded %zu register writes\n", entry_count);
    
    if (entry_count > 0) {
        char log_filename_bin[256];
        char log_filename_txt[256];
        
        // Generate output filenames
        snprintf(log_filename_bin, sizeof(log_filename_bin), 
                 "apu_log_track%d.bin", track);
        snprintf(log_filename_txt, sizeof(log_filename_txt), 
                 "apu_log_track%d.txt", track);
        
        // Save binary log
        if (apu_logger_save_binary_c(log_filename_bin)) {
            printf("Saved APU log to: %s\n", log_filename_bin);
        } else {
            printf("Failed to save binary APU log\n");
        }
        
        // Save text log (for debugging)
        if (apu_logger_save_text_c(log_filename_txt)) {
            printf("Saved text APU log to: %s\n", log_filename_txt);
        } else {
            printf("Failed to save text APU log\n");
        }
        
        // Print statistics
        if (g_apu_logger) {
            printf("Memory usage: %zu bytes\n", g_apu_logger->get_memory_usage());
            printf("Average writes per second: %.1f\n", 
                   (double)entry_count / record_time_sec);
        }
    }
    
    // Cleanup
    apu_logger_cleanup();
#endif

    /* Cleanup */
    gme_delete( emu );
    wave_close();

    printf("NSF playback and logging completed successfully\n");
    return 0;
}

void handle_error( const char* str )
{
    if ( str )
    {
        printf( "Error: %s\n", str );
        exit( EXIT_FAILURE );
    }
}

#ifdef GME_APU_LOGGER
/* Example of how to use the logged data for lightweight playback */
void example_playback_from_log(const char* log_filename) {
    printf("\n=== Example: Playback from APU log ===\n");
    
    // Initialize a new logger to load the data
    Apu_Logger logger;
    if (!logger.load_binary(log_filename)) {
        printf("Failed to load APU log: %s\n", log_filename);
        return;
    }
    
    const auto& entries = logger.get_entries();
    printf("Loaded %zu APU register writes\n", entries.size());
    
    // Example: Print first 10 entries
    printf("First 10 entries:\n");
    for (size_t i = 0; i < std::min(entries.size(), size_t(10)); i++) {
        printf("  Time: %8d, Addr: 0x%04X, Data: 0x%02X\n",
               entries[i].time, entries[i].addr, entries[i].data);
    }
    
    // In a real implementation, you would:
    // 1. Initialize just the APU (without CPU)
    // 2. Play back these register writes at the correct timing
    // 3. Generate audio samples from the APU
    printf("\nFor actual playback, implement:\n");
    printf("1. APU-only initialization\n");
    printf("2. Timed register write playback\n");
    printf("3. Audio sample generation\n");
}
#endif