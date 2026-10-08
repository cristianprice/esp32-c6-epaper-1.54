# One wake cycle: read time/sensors (+ daily NTP), redraw the face, deep sleep.

import asyncio
import machine
import sys
import time

import config
import persist
from hw import battery, board, sleep
from persist import NO_TEMP, NO_VALUE
from sensors import climate
from timekeeping import clock, tz
from ui import clock_face, display

try:
    from typing import Optional
    from apptypes import ClimateReading, FaceValues
    from persist import State
except ImportError:
    pass


async def _maybe_ntp(st: State, utc: int, valid: bool) -> Optional[int]:
    if clock.ntp_due(st, utc, valid):
        return await clock.ntp_sync(st, utc)
    return None


def _face_values(
    utc: int, valid: bool, climate_reading: Optional[ClimateReading], battery_pct: Optional[int]
) -> FaceValues:
    if valid:
        year, month, day, hour, minute, _, weekday, _ = time.gmtime(tz.local(utc))
    else:
        year, month, day, hour, minute, weekday = 0, 0, 0, NO_VALUE, 0, 0
    if climate_reading:
        temp_x10 = round(climate_reading[0] * 10)
        rh = round(climate_reading[1])
    else:
        temp_x10, rh = NO_TEMP, NO_VALUE
    return (year, month, day, weekday, hour, minute, temp_x10, rh,
            NO_VALUE if battery_pct is None else battery_pct)


async def run_cycle(dev: bool) -> None:
    await board.init()
    st = persist.load()
    if machine.reset_cause() != machine.DEEPSLEEP_RESET:
        st.cold = True

    utc, valid = await clock.aligned_now()
    utc_read_ms = time.ticks_ms()

    # Sensors and the (rare) NTP sync run concurrently.
    climate_reading, battery_pct, synced_utc = await asyncio.gather(
        climate.read(), battery.read(), _maybe_ntp(st, utc, valid)
    )
    if synced_utc is not None:
        utc, valid = clock.read_rtc()
        utc_read_ms = time.ticks_ms()

    values = _face_values(utc, valid, climate_reading, battery_pct)
    # Partial refreshes write both panel RAMs, so they also work after the panel was
    # unpowered; a full refresh is only needed periodically (ghosting) and on cold start.
    full = st.cold or st.shown is None or st.since_full + 1 >= config.FULL_REFRESH_EVERY

    new = display.Canvas()
    clock_face.render(new.fb, values)
    old: Optional[display.Canvas] = None
    if not full and st.shown is not None:
        old = display.Canvas()
        clock_face.render(old.fb, st.shown)
    await display.update(new, old, full)

    st.shown = values
    st.since_full = 0 if full else st.since_full + 1
    st.wakes += 1
    persist.save(st)
    board.prepare_sleep()

    if dev:
        print("dev mode: staying awake (remove %s or reset to resume)" % config.NOSLEEP_FILE)
        return
    sleep.deep_sleep(sleep.ms_until_next_wake(utc, time.ticks_diff(time.ticks_ms(), utc_read_ms)))


def main() -> None:
    dev = sleep.dev_mode()
    if not dev:
        machine.WDT(timeout=config.WATCHDOG_MS)  # reset if a cycle ever hangs
    try:
        asyncio.run(run_cycle(dev))
    except Exception as e:
        sys.print_exception(e)
        if not dev:
            # Never stay awake on battery after an error: try again next period.
            sleep.deep_sleep(config.WAKE_PERIOD_S * 1000)
