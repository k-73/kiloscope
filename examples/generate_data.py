#!/usr/bin/env python3
"""
Kilo — example UDP data generator.

Sends KSCP packets to localhost:9000 with 3 channels:
  ch0 (X): smooth Lissajous-like oscillation
  ch1 (Y): smooth Lissajous-like oscillation (phase-shifted)
  ch2 (Z): slow vertical oscillation

Protocol (little-endian):
  4B magic (0x4B534350 = "KSCP")
  2B channel_id
  2B sample_count
  8B timestamp
  N × (8B timestamp + 8B value)
"""

import socket, struct, time, math, random

ADDR = ("127.0.0.1", 9000)
MAGIC = 0x4B534350
RATE_HZ = 1000

def make_packet(channel_id: int, timestamp: float, value: float) -> bytes:
    return struct.pack("<IHHd dd", MAGIC, channel_id, 1, timestamp, timestamp, value)

def main():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    t0 = time.monotonic()
    dt = 1.0 / RATE_HZ
    print(f"Sending to {ADDR[0]}:{ADDR[1]} at {RATE_HZ} Hz — Ctrl+C to stop")

    try:
        while True:
            t = time.monotonic() - t0

            x = math.sin(t * 0.7) + 0.5 * math.sin(t * 1.3) + random.gauss(0, 0.03)
            y = math.cos(t * 0.9) + 0.5 * math.cos(t * 1.7) + random.gauss(0, 0.03)
            z = 0.6 * math.sin(t * 0.4) + 0.3 * math.sin(t * 1.1) + random.gauss(0, 0.02)

            for ch, val in enumerate((x, y, z)):
                sock.sendto(make_packet(ch, t, val), ADDR)

            time.sleep(dt)
    except KeyboardInterrupt:
        print("\nStopped.")

if __name__ == "__main__":
    main()
