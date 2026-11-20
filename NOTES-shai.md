# rp2350-switch-pad on your RP2350-USB-A — status & next steps

## TL;DR — it works

komabear's firmware **builds and runs on your Waveshare RP2350-USB-A unchanged**
(PIO pins GPIO12/13 and the native USB PHY are identical to the USB-C board it
targets — only the connector shape differs, which doesn't matter for USB 2.0).

I flashed the **SELFTEST** build and sniffed it on this PC:

```
20d6:a713 "BDA NSW wired controller"  ->  /dev/input/js0  (6 axes, 14 buttons)
live events: D-pad E/S/W/N cycling + A pulsing  ->  PASS
```

That is the exact licensed PowerA identity komabear verified on a **Switch 2**,
presented over the board's **native hardware USB** — the clean path. This is why
it should work on the Switch where the old Pro-Controller-over-PIO graft didn't:
that one bit-banged USB straight into the strict Switch host; this one gives the
Switch real hardware USB and only bit-bangs toward the (lenient) PC.

## What's on the board right now

The **SELFTEST** firmware (`out/bridge-SELFTEST.uf2`). It auto-cycles the D-pad +
A forever and needs no PC. Good for a no-PC dock test, but it ignores PC input.

## To actually drive it from this Linux PC

1. **Flash the drivable build.** Board in BOOTSEL (hold BOOT, tap RST), then:
   ```
   cp out/bridge-NORMAL.uf2 /run/media/$USER/RP2350/
   ```
2. **Cabling:** native **Type-C → Switch 2 dock USB port**; PIO **USB-A → this PC**.
3. **Grant access to the link** (it's a root-only hidraw by default on this box —
   that's why I couldn't drive it unattended):
   ```
   sudo cp linux/99-switch-pad.rules /etc/udev/rules.d/
   sudo udevadm control --reload-rules && sudo udevadm trigger
   ```
   then unplug/replug the PIO (USB-A) cable.
4. **Drive it:**
   ```
   python3 linux/drive_switch.py
   ```
   Arrow keys = D-pad, space = A, WASD/IJKL = sticks, etc. (`?` for the full map).

### PC-only test (no Switch), to confirm driving works
Plug the native Type-C into **this PC** instead of the dock, flash NORMAL, do
steps 3–4, and run `python3 linux/sniff_powa.py 10` in another terminal while you
press keys in `drive_switch.py`. You should see your presses show up on js0.

## Known gotchas (komabear's, confirmed relevant)
- **The PIO link is fragile.** It didn't even enumerate for me this session
  (only the native pad did). If `drive_switch.py` says "link not found", unplug
  the PIO (USB-A) cable, wait ~3 s, replug. Never leave PIO plugged into an idle
  board. Connect native first, then PIO.
- A more robust PC link would be a USB-UART adapter on the board's UART pins
  (komabear notes the frame parser is already link-agnostic) — a future option if
  the PIO link keeps dropping.

## Files I added / changed (in this komabear clone)
- `fw/pio_identity.c` — added one missing forward declaration
  (`extern const string_descriptor_t *pio_string_table(void);`). GCC 14 on Ubuntu
  26.04 rejects implicit declarations that older GCC only warned about; this is
  the only source change needed to build. Nothing functional changed.
- `linux/sniff_powa.py` — reads the native PowerA pad's js device (no root).
- `linux/drive_switch.py` — Linux replacement for komabear's Windows-only sender.
- `linux/99-switch-pad.rules` — udev rule to open the link hidraw as your user.
- `out/bridge-SELFTEST.uf2`, `out/bridge-NORMAL.uf2` — the two validated images.

## Build (if you change firmware)
Toolchain is installed under `~/toolchains`, SDK at `~/pico-sdk` (2.1.1 — builds
fine despite komabear suggesting 2.2.0). From the repo root:
```
export PICO_SDK_PATH=$HOME/pico-sdk
export PATH="$HOME/toolchains/cmake-3.31.6-linux-x86_64/bin:$HOME/toolchains/arm-gnu-toolchain-14.2.rel1-x86_64-arm-none-eabi/bin:$HOME/toolchains:$PATH"
cmake -S fw -B fw/build -G Ninja -DCMAKE_BUILD_TYPE=Release          # add -DSELFTEST=ON for the self-test build
cmake --build fw/build        # -> fw/build/bridge.uf2
```
