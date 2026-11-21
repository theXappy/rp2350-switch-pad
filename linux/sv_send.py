#!/usr/bin/env python3
"""
Send SwitchView-protocol packets to the board's "Pad Bridge Link" (2E8A:400B),
which the composite firmware turns into keyboard text (and controller input) on
the Switch. No pip; needs write access to the link hidraw (install
linux/99-switch-pad.rules first, then replug the PIO/USB-A cable).

Firmware must be the COMPOSITE build (controller + keyboard), NOT the SELFTEST one.

Usage:
  python3 linux/sv_send.py text "hello world"      # type a string
  echo "some text" | python3 linux/sv_send.py type # type stdin (lines)
  python3 linux/sv_send.py press A B               # tap controller buttons
  python3 linux/sv_send.py --link /dev/hidrawN ... # force the device
"""
import os, sys, glob, time, struct, fcntl

LINK_VID, LINK_PID = 0x2e8a, 0x400b
SV_MAGIC = b"SW"
SV_PKT_STATE, SV_PKT_TEXT = 1, 2
SV_TEXT_MAX = 16
SV_SEQ_OFFSET = 33
HAT_CENTER = 8
BTN = {"A":1<<0,"B":1<<1,"X":1<<2,"Y":1<<3,"L":1<<4,"R":1<<5,"ZL":1<<6,"ZR":1<<7,
       "PLUS":1<<8,"MINUS":1<<9,"HOME":1<<10,"CAPTURE":1<<11,"L3":1<<12,"R3":1<<13}

_SEQ_FILE = os.path.expanduser("~/.sv_send_seq")

def _load_seq():
    try: return int(open(_SEQ_FILE).read().strip()) & 0xFF
    except Exception: return 0

def _save_seq(v):
    try: open(_SEQ_FILE, "w").write(str(v & 0xFF))
    except Exception: pass

_seq = _load_seq()


def _IOWR(t, nr, size):
    return (3 << 30) | (size << 16) | (t << 8) | nr

def hidiocsfeature(fd, data):
    fcntl.ioctl(fd, _IOWR(ord('H'), 0x06, len(data)), bytes(data), True)


def find_link(argv):
    if "--link" in argv:
        return argv[argv.index("--link") + 1]
    for hr in sorted(glob.glob("/dev/hidraw*")):
        u = f"/sys/class/hidraw/{os.path.basename(hr)}/device/uevent"
        try: txt = open(u).read().upper()
        except Exception: continue
        if f"{LINK_VID:04X}" in txt and f"{LINK_PID:04X}" in txt:
            return hr
    return None


def sv_packet(ptype, length=0, buttons=0, lx=0, ly=0, rx=0, ry=0, hat=HAT_CENTER, text=b""):
    p = bytearray(33)
    p[0:2] = SV_MAGIC
    p[2] = ptype
    p[3] = length
    struct.pack_into("<I", p, 4, buttons & 0xFFFFFFFF)
    struct.pack_into("<hhhh", p, 8, lx, ly, rx, ry)
    p[16] = hat
    t = text[:SV_TEXT_MAX]
    p[17:17 + len(t)] = t
    return bytes(p)


def send(fd, packet33):
    global _seq
    _seq = (_seq + 1) & 0xFF
    buf = bytes([0]) + packet33 + bytes([_seq]) + bytes(64 - SV_SEQ_OFFSET - 1)
    hidiocsfeature(fd, buf)  # 65 bytes: report id 0 + 64 payload
    _save_seq(_seq)


def type_text(fd, s):
    data = s.encode("ascii", "ignore")
    for i in range(0, len(data), SV_TEXT_MAX):
        chunk = data[i:i + SV_TEXT_MAX]
        send(fd, sv_packet(SV_PKT_TEXT, length=len(chunk), text=chunk))
        time.sleep(len(chunk) * 0.105)  # let the firmware type it (~10 cps)


def main():
    argv = sys.argv[1:]
    link = find_link(argv)
    if not link:
        print(f"Link {LINK_VID:04X}:{LINK_PID:04X} not found. Composite firmware flashed, "
              f"PIO/USB-A cable in the PC (replug it), and udev rule installed?")
        sys.exit(2)
    # strip --link pair
    if "--link" in argv:
        j = argv.index("--link"); del argv[j:j + 2]
    try:
        fd = os.open(link, os.O_RDWR)
    except PermissionError:
        print(f"No access to {link}. Install linux/99-switch-pad.rules, reload udev, replug PIO.")
        sys.exit(2)
    print(f"# link {link}")

    if not argv:
        print(__doc__); sys.exit(0)
    cmd = argv[0]
    if cmd == "text":
        type_text(fd, " ".join(argv[1:]))
    elif cmd == "type":
        for line in sys.stdin:
            type_text(fd, line.rstrip("\n"))
            send(fd, sv_packet(SV_PKT_TEXT, length=1, text=b"\n"))  # Enter per line
            time.sleep(0.12)
    elif cmd == "press":
        mask = 0
        for name in argv[1:]:
            mask |= BTN.get(name.upper(), 0)
        send(fd, sv_packet(SV_PKT_STATE, buttons=mask))
        time.sleep(0.12)
        send(fd, sv_packet(SV_PKT_STATE, buttons=0))  # release
    else:
        print(__doc__)
    os.close(fd)


if __name__ == "__main__":
    main()
