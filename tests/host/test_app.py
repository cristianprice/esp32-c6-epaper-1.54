"""Host tests for the clock's Python code (run: python tests/host/test_app.py).

Uses small stand-ins for MicroPython-only modules (tests/host/shims) and a `time`
replacement with MicroPython's semantics (UTC mktime/gmtime, 2000 epoch).
Writes tests/host/out/face_*.png previews of the rendered clock face.
"""

import calendar
import os
import sys
import time as _ctime
import types
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path[:0] = [os.path.join(HERE, "shims"), os.path.join(ROOT, "python")]

import config  # noqa: E402
import persist  # noqa: E402
from timekeeping import clock, tz  # noqa: E402
from ui import clock_face, display  # noqa: E402

EPOCH_2000 = 946684800

mptime = types.SimpleNamespace(
    mktime=lambda t: calendar.timegm(tuple(t[:6]) + (0, 0, 0)) - EPOCH_2000,
    gmtime=lambda s: tuple(_ctime.gmtime(s + EPOCH_2000))[:8],
)
tz.time = mptime
clock.time = mptime


def utc(y, mo, d, h=0, mi=0, s=0):
    return mptime.mktime((y, mo, d, h, mi, s, 0, 0))


class TimeZoneTest(unittest.TestCase):
    def tearDown(self):
        config.TZ_OFFSET_MIN, config.DST_RULE = 0, None

    def test_eu(self):
        config.TZ_OFFSET_MIN, config.DST_RULE = 120, "EU"  # EET / EEST
        # 2026: DST from 29 Mar 01:00 UTC to 25 Oct 01:00 UTC.
        self.assertEqual(tz.offset_s(utc(2026, 3, 29, 0, 59)), 7200)
        self.assertEqual(tz.offset_s(utc(2026, 3, 29, 1, 0)), 10800)
        self.assertEqual(tz.offset_s(utc(2026, 10, 25, 0, 59)), 10800)
        self.assertEqual(tz.offset_s(utc(2026, 10, 25, 1, 0)), 7200)

    def test_us(self):
        config.TZ_OFFSET_MIN, config.DST_RULE = -300, "US"  # US Eastern
        # 2026: 8 Mar 02:00 EST (07:00 UTC) .. 1 Nov 02:00 EDT (06:00 UTC).
        self.assertEqual(tz.offset_s(utc(2026, 3, 8, 6, 59)), -18000)
        self.assertEqual(tz.offset_s(utc(2026, 3, 8, 7, 0)), -14400)
        self.assertEqual(tz.offset_s(utc(2026, 11, 1, 5, 59)), -14400)
        self.assertEqual(tz.offset_s(utc(2026, 11, 1, 6, 0)), -18000)

    def test_il(self):
        config.TZ_OFFSET_MIN, config.DST_RULE = 120, "IL"
        # 2026: Fri 27 Mar 02:00 IST (00:00 UTC) .. Sun 25 Oct 02:00 IDT (23:00 UTC on the 24th).
        self.assertEqual(tz.offset_s(utc(2026, 3, 26, 23, 59)), 7200)
        self.assertEqual(tz.offset_s(utc(2026, 3, 27, 0, 0)), 10800)
        self.assertEqual(tz.offset_s(utc(2026, 10, 24, 22, 59)), 10800)
        self.assertEqual(tz.offset_s(utc(2026, 10, 24, 23, 0)), 7200)


class NtpPolicyTest(unittest.TestCase):
    def state(self, last_day=0, last_try=0):
        st = persist.State()
        st.last_ntp_day, st.last_ntp_try_min = last_day, last_try
        return st

    def test_noon_window(self):
        st = self.state(last_day=20261007)
        self.assertFalse(clock.ntp_due(st, utc(2026, 10, 8, 11, 58), True))
        self.assertTrue(clock.ntp_due(st, utc(2026, 10, 8, 12, 0), True))
        self.assertTrue(clock.ntp_due(st, utc(2026, 10, 8, 12, 58), True))
        self.assertFalse(clock.ntp_due(st, utc(2026, 10, 8, 13, 0), True))

    def test_once_per_day(self):
        st = self.state(last_day=20261008)
        self.assertFalse(clock.ntp_due(st, utc(2026, 10, 8, 12, 2), True))
        self.assertTrue(clock.ntp_due(st, utc(2026, 10, 9, 12, 0), True))

    def test_retry_spacing(self):
        now = utc(2026, 10, 8, 12, 4)
        st = self.state(last_day=20261007, last_try=now // 60 - 4)
        self.assertFalse(clock.ntp_due(st, now, True))
        st.last_ntp_try_min = now // 60 - 10
        self.assertTrue(clock.ntp_due(st, now, True))

    def test_invalid_time_syncs_any_hour(self):
        self.assertTrue(clock.ntp_due(self.state(), utc(2000, 1, 1, 3, 0), False))


class PersistTest(unittest.TestCase):
    def test_roundtrip(self):
        st = persist.State()
        st.last_ntp_day, st.last_ntp_try_min, st.wakes, st.since_full = 20261008, 123456, 42, 7
        st.shown = (2026, 10, 8, 3, 14, 32, -15, 48, 78)
        persist.save(st)
        got = persist.load()
        self.assertFalse(got.cold)
        self.assertEqual(
            (got.last_ntp_day, got.last_ntp_try_min, got.wakes, got.since_full, got.shown),
            (20261008, 123456, 42, 7, st.shown),
        )

    def test_empty_memory_is_cold(self):
        import machine

        machine._rtc_memory = b""
        self.assertTrue(persist.load().cold)


class FaceTest(unittest.TestCase):
    def render(self, name, values):
        canvas = display.Canvas()
        clock_face.render(canvas.fb, values)
        out = os.path.join(HERE, "out")
        os.makedirs(out, exist_ok=True)
        try:
            from PIL import Image
        except ImportError:
            return canvas
        img = Image.frombytes("1", (200, 200), bytes(b ^ 0xFF for b in canvas.buf))
        img.resize((600, 600), Image.NEAREST).save(os.path.join(out, name + ".png"))
        return canvas

    def test_render_normal(self):
        canvas = self.render("face_normal", (2026, 10, 8, 3, 14, 32, 224, 48, 78))
        self.assertTrue(any(canvas.buf))

    def test_render_unknown(self):
        self.render("face_unknown", (0, 0, 0, 0, persist.NO_VALUE, 0, persist.NO_TEMP, persist.NO_VALUE, persist.NO_VALUE))

    def test_render_negative(self):
        self.render("face_negative", (2026, 1, 3, 5, 7, 4, -53, 91, 5))

    def test_deterministic(self):
        values = (2026, 10, 8, 3, 9, 0, 215, 50, 100)
        self.assertEqual(self.render("a", values).buf, self.render("b", values).buf)


if __name__ == "__main__":
    unittest.main(verbosity=2)
