/* APU Binary Log Parser - Standalone C implementation
 * 
 * Parses APU logger binary files and displays their contents
 * Based on the format defined in Apu_Logger.h
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* APU event types */
typedef enum {
    APU_EVENT_WRITE = 0,
    APU_EVENT_INIT_START,
    APU_EVENT_INIT_END,
    APU_EVENT_PLAY_START,
    APU_EVENT_PLAY_END
} apu_event_type_t;

/* Binary file format header */
typedef struct {
    char magic[8];
    uint32_t version;
    uint32_t entry_count;
    uint32_t frame_count;
    uint32_t reserved[3];
} apu_log_header_t;

/* APU register write event */
typedef struct {
    int32_t time;
    uint16_t addr;
    uint8_t data;
    uint8_t event_type;
    uint32_t frame_number;
} apu_log_entry_t;

/* Register names for display */
static const char* get_register_name(uint16_t addr) {
    static const char* reg_names[] = {
        "Pulse1_Vol", "Pulse1_Sweep", "Pulse1_Lo", "Pulse1_Hi",
        "Pulse2_Vol", "Pulse2_Sweep", "Pulse2_Lo", "Pulse2_Hi",
        "Tri_Linear", "Reserved", "Tri_Lo", "Tri_Hi",
        "Noise_Vol", "Reserved", "Noise_Lo", "Noise_Hi",
        "DMC_Freq", "DMC_Raw", "DMC_Start", "DMC_Len",
        "OAM_DMA", "Status", "Joypad1", "Joypad2"
    };
    
    if (addr >= 0x4000 && addr <= 0x4017) {
        return reg_names[addr - 0x4000];
    }
    return "Unknown";
}

/* Parse and display binary log file */
bool parse_apu_log(const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        fprintf(stderr, "Error: Cannot open file '%s'\n", filename);
        return false;
    }
    
    /* Read header */
    apu_log_header_t header;
    if (fread(&header, sizeof(header), 1, file) != 1) {
        fprintf(stderr, "Error: Failed to read header\n");
        fclose(file);
        return false;
    }
    
    /* Verify magic */
    if (memcmp(header.magic, "APULOG\0\0", 8) != 0) {
        fprintf(stderr, "Error: Invalid file format (bad magic)\n");
        fclose(file);
        return false;
    }
    
    printf("=== APU Binary Log File ===\n");
    printf("File: %s\n", filename);
    printf("Format version: %u\n", header.version);
    printf("Entry count: %u\n", header.entry_count);
    printf("Frame count: %u\n", header.frame_count);
    printf("\n");
    
    if (header.entry_count == 0) {
        printf("No entries in log file.\n");
        fclose(file);
        return true;
    }
    
    /* Allocate memory for entries */
    apu_log_entry_t* entries = malloc(header.entry_count * sizeof(apu_log_entry_t));
    if (!entries) {
        fprintf(stderr, "Error: Failed to allocate memory for entries\n");
        fclose(file);
        return false;
    }
    
    /* Read entries */
    size_t entries_read = fread(entries, sizeof(apu_log_entry_t), header.entry_count, file);
    if (entries_read != header.entry_count) {
        fprintf(stderr, "Error: Expected %u entries, read %zu\n", header.entry_count, entries_read);
        free(entries);
        fclose(file);
        return false;
    }
    
    fclose(file);
    
    /* Display entries */
    bool in_init = false;
    bool in_play = false;
    uint32_t current_frame = 0;
    
    printf("=== Log Entries ===\n");
    printf("   Index     Time  Addr  Data  Description\n");
    printf("-------- -------- ------ ---- -----------\n");
    
    for (uint32_t i = 0; i < header.entry_count; i++) {
        const apu_log_entry_t* entry = &entries[i];
        
        switch (entry->event_type) {
            case APU_EVENT_INIT_START:
                printf("\n>>> INIT START (Time %d) <<<\n", entry->time);
                in_init = true;
                break;
                
            case APU_EVENT_INIT_END:
                printf(">>> INIT END (Time %d) <<<\n\n", entry->time);
                in_init = false;
                break;
                
            case APU_EVENT_PLAY_START:
                printf("\n>>> PLAY START (Frame %u, Time %d) <<<\n", entry->frame_number, entry->time);
                in_play = true;
                current_frame = entry->frame_number;
                break;
                
            case APU_EVENT_PLAY_END:
                printf(">>> PLAY END (Frame %u, Time %d) <<<\n\n", entry->frame_number, entry->time);
                in_play = false;
                break;
                
            case APU_EVENT_WRITE:
            default:
                printf("%8u %8d 0x%04X 0x%02X %s", 
                       i + 1, entry->time, entry->addr, entry->data, get_register_name(entry->addr));
                if (in_init) printf(" [INIT]");
                else if (in_play) printf(" [PLAY Frame %u]", current_frame);
                printf("\n");
                break;
        }
    }
    
    /* Statistics */
    printf("\n=== Statistics ===\n");
    
    /* Count register writes */
    int reg_count[0x18] = {0};
    int write_count = 0;
    int32_t max_time = 0;
    
    for (uint32_t i = 0; i < header.entry_count; i++) {
        const apu_log_entry_t* entry = &entries[i];
        if (entry->event_type == APU_EVENT_WRITE) {
            write_count++;
            if (entry->addr >= 0x4000 && entry->addr <= 0x4017) {
                reg_count[entry->addr - 0x4000]++;
            }
            if (entry->time > max_time) {
                max_time = entry->time;
            }
        }
    }
    
    printf("Total register writes: %d\n", write_count);
    
    if (header.frame_count > 0) {
        double duration_sec = header.frame_count / 60.0; /* 60Hz NTSC */
        printf("Duration: %.3f seconds (%u frames @ 60Hz)\n", duration_sec, header.frame_count);
        printf("Average writes per frame: %.1f\n", (double)write_count / header.frame_count);
        printf("Average writes per second: %.1f\n", write_count / duration_sec);
    }
    
    printf("Max time value: %d CPU cycles\n", max_time);
    
    /* Register usage */
    printf("\n=== Register Usage ===\n");
    for (int i = 0; i < 0x18; i++) {
        if (reg_count[i] > 0) {
            printf("$%04X %s: %d writes\n", 0x4000 + i, get_register_name(0x4000 + i), reg_count[i]);
        }
    }
    
    /* Memory info */
    printf("\n=== Memory Info ===\n");
    printf("Header size: %zu bytes\n", sizeof(apu_log_header_t));
    printf("Entry size: %zu bytes\n", sizeof(apu_log_entry_t));
    printf("Total file size: %zu bytes\n", sizeof(apu_log_header_t) + header.entry_count * sizeof(apu_log_entry_t));
    
    free(entries);
    return true;
}

