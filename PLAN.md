# Development Plan — MicroPython firmware for Waveshare ESP32-C6-ePaper-1.54

Goal: a custom MicroPython firmware for the board that exposes the Waveshare ESP-IDF BSP
(`third_party/waveshare/02_Example/espidf_v5.5.3/11_FactoryProgram/components/port_bsp`)
as MicroPython modules, plus a low-power **asyncio**-based clock application showing time
(NTP-synced), temperature and humidity (on-board SHTC3) on the e-paper.

Requirements:
- **Clock face shows: date, time, temperature, humidity, battery status.**
- **Cycle**: deep sleep 2 minutes → wake → refresh values (time, T/RH, battery) → show them →
  deep sleep again.
- **NTP sync only once a day, at noon** (local time). The PCF85063 RTC keeps time in between.
- **Wi-Fi is enabled only for the NTP sync. Everything else stays down** (Bluetooth, audio, SD,
  LED, and Wi-Fi outside the noon sync) — see §2 "Power-down policy".
- **asyncio everywhere** on the Python side; no blocking waits. Slow C operations are split into
  *start* + *poll* so Python can `await` them.
- **Python code split by functionality**, one module per concern (see §5).
- No air pressure (no barometer on board).
- **All runtime state is persisted in RTC memory** (survives deep sleep, no flash wear).

---

## Status (2026-10-08)

Phases 0–6 and 8 are implemented in code and the firmware builds (Windows, ESP-IDF v5.5.5,
MicroPython v1.29.0). Host tests pass. **Not yet run on hardware** (Phase 7 measurements pending).

