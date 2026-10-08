"""SHTC3 temperature/humidity sensor."""

from typing import Tuple

MEASURE_MS: int

def init() -> int:
    """Wakes the sensor; returns its ID."""

def wakeup() -> None: ...
def sleep() -> None: ...
def measure_start() -> None: ...
def read() -> Tuple[float, float]:
    """(temperature_c, humidity_pct); OSError while the conversion is running."""
