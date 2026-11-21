# rp2350-switch-pad — SwitchView / USB fork

A fork of [komabear/rp2350-switch-pad](https://github.com/komabear/rp2350-switch-pad)
that turns a Waveshare **RP2350-USB-A** board into a USB device the **Nintendo
Switch 2** accepts as a **controller, keyboard, and mouse at once**, driven from a
Linux PC. It's the board-side firmware for
[SwitchView](https://github.com/theXappy/SwitchView): capture the Switch's screen
on your PC and drive it back over a single USB link — including **scripted text
input**, which is the main reason this fork exists.

```
PC (SwitchView app)
   │  SV protocol over USB HID feature reports
   ▼
board PIO-USB (USB-A plug) ═══ RP2350-USB-A ═══ board native USB (Type-C)
                                                     │  composite HID:
                                                     │  PowerA controller + keyboard + mouse
                                                     ▼
                                         Switch 2 dock USB port
```

The Switch sees a normal licensed wired controller (PowerA "BDA NSW wired
controller", `20D6:A713`) presented over the board's **native hardware USB**, plus
a standard boot keyboard and a relative mouse on the same device. The PC talks to
the board over the **PIO-USB** side (the USB-A male plug), which shows up as a HID
device `2E8A:400B`.

## Why a fork

komabear's project streams a friend's gamepad to your Switch over Parsec, with a
**Windows-only** PC app and a **USB-C** board. This fork keeps komabear's proven
"RP2350 → Switch 2" native-USB controller path and changes everything around it:

- **Composite device:** adds a **keyboard** and a **mouse** interface next to the
  controller, so one board covers everything SwitchView needs.
- **Scripted keyboard input:** send arbitrary text to the Switch from the PC (type
  into search boxes, SmileBASIC, name fields, etc.).
- **SwitchView SV protocol** over the PC link instead of komabear's XInput framing,
  so the SwitchView app drives it directly — no middleware.
- **Linux tooling** (the original PC app was Windows only) and a build that runs on
  the **RP2350-USB-A** board + Ubuntu 26.04 / GCC 14.

## Hardware

- **Waveshare RP2350-USB-A** (native USB on the Type-C port, PIO-USB on the USB-A
  male plug; PIO pins GPIO12/13). komabear's USB-C board also works — only the
  connector shape differs.
- Two data-capable USB cables / ports (some charge-only cables fail silently).
- A Switch 2 (+ dock) and a Linux PC. For capture, an HDMI capture card — see
  [SwitchView](https://github.com/theXappy/SwitchView).

## Flash

Prebuilt images are in [`out/`](out/):

| File | What it is |
|---|---|
| `bridge-COMPOSITE-pc.uf2` | **The one SwitchView uses:** controller + keyboard + mouse, driven from the PC over the SV protocol. |
| `bridge-NORMAL.uf2` | PowerA controller only, driven from the PC. |
| `bridge-SELFTEST.uf2` | Controller that cycles the D-pad by itself — no PC needed, good for a quick dock test. |
| `bridge-KEYBOARD-selftest.uf2` | Keyboard only; auto-types "hello " to prove typing works. |
| `bridge-COMPOSITE-selftest.uf2` | Composite that self-tests the keyboard. |

Put the board in BOOTSEL (hold BOOT, tap RST) and copy a UF2 onto the `RP2350`
drive:

```sh
cp out/bridge-COMPOSITE-pc.uf2 /run/media/$USER/RP2350/
```

Cabling: native **Type-C → Switch 2 dock USB port**; PIO **USB-A plug → the PC**.
Connect native first, then PIO. (The PIO link is fragile — if the PC can't find the
board, unplug the USB-A plug, wait ~3 s, replug. Never leave it plugged into an
idle board.)

## Drive it from Linux

The PC link (`2E8A:400B`) is a root-only hidraw by default. Install the udev rule
once so your user can open it:

```sh
sudo cp linux/99-switch-pad.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules && sudo udevadm trigger
```

then replug the USB-A plug.

- **From SwitchView:** run the SwitchView app with `-H usb` (it finds the board
  automatically). It sends controller state, keyboard text, and mouse motion over
  the same link.
- **Standalone text/input** with [`linux/sv_send.py`](linux/sv_send.py):

  ```sh
  python3 linux/sv_send.py text "hello world"      # type a string
  echo "some text" | python3 linux/sv_send.py type # type stdin (one line = one Enter)
  python3 linux/sv_send.py press A B               # tap controller buttons
  ```

- [`linux/drive_switch.py`](linux/drive_switch.py) — interactive controller driver
  (keyboard → pad; `?` for the map).
- [`linux/sniff_powa.py`](linux/sniff_powa.py) — read the native PowerA pad's js
  device on this PC to verify input (no Switch needed).

## Build

Toolchain: arm-none-eabi GCC + the Pico SDK (2.1.1 builds fine). From the repo root:

```sh
export PICO_SDK_PATH=$HOME/pico-sdk
cmake -S fw -B fw/build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCOMPOSITE_MODE=ON
cmake --build fw/build        # -> fw/build/bridge.uf2
```

Build options (CMake `-D…=ON`):

- `COMPOSITE_MODE` — controller + keyboard + mouse (what SwitchView uses).
- `KEYBOARD_MODE` — native port is a plain USB keyboard only.
- `SELFTEST` — board drives itself for a no-PC test; combine with the mode above.

Leaving all off builds the PowerA-controller-only image.

## Credits & license

Forked from [komabear/rp2350-switch-pad](https://github.com/komabear/rp2350-switch-pad),
which figured out the RP2350 → Switch 2 native-USB controller path. MIT licensed —
see [LICENSE](LICENSE). Third-party components are listed in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). See [NOTES-shai.md](NOTES-shai.md)
for the hands-on build/test log on the RP2350-USB-A.
