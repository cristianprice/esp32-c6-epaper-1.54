# Text and icon drawing on a MONO_HLSB framebuf (1 = black) using the generated
# bitmap fonts (ui/fonts) and icons (ui/icons).

import framebuf

try:
    from typing import Any, Dict, Optional, Tuple
    from apptypes import Icon
except ImportError:
    pass

LEFT = 0
CENTER = 1
RIGHT = 2


class Font:
    def __init__(self, module: Any) -> None:
        """`module` is a generated font module (HEIGHT, BASELINE, CHARS, WIDTHS, DATA)."""
        self.height: int = module.HEIGHT
        self.baseline: int = module.BASELINE
        # framebuf needs a writable buffer, so the glyph data is copied to RAM once.
        self._data = memoryview(bytearray(module.DATA))
        self._index: Dict[str, Tuple[int, int]] = {}
        offset = 0
        for i, ch in enumerate(module.CHARS):
            width = module.WIDTHS[i]
            self._index[ch] = (offset, width)
            offset += ((width + 7) >> 3) * self.height
        self._cache: Dict[str, Tuple[framebuf.FrameBuffer, int]] = {}

    def glyph(self, ch: str) -> Tuple[Optional[framebuf.FrameBuffer], int]:
        """(FrameBuffer, width) for ch; (None, 0) for characters not in the font."""
        g = self._cache.get(ch)
        if g is None:
            if ch not in self._index:
                return None, 0
            offset, width = self._index[ch]
            size = ((width + 7) >> 3) * self.height
            fb = framebuf.FrameBuffer(self._data[offset : offset + size], width, self.height, framebuf.MONO_HLSB)
            g = self._cache[ch] = (fb, width)
        return g

    def width(self, text: str) -> int:
        return sum(self.glyph(ch)[1] for ch in text)


def text(fb: framebuf.FrameBuffer, font: Font, s: str, x: int, baseline: int, align: int = LEFT) -> int:
    """Draw s with its baseline at y = baseline; x is the left/center/right edge.
    Returns the x coordinate after the last glyph."""
    if align == CENTER:
        x -= font.width(s) // 2
    elif align == RIGHT:
        x -= font.width(s)
    top = baseline - font.baseline
    for ch in s:
        g, width = font.glyph(ch)
        if g:
            fb.blit(g, x, top, 0)  # 0 = transparent background
            x += width
    return x


def icon(fb: framebuf.FrameBuffer, ico: Icon, x: int, y: int) -> None:
    """Draw an icon tuple (width, height, MONO_HLSB bytes) from ui.icons."""
    width, height, data = ico
    fb.blit(framebuf.FrameBuffer(bytearray(data), width, height, framebuf.MONO_HLSB), x, y, 0)
