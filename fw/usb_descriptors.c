/*
 * usb_descriptors.c - native USB descriptors: CDC serial device only.
 *
 * NOTE: we deliberately do NOT composite XInput with CDC. The Windows Xbox
 * 360 driver binds at device level (XnaComposite) and fails on a composite
 * device (tested on hardware: device enumerates with Error, CDC never
 * appears). The Parsec host already provides the virtual Xbox pad; the
 * bridge tool reads it via the XInput API, so the board only needs CDC.
 */
#include "tusb.h"
#ifdef NATIVE_PAD
#ifdef COMPOSITE_MODE
/* -------- native port = COMPOSITE: PowerA gamepad (itf0) + keyboard (itf1) ----
 * Interface 0 is the licensed PowerA controller byte-for-byte (same report desc,
 * same IN endpoint 0x81) so the Switch still sees the licensed pad; interface 1
 * is a standard boot keyboard on IN 0x82. This is the experiment: does the
 * Switch still accept the controller when a keyboard is bolted on? */
#include "powa_descriptors.h"
extern uint16_t diag_fill(uint8_t *buf, uint16_t len);

/* standard boot keyboard report descriptor (63 bytes, no report ID) */
#define KBD_REPORT_DESC_LEN 63
static const uint8_t kbd_boot_report_desc[KBD_REPORT_DESC_LEN] = {
    0x05, 0x01, 0x09, 0x06, 0xA1, 0x01, 0x05, 0x07, 0x19, 0xE0, 0x29, 0xE7,
    0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x08, 0x81, 0x02, 0x95, 0x01,
    0x75, 0x08, 0x81, 0x03, 0x95, 0x05, 0x75, 0x01, 0x05, 0x08, 0x19, 0x01,
    0x29, 0x05, 0x91, 0x02, 0x95, 0x01, 0x75, 0x03, 0x91, 0x03, 0x95, 0x06,
    0x75, 0x08, 0x15, 0x00, 0x25, 0x65, 0x05, 0x07, 0x19, 0x00, 0x29, 0x65,
    0x81, 0x00, 0xC0,
};

/* relative mouse: 3 buttons + 16-bit X/Y + 8-bit wheel (report = 6 bytes) */
#define MOUSE_REPORT_DESC_LEN 64
static const uint8_t mouse_report_desc[MOUSE_REPORT_DESC_LEN] = {
    0x05, 0x01, 0x09, 0x02, 0xA1, 0x01, 0x09, 0x01, 0xA1, 0x00,
    0x05, 0x09, 0x19, 0x01, 0x29, 0x03, 0x15, 0x00, 0x25, 0x01,
    0x95, 0x03, 0x75, 0x01, 0x81, 0x02, 0x95, 0x01, 0x75, 0x05,
    0x81, 0x03, 0x05, 0x01, 0x09, 0x30, 0x09, 0x31, 0x16, 0x01,
    0x80, 0x26, 0xFF, 0x7F, 0x75, 0x10, 0x95, 0x02, 0x81, 0x06,
    0x09, 0x38, 0x15, 0x81, 0x25, 0x7F, 0x75, 0x08, 0x95, 0x01,
    0x81, 0x06, 0xC0, 0xC0,
};

