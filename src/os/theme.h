// ClaudeOS — color themes
#pragma once
#include <stdint.h>

// RGB888 -> RGB565
#define C565(r, g, b) ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))

struct Theme {
    uint16_t bg;        // screen background
    uint16_t fg;        // primary text
    uint16_t dim;       // secondary text
    uint16_t panel;     // panels / cards
    uint16_t panelHi;   // raised panel / hint bar
    uint16_t accent;    // Claude orange
    uint16_t accent2;   // secondary accent (blue)
    uint16_t ok;        // success green
    uint16_t danger;    // error red
    uint16_t statusBg;  // status bar background
    uint16_t statusFg;  // status bar text
    uint16_t sel;       // selection background
    uint16_t selText;   // selection text
};

inline Theme darkTheme()
{
    Theme t;
    t.bg       = C565(0x10, 0x14, 0x18);
    t.fg       = C565(0xE8, 0xE4, 0xDC);
    t.dim      = C565(0x8A, 0x94, 0xA0);
    t.panel    = C565(0x1C, 0x22, 0x28);
    t.panelHi  = C565(0x26, 0x2E, 0x36);
    t.accent   = C565(0xD9, 0x77, 0x57);  // Claude orange
    t.accent2  = C565(0x6C, 0xA0, 0xDC);
    t.ok       = C565(0x7B, 0xC4, 0x7F);
    t.danger   = C565(0xE0, 0x5C, 0x4B);
    t.statusBg = C565(0x18, 0x1E, 0x24);
    t.statusFg = C565(0xC8, 0xC4, 0xBC);
    t.sel      = C565(0xD9, 0x77, 0x57);
    t.selText  = C565(0x14, 0x10, 0x0C);
    return t;
}

inline Theme lightTheme()
{
    Theme t;
    t.bg       = C565(0xF0, 0xEE, 0xE6);  // Claude cream
    t.fg       = C565(0x26, 0x26, 0x25);
    t.dim      = C565(0x78, 0x74, 0x6A);
    t.panel    = C565(0xE2, 0xDE, 0xD4);
    t.panelHi  = C565(0xD8, 0xD3, 0xC6);
    t.accent   = C565(0xC0, 0x5C, 0x3C);
    t.accent2  = C565(0x2E, 0x64, 0xA8);
    t.ok       = C565(0x2E, 0x7D, 0x32);
    t.danger   = C565(0xB0, 0x2E, 0x1E);
    t.statusBg = C565(0xE6, 0xE2, 0xD6);
    t.statusFg = C565(0x40, 0x3C, 0x36);
    t.sel      = C565(0xC0, 0x5C, 0x3C);
    t.selText  = C565(0xFF, 0xFA, 0xF2);
    return t;
}
