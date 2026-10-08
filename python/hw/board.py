# Board bring-up and power policy: only what the wake cycle needs is ever on.

import asyncio
import machine

import bsp
import config

EPD_POWER_SETTLE_MS = 10


async def init() -> None:
    """Initialise the board for one wake cycle."""
    machine.freq(config.CPU_FREQ_HZ)
    bsp.init()
    # Keep the board alive on battery first, then make sure everything unused is off.
    bsp.power.vbat_latch(True)
    bsp.power.audio(False)
    bsp.power.led(False)
    epd_was_off = not bsp.power.epd()
    bsp.power.epd(True)
    if epd_was_off:
        await asyncio.sleep_ms(EPD_POWER_SETTLE_MS)
    bsp.epd.init()
    bsp.rtc.init()


def prepare_sleep() -> None:
    """Last step before deep sleep (the panel must already be in its sleep mode)."""
    if config.EPD_POWER_OFF_IN_SLEEP:
        bsp.power.epd(False)
