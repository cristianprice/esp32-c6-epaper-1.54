"""Host stand-in for `machine`: RTC with user memory (for persist.py)."""

_rtc_memory = b""


class RTC:
    def memory(self, data=None):
        global _rtc_memory
        if data is None:
            return _rtc_memory
        _rtc_memory = bytes(data)