Changes against the original plan, decided during implementation:
- **TCA9554 and PCF85063 drivers are built into the `bsp` module** instead of the managed
  components (MicroPython's esp32 build has no hook for extra IDF components without patching it).
- **MicroPython's `machine.I2C` already uses the IDF "NG" I²C driver**, so there's no driver conflict.
  The `bsp` module still owns the board bus.
- **Native Windows builds need `patches/micropython-windows-build.patch`.** Upstream supports only
  Linux/WSL, because of 89K-char qstr command lines and `cat`/`sed`. `scripts/build.py` applies the
  patch on Windows only.
- **`mpy-cross` comes from PyPI** (`mpy-cross==1.29.0.post2`), so no host C compiler or make is needed.
- **Bitmap fonts come from `tools/make_fonts.py`** (Pillow + DejaVu Sans) instead of font_to_py.
  Icons come from `tools/icon_art.py` via `tools/make_icons.py`. Both are committed as generated code.
- **The PCF85063 RAM-byte marker is dropped.** The RTC has its own oscillator-stop flag, and the
  board battery-backs it (schematic: diodes from VBAT).
- **Dev access:** pressing PWR (GPIO2, ext1 wake) wakes into dev mode (REPL, no sleep); so does a
  `/nosleep` file. BOOT isn't used, because it's a strapping pin.
- **The panel rail is switched off in deep sleep** (`EPD_POWER_OFF_IN_SLEEP`). Partial refresh
  still works because both panel RAMs are rewritten on every wake.
- **Time zone:** `config.TZ_OFFSET_MIN` + `DST_RULE` (`EU`/`US`/`IL`). The default is UTC.

---

## 1. Hardware summary (from BSP `epaper_config.h` + Waveshare docs)

| Function | Part | Bus / Pin |
|---|---|---|
| MCU | ESP32-C6 (RISC-V 160 MHz, 512 KB SRAM, Wi-Fi 6/BLE 5), 16 MB flash | — |
| E-paper 1.54" V2, 200×200 B/W | SSD1681-class controller | SPI2: SCK 6, MOSI 5, MISO 4, CS 7, DC 15, RST 11, BUSY 10 |
| TF card | SDSPI | shares SPI2, CS 3 (not used in v1) |
| I²C bus | — | SDA 18, SCL 8, I2C_NUM_0 |
| RTC | PCF85063A (crystal) | I²C 0x51 |
| Temp / humidity | SHTC3 | I²C 0x70 |
| IO expander | TCA9554 | I²C 0x20 — EXIO0 EPD power, EXIO1 audio power, EXIO3 speaker amp, EXIO4 LED, EXIO5 VBAT latch |
| Battery voltage | ADC1 CH0 (GPIO0), divider ×2 | — |
| Buttons | BOOT GPIO9, PWR GPIO2 (active low) | GPIO2 is an LP GPIO → can wake from deep sleep; GPIO9 cannot |
| Audio | ES8311 codec + amp + mic | out of scope |

---

## 2. Operating model — wake / work / deep sleep

Each deep-sleep wake is a **fresh boot** of MicroPython: RAM is lost, `main.py` runs again.
So the app is a **single short "cycle"** per wake, not a long-running loop.

```
 cold boot / wake (timer: every 2 min)
   │
   ├─ bsp.init()   (I²C, TCA9554 — keep VBAT latch & EPD rail ON, don't glitch outputs)
   ├─ read time from PCF85063  → set machine.RTC
   ├─ load persisted state from RTC memory  (last frame data, counters, last NTP day)
   │
   ├─ asyncio.gather(
   │     climate.measure(),            # SHTC3 wake → measure → sleep (~15 ms)
   │     battery.measure(),
   │     ntp_if_due(),                 # only once a day at noon (see §3); Wi-Fi off otherwise
   │  )
   ├─ render frame (date, time HH:MM, temperature, humidity, battery status)
   ├─ await display.update()           # partial refresh; full refresh every N cycles
   ├─ panel → deep sleep, SHTC3 → sleep, save state to RTC memory
   └─ machine.deepsleep(ms until next even-minute boundary)
```

Key points:
- **Timing source is the PCF85063** (crystal, accurate). The ESP32-C6's internal RC slow clock
  drifts several %, so it is only used as the sleep timer. Every wake re-reads the PCF85063 and
  computes the sleep duration to the **next 2-minute boundary** (…:00, :02, :04…), plus a small
  margin so we wake just after the boundary. Drift never accumulates.
- **Wake work target: < 1.5 s awake** (excluding NTP days). Frozen bytecode for fast startup.
- **All persisted data lives in RTC memory** (ESP32-C6 RTC/LP SRAM via `machine.RTC().memory()`,
  ≤ 2 KB, survives deep sleep; **no flash writes** during normal operation): last displayed values
  (to re-render the previous frame for partial refresh), partial-refresh counter, last NTP sync
  day, NTP failure count, boot/wake counters. Small, struct-packed.
- **E-paper partial refresh after deep sleep**: the controller needs the *old* image in RAM 0x26.
  We re-render the previous frame from the persisted values (deterministic rendering) and write
  it to 0x26, new frame to 0x24, then partial refresh. No 5 KB buffer needs to survive sleep,
  and no flash writes every 2 minutes.
- **Full refresh** every N wakes (default 30 = once per hour) and at cold boot, to clear ghosting.
- **Power rails during sleep**: TCA9554 stays powered and holds its outputs, so the VBAT latch
  (EXIO5) keeps the board on when running from battery. Init code must read-modify-write the
  expander and never drive EXIO5 low on wake. EPD panel goes to its own deep-sleep command;
  EPD rail can optionally be switched off (measure which saves more vs. re-init cost).
- **Wake source**: timer only (2 min). (PWR button keeps its hardware power-on/off role via the
  VBAT latch; long press = power off is handled if pressed while awake.)
- **Power-down policy** — only what the cycle needs is ever turned on:
  - Wi-Fi: not initialized on normal wakes; on the noon NTP wake it is started, used, then
    `active(False)` before sleep. Bluetooth: never initialized.
  - Audio rail (EXIO1) and speaker amp (EXIO3): OFF. LED (EXIO4): OFF. SD card: not mounted.
  - SHTC3: put back to sleep right after the measurement. EPD panel: deep-sleep command after refresh.
  - Battery divider: read once per wake, then idle.
  - CPU kept at a low clock (e.g. 80 MHz) during the cycle; raised only for Wi-Fi if needed.
- **Development escape hatch**: deep sleep kills the USB REPL. If BOOT (GPIO9) is held during
  wake, or a `/nosleep` file exists on the filesystem, the app skips deep sleep and stays awake
  with the REPL available (`config.DEV_MODE` too).

---

## 3. NTP policy — once a day at noon

- The **first wake at or after 12:00** local time with `last_ntp_day != today` connects Wi-Fi,
  syncs, writes the PCF85063, records `last_ntp_day`, and turns Wi-Fi off again.
- On failure: retry on later wakes at most every 10 min until 13:00, then give up until
  tomorrow's noon. Wi-Fi attempt is time-boxed (e.g. 15 s connect,
  3 s NTP) so a bad network never drains the battery.
