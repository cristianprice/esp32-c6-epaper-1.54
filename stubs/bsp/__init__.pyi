"""Type stub for the `bsp` C module (firmware/modules/bsp/modbsp.c)."""

from . import battery as battery
from . import epd as epd
from . import i2c as i2c
from . import power as power
from . import rtc as rtc
from . import shtc3 as shtc3

PIN_BOOT_BUTTON: int
PIN_PWR_BUTTON: int

def init() -> None:
    """Shared I2C bus (SDA 18 / SCL 8) + TCA9554 expander. Call first."""
