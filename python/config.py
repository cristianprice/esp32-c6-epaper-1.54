# Clock configuration. Wi-Fi credentials live in secrets.py on the board's filesystem.

try:
    from typing import Optional, Tuple
except ImportError:
    pass

# --- Wake cycle -----------------------------------------------------------
WAKE_PERIOD_S: int = 120             # wake on every even minute, update, deep sleep again
WAKE_MARGIN_MS: int = 1500           # wake this long after the boundary (sleep timer is RC based)
EARLY_WAKE_WINDOW_S: int = 15        # woke this close before a boundary -> wait for it
FULL_REFRESH_EVERY: int = 30         # full e-paper refresh every N wakes (30 x 2 min = hourly)

# --- Time zone ------------------------------------------------------------
# The PCF85063 RTC keeps UTC; the display shows local time.
TZ_OFFSET_MIN: int = 120             # standard-time offset from UTC in minutes: EET (Romania)
DST_RULE: Optional[str] = "EU"       # None, "EU", "US" or "IL"; EU = EEST (UTC+3) in summer

# --- NTP: once a day at noon (local), Wi-Fi is off at all other times -----
NTP_SERVERS: Tuple[str, ...] = ("pool.ntp.org", "time.google.com", "time.cloudflare.com")
NTP_HOUR: int = 12                   # first wake at or after 12:00 syncs
NTP_WINDOW_END_HOUR: int = 13        # give up for the day at 13:00 if all attempts failed
NTP_RETRY_MIN: int = 10              # minimum minutes between attempts
NTP_TIMEOUT_MS: int = 3000           # per NTP server
WIFI_CONNECT_TIMEOUT_MS: int = 15000

# --- Sensors --------------------------------------------------------------
# The Waveshare BSP subtracts 4 °C for self-heating of an always-on board; with the
# board in deep sleep most of the time the offset is much smaller. Calibrate here.
TEMP_OFFSET_C: float = 0.0

# --- Power ----------------------------------------------------------------
CPU_FREQ_HZ: int = 80_000_000
EPD_POWER_OFF_IN_SLEEP: bool = True  # cut the panel's 3V3 rail during deep sleep

# --- Development ----------------------------------------------------------
# Stay awake with the REPL available instead of deep sleeping when the file below
# exists, or when the board was woken by pressing the PWR button.
NOSLEEP_FILE: str = "/nosleep"
DEV_WAKE_ON_PWR_BUTTON: bool = True
WATCHDOG_MS: int = 60000             # never stay stuck awake on battery (normal cycles only)
