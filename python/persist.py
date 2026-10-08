# State that survives deep sleep, kept in RTC memory (no flash writes).
# Lost on a full power loss; the app then does a cold start.

import machine
import struct

try:
    from typing import Optional
    from apptypes import FaceValues
except ImportError:
    pass

_MAGIC = 0xC10C
_VERSION = 1
# magic, version, last_ntp_day, last_ntp_try_min, wakes, since_full,
# shown frame: year, month, day, weekday, hour, minute, temp_x10, rh, battery
_FMT = "<HBIIIHHBBBBBhBB"

NO_TEMP = -32768
NO_VALUE = 255


class State:
    def __init__(self) -> None:
        self.cold: bool = True                  # no valid state found (first boot / power loss)
        self.last_ntp_day: int = 0              # local yyyymmdd of the last successful sync
        self.last_ntp_try_min: int = 0          # UTC minutes of the last attempt
        self.wakes: int = 0
        self.since_full: int = 0                # partial refreshes since the last full refresh
        self.shown: Optional[FaceValues] = None  # face values currently on the panel


def load() -> State:
    st = State()
    try:
        raw = machine.RTC().memory()
        if len(raw) < struct.calcsize(_FMT):
            return st
        f = struct.unpack(_FMT, raw[: struct.calcsize(_FMT)])
    except Exception:
        return st
    if f[0] != _MAGIC or f[1] != _VERSION:
        return st
    st.cold = False
    st.last_ntp_day, st.last_ntp_try_min, st.wakes, st.since_full = f[2:6]
    if f[6]:
        st.shown = f[6:15]
    return st


def save(st: State) -> None:
    shown = st.shown or (0, 0, 0, 0, 0, 0, 0, 0, 0)
    machine.RTC().memory(
        struct.pack(
            _FMT,
            _MAGIC,
            _VERSION,
            st.last_ntp_day,
            st.last_ntp_try_min,
            st.wakes & 0xFFFFFFFF,
            min(st.since_full, 0xFFFF),
            *shown
        )
    )