- Only exception: at **first power-up** if the PCF85063 has no valid time (oscillator-stop flag /
  year < 2025) — otherwise the clock could not show anything meaningful until noon.
- Wi-Fi is fully off on all other wakes (never even initialized).

---

## 4. Architecture

```
┌──────────────────────── Python (frozen, asyncio) ─────────────────────────┐
│ main.py → app.py (one wake cycle)                                          │
│  ui/      net/        timekeeping/     sensors/        hw/   persist.py    │
└───────┬───────────────────────────────────────────────────────────────────┘
        │  MicroPython C user module  `bsp`  (USER_C_MODULES)
┌───────▼───────────────────────────────────────────────────────────────────┐
│ bsp.power  bsp.epd  bsp.shtc3  bsp.rtc  bsp.battery  bsp.i2c               │
├───────────────────────────────────────────────────────────────────────────┤
│ Ported Waveshare port_bsp (C/C++) + managed components:                    │
│ esp_io_expander_tca9554, waveshare/pcf85063a                               │
├───────────────────────────────────────────────────────────────────────────┤
│ MicroPython ports/esp32 + ESP-IDF (version pinned to what MicroPython supports) │
└───────────────────────────────────────────────────────────────────────────┘
```

Design decisions:
- **Custom board definition** `boards/WAVESHARE_C6_EPAPER_154/` (out-of-tree `BOARD_DIR`) with its
  own `sdkconfig.board`, `mpconfigboard.h`, 16 MB partition table, and `manifest.py` freezing all
  Python code.
- **No LVGL.** E-paper is 1-bit at 200×200, so the EPD frame buffer (5000 bytes) is exposed as a
  `bytearray` that Python wraps in `framebuf.FrameBuffer(..., MONO_HLSB)`.
- **The C module owns the I²C bus** (IDF `i2c_master` "NG" driver, as the BSP uses). `bsp.i2c`
  exposes generic `readfrom_mem/writeto_mem/scan`. Reason: mixing the legacy and NG I²C drivers
  aborts at boot on IDF 5.x (to be verified in Phase 0).
- **Do not use `esp_io_expander_gpio_wrapper`** (it hooks `gpio_set_level` via linker wrap and can
  conflict with `machine.Pin`). Call `esp_io_expander_set_level()` directly.
- **Non-blocking C API for slow operations**: `epd.refresh_start()` + `epd.busy()`;
  `shtc3.measure_start()` + `shtc3.read()`. Python awaits with `asyncio.sleep_ms()` polling.
  The BSP's blocking `read_busy()` loops are replaced.

---

## 5. C module `bsp` — proposed API

```python
import bsp

bsp.init()                     # I²C bus + TCA9554 (read-modify-write, no output glitches)

# bsp.power
bsp.power.epd(on); bsp.power.audio(on); bsp.power.vbat_latch(on); bsp.power.led(on)

# bsp.epd   (200×200, 1-bit)
bsp.epd.init(full: bool)               # HW reset + init sequence + LUT (full or partial)
bsp.epd.buffer() -> bytearray(5000)    # share with framebuf
bsp.epd.write_old(buf)                 # RAM 0x26 (previous image, for partial after deep sleep)
bsp.epd.write_new(buf)                 # RAM 0x24
bsp.epd.refresh_start(full: bool)      # trigger update, returns at once
bsp.epd.busy() -> bool
bsp.epd.sleep()                        # panel deep sleep (cmd 0x10)

# bsp.shtc3
bsp.shtc3.wakeup(); bsp.shtc3.measure_start(); bsp.shtc3.read() -> (temp_c, rh_pct); bsp.shtc3.sleep()

# bsp.rtc  (PCF85063A)
bsp.rtc.datetime() -> (Y, M, D, wd, h, m, s); bsp.rtc.datetime(tuple); bsp.rtc.valid() -> bool

# bsp.battery
bsp.battery.voltage() -> float; bsp.battery.percent() -> int

# bsp.i2c   (shared bus)
bsp.i2c.scan(); bsp.i2c.readfrom_mem(addr, reg, n); bsp.i2c.writeto_mem(addr, reg, buf)
```

