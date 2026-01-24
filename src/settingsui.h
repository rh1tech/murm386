/**
 * Settings UI for murm386
 *
 * On-screen settings manager triggered by Win+F11.
 * Allows changing emulator configuration at runtime.
 */

#ifndef SETTINGSUI_H
#define SETTINGSUI_H

#include <stdint.h>
#include <stdbool.h>

// Initialize settings UI
void settingsui_init(void);

// Open settings menu
void settingsui_open(void);

// Close settings menu
void settingsui_close(void);

// Check if settings menu is open
bool settingsui_is_open(void);

// Handle keyboard input
// Returns true if key was consumed
bool settingsui_handle_key(int keycode, bool is_down);

// Check if restart is requested (settings changed and confirmed)
bool settingsui_restart_requested(void);

// Clear restart request flag
void settingsui_clear_restart(void);

// Linux keycodes
#define KEY_F11     87

#endif // SETTINGSUI_H
