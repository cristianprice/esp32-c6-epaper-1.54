"""PCF85063A RTC (battery backed). Times are (year, month, day, weekday, hour, minute, second),
weekday 0 = Monday."""

from typing import Sequence, Tuple, overload

def init() -> None: ...
@overload
def datetime() -> Tuple[int, int, int, int, int, int, int]: ...
@overload
def datetime(t: Sequence[int]) -> None: ...
def valid() -> bool:
    """False if the oscillator stopped since the time was last set."""

@overload
def ram() -> int: ...
@overload
def ram(value: int) -> None: ...
