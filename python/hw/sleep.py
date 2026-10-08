# Deep sleep scheduling: wake right after the next WAKE_PERIOD_S boundary.

import esp32
import machine
import os

import bsp
import config

try:
    from typing import NoReturn
except ImportError:
    pass


def ms_until_next_wake(utc_s: int, ms_since_utc_read: int) -> int:
    """Milliseconds from now until just after the next period boundary."""
    period = config.WAKE_PERIOD_S
    next_boundary = (utc_s // period + 1) * period
    ms = (next_boundary - utc_s) * 1000 - ms_since_utc_read + config.WAKE_MARGIN_MS
    return max(ms, 1000)


def dev_mode() -> bool:
    """Stay awake (REPL available) instead of sleeping?"""
    try:
        os.stat(config.NOSLEEP_FILE)
        return True
    except OSError:
        pass
    return config.DEV_WAKE_ON_PWR_BUTTON and machine.wake_reason() == machine.EXT1_WAKE


def deep_sleep(ms: int) -> NoReturn:
    if config.DEV_WAKE_ON_PWR_BUTTON:
        pwr = machine.Pin(bsp.PIN_PWR_BUTTON, machine.Pin.IN, machine.Pin.PULL_UP)
        esp32.wake_on_ext1((pwr,), esp32.WAKEUP_ALL_LOW)
    machine.deepsleep(ms)
    raise RuntimeError("deepsleep returned")  # never reached: the chip resets on wake
