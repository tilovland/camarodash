# Camaro VEMS display (ESP-IDF)

Native ESP-IDF firmware for a Waveshare ESP32-S3-Touch-LCD-7 (800 x 480).
It reads 5-byte VEMS AIM packets at 19200 baud and displays RPM, MAP, coolant
temperature and lambda using LVGL 9.

The dashboard includes:

- GT911 capacitive touch support
- 12-stage green/yellow/red shiftlight with redline flashing
- Live minimum and maximum values for RPM, MAP, coolant and lambda
- A touch button for resetting all minimum/maximum values
- Configurable RPM scale, shiftlight thresholds and coolant warning

## Hardware

The LCD pinout and timing follow Waveshare's official ESP-IDF example. The
original Arduino source assigned several LCD signals to the same GPIO and also
put the VEMS UART on GPIOs used by the LCD. This project uses the board's
dedicated external UART instead:

| Signal | ESP32-S3 pin |
| --- | ---: |
| VEMS / MAX3232 TXD -> display RX | GPIO 44 |
| VEMS / MAX3232 RXD <- display TX | GPIO 43 |
| UART format | 19200, 8N1 |

The TX connection is not required if the display only receives AIM telemetry.
Remember to cross TX/RX and connect ground. GPIOs use 3.3 V logic; use a
MAX3232 only on the RS-232 side, never feed RS-232 voltage directly into the
ESP32-S3.

## Build and flash

Use ESP-IDF 5.2 through 5.5 (Waveshare currently documents 5.5.2). Open an
ESP-IDF terminal in this folder:

```powershell
idf.py set-target esp32s3
idf.py build
idf.py -p COMx flash monitor
```

The first build downloads LVGL, the GT911 driver and Espressif's LVGL adapter
through the ESP-IDF Component Manager. To change UART pins, baud rate, RPM
scale, shiftlight thresholds or coolant warning, use `idf.py menuconfig` and
open **Camaro display configuration**.

## AIM packet format

The parser keeps the packet format from the original sketch:

```text
[channel][0xA3][value high][value low][8-bit checksum]
```

The checksum is the low byte of the sum of the first four bytes. Supported
channels are RPM (`0x01`), MAP (`0x02`), coolant temperature (`0x04`) and
lambda (`0x06`).
