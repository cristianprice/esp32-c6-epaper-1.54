"""Battery voltage on ADC1 channel 0 (VBAT / 2)."""

from typing import Optional

def init() -> None: ...
def voltage() -> float: ...
def percent(volts: Optional[float] = None) -> int:
    """0..100 (3.00 V .. 4.12 V); measures if no voltage is given."""
