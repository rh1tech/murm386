/*
 * Disk management - adapted from pico-286
 * Provides INT 13h disk handler for floppy and hard drives
 */
#ifndef DISK_H
#define DISK_H

#include "i386.h"

// External variables
extern int hdcount, fdcount;

// Disk management functions
uint8_t insertdisk(uint8_t drivenum, const char *pathname);
void disk_set_cpu(CPUI386 *cpu);
void diskhandler(CPUI386 *cpu);

// INT 13h handler wrapper for CPU hook (matches int13_handler_t signature)
static inline void diskhandler_wrapper(CPUI386 *cpu, void *opaque)
{
	(void)opaque;
	diskhandler(cpu);
}

#endif /* DISK_H */
