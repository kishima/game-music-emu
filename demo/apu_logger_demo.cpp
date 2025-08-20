/* APU Logger Demo - NSF playback with APU register logging
 * 
 * Demonstrates how to use the APU logger to record register writes
 * during NSF playback for later lightweight playback.
 * 
 * Compile with: -DGME_APU_LOGGER
 */

#include "gme/gme.h"
#include "gme/Apu_Logger.h"

#include "Wave_Writer.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

extern "C" {
    void handle_error( const char* str );
}

void show_usage(const char* program_name) {
    printf("Usage:\n");
    printf("  %s <nsf_file> [track] [duration]     - Record APU log from NSF\n", program_name);
    printf("  %s --parse <bin_file>                - Parse and display APU log\n", program_name);
    printf("\nExamples:\n");
    printf("  %s test.nsf 0 10                     - Record track 0 for 10 seconds\n", program_name);
    printf("  %s --parse apu_log_track0.bin        - Parse and display log file\n", program_name);
}

bool parse_apu_log(const char* bin_filename) {
    printf("Parsing APU log file: %s\n", bin_filename);
    
    Apu_Logger logger;
    if (!logger.load_binary(bin_filename)) {
        printf("Error: Failed to load APU log file: %s\n", bin_filename);
        return false;
    }
    
    const auto& entries = logger.get_entries();
    printf("\n=== APU Register Log Analysis ===\n");
    printf("Total entries: %zu\n", entries.size());
    
    if (entries.empty()) {
        printf("No entries found in log file.\n");
        return true;
    }
    
    // Display header
    printf("\n%8s %8s %4s %s\n", "Entry#", "Time", "Addr", "Data");
    printf("-------- -------- ---- ----\n");
    
    // Display all entries
    for (size_t i = 0; i < entries.size(); i++) {
        const auto& entry = entries[i];
        printf("%8zu %8d %04X %02X\n", 
               i + 1, entry.time, entry.addr, entry.data);
    }
    
    // Statistics
    if (entries.size() > 1) {
        int32_t total_time = entries.back().time - entries.front().time;
        double duration_sec = total_time / 1789773.0; // NTSC CPU frequency
        printf("\n=== Statistics ===\n");
        printf("Duration: %.3f seconds (%d CPU cycles)\n", duration_sec, total_time);
        printf("Average writes per second: %.1f\n", entries.size() / duration_sec);
        
        // Register usage analysis
        int reg_count[0x18] = {0}; // 0x4000-0x4017
        for (const auto& entry : entries) {
            if (entry.addr >= 0x4000 && entry.addr <= 0x4017) {
                reg_count[entry.addr - 0x4000]++;
            }
        }
        
        printf("\n=== Register Usage ===\n");
        const char* reg_names[] = {
            "4000 Pulse1_Vol", "4001 Pulse1_Sweep", "4002 Pulse1_Lo", "4003 Pulse1_Hi",
            "4004 Pulse2_Vol", "4005 Pulse2_Sweep", "4006 Pulse2_Lo", "4007 Pulse2_Hi", 
            "4008 Tri_Linear", "4009 Reserved", "400A Tri_Lo", "400B Tri_Hi",
            "400C Noise_Vol", "400D Reserved", "400E Noise_Lo", "400F Noise_Hi",
            "4010 DMC_Freq", "4011 DMC_Raw", "4012 DMC_Start", "4013 DMC_Len",
            "4014 OAM_DMA", "4015 Status", "4016 Joypad1", "4017 Joypad2"
        };
        
        for (int i = 0; i < 0x18; i++) {
            if (reg_count[i] > 0) {
                printf("%s: %d writes\n", reg_names[i], reg_count[i]);
            }
        }
    }
    
    return true;
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        show_usage(argv[0]);
        return 1;
    }
    
    // Check for parse mode
    if (argc >= 3 && strcmp(argv[1], "--parse") == 0) {
        return parse_apu_log(argv[2]) ? 0 : 1;
    }
    
    // NSF recording mode
    const char *filename = argv[1];
    int sample_rate = 44100;
    int track = argc >= 3 ? atoi(argv[2]) : 0;
    int record_time_sec = argc >= 4 ? atoi(argv[3]) : 10; /* Record time in seconds */

    printf("APU Logger enabled - will record register writes\n");
    
    // Initialize APU logger using library function
    init_apu_logger();
    Apu_Logger* logger = get_apu_logger();
    if (logger) {
        get_apu_logger()->set_enabled(true);
        get_apu_logger()->clear();
        printf("APU Logger initialized and enabled\n");
        printf("Global logger address: %p\n", get_apu_logger());
        printf("Logger enabled: %d\n", get_apu_logger()->is_enabled());
    } else {
        printf("Failed to initialize APU Logger\n");
        exit(1);
    }

    /* Open music file in new emulator */
    Music_Emu* emu;
    handle_error( gme_open_file( filename, &emu, sample_rate ) );

    printf("Opened NSF file: %s\n", filename);
    printf("Track count: %d\n", gme_track_count(emu));

    /* Start track */
    handle_error( gme_start_track( emu, track ) );
    printf("Started track %d\n", track);

    // Reset time base at start of track
    logger->set_time_base(gme_tell_samples(emu));

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

    // Save APU log
    size_t entry_count = 0;
    if (logger) {
        entry_count = logger->entry_count();
    }
    printf("APU Logger recorded %zu register writes\n", entry_count);
    
    if (entry_count > 0 && logger) {
        char log_filename_bin[256];
        char log_filename_txt[256];
        
        // Generate output filenames
        snprintf(log_filename_bin, sizeof(log_filename_bin), 
                 "apu_log_track%d.bin", track);
        snprintf(log_filename_txt, sizeof(log_filename_txt), 
                 "apu_log_track%d.txt", track);
        
        // Save binary log
        if (logger->save_binary(log_filename_bin)) {
            printf("Saved APU log to: %s\n", log_filename_bin);
        } else {
            printf("Failed to save binary APU log\n");
        }
        
        // Save text log (for debugging)
        if (logger->save_text(log_filename_txt)) {
            printf("Saved text APU log to: %s\n", log_filename_txt);
        } else {
            printf("Failed to save text APU log\n");
        }
        
        // Print statistics
        printf("Memory usage: %zu bytes\n", logger->get_memory_usage());
        printf("Average writes per second: %.1f\n", 
               (double)entry_count / record_time_sec);
    }
    
    // Cleanup - no explicit cleanup needed, managed by library

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
