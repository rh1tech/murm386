// https://archive.org/details/dss-programmers-guide
#pragma GCC optimize("Ofast")
#include "audio.h"
#include "dss.h"
#define INLINE inline

#define FIFO_BUFFER_SIZE 16

static uint8_t fifo_buffer[FIFO_BUFFER_SIZE] = { 0 };
static uint8_t fifo_head = 0;  // Write position
static uint8_t fifo_tail = 0;  // Read position
static uint8_t fifo_count = 0; // Number of elements
static uint8_t dss_data;

static INLINE uint8_t dss_sample() {
    static uint8_t held = 0;
    if (fifo_count == 0) {
        return held;
    }
    held = fifo_buffer[fifo_tail];
    fifo_tail = (fifo_tail + 1) & (FIFO_BUFFER_SIZE - 1);
    --fifo_count;
    return held;
}

static INLINE void fifo_push_byte(uint8_t value) {
    if (__builtin_expect(fifo_count >= FIFO_BUFFER_SIZE, 0))
        return;

    fifo_buffer[fifo_head] = value;
    fifo_head = (fifo_head + 1) & (FIFO_BUFFER_SIZE - 1);
    ++fifo_count;
}

static INLINE uint8_t fifo_is_full() {
    return fifo_count == FIFO_BUFFER_SIZE ? 0x40 : 0x00;
}

uint8_t dss_in(const uint16_t portnum) {
    return portnum & 1 ? fifo_is_full() : dss_data;
}

void dss_out(uint16_t portnum, uint8_t value) {
    static uint8_t control = 0;

    if (portnum == 0x378) {
        dss_data = value;          // только запомнить
        return;
    }

    if (portnum == 0x37A) {
        // pin17 = SELECTIN = control bit3 (инвертированный).
        // Rising edge on pin17 => bit3 1->0
        const uint8_t old_b3 = control & 0x08;
        const uint8_t new_b3 = value   & 0x08;

        if (old_b3 && !new_b3) {
            fifo_push_byte(dss_data);
        }

        control = value;
    }
}

// Ring buffer for DSS output samples at 7000 Hz
#define DSS_OUT_BUFFER_SIZE 512
static uint8_t __scratch_x("dss_out_buffer") dss_out_buffer[DSS_OUT_BUFFER_SIZE] __aligned(4) = { 0 };
static size_t dss_out_idx = 0;
static size_t dss_in_idx = 0;

// calling from core1 (7000 times per second)
void __not_in_flash() dss_process_sample(void) {
    if (dss_out_idx == DSS_OUT_BUFFER_SIZE) dss_out_idx = 0;
    uint8_t dss_v = dss_sample();
    dss_out_buffer[dss_out_idx++] = dss_v;
}

// calling from core1 (for case we have some free buffer size)
// buff is stereo, 16-bit per sample, signed, free - total number of 16-bit samples
void __not_in_flash() dss_samples(int16_t* buff, size_t free) {
    size_t dss_out_idx_read = 0;
    for (size_t i = 0; i < free / 2; i += 2) {
        // Resample 7000 Hz -> 44100 Hz
        dss_out_idx_read = i * 7000 / 44100 + dss_in_idx;
        while (dss_out_idx_read >= DSS_OUT_BUFFER_SIZE) dss_out_idx_read -= DSS_OUT_BUFFER_SIZE;
        uint8_t dss_v = dss_out_buffer[dss_out_idx_read];
        int v = ((int)dss_v << 4);
        // Left
        int res = buff[i] + v;
        if (res > 32767) res = 32767;
        if (res < -32768) res = -32768;
        buff[i] = res;
        // Right
        res = buff[i + 1] + v;
        if (res > 32767) res = 32767;
        if (res < -32768) res = -32768;
        buff[i + 1] = res;
    }
    dss_in_idx = dss_out_idx_read;
}
