/**
 * Stub implementations for disabled features on RP2350
 *
 * These functions provide no-op implementations for features that are
 * disabled in the initial RP2350 port (sound, network, FPU).
 */

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

//=============================================================================
// Adlib/OPL2 Stubs
//=============================================================================

typedef struct AdlibState AdlibState;

AdlibState *adlib_new(void) {
    return NULL;
}

uint8_t adlib_read(AdlibState *s, uint32_t addr) {
    (void)s;
    (void)addr;
    return 0xFF;
}

void adlib_write(AdlibState *s, uint32_t addr, uint8_t val) {
    (void)s;
    (void)addr;
    (void)val;
}

void adlib_callback(AdlibState *s, uint8_t *buf, int len) {
    (void)s;
    (void)buf;
    (void)len;
}

//=============================================================================
// NE2000 Network Stubs
//=============================================================================

typedef struct NE2000State NE2000State;

NE2000State *isa_ne2000_init(int base, int irq, void *pic,
                              void (*set_irq)(void *pic, int irq, int level)) {
    (void)base;
    (void)irq;
    (void)pic;
    (void)set_irq;
    return NULL;
}

uint8_t ne2000_ioport_read(NE2000State *s, uint32_t addr) {
    (void)s;
    (void)addr;
    return 0xFF;
}

void ne2000_ioport_write(NE2000State *s, uint32_t addr, uint8_t val) {
    (void)s;
    (void)addr;
    (void)val;
}

uint16_t ne2000_asic_ioport_read(NE2000State *s, uint32_t addr) {
    (void)s;
    (void)addr;
    return 0xFFFF;
}

void ne2000_asic_ioport_write(NE2000State *s, uint32_t addr, uint16_t val) {
    (void)s;
    (void)addr;
    (void)val;
}

uint8_t ne2000_reset_ioport_read(NE2000State *s, uint32_t addr) {
    (void)s;
    (void)addr;
    return 0xFF;
}

void ne2000_reset_ioport_write(NE2000State *s, uint32_t addr, uint8_t val) {
    (void)s;
    (void)addr;
    (void)val;
}

void ne2000_step(NE2000State *s) {
    (void)s;
}

//=============================================================================
// Sound Blaster 16 Stubs
//=============================================================================

typedef struct SB16State SB16State;

SB16State *sb16_new(int base, int irq, void *dma, void *hdma,
                    void *pic, void (*set_irq)(void *pic, int irq, int level)) {
    (void)base;
    (void)irq;
    (void)dma;
    (void)hdma;
    (void)pic;
    (void)set_irq;
    return NULL;
}

uint8_t sb16_mixer_read(SB16State *s, uint32_t addr) {
    (void)s;
    (void)addr;
    return 0xFF;
}

void sb16_mixer_write_indexb(SB16State *s, uint32_t addr, uint8_t val) {
    (void)s;
    (void)addr;
    (void)val;
}

void sb16_mixer_write_datab(SB16State *s, uint32_t addr, uint8_t val) {
    (void)s;
    (void)addr;
    (void)val;
}

uint8_t sb16_dsp_read(SB16State *s, uint32_t addr) {
    (void)s;
    (void)addr;
    return 0xFF;
}

void sb16_dsp_write(SB16State *s, uint32_t addr, uint8_t val) {
    (void)s;
    (void)addr;
    (void)val;
}

void sb16_audio_callback(SB16State *s, uint8_t *stream, int len) {
    (void)s;
    (void)stream;
    (void)len;
}

//=============================================================================
// PC Speaker Stubs
//=============================================================================

typedef struct PCSpkState PCSpkState;

PCSpkState *pcspk_init(void *pit) {
    (void)pit;
    return NULL;
}

uint8_t pcspk_ioport_read(PCSpkState *s) {
    (void)s;
    return 0;
}

void pcspk_ioport_write(PCSpkState *s, uint8_t val) {
    (void)s;
    (void)val;
}

int pcspk_get_active_out(PCSpkState *s) {
    (void)s;
    return 0;
}

void pcspk_callback(PCSpkState *s, uint8_t *buf, int len) {
    (void)s;
    (void)buf;
    (void)len;
}

//=============================================================================
// FPU Stubs (if FPU is disabled)
//=============================================================================

#ifdef NO_FPU
typedef struct FPU FPU;

FPU *fpu_new(void) {
    return NULL;
}

int fpu_exec1(FPU *f, uint32_t op) {
    (void)f;
    (void)op;
    return 0;  // Not handled
}

int fpu_exec2(FPU *f, uint32_t op, uint32_t addr, uint8_t *mem) {
    (void)f;
    (void)op;
    (void)addr;
    (void)mem;
    return 0;  // Not handled
}
#endif
