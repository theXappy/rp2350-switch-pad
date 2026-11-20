#!/usr/bin/env python3
"""
Linux driver for rp2350-switch-pad (replaces komabear's Windows-only host).

Sends pad frames to the board's "Pad Bridge Link" (2E8A:400B) as 64-byte HID
feature reports, matching host/bridge_hid.py's wire format (proto.h). The board
turns them into PowerA-pad inputs on its NATIVE port -> the Switch 2.

Needs write access to the link's /dev/hidraw* — install linux/99-switch-pad.rules
first (see that file), then replug the PIO (USB-A) cable. Run NORMAL firmware
(not the SELFTEST build) for this to do anything.

Keys (same spirit as the earlier drive.py):
  Arrows=D-pad  w a s d=L-stick  i j k l=R-stick
  space/f=A  e=B  r=X  q=Y   [=L  ]=R  -=ZL  ==ZR
  +=Plus  _=Minus  h=Home  c=Capture   n=neutral   ?=help   Q=quit
Each key is a momentary tap (press, brief hold, release). A background keepalive
frame is sent ~60/s so the firmware's 400 ms release watchdog never fires.

Usage: python3 linux/drive_switch.py [/dev/hidrawN]
"""
import os, sys, glob, time, termios, tty, select, struct, fcntl

LINK_VID, LINK_PID = 0x2e8a, 0x400b
HOLD = 0.12

# proto.h frame: '$','P',flags1,flags2,lx,ly,rx,ry,hat,reserved,xor  (11 bytes)
PF1 = {"Y":1<<0,"B":1<<1,"A":1<<2,"X":1<<3,"L":1<<4,"R":1<<5,"ZL":1<<6,"ZR":1<<7}
PF2 = {"MINUS":1<<0,"PLUS":1<<1,"L3":1<<2,"R3":1<<3,"HOME":1<<4,"CAPTURE":1<<5}
HAT = {"up":0,"right":2,"down":4,"left":6,"center":8}


def _IOWR(t, nr, size):
    return (3 << 30) | (size << 16) | (t << 8) | nr

def hidiocsfeature(fd, data):
    return fcntl.ioctl(fd, _IOWR(ord('H'), 0x06, len(data)), bytes(data), True)


def find_link():
    if len(sys.argv) > 1:
        return sys.argv[1]
    for hr in sorted(glob.glob("/dev/hidraw*")):
        base = os.path.basename(hr)
        u = f"/sys/class/hidraw/{base}/device/uevent"
        try: txt = open(u).read().upper()
        except Exception: continue
        if f"{LINK_VID:04X}:{LINK_PID:04X}".replace(":", ":") in txt or \
           (f"{LINK_VID:04X}" in txt and f"{LINK_PID:04X}" in txt):
            return hr
    return None


def build_report(f1, f2, lx, ly, rx, ry, hat, seq):
    # 9-byte payload: flags1,flags2,lx,ly,rx,ry,hat,seq,xor
    def i8(v): return v & 0xFF
    p = [f1 & 0xFF, f2 & 0xFF, i8(lx), i8(ly), i8(rx), i8(ry), hat & 0xFF, seq & 0xFF]
    x = 0
    for b in p: x ^= b
    p.append(x)
    return bytes([0]) + bytes(p) + bytes(55)   # 65 bytes: id 0 + 64


def read_key(timeout):
    r, _, _ = select.select([sys.stdin], [], [], timeout)
    if not r: return None
    ch = sys.stdin.read(1)
    if ch == "\x1b":
        rest = ""
        while select.select([sys.stdin], [], [], 0.005)[0]:
            rest += sys.stdin.read(1)
        return "\x1b" + rest
    return ch

ARROWS = {"\x1b[A":"up","\x1b[B":"down","\x1b[C":"right","\x1b[D":"left"}
KEYBTN = {" ":"A","f":"A","e":"B","r":"X","q":"Y","[":"L","]":"R","-":"ZL","=":"ZR",
          "+":"PLUS","_":"MINUS","h":"HOME","c":"CAPTURE"}
STICK = {"w":("ly",127),"s":("ly",-127),"a":("lx",-127),"d":("lx",127),
         "i":("ry",127),"k":("ry",-127),"j":("rx",-127),"l":("rx",127)}


def main():
    link = find_link()
    if not link:
        print(f"Pad Bridge Link ({LINK_VID:04X}:{LINK_PID:04X}) not found.")
        print("Is NORMAL firmware flashed and the PIO (USB-A) cable in the PC? Try replugging it.")
        sys.exit(2)
    try:
        fd = os.open(link, os.O_RDWR)
    except PermissionError:
        print(f"No access to {link}. Install linux/99-switch-pad.rules (see its header), then replug PIO.")
        sys.exit(2)
    print(f"# link {link}")

    seq = 0
    def send(f1=0,f2=0,lx=0,ly=0,rx=0,ry=0,hat=8):
        nonlocal seq
        seq = (seq + 1) & 0xFF
        try: hidiocsfeature(fd, build_report(f1,f2,lx,ly,rx,ry,hat,seq))
        except OSError as e: print("send failed:", e)

    def tap(**kw):
        send(**kw); time.sleep(HOLD); send()

    print(__doc__[__doc__.index("Keys"):])
    old = termios.tcgetattr(sys.stdin)
    try:
        tty.setcbreak(sys.stdin.fileno())
        last = time.time()
        while True:
            k = read_key(0.016)
            if k is None:
                if time.time() - last > 0.2: send(); last = time.time()  # keepalive
                continue
            last = time.time()
            if k in ("Q","\x03"): break
            elif k == "?": print(__doc__[__doc__.index("Keys"):])
            elif k == "n": send(); print("neutral")
            elif k in ARROWS: d=ARROWS[k]; tap(hat=HAT[d]); print(f"dpad {d}")
            elif k in KEYBTN:
                nm=KEYBTN[k]
                if nm in PF1: tap(f1=PF1[nm])
                else: tap(f2=PF2[nm])
                print(nm)
            elif k in STICK:
                ax,val=STICK[k]; tap(**{ax:val}); print(f"stick {ax}={val}")
    finally:
        termios.tcsetattr(sys.stdin, termios.TCSADRAIN, old)
        send()  # neutral
        os.close(fd)
        print("\nbye")


if __name__ == "__main__":
    main()
