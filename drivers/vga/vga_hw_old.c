/**
 * VGA Driver for RP2350
 * 
 * Based on pico-286's vga-nextgen driver approach.
 */

#pragma GCC optimize("Ofast")

#include "vga_hw.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/pio.h"

// ============================================================================
// VGA Timing Configuration (640x480 @ 60Hz)
// ============================================================================

#define LINE_SIZE       800     // Pixels per line (including blanking)
#define N_LINES_TOTAL   525     // Total scanlines per frame
#define N_LINES_VISIBLE 480     // Visible scanlines
#define LINE_VS_BEGIN   490     // V-sync start line
#define LINE_VS_END     491     // V-sync end line

// Horizontal timing within the 800 pixel line:
// 0-95: H-sync pulse (96 pixels)
// 96-143: Back porch (48 pixels)  
// 144-783: Active video (640 pixels)
// 784-799: Front porch (16 pixels)
#define HS_SIZE         96      // H-sync pulse width
#define SHIFT_PICTURE   (LINE_SIZE - 656)  // Where active video starts (144)

// Sync signal encoding in bits 6-7:
// Bit 6 = H-Sync, Bit 7 = V-Sync (both active LOW for VGA)
#define TMPL_LINE       0xC0    // Normal line (both syncs inactive/high)
#define TMPL_HS         0x80    // H-sync active (low)
#define TMPL_VS         0x40    // V-sync active (low)
#define TMPL_VHS        0x00    // Both syncs active (low)

// ============================================================================
// PIO Program (inline)
// ============================================================================

static const uint16_t pio_vga_instructions[] = {
    0x6008,  // out pins, 8
};

static const struct pio_program pio_vga_program = {
    .instructions = pio_vga_instructions,
    .length = 1,
    .origin = -1,
};

// ============================================================================
// Module State
// ============================================================================

// Framebuffer - 6-bit color (RRGGBB format)
static uint8_t framebuffer[VGA_FB_WIDTH * VGA_FB_HEIGHT];

// Line pattern templates (4 buffers, each LINE_SIZE bytes)
static uint32_t *lines_pattern[4];
static uint32_t *lines_pattern_data = NULL;

// DMA channels
static int dma_data_chan;
static int dma_ctrl_chan;

// PIO state
static uint vga_sm = 0;

// Current state
static volatile uint32_t current_line = 0;
static volatile uint32_t frame_count = 0;

// Palette: maps 6-bit color to 8-bit output with sync bits
static uint8_t palette[64];

// ============================================================================
// Internal Functions
// ============================================================================

static void init_palette(void) {
    for (int i = 0; i < 64; i++) {
        palette[i] = i | TMPL_LINE;  // Add sync bits (both high = no sync)
    }
}

static void __isr __time_critical_func(dma_handler_vga)(void) {
    // Clear interrupt
    dma_hw->ints0 = 1u << dma_ctrl_chan;
    
    current_line++;
    if (current_line >= N_LINES_TOTAL) {
        current_line = 0;
        frame_count++;
    }
    
    // Determine which line pattern to use
    if (current_line >= N_LINES_VISIBLE) {
        // Vertical blanking region
        if (current_line >= LINE_VS_BEGIN && current_line <= LINE_VS_END) {
            dma_channel_set_read_addr(dma_ctrl_chan, &lines_pattern[1], false);
        } else {
            dma_channel_set_read_addr(dma_ctrl_chan, &lines_pattern[0], false);
        }
        return;
    }
    
    // Active video region - fill the video line buffer
    uint32_t *output_buffer = lines_pattern[2 + (current_line & 1)];
    uint8_t *output = (uint8_t *)output_buffer + SHIFT_PICTURE;
    
    // Scale: 480 visible lines -> 200 FB lines (2.4x vertical)
    int fb_y = current_line * VGA_FB_HEIGHT / N_LINES_VISIBLE;
    if (fb_y >= VGA_FB_HEIGHT) fb_y = VGA_FB_HEIGHT - 1;
    
    const uint8_t *fb_row = &framebuffer[fb_y * VGA_FB_WIDTH];
    
    // Output 640 pixels from 320 FB pixels (2x horizontal scaling)
    for (int x = 0; x < VGA_FB_WIDTH; x++) {
        uint8_t color = palette[fb_row[x] & 0x3F];
        *output++ = color;
        *output++ = color;  // 2x horizontal
    }
    
    dma_channel_set_read_addr(dma_ctrl_chan, &lines_pattern[2 + (current_line & 1)], false);
}

// ============================================================================
// Public API
// ============================================================================

