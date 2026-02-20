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

int16_t covox_sample = 0;

static const int16_t sample_lut[256] = {
    // Pre-computed (i-128) << 6 for i = 0..255
    -8192, -8128, -8064, -8000, -7936, -7872, -7808, -7744,
    -7680, -7616, -7552, -7488, -7424, -7360, -7296, -7232,
    -7168, -7104, -7040, -6976, -6912, -6848, -6784, -6720,
    -6656, -6592, -6528, -6464, -6400, -6336, -6272, -6208,
    -6144, -6080, -6016, -5952, -5888, -5824, -5760, -5696,
    -5632, -5568, -5504, -5440, -5376, -5312, -5248, -5184,
    -5120, -5056, -4992, -4928, -4864, -4800, -4736, -4672,
    -4608, -4544, -4480, -4416, -4352, -4288, -4224, -4160,
    -4096, -4032, -3968, -3904, -3840, -3776, -3712, -3648,
    -3584, -3520, -3456, -3392, -3328, -3264, -3200, -3136,
    -3072, -3008, -2944, -2880, -2816, -2752, -2688, -2624,
    -2560, -2496, -2432, -2368, -2304, -2240, -2176, -2112,
    -2048, -1984, -1920, -1856, -1792, -1728, -1664, -1600,
    -1536, -1472, -1408, -1344, -1280, -1216, -1152, -1088,
    -1024,  -960,  -896,  -832,  -768,  -704,  -640,  -576,
     -512,  -448,  -384,  -320,  -256,  -192,  -128,   -64,
        0,    64,   128,   192,   256,   320,   384,   448,
      512,   576,   640,   704,   768,   832,   896,   960,
     1024,  1088,  1152,  1216,  1280,  1344,  1408,  1472,
     1536,  1600,  1664,  1728,  1792,  1856,  1920,  1984,
     2048,  2112,  2176,  2240,  2304,  2368,  2432,  2496,
     2560,  2624,  2688,  2752,  2816,  2880,  2944,  3008,
     3072,  3136,  3200,  3264,  3328,  3392,  3456,  3520,
     3584,  3648,  3712,  3776,  3840,  3904,  3968,  4032,
     4096,  4160,  4224,  4288,  4352,  4416,  4480,  4544,
     4608,  4672,  4736,  4800,  4864,  4928,  4992,  5056,
     5120,  5184,  5248,  5312,  5376,  5440,  5504,  5568,
     5632,  5696,  5760,  5824,  5888,  5952,  6016,  6080,
     6144,  6208,  6272,  6336,  6400,  6464,  6528,  6592,
     6656,  6720,  6784,  6848,  6912,  6976,  7040,  7104,
     7168,  7232,  7296,  7360,  7424,  7488,  7552,  7616,
     7680,  7744,  7808,  7872,  7936,  8000,  8064,  8128
};

inline static int16_t _dss_sample() {
    static int16_t held = 0;

    if (fifo_count == 0) {
        // Decay toward silence so playback tail doesn't linger indefinitely.
        // Rate: ~1/16 per DSS tick = ~437 ticks to reach 0 from max.
        // At 7000 Hz that is ~62ms – enough to avoid pops, short enough
        // to prevent audible reverb tail.
        if (held > 0)      held -= (held >> 4) + 1;
        else if (held < 0) held -= (held >> 4) - 1;
        return held;
    }

    uint8_t s = fifo_buffer[fifo_tail];
    fifo_tail = (fifo_tail + 1) & (FIFO_BUFFER_SIZE - 1);
    --fifo_count;

    held = sample_lut[s];
    return held;
}

#define DSS_RATE 7000
static uint32_t phase = 0;
static uint32_t step = (uint32_t)(((uint64_t)DSS_RATE << 32) / AUDIO_SAMPLE_RATE);

static int16_t last_sample = 0;

int16_t dss_sample_step() {
    phase += step;
    if (phase < step) {          // overflow
        last_sample = _dss_sample();
    }
    return last_sample;
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
