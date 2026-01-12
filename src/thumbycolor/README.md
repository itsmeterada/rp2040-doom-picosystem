# ThumbyColor DOOM Port

This directory contains the ThumbyColor-specific implementation for DOOM.

## Hardware Specifications

- **Display**: GC9107 128x128 IPS LCD (RGB565)
- **MCU**: RP2350 (ARM Cortex-M33 or RISC-V)
- **RAM**: 520KB SRAM

## GPIO Pin Mapping

### Display (SPI0)
| Signal | GPIO |
|--------|------|
| MOSI   | 19   |
| SCK    | 18   |
| CS     | 17   |
| DC     | 16   |
| RST    | 4    |
| BL     | 7    |

### Buttons
| Button | GPIO |
|--------|------|
| Up     | 1    |
| Down   | 3    |
| Left   | 0    |
| Right  | 2    |
| A      | 21   |
| B      | 25   |
| L      | 6    |
| R      | 22   |
| Menu   | 26   |

## Building

ThumbyColor uses the RP2350 chip, which requires Pico SDK 2.0 or later.

### Prerequisites

1. Install Pico SDK 2.0+
2. Set environment variables:
   ```bash
   export PICO_SDK_PATH=/path/to/pico-sdk
   export PICO_PLATFORM=rp2350
   ```

### Build Commands

```bash
mkdir build_thumbycolor
cd build_thumbycolor
cmake -DPICO_PLATFORM=rp2350 -DTHUMBYCOLOR=ON ..
make doom_thumbycolor
```

## Resolution

- Internal: 128x80 pixels
- Display: 128x80 (centered on 128x128 display with 24px black bars top/bottom)
- Aspect ratio: 1.6:1 (matches original DOOM)

## Controls

- D-pad: Movement
- A: Fire
- B: Use/Open doors
- L: Previous weapon
- R: Next weapon
- Menu: Game menu (ESC)

## Notes

- The RP2350 has more RAM (520KB vs 264KB) than RP2040
- The shortptr system may need adjustment for RP2350's memory map
- Audio uses PWM on GPIO 23