/* Export to text format */
bool export_to_text(const char* bin_filename, const char* txt_filename) {
    FILE* file = fopen(bin_filename, "rb");
    if (!file) {
        fprintf(stderr, "Error: Cannot open input file '%s'\n", bin_filename);
        return false;
    }
    
    /* Read header */
    apu_log_header_t header;
    if (fread(&header, sizeof(header), 1, file) != 1) {
        fprintf(stderr, "Error: Failed to read header\n");
        fclose(file);
        return false;
    }
    
    /* Verify magic */
    if (memcmp(header.magic, "APULOG\0\0", 8) != 0) {
        fprintf(stderr, "Error: Invalid file format\n");
        fclose(file);
        return false;
    }
    
    /* Allocate and read entries */
    apu_log_entry_t* entries = malloc(header.entry_count * sizeof(apu_log_entry_t));
    if (!entries) {
        fprintf(stderr, "Error: Failed to allocate memory\n");
        fclose(file);
        return false;
    }
    
    if (fread(entries, sizeof(apu_log_entry_t), header.entry_count, file) != header.entry_count) {
        fprintf(stderr, "Error: Failed to read entries\n");
        free(entries);
        fclose(file);
        return false;
    }
    
    fclose(file);
    
    /* Write text file */
    FILE* out = fopen(txt_filename, "w");
    if (!out) {
        fprintf(stderr, "Error: Cannot create output file '%s'\n", txt_filename);
        free(entries);
        return false;
    }
    
    fprintf(out, "# APU Log Export\n");
    fprintf(out, "# Version: %u\n", header.version);
    fprintf(out, "# Entries: %u\n", header.entry_count);
    fprintf(out, "# Frames: %u\n", header.frame_count);
    fprintf(out, "#\n");
    fprintf(out, "# Format: Index Time Address Data EventType FrameNumber\n");
    fprintf(out, "#\n");
    
    for (uint32_t i = 0; i < header.entry_count; i++) {
        const apu_log_entry_t* entry = &entries[i];
        fprintf(out, "%u %d 0x%04X 0x%02X %u %u\n",
                i, entry->time, entry->addr, entry->data, entry->event_type, entry->frame_number);
    }
    
    fclose(out);
    free(entries);
    
    printf("Exported to: %s\n", txt_filename);
    return true;
}

/* Show usage */
void show_usage(const char* program) {
    printf("APU Binary Log Parser\n");
    printf("Usage:\n");
    printf("  %s <bin_file>                    - Parse and display APU log\n", program);
    printf("  %s <bin_file> --export <txt_file> - Export to text format\n", program);
    printf("\nExamples:\n");
    printf("  %s apu_log_track0.bin\n", program);
    printf("  %s apu_log_track0.bin --export log.txt\n", program);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        show_usage(argv[0]);
        return 1;
    }
    
    const char* bin_filename = argv[1];
    
    /* Check for export mode */
    if (argc >= 4 && strcmp(argv[2], "--export") == 0) {
        return export_to_text(bin_filename, argv[3]) ? 0 : 1;
    }
    
    /* Default: parse and display */
    return parse_apu_log(bin_filename) ? 0 : 1;
}