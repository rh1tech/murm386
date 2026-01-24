/**
 * Configuration Save for murm386
 *
 * Writes configuration to INI file on SD card.
 */

#ifndef CONFIG_SAVE_H
#define CONFIG_SAVE_H

#include <stdint.h>
#include <stdbool.h>

// Save all configuration to INI file
// Returns true on success
bool config_save_all(void);

// Save just disk configuration
bool config_save_disks(void);

// Get/set configuration values (stored in memory until saved)
int config_get_mem_size_mb(void);
void config_set_mem_size_mb(int mb);

int config_get_vga_mem_kb(void);
void config_set_vga_mem_kb(int kb);

int config_get_cpu_gen(void);
void config_set_cpu_gen(int gen);

int config_get_fpu(void);
void config_set_fpu(int enabled);

// Check if config has unsaved changes
bool config_has_changes(void);

// Reset change tracking (call after save)
void config_clear_changes(void);

// Initialize config from current PCConfig
void config_init_from_current(void);

#endif // CONFIG_SAVE_H
