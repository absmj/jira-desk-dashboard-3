#!/usr/bin/env python3
"""Generate small test MP3s data/MP3/0001.mp3 .. 0005.mp3 (mono, 44.1 kHz, 64 kbps CBR).

The melodies mirror src/audio/BuzzerAudio.cpp, so what you hear on a real DFPlayer
roughly matches what the buzzer plays in the simulator. Copy the files to the DFPlayer
SD card (FAT32) under /MP3/ for the real device.

    pip install lameenc
    python3 tools/make_test_mp3.py
"""
import math
import pathlib
import struct

import lameenc

RATE = 44100
KBPS = 64  # keep in sync with SimMp3Audio::kAssumedKbps

# (hz, ms); hz 0 = rest
TRACKS = {
    1: [(880, 150)],
    2: [(1200, 120), (0, 80), (1200, 120), (0, 80), (1200, 120)],
    3: [(523, 120), (659, 120), (784, 120), (1046, 200)],
    4: [(300, 250), (0, 100), (300, 250)],
    5: [(784, 200), (988, 200), (1175, 300)],
}


def render(notes):
    samples = []
    for hz, ms in notes:
        n = RATE * ms // 1000
        for i in range(n):
            if hz == 0:
                samples.append(0)
                continue
            env = min(1.0, i / 200, (n - i) / 200)  # short fade avoids clicks
            samples.append(int(12000 * env * math.sin(2 * math.pi * hz * i / RATE)))
    samples.extend([0] * (RATE // 10))  # 100 ms tail
    return struct.pack("<%dh" % len(samples), *samples)


def main():
    out = pathlib.Path(__file__).resolve().parent.parent / "data" / "MP3"
    out.mkdir(parents=True, exist_ok=True)
    for track, notes in TRACKS.items():
        enc = lameenc.Encoder()
        enc.set_bit_rate(KBPS)
        enc.set_in_sample_rate(RATE)
        enc.set_channels(1)
        enc.set_quality(2)
        data = enc.encode(render(notes)) + enc.flush()
        path = out / ("%04d.mp3" % track)
        path.write_bytes(data)
        print("%s  %6d bytes  ~%d ms" % (path.name, len(data), len(data) * 8 // KBPS))


if __name__ == "__main__":
    main()
