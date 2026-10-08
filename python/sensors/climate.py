# Temperature and humidity from the on-board SHTC3 (awake only during the measurement).

import asyncio

import bsp
import config

try:
    from typing import Optional
    from apptypes import ClimateReading
except ImportError:
    pass

READ_RETRIES = 5


async def read() -> Optional[ClimateReading]:
    """(temperature_c, humidity_pct), or None if the sensor doesn't answer."""
    try:
        bsp.shtc3.init()  # wakes the sensor
        bsp.shtc3.measure_start()
        await asyncio.sleep_ms(bsp.shtc3.MEASURE_MS)
        for _ in range(READ_RETRIES):
            try:
                temp, rh = bsp.shtc3.read()
                return temp + config.TEMP_OFFSET_C, min(max(rh, 0.0), 100.0)
            except OSError:
                await asyncio.sleep_ms(2)  # conversion not finished yet
        return None
    except OSError:
        return None
    finally:
        try:
            bsp.shtc3.sleep()
        except OSError:
            pass
