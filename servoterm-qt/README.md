# servoterm-qt

Qt-based console for stmbl servo drives, modelled on
[QtServoterm](https://github.com/STMBL/QtServoterm.git).

## Features

| Feature | Status |
|---|---|
| 8-channel scrolling oscilloscope | ✓ |
| XY oscilloscope with persistence fade | ✓ |
| Command history (↑/↓, max 100) | ✓ |
| Config edit/load/save dialog (CRC-32/MPEG-2) | ✓ |
| Serial port connection (115200 baud) | ✓ |
| TCP connection | ✓ |
| Simulator mode (pipe to `stmbl_host --serve`) | ✓ |
| Enable/Disable drive | ✓ |
| Jog (←/→ arrows, 250 ms repeat) | ✓ |
| E-stop (Escape key) | ✓ |
| CSV scope data recording | ✓ |
| Drag-and-drop HAL script files | ✓ |
| Settings persistence (geometry, last port) | ✓ |
| Text console with HTML-safe rendering | ✓ |

## Wire protocol

Same as the USB device:

- `0xFF <b0..b7>` — 8-channel scope sample; channel value = `(byte − 128) / 128.0`
- `0xFE` — scope sweep reset
- all other bytes — text console output

## Build

Requires Qt 5.15+ (or Qt 6) with `QtSerialPort` and `QtNetwork`.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
./build/servoterm
```

On Ubuntu: `apt install qtbase5-dev libqt5serialport5-dev`

## Keyboard shortcuts

| Key | Action |
|---|---|
| Escape | E-stop (disable drive) |
| ↑ / ↓ | Command history (when input is focused) |
| ← / → | Jog CCW / CW (when Jog mode is enabled) |
| Enter | Send command |

## Connection modes

**Serial** — direct USB/serial connection to hardware (VID 0x0483 / PID 0x5740, 115200 baud).

**TCP** — connect to `host:port` (e.g. `localhost:5000`).

**Simulator** — launches `stmbl_host --serve [script]` as a subprocess and connects via stdin/stdout pipe. Useful for offline testing with the host simulator.
