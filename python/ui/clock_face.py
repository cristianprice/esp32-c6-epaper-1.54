# The clock face (layout: docs/clock_face_mockup.svg). render() is deterministic, so
# the previous frame can be re-created from persisted values for a partial refresh.
#
# Face values: (year, month, day, weekday, hour, minute, temp_x10, rh, battery)
#   weekday 0 = Monday; hour = NO_VALUE when the time isn't known yet;
#   temp_x10 = NO_TEMP / rh, battery = NO_VALUE when not available.

from persist import NO_TEMP, NO_VALUE
from ui import icons, writer
from ui.fonts import clock as f_clock
from ui.fonts import date as f_date
from ui.fonts import small as f_small
from ui.fonts import unit as f_unit
from ui.fonts import value as f_value

try:
    from typing import Optional
    from framebuf import FrameBuffer
    from apptypes import FaceValues
except ImportError:
    pass

DAYS = ("Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun")
MONTHS = ("Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec")

DATE = writer.Font(f_date)
SMALL = writer.Font(f_small)
CLOCK = writer.Font(f_clock)
VALUE = writer.Font(f_value)
UNIT = writer.Font(f_unit)

BLACK = 1
UNIT_RAISE = 8  # superscript-style units next to the values


def _battery(fb: FrameBuffer, percent: Optional[int], x: int = 166, y: int = 5) -> None:
    fb.rect(x, y, 25, 12, BLACK)
    fb.fill_rect(x + 25, y + 3, 2, 6, BLACK)
    if percent is not None:
        fb.fill_rect(x + 3, y + 3, (19 * percent + 50) // 100, 6, BLACK)
    writer.text(fb, SMALL, "--%" if percent is None else "%d%%" % percent, 194, 32, writer.RIGHT)


def _value(fb: FrameBuffer, value: str, unit: str, cx: int, baseline: int = 183) -> None:
    width = VALUE.width(value) + UNIT.width(unit)
    x = writer.text(fb, VALUE, value, cx - width // 2, baseline)
    writer.text(fb, UNIT, unit, x, baseline - UNIT_RAISE)


def render(fb: FrameBuffer, values: FaceValues) -> None:
    year, month, day, weekday, hour, minute, temp_x10, rh, battery = values
    fb.fill(0)
    time_known = hour != NO_VALUE

    # Top bar: date + battery.
    if time_known:
        date = "%s %02d %s %04d" % (DAYS[weekday], day, MONTHS[month - 1], year)
    else:
        date = "Time not set"
    writer.text(fb, DATE, date, 6, 17)
    _battery(fb, None if battery == NO_VALUE else battery)
    fb.fill_rect(4, 37, 192, 2, BLACK)

    # Plumber + time.
    writer.icon(fb, icons.PLUMBER, 8, 72)
    hhmm = "%02d:%02d" % (hour, minute) if time_known else "--:--"
    writer.text(fb, CLOCK, hhmm, 120, 102, writer.CENTER)
    fb.fill_rect(4, 121, 192, 2, BLACK)
    fb.fill_rect(99, 128, 2, 66, BLACK)

    # Temperature (left) and humidity (right).
    writer.icon(fb, icons.THERMOMETER, 10, 130)
    writer.text(fb, SMALL, "Temp", 30, 143)
    if temp_x10 == NO_TEMP:
        _value(fb, "--.-", "°C", 50)
    else:
        sign = "-" if temp_x10 < 0 else ""
        _value(fb, "%s%d.%d" % (sign, abs(temp_x10) // 10, abs(temp_x10) % 10), "°C", 50)

    writer.icon(fb, icons.DROPLET, 110, 130)
    writer.text(fb, SMALL, "Humidity", 130, 143)
    _value(fb, "--" if rh == NO_VALUE else "%d" % rh, "%", 150)