Deep sleep, RTC memory and wake sources use stock MicroPython (`machine.deepsleep`,
`machine.RTC().memory()`, `esp32.wake_on_ext1`, `machine.wake_reason()`).

---

## 6. Python application layout (frozen into firmware, one module per concern)

```
python/
├── main.py                  # asyncio.run(app.run_cycle()); handles dev-mode escape
├── app.py                   # one wake cycle: gather work, render, display, schedule sleep
├── config.py                # timezone, NTP servers, sync window, full-refresh interval, wake period
├── secrets.py               # Wi-Fi SSID/password (on filesystem, git-ignored, not frozen)
├── persist.py               # struct-packed state in RTC memory (survives deep sleep)
├── hw/
│   ├── board.py             # bsp.init(), power rails, LED
│   ├── battery.py           # async battery read
│   └── sleep.py             # compute next 2-min boundary, configure wake sources, deep sleep
├── net/
│   ├── wifi.py              # async connect with timeout, then full shutdown
│   └── ntp.py               # async SNTP client (non-blocking UDP socket, timeout, multi-server)
├── timekeeping/
│   ├── clock.py             # PCF85063 ⇄ machine.RTC; NTP-due policy (§3)
│   └── tz.py                # UTC offset + DST rules
├── sensors/
│   └── climate.py           # async SHTC3 reading
└── ui/
    ├── display.py           # async EPD wrapper: old/new frame, partial vs. full policy
    ├── fonts/               # large digit font(s) generated with font_to_py
    ├── writer.py            # text rendering with custom fonts
    └── clock_face.py        # deterministic render(state) → framebuf: date, HH:MM, temperature, humidity, battery
```

---

## 7. Build environment (Windows and Linux hosts)

- **Uses the locally installed ESP-IDF, provided by the user** — the build does not download or
  bundle its own IDF. The IDF location is passed explicitly:
  1. `--idf-path <dir>` argument to the build script, or
  2. `IDF_PATH` environment variable (e.g. from an already-activated IDF shell / `export.sh` / `export.ps1`).
  The script sources the IDF environment (`export.sh` on Linux, `export.ps1`/`export.bat` on Windows)
  if it isn't active yet, and fails with a clear message if neither is given.
- **Required IDF version: v5.5.x** (matches the Waveshare BSP, which targets 5.5; MicroPython's esp32
  port supports IDF 5.x, not 6.x). The script checks `version.txt`/`git describe` and refuses
  an unsupported version. Exact supported range confirmed against the pinned MicroPython release in Phase 0.
  - This machine (Windows): `C:\esp\Espressif\frameworks\esp-idf-v5.5.5` ✅ (v6.1 at `C:\esp\v6.1\esp-idf` is **not** used).
  - Linux: any v5.5.x install, typically `~/esp/esp-idf`.
- **Cross-platform scripts in Python** (`scripts/build.py`, `flash.py`, `deploy.py`) so one
  implementation runs on both OSes; thin wrappers `build.sh` / `build.ps1` just call them.
- **Flashing / REPL** with `esptool` (from the IDF env) and `mpremote`. Serial port auto-detected or
  `--port` (`COMx` on Windows, `/dev/ttyACM*` on Linux).
- `requirements-dev.txt` for extra host tools (mpremote, font_to_py deps).
- Line endings: `.gitattributes` forces LF for `*.sh`, `*.py`, `*.cmake`, `Makefile`, `*.c/h`.
- **No Docker.** Builds always run against the local IDF on the host (Windows or Linux).
- Repo layout:
```
/                      PLAN.md, README.md, .gitignore
firmware/
  boards/WAVESHARE_C6_EPAPER_154/   board definition + manifest.py
  modules/bsp/                      C user module (micropython.cmake, mod*.c, ported port_bsp)
python/                             app (§6)
scripts/                            build.py / flash.py / deploy.py (+ .sh/.ps1 wrappers)
third_party/micropython (git submodule, pinned tag), third_party/waveshare (reference only)
```
- Development loop: dev mode (no deep sleep) + `mpremote mount python/` for fast iteration;
  freeze into firmware once stable.

