/*
 * keyboard_native.h - native-port USB keyboard support (KEYBOARD_MODE).
 *
 * Included by main.c only. Provides ASCII -> HID usage-code translation and the
 * per-loop keyboard task: with SELFTEST it auto-types a string (to prove the
 * Switch accepts the keyboard); otherwise it emits whatever text the PC has
 * queued over the link (filled by kbd_queue_text(), Phase 2).
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "tusb.h"
#include "pico/stdlib.h"

/* Which TinyUSB HID instance the keyboard lives on: 0 standalone (KEYBOARD_MODE),
 * 1 in the composite (COMPOSITE_MODE: gamepad=0, keyboard=1). */
#ifndef KBD_HID_INSTANCE
#define KBD_HID_INSTANCE 0
#endif

/* ASCII -> (modifier, HID usage). Returns false for characters we can't type. */
static bool ascii_to_hid(char ch, uint8_t *mod, uint8_t *key) {
    uint8_t c = (uint8_t)ch;
    *mod = 0; *key = 0;
    if (c >= 'a' && c <= 'z') { *key = (uint8_t)(0x04 + (c - 'a')); return true; }
    if (c >= 'A' && c <= 'Z') { *key = (uint8_t)(0x04 + (c - 'A')); *mod = KEYBOARD_MODIFIER_LEFTSHIFT; return true; }
    if (c >= '1' && c <= '9') { *key = (uint8_t)(0x1E + (c - '1')); return true; }
    switch (c) {
        case '0': *key = 0x27; return true;
        case '\n': *key = 0x28; return true;   /* Enter */
        case '\x1b': *key = 0x29; return true;  /* Esc */
        case '\b': *key = 0x2A; return true;    /* Backspace */
        case '\t': *key = 0x2B; return true;    /* Tab */
        case ' ': *key = 0x2C; return true;
        case '-': *key = 0x2D; return true;
        case '_': *key = 0x2D; *mod = KEYBOARD_MODIFIER_LEFTSHIFT; return true;
        case '=': *key = 0x2E; return true;
        case '+': *key = 0x2E; *mod = KEYBOARD_MODIFIER_LEFTSHIFT; return true;
        case '[': *key = 0x2F; return true;
        case '{': *key = 0x2F; *mod = KEYBOARD_MODIFIER_LEFTSHIFT; return true;
        case ']': *key = 0x30; return true;
        case '}': *key = 0x30; *mod = KEYBOARD_MODIFIER_LEFTSHIFT; return true;
        case '\\': *key = 0x31; return true;
        case '|': *key = 0x31; *mod = KEYBOARD_MODIFIER_LEFTSHIFT; return true;
        case ';': *key = 0x33; return true;
        case ':': *key = 0x33; *mod = KEYBOARD_MODIFIER_LEFTSHIFT; return true;
        case '\'': *key = 0x34; return true;
        case '"': *key = 0x34; *mod = KEYBOARD_MODIFIER_LEFTSHIFT; return true;
        case '`': *key = 0x35; return true;
        case '~': *key = 0x35; *mod = KEYBOARD_MODIFIER_LEFTSHIFT; return true;
        case ',': *key = 0x36; return true;
        case '<': *key = 0x36; *mod = KEYBOARD_MODIFIER_LEFTSHIFT; return true;
        case '.': *key = 0x37; return true;
        case '>': *key = 0x37; *mod = KEYBOARD_MODIFIER_LEFTSHIFT; return true;
        case '/': *key = 0x38; return true;
        case '?': *key = 0x38; *mod = KEYBOARD_MODIFIER_LEFTSHIFT; return true;
        case '!': *key = 0x1E; *mod = KEYBOARD_MODIFIER_LEFTSHIFT; return true;
        case '@': *key = 0x1F; *mod = KEYBOARD_MODIFIER_LEFTSHIFT; return true;
        case '#': *key = 0x20; *mod = KEYBOARD_MODIFIER_LEFTSHIFT; return true;
        case '$': *key = 0x21; *mod = KEYBOARD_MODIFIER_LEFTSHIFT; return true;
        case '%': *key = 0x22; *mod = KEYBOARD_MODIFIER_LEFTSHIFT; return true;
        case '^': *key = 0x23; *mod = KEYBOARD_MODIFIER_LEFTSHIFT; return true;
        case '&': *key = 0x24; *mod = KEYBOARD_MODIFIER_LEFTSHIFT; return true;
        case '*': *key = 0x25; *mod = KEYBOARD_MODIFIER_LEFTSHIFT; return true;
        case '(': *key = 0x26; *mod = KEYBOARD_MODIFIER_LEFTSHIFT; return true;
        case ')': *key = 0x27; *mod = KEYBOARD_MODIFIER_LEFTSHIFT; return true;
        default: return false;
    }
}

/* ---- Text queue the PC fills (Phase 2); drained one keystroke at a time ---- */
#define KBD_QUEUE_LEN 256
static volatile char kbd_q[KBD_QUEUE_LEN];
static volatile uint16_t kbd_q_head, kbd_q_tail; /* head=write, tail=read */

static inline void kbd_queue_char(char c) {
    uint16_t nh = (uint16_t)((kbd_q_head + 1) % KBD_QUEUE_LEN);
    if (nh != kbd_q_tail) { kbd_q[kbd_q_head] = c; kbd_q_head = nh; }
}
static inline bool kbd_queue_pop(char *out) {
    if (kbd_q_tail == kbd_q_head) return false;
    *out = kbd_q[kbd_q_tail];
    kbd_q_tail = (uint16_t)((kbd_q_tail + 1) % KBD_QUEUE_LEN);
    return true;
}

/*
 * Per-loop keyboard task. Two-phase per character: press (report with the key
 * held), then release (empty report). ~12 ms between phases is fast but gives
 * the host clean down/up edges. Call it every main-loop iteration.
 */
static void kbd_native_task(void) {
    if (!tud_hid_n_ready(KBD_HID_INSTANCE)) return;
    static uint32_t last_ms;
    static bool holding;
    static char cur;
    uint32_t now = board_millis();
    if ((now - last_ms) < 12) return;

#ifdef SELFTEST
    /* auto-type a visible marker so the Switch's text field fills up */
    static const char msg[] = "hello ";
    static uint8_t mi;
    if (!holding) { cur = msg[mi]; mi = (uint8_t)((mi + 1) % (sizeof(msg) - 1)); }
#else
    if (!holding) { if (!kbd_queue_pop(&cur)) return; }
#endif

    last_ms = now;
    if (!holding) {
        uint8_t mod = 0, key = 0, kc[6] = {0};
        if (ascii_to_hid(cur, &mod, &key)) {
            kc[0] = key;
            tud_hid_n_keyboard_report(KBD_HID_INSTANCE, 0, mod, kc);
            holding = true;
        }
        /* unmapped char: just skip it (stay not-holding) */
    } else {
        tud_hid_n_keyboard_report(KBD_HID_INSTANCE, 0, 0, NULL); /* release */
        holding = false;
    }
}
