# Time source of truth: the battery-backed PCF85063 (UTC). NTP corrects it once a day.

import asyncio
import machine
import time

import bsp
import config
from timekeeping import tz

try:
    from typing import Optional, Tuple
    from persist import State
except ImportError:
    pass

MIN_VALID_YEAR = 2025


def read_rtc() -> Tuple[int, bool]:
    """(utc_seconds, valid) from the PCF85063; also sets machine.RTC to match."""
    y, mo, d, wd, h, mi, s = bsp.rtc.datetime()
    valid = bsp.rtc.valid() and y >= MIN_VALID_YEAR
    machine.RTC().datetime((y, mo, d, wd, h, mi, s, 0))
    return time.mktime((y, mo, d, h, mi, s, 0, 0)), valid


def set_time(utc: int) -> None:
    """Set both the PCF85063 and machine.RTC (UTC)."""
    y, mo, d, h, mi, s, wd, _ = time.gmtime(utc)
    bsp.rtc.datetime((y, mo, d, wd, h, mi, s))
    machine.RTC().datetime((y, mo, d, wd, h, mi, s, 0))


async def aligned_now() -> Tuple[int, bool]:
    """Current (utc, valid). If we woke just before a period boundary (the deep-sleep
    timer runs on the less accurate RC clock), wait for the boundary first."""
    utc, valid = read_rtc()
    if valid:
        until_boundary = config.WAKE_PERIOD_S - utc % config.WAKE_PERIOD_S
        if until_boundary <= config.EARLY_WAKE_WINDOW_S:
            await asyncio.sleep(until_boundary)
            utc, valid = read_rtc()
    return utc, valid


def local_day(utc: int) -> int:
    y, mo, d = time.gmtime(tz.local(utc))[:3]
    return y * 10000 + mo * 100 + d


def ntp_due(st: State, utc: int, valid: bool) -> bool:
    """Daily sync policy: first wake at/after NTP_HOUR local, retries until
    NTP_WINDOW_END_HOUR. Without a valid time, retry every NTP_RETRY_MIN."""
    now_min = utc // 60
    if 0 <= now_min - st.last_ntp_try_min < config.NTP_RETRY_MIN:
        return False
    if not valid:
        return True
    if st.last_ntp_day == local_day(utc):
        return False
    hour = time.gmtime(tz.local(utc))[3]
    return config.NTP_HOUR <= hour < config.NTP_WINDOW_END_HOUR


async def ntp_sync(st: State, utc: int) -> Optional[int]:
    """Turn Wi-Fi on, sync, turn Wi-Fi off. Returns the new UTC time or None."""
    from net import ntp, wifi  # only imported on sync wakes: Wi-Fi stays untouched otherwise

    st.last_ntp_try_min = utc // 60
    try:
        await wifi.connect()
        new_utc = await ntp.get_time()
    except Exception as e:
        print("NTP sync failed:", repr(e))
        return None
    finally:
        wifi.off()
    set_time(new_utc)
    st.last_ntp_day = local_day(new_utc)
    st.last_ntp_try_min = new_utc // 60
    print("NTP sync ok, drift %+d s" % (new_utc - utc))
    return new_utc
