# UTC -> local time: fixed offset plus optional DST rule (config.TZ_OFFSET_MIN / DST_RULE).
# Times are seconds since the MicroPython epoch; time.mktime/gmtime work in UTC.

import time

import config

try:
    from typing import Optional, Tuple
except ImportError:
    pass

HOUR = 3600


def _midnight(year: int, month: int, day: int) -> int:
    return time.mktime((year, month, day, 0, 0, 0, 0, 0))


def _weekday(year: int, month: int, day: int) -> int:
    return time.gmtime(_midnight(year, month, day))[6]  # 0 = Monday


def _last_sunday(year: int, month: int) -> int:
    if month == 12:
        last = time.gmtime(_midnight(year + 1, 1, 1) - 86400)[2]
    else:
        last = time.gmtime(_midnight(year, month + 1, 1) - 86400)[2]
    return last - (_weekday(year, month, last) + 1) % 7


def _nth_sunday(year: int, month: int, n: int) -> int:
    first_sunday = 1 + (6 - _weekday(year, month, 1)) % 7
    return first_sunday + 7 * (n - 1)


def _dst_bounds_utc(year: int, base: int) -> Optional[Tuple[int, int]]:
    """(start, end) of DST in UTC seconds for config.DST_RULE, base = standard offset."""
    rule = config.DST_RULE
    if rule == "EU":  # last Sunday of March/October, 01:00 UTC
        return (
            _midnight(year, 3, _last_sunday(year, 3)) + 1 * HOUR,
            _midnight(year, 10, _last_sunday(year, 10)) + 1 * HOUR,
        )
    if rule == "US":  # 2nd Sunday of March 02:00 local .. 1st Sunday of November 02:00 local (DST)
        return (
            _midnight(year, 3, _nth_sunday(year, 3, 2)) + 2 * HOUR - base,
            _midnight(year, 11, _nth_sunday(year, 11, 1)) + 2 * HOUR - base - HOUR,
        )
    if rule == "IL":  # Friday before the last Sunday of March 02:00 .. last Sunday of October 02:00 (DST)
        return (
            _midnight(year, 3, _last_sunday(year, 3) - 2) + 2 * HOUR - base,
            _midnight(year, 10, _last_sunday(year, 10)) + 2 * HOUR - base - HOUR,
        )
    return None


def offset_s(utc: int) -> int:
    base = config.TZ_OFFSET_MIN * 60
    bounds = _dst_bounds_utc(time.gmtime(utc)[0], base)
    if bounds and bounds[0] <= utc < bounds[1]:
        return base + HOUR
    return base


def local(utc: int) -> int:
    """Local time as seconds (use time.gmtime() on the result to split it)."""
    return utc + offset_s(utc)
