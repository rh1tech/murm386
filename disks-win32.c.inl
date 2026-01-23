/*
 * Disk management for Win32/Linux/macOS - adapted from pico-286
 * Uses standard C file I/O
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "i386.h"

int hdcount = 0, fdcount = 0;

static uint8_t sectorbuffer[512];

struct struct_drive {
    FILE *diskfile;
    size_t filesize;
    uint16_t cyls;
    uint16_t sects;
    uint16_t heads;
    uint8_t inserted;
    uint8_t readonly;
} disk[4];

static CPUI386 *disk_cpu = NULL;
static uint8_t *disk_mem = NULL;

void disk_set_cpu(CPUI386 *cpu) {
    disk_cpu = cpu;
    disk_mem = cpu_get_phys_mem(cpu);
}

static inline void ejectdisk(uint8_t drivenum) {
    if (drivenum & 0x80) drivenum -= 126;

    if (disk[drivenum].inserted) {
        if (disk[drivenum].diskfile) {
            fclose(disk[drivenum].diskfile);
            disk[drivenum].diskfile = NULL;
        }
        disk[drivenum].inserted = 0;
        if (drivenum >= 2)
            hdcount--;
        else
            fdcount--;
    }
}

uint8_t insertdisk(uint8_t drivenum, const char *pathname) {
    FILE *file;

    if (drivenum & 0x80) drivenum -= 126;  // Normalize hard drive numbers

    file = fopen(pathname, "r+b");
    if (!file) {
        return 0;
    }

    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Validate size constraints
    if (size < 360 * 1024 || size > 0x1f782000UL || (size & 511)) {
        fclose(file);
        return 0;
    }

    // Determine geometry (cyls, heads, sects)
    uint16_t cyls = 0, heads = 0, sects = 0;

    if (drivenum >= 2) {  // Hard disk
        sects = 63;
        heads = 16;
        cyls = size / (sects * heads * 512);
    } else {  // Floppy disk
        cyls = 80;
        sects = 18;
        heads = 2;

        if (size <= 368640) {  // 360 KB or lower
            cyls = 40;
            sects = 9;
            heads = 2;
        } else if (size <= 737280) {
            sects = 9;
        } else if (size <= 1228800) {
            sects = 15;
        }
    }

    // Eject any existing disk and insert the new one
    ejectdisk(drivenum);

    disk[drivenum].diskfile = file;
    disk[drivenum].filesize = size;
    disk[drivenum].inserted = 1;
    disk[drivenum].readonly = 0;
    disk[drivenum].cyls = cyls;
    disk[drivenum].heads = heads;
    disk[drivenum].sects = sects;

    // Update drive counts
    if (drivenum >= 2) {
        hdcount++;
    } else {
        fdcount++;
    }

    printf("DISK: Disk %02Xh attached from file %s, size=%luK, CHS=%d,%d,%d\n",
           drivenum, pathname, (unsigned long) (size >> 10), cyls, heads, sects);

    return 1;
}

// Call this ONLY if all parameters are valid! There is no check here!
static inline size_t chs2ofs(int drivenum, int cyl, int head, int sect) {
    return (
                   ((size_t)cyl * (size_t)disk[drivenum].heads + (size_t)head) * (size_t)disk[drivenum].sects + (size_t) sect - 1
           ) * 512UL;
}


static void readdisk(uint8_t drivenum,
              uint16_t dstseg, uint16_t dstoff,
              uint16_t cyl, uint16_t sect, uint16_t head,
              uint16_t sectcount, int is_verify
) {
    uint32_t memdest = ((uint32_t) dstseg << 4) + (uint32_t) dstoff;
    uint32_t cursect = 0;

    // Check if disk is inserted
    if (!disk[drivenum].inserted) {
        cpu_set_ah(disk_cpu, 0x31);    // no media in drive
        cpu_set_al(disk_cpu, 0);
        cpu_set_cf(disk_cpu, 1);
        return;
    }

    // Check if CHS parameters are valid
    if (sect == 0 || sect > disk[drivenum].sects || cyl >= disk[drivenum].cyls || head >= disk[drivenum].heads) {
        cpu_set_ah(disk_cpu, 0x04);    // sector not found
        cpu_set_al(disk_cpu, 0);
        cpu_set_cf(disk_cpu, 1);
        return;
    }

    // Convert CHS to file offset
    size_t fileoffset = chs2ofs(drivenum, cyl, head, sect);

    // Check if fileoffset is valid
    if (fileoffset > disk[drivenum].filesize) {
        cpu_set_ah(disk_cpu, 0x04);    // sector not found
        cpu_set_al(disk_cpu, 0);
        cpu_set_cf(disk_cpu, 1);
        return;
    }

    // Set file position
    fseek(disk[drivenum].diskfile, fileoffset, SEEK_SET);

    // Process sectors
    for (cursect = 0; cursect < sectcount; cursect++) {
        // Read the sector into buffer
        size_t br = fread(&sectorbuffer[0], 1, 512, disk[drivenum].diskfile);

        if (br != 512) {
            cpu_set_ah(disk_cpu, 0x04);    // sector not found
            cpu_set_al(disk_cpu, 0);
            cpu_set_cf(disk_cpu, 1);
            return;
        }

        if (is_verify) {
            for (int sectoffset = 0; sectoffset < 512; sectoffset++) {
                // Verify sector data
                if (disk_mem[memdest++] != sectorbuffer[sectoffset]) {
                    // Sector verify failed
                    cpu_set_al(disk_cpu, cursect);
                    cpu_set_cf(disk_cpu, 1);
                    cpu_set_ah(disk_cpu, 0xBB);
                    return;
                }
            }
        } else {
            // Copy sector data to memory
            memcpy(disk_mem + memdest, sectorbuffer, 512);
            memdest += 512;
        }

        // Update file offset for next sector
        fileoffset += 512;
    }

    // If no sectors could be read, handle the error
    if (cursect == 0) {
        cpu_set_ah(disk_cpu, 0x04);    // sector not found
        cpu_set_al(disk_cpu, 0);
        cpu_set_cf(disk_cpu, 1);
        return;
    }

    // Set success flags
    cpu_set_al(disk_cpu, cursect);
    cpu_set_cf(disk_cpu, 0);
    cpu_set_ah(disk_cpu, 0);
}

static void writedisk(uint8_t drivenum,
               uint16_t dstseg, uint16_t dstoff,
               uint16_t cyl, uint16_t sect, uint16_t head,
               uint16_t sectcount
) {
    uint32_t memdest = ((uint32_t) dstseg << 4) + (uint32_t) dstoff;
    uint32_t cursect;

    // Check if disk is inserted
    if (!disk[drivenum].inserted) {
        cpu_set_ah(disk_cpu, 0x31);    // no media in drive
        cpu_set_al(disk_cpu, 0);
        cpu_set_cf(disk_cpu, 1);
        return;
    }

    // Convert CHS to file offset
    size_t fileoffset = chs2ofs(drivenum, cyl, head, sect);

    // check if sector can be found
    if (
            ((sect == 0 || sect > disk[drivenum].sects || cyl >= disk[drivenum].cyls || head >= disk[drivenum].heads))
            || fileoffset > disk[drivenum].filesize
            || disk[drivenum].filesize < fileoffset
            ) {
        cpu_set_ah(disk_cpu, 0x04);    // sector not found
        cpu_set_al(disk_cpu, 0);
        cpu_set_cf(disk_cpu, 1);
        return;
    }

    // Check if drive is read-only
    if (disk[drivenum].readonly) {
        cpu_set_ah(disk_cpu, 0x03);    // drive is read-only
        cpu_set_al(disk_cpu, 0);
        cpu_set_cf(disk_cpu, 1);
        return;
    }

    // Set file position
    fseek(disk[drivenum].diskfile, fileoffset, SEEK_SET);

    // Write each sector
    for (cursect = 0; cursect < sectcount; cursect++) {
        // Copy from memory to sector buffer
        memcpy(sectorbuffer, disk_mem + memdest, 512);
        memdest += 512;

        // Write the buffer to the file
        size_t bw = fwrite(sectorbuffer, 1, 512, disk[drivenum].diskfile);
        if (bw != 512) {
            cpu_set_ah(disk_cpu, 0x04);
            cpu_set_al(disk_cpu, cursect);
            cpu_set_cf(disk_cpu, 1);
            return;
        }
    }
    fflush(disk[drivenum].diskfile);

    // Handle the case where no sectors were written
    if (sectcount && cursect == 0) {
        cpu_set_ah(disk_cpu, 0x04);    // sector not found
        cpu_set_al(disk_cpu, 0);
        cpu_set_cf(disk_cpu, 1);
        return;
    }

    // Set success flags
    cpu_set_al(disk_cpu, cursect);
    cpu_set_cf(disk_cpu, 0);
    cpu_set_ah(disk_cpu, 0);
}


void diskhandler(CPUI386 *cpu) {
    static uint8_t lastdiskah[4] = { 0 }, lastdiskcf[4] = { 0 };

    disk_cpu = cpu;
    disk_mem = cpu_get_phys_mem(cpu);

    uint8_t drivenum = cpu_get_dl(cpu);

    // Normalize drivenum for hard drives
    if (drivenum & 0x80) drivenum -= 126;

    uint8_t ah = cpu_get_ah(cpu);

    // Handle the interrupt service based on the function requested in AH
    switch (ah) {
        case 0x00:  // Reset disk system
            if (disk[drivenum].inserted) {
                cpu_set_ah(cpu, 0);
                cpu_set_cf(cpu, 0);  // Successful reset (no-op in emulator)
            } else {
                cpu_set_cf(cpu, 1);  // Disk not inserted
            }
            break;

        case 0x01:  // Return last status
            cpu_set_ah(cpu, lastdiskah[drivenum]);
            cpu_set_cf(cpu, lastdiskcf[drivenum]);
            return;

        case 0x02:  // Read sector(s) into memory
            readdisk(drivenum, cpu_get_es(cpu), cpu_get_bx(cpu),
                     cpu_get_ch(cpu) + (cpu_get_cl(cpu) / 64) * 256,  // Cylinder
                     cpu_get_cl(cpu) & 63,                            // Sector
                     cpu_get_dh(cpu),                                 // Head
                     cpu_get_al(cpu),                                 // Sector count
                     0);                                              // Read operation
            break;

        case 0x03:  // Write sector(s) from memory
            writedisk(drivenum, cpu_get_es(cpu), cpu_get_bx(cpu),
                      cpu_get_ch(cpu) + (cpu_get_cl(cpu) / 64) * 256,  // Cylinder
                      cpu_get_cl(cpu) & 63,                            // Sector
                      cpu_get_dh(cpu),                                 // Head
                      cpu_get_al(cpu));                                // Sector count
            break;

        case 0x04:  // Verify sectors
            readdisk(drivenum, cpu_get_es(cpu), cpu_get_bx(cpu),
                     cpu_get_ch(cpu) + (cpu_get_cl(cpu) / 64) * 256,   // Cylinder
                     cpu_get_cl(cpu) & 63,                             // Sector
                     cpu_get_dh(cpu),                                  // Head
                     cpu_get_al(cpu),                                  // Sector count
                     1);                                               // Verify operation
            break;

        case 0x05:  // Format track
            cpu_set_cf(cpu, 0);  // Success (no-op for emulator)
            cpu_set_ah(cpu, 0);
            break;

        case 0x08:  // Get drive parameters
            if (disk[drivenum].inserted) {
                cpu_set_cf(cpu, 0);
                cpu_set_ah(cpu, 0);
                cpu_set_ch(cpu, disk[drivenum].cyls - 1);
                cpu_set_cl(cpu, (disk[drivenum].sects & 63) + ((disk[drivenum].cyls / 256) * 64));
                cpu_set_dh(cpu, disk[drivenum].heads - 1);

                // Set DL and BL for floppy or hard drive
                if (cpu_get_dl(cpu) < 2) {
                    cpu_set_bl(cpu, 4);  // Floppy
                    cpu_set_dl(cpu, 2);
                } else {
                    cpu_set_dl(cpu, hdcount);  // Hard disk
                }
            } else {
                cpu_set_cf(cpu, 1);
                cpu_set_ah(cpu, 0xAA);  // Error code for no disk inserted
            }
            break;

        default:  // Unknown function requested
            cpu_set_cf(cpu, 1);  // Error
            break;
    }

    // Update last disk status
    lastdiskah[drivenum] = cpu_get_ah(cpu);
    lastdiskcf[drivenum] = cpu_get_cf(cpu);

    // Set the last status in BIOS Data Area (for hard drives)
    if (cpu_get_dl(cpu) & 0x80) {
        disk_mem[0x474] = cpu_get_ah(cpu);
    }
}
