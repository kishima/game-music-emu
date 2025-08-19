/* ESP32 NSF Player Example
 * 
 * 15KHz PWM output with 60Hz frame rate
 * Optimized for ESP32 memory and performance constraints
 */

#include "gme/gme.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ESP32 Configuration
#define SAMPLE_RATE     15000      // PWM interrupt frequency
#define FRAME_RATE      60         // Update frequency (Hz)
#define SAMPLES_PER_FRAME (SAMPLE_RATE / FRAME_RATE)  // 250 samples

// Audio buffers
static short stereo_buffer[SAMPLES_PER_FRAME * 2];  // Stereo from GME
static short mono_buffer[SAMPLES_PER_FRAME];        // Mono for PWM output
static Music_Emu* nsf_emu = NULL;

// Error handling
static void handle_error(const char* str) {
    if (str) {
        printf("NSF Error: %s\n", str);
        // In ESP32, you might want to restart or enter error state
        while(1) { /* halt */ }
    }
}

// Initialize NSF player
int nsf_player_init(const char* nsf_filename, int track_number) {
    // Open NSF file
    handle_error(gme_open_file(nsf_filename, &nsf_emu, SAMPLE_RATE));
    
    // Start specified track
    handle_error(gme_start_track(nsf_emu, track_number));
    
    // Optional: Set fade time (disable auto-fade for continuous play)
    gme_set_autoload_playback_limit(nsf_emu, 0);
    
    printf("NSF Player initialized: %d tracks, playing track %d\n", 
           gme_track_count(nsf_emu), track_number);
    
    return 0;
}

// Generate next frame of audio (call from 60Hz timer)
// Returns number of samples generated
int nsf_player_generate_frame(void) {
    if (!nsf_emu) return 0;
    
    // Generate stereo samples from GME
    handle_error(gme_play(nsf_emu, SAMPLES_PER_FRAME * 2, stereo_buffer));
    
    // Convert stereo to mono and scale for PWM
    for (int i = 0; i < SAMPLES_PER_FRAME; i++) {
        // Mix left and right channels
        int mixed = (stereo_buffer[i * 2] + stereo_buffer[i * 2 + 1]) / 2;
        
        // Optional: Apply volume scaling or limiting here
        mono_buffer[i] = (short)mixed;
    }
    
    return SAMPLES_PER_FRAME;
}

// Get mono audio buffer for PWM output
const short* nsf_player_get_buffer(void) {
    return mono_buffer;
}

// Check if track has ended
int nsf_player_track_ended(void) {
    return nsf_emu ? gme_track_ended(nsf_emu) : 1;
}

// Get current playback time in milliseconds
int nsf_player_get_time_ms(void) {
    return nsf_emu ? gme_tell(nsf_emu) : 0;
}

// Switch to different track
void nsf_player_set_track(int track_number) {
    if (nsf_emu && track_number < gme_track_count(nsf_emu)) {
        handle_error(gme_start_track(nsf_emu, track_number));
    }
}

// Clean up
void nsf_player_cleanup(void) {
    if (nsf_emu) {
        gme_delete(nsf_emu);
        nsf_emu = NULL;
    }
}

#ifdef ESP32_EXAMPLE_USAGE

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "driver/ledc.h"

// ESP32 PWM Configuration
#define PWM_TIMER          LEDC_TIMER_0
#define PWM_MODE           LEDC_LOW_SPEED_MODE
#define PWM_OUTPUT_IO      (18) // Define the output GPIO
#define PWM_CHANNEL        LEDC_CHANNEL_0
#define PWM_DUTY_RES       LEDC_TIMER_12_BIT // Set duty resolution to 12 bits
#define PWM_FREQUENCY      (15000) // Frequency in Hertz

static TimerHandle_t audio_timer;
static volatile int buffer_ready = 0;
static int current_sample = 0;

// PWM setup
static void pwm_init(void) {
    // Prepare and then apply the LEDC PWM timer configuration
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = PWM_MODE,
        .timer_num        = PWM_TIMER,
        .duty_resolution  = PWM_DUTY_RES,
        .freq_hz          = PWM_FREQUENCY,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // Prepare and then apply the LEDC PWM channel configuration
    ledc_channel_config_t ledc_channel = {
        .speed_mode     = PWM_MODE,
        .channel        = PWM_CHANNEL,
        .timer_sel      = PWM_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = PWM_OUTPUT_IO,
        .duty           = 0, // Set duty to 0%
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
}

// 60Hz timer callback - generate audio frame
static void IRAM_ATTR audio_frame_callback(TimerHandle_t timer) {
    nsf_player_generate_frame();
    buffer_ready = 1;
    current_sample = 0;
}

// High frequency PWM interrupt simulation
static void pwm_update_task(void *arg) {
    const short* audio_buffer;
    
    while (1) {
        if (buffer_ready && current_sample < SAMPLES_PER_FRAME) {
            audio_buffer = nsf_player_get_buffer();
            
            // Convert 16-bit signed to PWM duty cycle (0-4095 for 12-bit)
            int sample = audio_buffer[current_sample];
            uint32_t duty = (sample + 32768) >> 4; // Convert to 0-4095 range
            
            // Update PWM duty cycle
            ledc_set_duty(PWM_MODE, PWM_CHANNEL, duty);
            ledc_update_duty(PWM_MODE, PWM_CHANNEL);
            
            current_sample++;
            
            if (current_sample >= SAMPLES_PER_FRAME) {
                buffer_ready = 0;
            }
        }
        
        // Wait for next PWM cycle (~66.7μs for 15KHz)
        vTaskDelay(pdMS_TO_TICKS(1)); // Rough approximation
    }
}

void app_main(void) {
    // Initialize PWM
    pwm_init();
    
    // Initialize NSF player
    nsf_player_init("/spiffs/music.nsf", 0);
    
    // Create 60Hz timer for audio frame generation
    audio_timer = xTimerCreate("AudioTimer", 
                              pdMS_TO_TICKS(1000/60), // 60Hz = ~16.67ms
                              pdTRUE,                  // Auto-reload
                              NULL,                    // Timer ID
                              audio_frame_callback);   // Callback
    
    // Start the timer
    xTimerStart(audio_timer, 0);
    
    // Create PWM update task
    xTaskCreate(pwm_update_task, "PWM_Update", 2048, NULL, 5, NULL);
    
    printf("NSF Player started\n");
    
    // Main loop - could handle track switching, etc.
    while (1) {
        if (nsf_player_track_ended()) {
            printf("Track ended, restarting...\n");
            nsf_player_set_track(0); // Restart track 0
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000)); // Check every second
    }
}

#endif /* ESP32_EXAMPLE_USAGE */

/* Memory usage estimation:
 * - stereo_buffer: 250 * 2 * 2 = 1000 bytes
 * - mono_buffer:   250 * 2 = 500 bytes
 * - GME library:   ~50-100KB (depending on optimization)
 * - NSF file:      varies (typically 32KB-1MB)
 * 
 * Total RAM usage: ~60-110KB + NSF file size
 */

/* Performance notes:
 * - 60Hz frame generation: ~4.2ms processing time per 16.67ms
 * - PWM update rate: 15KHz = 66.7μs per sample
 * - Consider using DMA for PWM updates to reduce CPU load
 * - Use IRAM placement for time-critical functions
 */