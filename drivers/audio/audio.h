/**
 * murm386 - I2S Audio Driver for RP2350
 *
 * DMA-based I2S audio output using PIO for the Sound Blaster 16,
 * Adlib/OPL2, and PC Speaker emulation.
 */

#ifndef AUDIO_H
#define AUDIO_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <hardware/pio.h>
#include <hardware/clocks.h>
#include <hardware/dma.h>

// Audio sample rate - Sound Blaster uses 44100 Hz
#define AUDIO_SAMPLE_RATE 44100

// Audio buffer size - enough samples for ~20ms at 44.1kHz
// 44100 / 50 = 882 samples per frame at 50Hz
// Use 1024 for headroom
#define AUDIO_BUFFER_SAMPLES 1024

//=============================================================================
// High-level audio API for murm386
//=============================================================================

// Initialize audio system
bool audio_init(void);

// Process audio for one frame - call from main loop
// This invokes mixer_callback and sends samples to I2S
void audio_process_frame(void *pc);

// Set master volume (0-128)
void audio_set_volume(int volume);

// Get current master volume
int audio_get_volume(void);

// Enable/disable audio
void audio_set_enabled(bool enabled);

// Check if audio is enabled
bool audio_is_enabled(void);

// Check if audio system needs more samples (buffer free)
bool audio_needs_samples(void);

/// TODO: 
void dss_process_sample(void);

#endif // AUDIO_H
