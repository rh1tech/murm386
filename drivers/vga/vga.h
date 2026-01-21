/**
 * VGA Driver for RP2350
 * 
 * Based on pico-286's vga-nextgen driver approach:
 * - PIO outputs 8 bits per pixel with sync embedded in bits 6-7
 * - DMA feeds line templates to PIO
 * - DMA IRQ switches between templates per scanline
 * 
 * GPIO Pinout (directly connected to VGA via resistor DAC):
 *   GPIO6  = Blue low bit
 *   GPIO7  = Blue high bit  
 *   GPIO8  = Green low bit
 *   GPIO9  = Green high bit
 *   GPIO10 = Red low bit
 *   GPIO11 = Red high bit
 *   GPIO12 = H-Sync (directly from bit 6)
 *   GPIO13 = V-Sync (directly from bit 7)
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "hardware/pio.h"

// VGA Pin Configuration
#ifndef VGA_BASE_PIN
#define VGA_BASE_PIN 6
#endif

#define VGA_PIO      pio0
#define VGA_DMA_IRQ  DMA_IRQ_0

// Framebuffer configuration (320x200, scaled to 640x480 VGA)
#define VGA_FB_WIDTH  320
#define VGA_FB_HEIGHT 200

// Initialize VGA subsystem (call after setting system clock)
void vga_init(void);

// Get pointer to framebuffer (320x200, 6-bit color RRGGBB format)
uint8_t *vga_get_framebuffer(void);

// Get current frame count (for timing/animation)
uint32_t vga_get_frame_count(void);

// Clear framebuffer to solid color
void vga_clear(uint8_t color);

// Set a single pixel (bounds checked)
void vga_set_pixel(int x, int y, uint8_t color);
