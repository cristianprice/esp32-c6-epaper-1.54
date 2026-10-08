// Waveshare ESP32-C6-ePaper-1.54: ESP32-C6, 16MiB flash, 1.54" 200x200 e-paper.

#define MICROPY_HW_BOARD_NAME               "Waveshare ESP32-C6-ePaper-1.54"
#define MICROPY_HW_MCU_NAME                 "ESP32-C6"

// The USB-C port is wired to the C6's USB-Serial-JTAG, there is no USB-UART bridge.
#define MICROPY_HW_ENABLE_UART_REPL         (0)

// Not needed by a battery clock; saves flash and RAM.
#define MICROPY_PY_BLUETOOTH                (0)
#define MICROPY_PY_ESPNOW                   (0)
