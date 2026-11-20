#!/usr/bin/env python3
"""
Sniff the PowerA pad (20D6:A713) the board presents on its NATIVE port.

With SELFTEST firmware the board cycles D-pad E,S,W,N (1 s each) then pulses A,
forever. This reads the joystick device Linux creates and confirms those inputs
are really arriving — i.e. the Switch-facing pad enumerates AND emits input,
all without needing the (root-only) PIO link.

No pip, no sudo: /dev/input/js* is world-readable.

Usage: python3 linux/sniff_powa.py [seconds]
"""
import os, sys, glob, time, struct, select

SECONDS = float(sys.argv[1]) if len(sys.argv) > 1 else 8.0
JS_EVENT = struct.Struct("<IhBB")          # time(ms), value, type, number
JS_BUTTON, JS_AXIS, JS_INIT = 0x01, 0x02, 0x80

POWA_VID, POWA_PID = 0x20d6, 0xa713


def find_powa_js():
    """Return (js_path, name) for the PowerA pad, else (None, reason)."""
    # Match via /sys so we are sure it's 20D6:A713, not some other pad.
    for js in sorted(glob.glob("/dev/input/js*")):
        base = os.path.basename(js)
        # /sys/class/input/jsN/device/ -> walk up to the hid uevent with the ids
        syspath = f"/sys/class/input/{base}/device"
        uevent = ""
        for _ in range(6):
            u = os.path.join(syspath, "uevent")
            if os.path.exists(u):
                try: uevent += open(u).read()
                except Exception: pass
            syspath = os.path.join(syspath, "..")
        up = uevent.upper()
        hexid = f"{POWA_VID:04X}:{POWA_PID:04X}"
        if hexid in up or "POWERA" in up or "BDA" in up or "NSW" in up:
            name = ""
            np = f"/sys/class/input/{base}/device/name"
            try: name = open(np).read().strip()
            except Exception: pass
            return js, name or hexid
    return None, None


def main():
    js, name = find_powa_js()
    if not js:
        alljs = glob.glob("/dev/input/js*")
        print("PowerA pad (20D6:A713) not found among:", alljs or "(no js devices)")
        print("Is the board's NATIVE (Type-C) port plugged into this PC and flashed?")
        # fall back: if exactly one js exists, sniff it anyway
        if len(alljs) == 1:
            js = alljs[0]; name = "(unverified) " + js
            print(f"Only one joystick present; sniffing it anyway: {js}")
        else:
            sys.exit(2)
    print(f"# sniffing {js}  name={name!r}  for {SECONDS:.0f}s")

    fd = os.open(js, os.O_RDONLY | os.O_NONBLOCK)
    # drain init burst (reports current state)
    t0 = time.time()
    init_axes, init_btns = set(), set()
    while time.time() - t0 < 0.4:
        r, _, _ = select.select([fd], [], [], 0.1)
        if not r: continue
        d = os.read(fd, JS_EVENT.size * 64)
        for i in range(0, len(d) - JS_EVENT.size + 1, JS_EVENT.size):
            _, v, t, n = JS_EVENT.unpack_from(d, i)
            if t & JS_AXIS: init_axes.add(n)
            if t & JS_BUTTON: init_btns.add(n)
    print(f"# device exposes {len(init_axes)} axes, {len(init_btns)} buttons (from init report)")

    print("# live events (type/number=value):")
    btn_events = axis_events = 0
    changes = []
    end = time.time() + SECONDS
    while time.time() < end:
        r, _, _ = select.select([fd], [], [], max(0, end - time.time()))
        if not r: continue
        d = os.read(fd, JS_EVENT.size * 64)
        for i in range(0, len(d) - JS_EVENT.size + 1, JS_EVENT.size):
            tms, v, t, n = JS_EVENT.unpack_from(d, i)
            if t & JS_INIT: continue
            kind = "BTN" if (t & JS_BUTTON) else ("AXIS" if (t & JS_AXIS) else "?")
            if t & JS_BUTTON: btn_events += 1
            if t & JS_AXIS: axis_events += 1
            line = f"  t={tms:>7}ms  {kind} #{n} = {v}"
            print(line)
            changes.append((tms, kind, n, v))
    os.close(fd)

    print()
    print(f"# summary: {axis_events} axis events, {btn_events} button events over {SECONDS:.0f}s")
    # SELFTEST produces a change about once per second; expect several events.
    if axis_events + btn_events >= 3:
        print("PASS: the native PowerA pad is enumerated AND emitting live inputs.")
        print("      => the Switch-facing side works; this is what the Switch 2 will see.")
        sys.exit(0)
    else:
        print("INCONCLUSIVE: device present but few/no input events seen.")
        print("      The pad enumerated but the SELFTEST cycle wasn't observed in-window.")
        sys.exit(1)


if __name__ == "__main__":
    main()
