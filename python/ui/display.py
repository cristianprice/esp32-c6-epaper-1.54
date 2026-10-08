# Async e-paper updates on top of bsp.epd (whose slow steps are polled, never blocking).

import asyncio
import framebuf

import bsp

try:
    from typing import Optional
except ImportError:
    pass

POLL_MS = 10
BUF_SIZE = bsp.epd.WIDTH * bsp.epd.HEIGHT // 8


class Canvas:
    """A 200x200 1-bit image: `buf` for the panel, `fb` (1 = black) for drawing."""

    def __init__(self) -> None:
        self.buf = bytearray(BUF_SIZE)
        self.fb = framebuf.FrameBuffer(self.buf, bsp.epd.WIDTH, bsp.epd.HEIGHT, framebuf.MONO_HLSB)


async def _done() -> None:
    while not bsp.epd.poll():
        await asyncio.sleep_ms(POLL_MS)


async def update(new: Canvas, old: Optional[Canvas] = None, full: bool = False) -> None:
    """Show canvas `new`. Partial refresh needs `old` = what the panel shows now
    (it is re-rendered after deep sleep, since the panel RAM may be lost).
    Leaves the panel in deep sleep."""
    full = full or old is None
    bsp.epd.begin(full)
    await _done()
    bsp.epd.show(new.buf, None if full or old is None else old.buf, full=full)
    await _done()
    bsp.epd.sleep()
    await _done()