/* composite config: 3 HID interfaces. 9(cfg)+25*3 = 84 */
#define COMPO_CONFIG_TOTAL_LEN 84
static const uint8_t compo_configuration_descriptor[COMPO_CONFIG_TOTAL_LEN] = {
    /* Configuration */
    0x09, 0x02, COMPO_CONFIG_TOTAL_LEN, 0x00, 0x03, 0x01, 0x00, 0x80, 0xFA,
    /* Interface 0 — PowerA gamepad (identical to the standalone pad) */
    0x09, 0x04, 0x00, 0x00, 0x01, 0x03, 0x00, 0x00, 0x00,
    0x09, 0x21, 0x11, 0x01, 0x00, 0x01, 0x22,
    (uint8_t)(POWA_REPORT_DESC_LEN & 0xFF), (uint8_t)((POWA_REPORT_DESC_LEN >> 8) & 0xFF),
    0x07, 0x05, 0x81, 0x03, 0x40, 0x00, 0x01, /* IN1 64B 1ms */
    /* Interface 1 — boot keyboard */
    0x09, 0x04, 0x01, 0x00, 0x01, 0x03, 0x01, 0x01, 0x00,
    0x09, 0x21, 0x11, 0x01, 0x00, 0x01, 0x22,
    (uint8_t)(KBD_REPORT_DESC_LEN & 0xFF), (uint8_t)((KBD_REPORT_DESC_LEN >> 8) & 0xFF),
    0x07, 0x05, 0x82, 0x03, 0x40, 0x00, 0x0A, /* IN2 64B 10ms */
    /* Interface 2 — relative mouse */
    0x09, 0x04, 0x02, 0x00, 0x01, 0x03, 0x00, 0x00, 0x00,
    0x09, 0x21, 0x11, 0x01, 0x00, 0x01, 0x22,
    (uint8_t)(MOUSE_REPORT_DESC_LEN & 0xFF), (uint8_t)((MOUSE_REPORT_DESC_LEN >> 8) & 0xFF),
    0x07, 0x05, 0x83, 0x03, 0x40, 0x00, 0x0A, /* IN3 64B 10ms */
};

uint8_t const *tud_descriptor_device_cb(void) { return powa_device_descriptor; }
uint8_t const *tud_descriptor_configuration_cb(uint8_t i) { (void)i; return compo_configuration_descriptor; }
uint16_t tud_hid_get_report_cb(uint8_t i, uint8_t id, hid_report_type_t t, uint8_t *b, uint16_t n) {
    (void)id;
    if (i == 0 && t == HID_REPORT_TYPE_FEATURE) return diag_fill(b, n); /* PowerA vendor feature */
    return 0;
}
void tud_hid_set_report_cb(uint8_t i, uint8_t id, hid_report_type_t t, uint8_t const *b, uint16_t n) { (void)i;(void)id;(void)t;(void)b;(void)n; }
uint8_t const *tud_hid_descriptor_report_cb(uint8_t i) {
    if (i == 1) return kbd_boot_report_desc;
    if (i == 2) return mouse_report_desc;
    return powa_report_descriptor;
}
uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void)langid;
    static uint16_t d[32];
    static char const *s[] = { [1] = "BDA", [2] = "NSW wired controller" };
    uint8_t len = 0;
    if (index == 0) { d[1] = 0x0409; len = 1; }
    else if (index < 3) { for (; s[index][len]; len++) d[1 + len] = (uint16_t)s[index][len]; }
    else return NULL;
    d[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * len + 2));
    return d;
}
#elif defined(KEYBOARD_MODE)
/* -------- native port = standard USB HID boot keyboard (for the Switch) -------
 * A generic keyboard needs no licensing, so the Switch accepts it for text entry
 * anywhere a field is open. Built from TinyUSB's own keyboard macros. */