---

## 8. Phases & milestones

**Phase 0 — Feasibility spike**
- Pick a MicroPython release with ESP32-C6 support; pin the matching ESP-IDF version.
- Build stock `ESP32_GENERIC_C6` with the local ESP-IDF v5.5.5 via `scripts/build.py --idf-path …`
  (verify on Windows; Linux with a local v5.5.x), flash, get REPL over USB-Serial-JTAG.
- Check: `machine.I2C` legacy vs NG driver; `machine.RTC().memory()` size on C6;
  deep sleep + timer wake + ext wake on GPIO2 work.
- ✅ Exit: REPL works; I²C scan lists 0x20/0x51/0x70; a deep-sleep/wake loop runs.

**Phase 1 — Board definition + `bsp` skeleton**
- Custom board dir, 16 MB partitions, user C module building with managed components.
- `bsp.init()`, `bsp.power.*`, `bsp.i2c.*`. ✅ Exit: LED blinks; VBAT latch holds through deep sleep on battery.

**Phase 2 — E-paper**
- Port `port_display.cpp` → `bsp.epd` with non-blocking refresh and old/new RAM writes.
- `ui/display.py` async wrapper. ✅ Exit: partial refresh works correctly **across deep sleep**.

**Phase 3 — Sensors, RTC, persistence**
- `bsp.shtc3`, `bsp.rtc`, `bsp.battery`; `sensors/climate.py`, `hw/battery.py`,
  `timekeeping/clock.py`, `persist.py`.
- ✅ Exit: T/RH and battery read; RTC time and persisted state survive deep sleep.

**Phase 4 — Wake cycle**
- `app.py`, `hw/sleep.py`, power-down policy, dev-mode escape.
- ✅ Exit: board wakes on every even minute, updates screen, sleeps; awake time measured.

**Phase 5 — Networking & NTP**
- `net/wifi.py`, `net/ntp.py`, `timekeeping/tz.py`, noon sync policy.
- ✅ Exit: daily noon sync (testable by config override of the window), failure retry/giving up, DST correct.

**Phase 6 — Clock face**
- Fonts, `clock_face.py` layout: date, time, temperature, humidity, battery status.
- ✅ Exit: final UI; hourly full refresh keeps ghosting under control.

**Phase 7 — Power tuning & robustness**
- Measure sleep/awake current, decide EPD rail off vs. panel sleep, trim boot time,
  watchdog, exception handler that always reaches deep sleep (never stuck awake).

**Phase 8 — Docs**: README (build, flash, configure Wi-Fi/timezone, dev mode, API reference).

---

## 9. Risks

1. ESP-IDF version: using local v5.5.5; if the chosen MicroPython release doesn't support 5.5.5 yet,
   pick a newer MicroPython (or master) rather than installing another IDF. The BSP uses
   only standard APIs (`i2c_master`, `esp_lcd` SPI IO, `adc_oneshot`); verify in Phase 0.
2. VBAT latch / TCA9554 behaviour across deep sleep and reset — if outputs reset, the board
   powers off on battery. Verify early (Phase 1).
3. E-paper partial refresh after a controller reset — handled by re-rendering the previous frame
   into RAM 0x26; if unreliable, fall back to keeping the panel powered in deep-sleep mode 1
   (RAM retained) and skipping HW reset.
4. Displayed time lags real time by up to 2 minutes by design (HH:MM shown at each even minute).
5. ESP32-C6 RC slow clock drift — mitigated by re-aligning to the PCF85063 on every wake.
6. RTC memory is lost on full power loss (battery removed / USB unplugged without battery).
   Then the app does a cold start: reads time from the PCF85063 if still valid, else NTP sync,
   and a full refresh. (The PCF85063 itself has only 1 user RAM byte, so it can't hold the state;
   it's used as a "state version" marker to detect RTC-memory/PCF mismatch.)
7. Flash size: example sdkconfig says 4 MB, board has 16 MB → use 16 MB layout.
