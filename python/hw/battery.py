# Battery level from the VBAT/2 divider on ADC1 (BSP curve: 3.00 V = 0 %, 4.12 V = 100 %).

import bsp

try:
    from typing import Optional
except ImportError:
    pass


async def read() -> Optional[int]:
    """Battery charge in percent, or None if the ADC can't be read."""
    try:
        bsp.battery.init()
        return bsp.battery.percent(bsp.battery.voltage())
    except OSError:
        return None
