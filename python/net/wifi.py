# Wi-Fi station, used only for the daily NTP sync: connect, then fully off again.

import asyncio
import network
import time

import config

try:
    from typing import Tuple
except ImportError:
    pass


def _credentials() -> Tuple[str, str]:
    try:
        import secrets
    except ImportError:
        raise OSError("no secrets.py with WIFI_SSID / WIFI_PASSWORD on the board")
    return secrets.WIFI_SSID, secrets.WIFI_PASSWORD


async def connect() -> network.WLAN:
    ssid, password = _credentials()
    wlan = network.WLAN(network.STA_IF)
    wlan.active(True)
    wlan.connect(ssid, password)
    start = time.ticks_ms()
    while not wlan.isconnected():
        if time.ticks_diff(time.ticks_ms(), start) > config.WIFI_CONNECT_TIMEOUT_MS:
            raise OSError("Wi-Fi connect timeout")
        await asyncio.sleep_ms(100)
    return wlan


def off() -> None:
    wlan = network.WLAN(network.STA_IF)
    try:
        wlan.disconnect()
    except OSError:
        pass
    wlan.active(False)
