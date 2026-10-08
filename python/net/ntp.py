# Minimal asyncio SNTP client (non-blocking UDP socket polled from the event loop).

import asyncio
import socket
import struct
import time

import config

# Seconds between the NTP epoch (1900) and the MicroPython epoch.
NTP_DELTA = 3155673600 if time.gmtime(0)[0] == 2000 else 2208988800


async def query(host: str, timeout_ms: int) -> int:
    """UTC seconds (MicroPython epoch) from one NTP server."""
    addr = socket.getaddrinfo(host, 123, 0, socket.SOCK_DGRAM)[0][-1]
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        sock.setblocking(False)
        request = bytearray(48)
        request[0] = 0x1B  # LI = 0, version 3, mode 3 (client)
        sock.sendto(request, addr)
        start = time.ticks_ms()
        while True:
            try:
                data = sock.recv(48)
                break
            except OSError:
                if time.ticks_diff(time.ticks_ms(), start) > timeout_ms:
                    raise OSError("NTP timeout: " + host)
                await asyncio.sleep_ms(20)
    finally:
        sock.close()
    if len(data) < 48 or data[0] >> 6 == 3 or data[1] == 0:
        raise OSError("bad NTP reply from " + host)  # unsynchronised or kiss-of-death
    secs: int
    frac: int
    secs, frac = struct.unpack("!II", data[40:48])
    return secs - NTP_DELTA + (1 if frac >= 0x80000000 else 0)


async def get_time() -> int:
    """Try the configured servers in order; raises OSError if all fail."""
    error = OSError("no NTP servers configured")
    for host in config.NTP_SERVERS:
        try:
            return await query(host, config.NTP_TIMEOUT_MS)
        except OSError as e:
            error = e
    raise error
