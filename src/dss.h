#include <stdint.h>

void dss_out(const uint16_t portnum, const uint8_t value);
uint8_t dss_in(const uint16_t portnum);
// core1 functions
void dss_process_sample(void);
void dss_samples(int16_t* buff, size_t free);