#ifndef KBD_VID
#define KBD_VID 0x2E8A /* Raspberry Pi */
#endif
#ifndef KBD_PID
#define KBD_PID 0xA7B0
#endif
static const uint8_t kbd_report_desc[] = { TUD_HID_REPORT_DESC_KEYBOARD() };
static const tusb_desc_device_t kbd_dev_desc = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = 0x00, /* class defined at interface (HID) */
    .bDeviceSubClass = 0x00,
    .bDeviceProtocol = 0x00,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = KBD_VID,
    .idProduct = KBD_PID,
    .bcdDevice = 0x0100,
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x00,
    .bNumConfigurations = 0x01,
};
#define KBD_CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN)
static const uint8_t kbd_cfg_desc[] = {
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, KBD_CONFIG_TOTAL_LEN, 0x00, 100),
    TUD_HID_DESCRIPTOR(0, 0, HID_ITF_PROTOCOL_KEYBOARD, sizeof(kbd_report_desc),
                       0x81, CFG_TUD_HID_EP_BUFSIZE, 10),
};
uint8_t const *tud_descriptor_device_cb(void) { return (uint8_t const *)&kbd_dev_desc; }
uint8_t const *tud_descriptor_configuration_cb(uint8_t i) { (void)i; return kbd_cfg_desc; }
uint16_t tud_hid_get_report_cb(uint8_t i, uint8_t id, hid_report_type_t t, uint8_t *b, uint16_t n) { (void)i;(void)id;(void)t;(void)b;(void)n; return 0; }
void tud_hid_set_report_cb(uint8_t i, uint8_t id, hid_report_type_t t, uint8_t const *b, uint16_t n) { (void)i;(void)id;(void)t;(void)b;(void)n; /* ignore LED state */ }
uint8_t const *tud_hid_descriptor_report_cb(uint8_t i) { (void)i; return kbd_report_desc; }
uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void)langid;
    static uint16_t d[32];
    static char const *s[] = { [1] = "RP2350", [2] = "RP2350 Keyboard" };
    uint8_t len = 0;
    if (index == 0) { d[1] = 0x0409; len = 1; }
    else if (index < 3) { for (; s[index][len]; len++) d[1 + len] = (uint16_t)s[index][len]; }
    else return NULL;
    d[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * len + 2));
    return d;
}
#else /* NATIVE_PAD, not KEYBOARD_MODE: the PowerA controller */
#include "powa_descriptors.h"
uint8_t const *tud_descriptor_device_cb(void) { return powa_device_descriptor; }
uint8_t const *tud_descriptor_configuration_cb(uint8_t i) { (void)i; return powa_configuration_descriptor; }
extern uint16_t diag_fill(uint8_t *buf, uint16_t len);
uint16_t tud_hid_get_report_cb(uint8_t i, uint8_t id, hid_report_type_t t, uint8_t *b, uint16_t n) { (void)i;(void)id; return t == HID_REPORT_TYPE_FEATURE ? diag_fill(b, n) : 0; }
void tud_hid_set_report_cb(uint8_t i, uint8_t id, hid_report_type_t t, uint8_t const *b, uint16_t n) { (void)i;(void)id;(void)t;(void)b;(void)n; }
uint8_t const *tud_hid_descriptor_report_cb(uint8_t i) { (void)i; return powa_report_descriptor; }
uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void)langid;
    static uint16_t d[32];
    static char const *s[] = { [1] = "BDA", [2] = "NSW wired controller" };
    uint8_t len = 0;
    if (index == 0) { d[1] = 0x0409; len = 1; }
    else if (index < 3) { for (; s[index][len]; len++) d[1 + len] = (uint16_t)s[index][len]; }
    else return NULL;
    d[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * len + 2));
    return d;
}
#endif /* KEYBOARD_MODE */
#else


enum {
    ITF_NUM_CDC = 0,
    ITF_NUM_CDC_DATA,
    ITF_NUM_TOTAL
};

#define EPNUM_CDC_NOTIF 0x81
#define EPNUM_CDC_OUT   0x02
#define EPNUM_CDC_IN    0x82

#define USBD_VID 0x2E8A /* Raspberry Pi */
#define USBD_PID 0x000A

#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN)

static const tusb_desc_device_t dev_desc = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = USBD_VID,
    .idProduct          = USBD_PID,
    .bcdDevice          = 0x0100,
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x00,
    .bNumConfigurations = 0x01,
};

static const uint8_t cfg_desc[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 100),
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, 0, EPNUM_CDC_NOTIF, 64,
                       EPNUM_CDC_OUT, EPNUM_CDC_IN, 64),
};

uint8_t const *tud_descriptor_device_cb(void) {
    return (uint8_t const *)&dev_desc;
}

uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
    (void)index;
    return cfg_desc;
}

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void)langid;
    static uint16_t desc_str[32];

    static char const *strings[] = {
        [1] = "mayflash_switch2_controller",
        [2] = "Pad Bridge",
    };

    uint8_t len = 0;
    if (index == 0) {
        desc_str[1] = 0x0409;
        len = 1;
    } else if (index < sizeof(strings) / sizeof(strings[0]) && strings[index]) {
        char const *s = strings[index];
        for (; s[len] && len < 31; len++) {
            desc_str[1 + len] = (uint16_t)s[len];
        }
    } else {
        return NULL;
    }

    desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * len + 2));
    return desc_str;
}

#endif
