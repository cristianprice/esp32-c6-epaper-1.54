"""1.54" 200x200 e-paper. begin/show/sleep start a sequence; poll() until it returns True."""

from typing import Optional

from _typeshed import ReadableBuffer

WIDTH: int
HEIGHT: int

def init() -> None: ...
def begin(full: bool) -> None: ...
def show(new: ReadableBuffer, old: Optional[ReadableBuffer] = None, full: bool = False) -> None:
    """Images are 5000 bytes MONO_HLSB, 1 = black. Raises OSError(EBUSY) if not idle."""

def sleep() -> None: ...
def poll() -> bool: ...
def busy() -> bool: ...
