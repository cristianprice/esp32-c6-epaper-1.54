# Shared type aliases. MicroPython has no `typing` module and ignores annotations, so
# the aliases only exist where `typing` does (CPython host tests, type checkers).
# Import them like typing names:
#
#     try:
#         from apptypes import FaceValues
#     except ImportError:
#         pass

try:
    from typing import Tuple

    # (year, month, day, weekday, hour, minute, temp_x10, rh, battery), see ui/clock_face.py
    FaceValues = Tuple[int, int, int, int, int, int, int, int, int]
    # (temperature_c, humidity_pct)
    ClimateReading = Tuple[float, float]
    # MONO_HLSB icon: (width, height, bitmap)
    Icon = Tuple[int, int, bytes]
except ImportError:
    pass
