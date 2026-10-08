"""Minimal CPython stand-in for MicroPython's framebuf (MONO_HLSB only), for host tests."""

MONO_HLSB = 3


class FrameBuffer:
    def __init__(self, buf, width, height, fmt, stride=None):
        assert fmt == MONO_HLSB
        self.buf = buf
        self.width = width
        self.height = height
        self.stride = (width + 7) & ~7

    def _index(self, x, y):
        return (y * self.stride + x) >> 3, 0x80 >> (x & 7)

    def pixel(self, x, y, c=None):
        if not (0 <= x < self.width and 0 <= y < self.height):
            return None
        i, bit = self._index(x, y)
        if c is None:
            return 1 if self.buf[i] & bit else 0
        if c:
            self.buf[i] |= bit
        else:
            self.buf[i] &= ~bit & 0xFF

    def fill(self, c):
        for i in range(len(self.buf)):
            self.buf[i] = 0xFF if c else 0

    def fill_rect(self, x, y, w, h, c):
        for yy in range(y, y + h):
            for xx in range(x, x + w):
                self.pixel(xx, yy, c)

    def rect(self, x, y, w, h, c, fill=False):
        if fill:
            return self.fill_rect(x, y, w, h, c)
        self.fill_rect(x, y, w, 1, c)
        self.fill_rect(x, y + h - 1, w, 1, c)
        self.fill_rect(x, y, 1, h, c)
        self.fill_rect(x + w - 1, y, 1, h, c)

    def blit(self, src, x, y, key=-1):
        for yy in range(src.height):
            for xx in range(src.width):
                c = src.pixel(xx, yy)
                if c != key:
                    self.pixel(x + xx, y + yy, c)
