#ifndef ADLIB_H
#define ADLIB_H

#include <stdint.h>
typedef struct AdlibState AdlibState;

void adlib_write(void *opaque, uint32_t nport, uint32_t val);
uint32_t adlib_read(void *opaque, uint32_t nport);
void adlib_callback (void *opaque, uint8_t *stream, int free);
void adlib_free(AdlibState *s);
AdlibState *adlib_new();
// call it 44100 times per sec
int16_t adlib_getsample(AdlibState *s);

#endif /* ADLIB_H */
