/*
 * sv_proto.h - SwitchView wire protocol, as understood over the board's PC link.
 *
 * Mirrors proto/switchview_proto.h from the SwitchView project so this firmware
 * can accept the same packets SwitchView's app sends. Over SwitchView it's UDP;
 * here the 33-byte sv_packet rides in the link's 64-byte HID feature report,
 * with a 1-byte rolling sequence appended at offset SV_SEQ_OFFSET (our link is
 * polled, so we need new-packet detection that UDP doesn't).
 */
#pragma once
#include <stdint.h>

#define SV_MAGIC0 'S'
#define SV_MAGIC1 'W'

enum { SV_PKT_STATE = 1, SV_PKT_TEXT = 2, SV_PKT_PING = 3, SV_PKT_MOUSE = 4 };

enum {
  SV_BTN_A = 1u << 0, SV_BTN_B = 1u << 1, SV_BTN_X = 1u << 2, SV_BTN_Y = 1u << 3,
  SV_BTN_L = 1u << 4, SV_BTN_R = 1u << 5, SV_BTN_ZL = 1u << 6, SV_BTN_ZR = 1u << 7,
  SV_BTN_PLUS = 1u << 8, SV_BTN_MINUS = 1u << 9, SV_BTN_HOME = 1u << 10,
  SV_BTN_CAPTURE = 1u << 11, SV_BTN_L3 = 1u << 12, SV_BTN_R3 = 1u << 13,
};

#define SV_TEXT_MAX 16
#define SV_PACKET_SIZE 33
#define SV_SEQ_OFFSET 33   /* our link framing: rolling seq byte after the packet */

/* byte offsets inside the sv_packet */
#define SV_OFF_MAGIC0 0
#define SV_OFF_MAGIC1 1
#define SV_OFF_TYPE 2
#define SV_OFF_LENGTH 3
#define SV_OFF_BUTTONS 4   /* u32 LE */
#define SV_OFF_LX 8        /* i16 LE each */
#define SV_OFF_LY 10
#define SV_OFF_RX 12
#define SV_OFF_RY 14
#define SV_OFF_HAT 16
#define SV_OFF_TEXT 17     /* 16 bytes */
