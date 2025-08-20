// APU Register Logger implementation

#include "Apu_Logger.h"

#include <string.h>
#include <algorithm>

// Global logger instance
Apu_Logger* g_apu_logger = nullptr;

void init_apu_logger()
{
    if(!g_apu_logger){
        g_apu_logger = new Apu_Logger();
    }
}

Apu_Logger* get_apu_logger()
{
    return g_apu_logger;
}

Apu_Logger::Apu_Logger() 
    : enabled_(false)
    , max_entries_(0)
    , time_base_(0)
{
    entries_.reserve(10000); // Reserve space for typical usage
}

Apu_Logger::~Apu_Logger() {
    // Cleanup handled automatically by vector
}

bool Apu_Logger::is_apu_addr(nes_addr_t addr) const {
    // APU registers: 0x4000-0x4017 (excluding 0x4014 DMA and 0x4016 controller)
    if (addr >= 0x4000 && addr <= 0x4017) {
        return (addr != 0x4014 && addr != 0x4016);
    }
    return false;
}

void Apu_Logger::log_write(nes_time_t time, nes_addr_t addr, int data) {
    if (!enabled_ || !is_apu_addr(addr)) {
        return;
    }
    
    // Apply memory limit if set
    if (max_entries_ > 0 && entries_.size() >= max_entries_) {
        return; // Could implement circular buffer here if needed
    }
    
    // Convert to relative time if time base is set
    nes_time_t relative_time = time - time_base_;
    
    // Add entry
    entries_.emplace_back(relative_time, static_cast<uint16_t>(addr), static_cast<uint8_t>(data));
}

void Apu_Logger::clear() {
    entries_.clear();
    time_base_ = 0;
}

size_t Apu_Logger::get_memory_usage() const {
    return entries_.size() * sizeof(apu_log_entry_t) + 
           entries_.capacity() * sizeof(apu_log_entry_t);
}

bool Apu_Logger::save_binary(const char* filename) const {
    if (!filename || entries_.empty()) {
        return false;
    }
    
    FILE* file = fopen(filename, "wb");
    if (!file) {
        return false;
    }
    
    // Write header
    apu_log_header_t header;
    memset(&header, 0, sizeof(header));
    strcpy(header.magic, "APULOG");
    header.version = 1;
    header.entry_count = static_cast<uint32_t>(entries_.size());
    
    if (fwrite(&header, sizeof(header), 1, file) != 1) {
        fclose(file);
        return false;
    }
    
    // Write entries
    size_t written = fwrite(entries_.data(), sizeof(apu_log_entry_t), entries_.size(), file);
    fclose(file);
    
    return written == entries_.size();
}

bool Apu_Logger::save_text(const char* filename) const {
    if (!filename || entries_.empty()) {
        return false;
    }
    
    FILE* file = fopen(filename, "w");
    if (!file) {
        return false;
    }
    
    fprintf(file, "# APU Register Log\n");
    fprintf(file, "# Format: Time(cycles) Address Data\n");
    fprintf(file, "# Total entries: %zu\n", entries_.size());
    fprintf(file, "\n");
    
    for (const auto& entry : entries_) {
        fprintf(file, "%10d 0x%04X 0x%02X\n", 
                entry.time, entry.addr, entry.data);
    }
    
    fclose(file);
    return true;
}

bool Apu_Logger::load_binary(const char* filename) {
    if (!filename) {
        return false;
    }
    
    FILE* file = fopen(filename, "rb");
    if (!file) {
        return false;
    }
    
    // Read header
    apu_log_header_t header;
    if (fread(&header, sizeof(header), 1, file) != 1) {
        fclose(file);
        return false;
    }
    
    // Verify magic
    if (strcmp(header.magic, "APULOG") != 0) {
        fclose(file);
        return false;
    }
    
    // Check version
    if (header.version != 1) {
        fclose(file);
        return false; // Unsupported version
    }
    
    // Read entries
    clear();
    entries_.resize(header.entry_count);
    
    size_t read_count = fread(entries_.data(), sizeof(apu_log_entry_t), 
                             header.entry_count, file);
    fclose(file);
    
    if (read_count != header.entry_count) {
        entries_.clear();
        return false;
    }
    
    return true;
}

// Convenience function to create and initialize global logger
extern "C" {
    void apu_logger_init() {
        if (!g_apu_logger) {
            g_apu_logger = new Apu_Logger();
            g_apu_logger->set_enabled(true);
        }
    }
    
    void apu_logger_cleanup() {
        delete g_apu_logger;
        g_apu_logger = nullptr;
    }
    
    // C interface functions for easier integration
    void apu_logger_set_enabled(int enabled) {
        if (g_apu_logger) {
            g_apu_logger->set_enabled(enabled != 0);
        }
    }
    
    int apu_logger_is_enabled() {
        return (g_apu_logger && g_apu_logger->is_enabled()) ? 1 : 0;
    }
    
    void apu_logger_log_write_c(int32_t time, unsigned addr, int data) {
        apu_log_write(time, addr, data);
    }
    
    int apu_logger_save_binary_c(const char* filename) {
        return (g_apu_logger && g_apu_logger->save_binary(filename)) ? 1 : 0;
    }
    
    int apu_logger_save_text_c(const char* filename) {
        return (g_apu_logger && g_apu_logger->save_text(filename)) ? 1 : 0;
    }
    
    size_t apu_logger_get_entry_count() {
        return g_apu_logger ? g_apu_logger->entry_count() : 0;
    }
}