void vga_hw_init(void) {
    printf("VGA Init...\n");
    
    init_palette();
    
    // Calculate clock divider for 25.175 MHz pixel clock
    float sys_clk = (float)clock_get_hz(clk_sys);
    float clk_div = sys_clk / 25175000.0f;
    
    printf("  System clock: %.1f MHz\n", sys_clk / 1e6);
    printf("  Clock divider: %.4f\n", clk_div);
    
    // Allocate line pattern buffers
    lines_pattern_data = (uint32_t *)calloc(LINE_SIZE * 4 / 4, sizeof(uint32_t));
    if (!lines_pattern_data) {
        printf("ERROR: Failed to allocate VGA buffers!\n");
        return;
    }
    
    for (int i = 0; i < 4; i++) {
        lines_pattern[i] = &lines_pattern_data[i * (LINE_SIZE / 4)];
    }
    
    // Initialize line templates
    uint8_t *base = (uint8_t *)lines_pattern[0];
    memset(base, TMPL_LINE, LINE_SIZE);
    memset(base, TMPL_HS, HS_SIZE);
    
    base = (uint8_t *)lines_pattern[1];
    memset(base, TMPL_VS, LINE_SIZE);
    memset(base, TMPL_VHS, HS_SIZE);
    
    memcpy(lines_pattern[2], lines_pattern[0], LINE_SIZE);
    memcpy(lines_pattern[3], lines_pattern[0], LINE_SIZE);
    
    // Initialize PIO
    uint offset = pio_add_program(VGA_PIO, &pio_vga_program);
    vga_sm = pio_claim_unused_sm(VGA_PIO, true);
    
    // Configure GPIO pins
    for (int i = 0; i < 8; i++) {
        gpio_init(VGA_BASE_PIN + i);
        gpio_set_dir(VGA_BASE_PIN + i, GPIO_OUT);
        pio_gpio_init(VGA_PIO, VGA_BASE_PIN + i);
        gpio_set_slew_rate(VGA_BASE_PIN + i, GPIO_SLEW_RATE_FAST);
        gpio_set_drive_strength(VGA_BASE_PIN + i, GPIO_DRIVE_STRENGTH_8MA);
    }
    
    // Configure PIO state machine
    pio_sm_set_consecutive_pindirs(VGA_PIO, vga_sm, VGA_BASE_PIN, 8, true);
    
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset, offset);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);
    sm_config_set_out_shift(&c, true, true, 32);
    sm_config_set_out_pins(&c, VGA_BASE_PIN, 8);
    
    pio_sm_init(VGA_PIO, vga_sm, offset, &c);
    
    // Set clock divider
    uint32_t div32 = (uint32_t)(clk_div * (1 << 16));
    VGA_PIO->sm[vga_sm].clkdiv = div32 & 0xfffff000;
    
    pio_sm_set_enabled(VGA_PIO, vga_sm, true);
    
    // Initialize DMA
    dma_data_chan = dma_claim_unused_channel(true);
    dma_ctrl_chan = dma_claim_unused_channel(true);
    
    // Data channel
    dma_channel_config c0 = dma_channel_get_default_config(dma_data_chan);
    channel_config_set_transfer_data_size(&c0, DMA_SIZE_32);
    channel_config_set_read_increment(&c0, true);
    channel_config_set_write_increment(&c0, false);
    
    uint dreq = (VGA_PIO == pio0) ? DREQ_PIO0_TX0 + vga_sm : DREQ_PIO1_TX0 + vga_sm;
    channel_config_set_dreq(&c0, dreq);
    channel_config_set_chain_to(&c0, dma_ctrl_chan);
    
    dma_channel_configure(dma_data_chan, &c0, &VGA_PIO->txf[vga_sm], 
                          lines_pattern[0], LINE_SIZE / 4, false);
    
    // Control channel
    dma_channel_config c1 = dma_channel_get_default_config(dma_ctrl_chan);
    channel_config_set_transfer_data_size(&c1, DMA_SIZE_32);
    channel_config_set_read_increment(&c1, false);
    channel_config_set_write_increment(&c1, false);
    channel_config_set_chain_to(&c1, dma_data_chan);
    
    dma_channel_configure(dma_ctrl_chan, &c1, 
                          &dma_hw->ch[dma_data_chan].read_addr,
                          &lines_pattern[0], 1, false);
    
    // Set up interrupt
    irq_set_exclusive_handler(VGA_DMA_IRQ, dma_handler_vga);
    dma_channel_set_irq0_enabled(dma_ctrl_chan, true);
    irq_set_enabled(VGA_DMA_IRQ, true);
    
    // Start DMA
    dma_start_channel_mask(1u << dma_data_chan);
    
    printf("  VGA started!\n");
}

uint8_t *vga_hw_get_framebuffer(void) {
    return framebuffer;
}

uint32_t vga_hw_get_frame_count(void) {
    return frame_count;
}

void vga_hw_clear(uint8_t color) {
    memset(framebuffer, color & 0x3F, sizeof(framebuffer));
}

void vga_hw_set_pixel(int x, int y, uint8_t color) {
    if (x >= 0 && x < VGA_FB_WIDTH && y >= 0 && y < VGA_FB_HEIGHT) {
        framebuffer[y * VGA_FB_WIDTH + x] = color & 0x3F;
    }
}
