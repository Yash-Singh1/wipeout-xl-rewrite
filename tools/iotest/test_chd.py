#!/usr/bin/env python3
"""Generated CHDs, without a copyrighted disc or chdman. Run with the chd_decode executable's path."""
import binascii
import lzma
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import zlib

FRAME = 2448
SECTOR = 2352


def crc(data):
    return binascii.crc_hqx(data, 65535)


class Bits:
    def __init__(self):
        self.bits = ""

    def put(self, value, count):
        self.bits += f"{value:0{count}b}" if count else ""

    def data(self):
        return int(self.bits.ljust((len(self.bits) + 7) // 8 * 8, "0"), 2).to_bytes((len(self.bits) + 7) // 8, "big")


def deflate(data, strategy=zlib.Z_DEFAULT_STRATEGY, level=6):
    z = zlib.compressobj(level, zlib.DEFLATED, -15, strategy=strategy)
    return z.compress(data) + z.flush()


def huffman_zeros(size):
    b = Bits()
    # Small tree: repeat=1 bit, lengths 0 and 1=2 bits each, then its terminator.
    for value, count in [(1, 3), (0, 3), (2, 3), (2, 3), (7, 3)]:
        b.put(value, count)
    b.put(1, 2)  # new length 1 at symbol 0
    b.put(0, 2)  # new length 0 at symbol 1
    b.put(1, 1)  # repeat the zero length 254 times
    b.put(7, 3)
    b.put(245, 8)
    b.put(0, size)
    return b.data()


def make_chd(path, frames, codec=b"zlib", pack=deflate, tracks=None, types=None, map_symbols=None):
    hunk = 4 * FRAME
    assert len(frames) % 4 == 0
    raw = [b"".join(frames[i:i + 4]) for i in range(0, len(frames), 4)]
    tracks = tracks or ["TRACK:1 TYPE:MODE2_RAW SUBTYPE:NONE FRAMES:%d" % len(frames)]
    metadata = bytearray()
    for i, text in enumerate(tracks):
        text = text.encode() + b"\0"
        tag = b"CHT2" if b"PREGAP:" in text else b"CHTR"
        next_at = 124 + len(metadata) + 16 + len(text) if i + 1 < len(tracks) else 0
        metadata += tag + struct.pack(">IQ", len(text) | 0x1000000, next_at) + text
    payload = bytearray()
    entries = bytearray()
    codes = types or [0] * len(raw)
    b = Bits()
    for _ in range(16):
        b.put(4, 4)  # all 16 map symbols have 4-bit codes
    for code in map_symbols if map_symbols is not None else codes:
        b.put(code, 4)
    lengths = []
    for i, data in enumerate(raw):
        if codes[i] in (5, 9, 10):
            lengths.append((5, 0, 0, 0))
            continue
        packed = data if codes[i] == 4 else pack(data)
        lengths.append((codes[i], len(packed), 124 + len(metadata) + len(payload), crc(data)))
        payload += packed
    for kind, length, offset, checksum in lengths:
        if kind == 5:
            if codes[len(entries) // 12] == 5:
                b.put(0, 1)
        else:
            if kind < 4:
                b.put(length, 16)
            b.put(checksum, 16)
        entries += bytes([kind]) + length.to_bytes(3, "big") + offset.to_bytes(6, "big") + checksum.to_bytes(2, "big")
    map_data = b.data()
    map_at = 124 + len(metadata) + len(payload)
    map_header = len(map_data).to_bytes(4, "big") + (124 + len(metadata)).to_bytes(6, "big")
    map_header += struct.pack(">H4B", crc(entries), 16, 1, 0, 0)
    header = b"MComprHD" + struct.pack(">II", 124, 5) + codec + bytes(12)
    header += struct.pack(">QQQII", len(frames) * FRAME, map_at, 124, hunk, FRAME) + bytes(60)
    path.write_bytes(header + metadata + payload + map_header + map_data)


def flac_constant(channels, samples=2352):
    def polynomial_crc(data, bits, polynomial):
        value = 0
        for byte in data:
            value ^= byte << (bits - 8)
            for _ in range(8):
                value = ((value << 1) ^ (polynomial if value & (1 << (bits - 1)) else 0)) & ((1 << bits) - 1)
        return value
    b = Bits()
    for value, width in [(0x3FFE, 14), (0, 1), (0, 1), (7, 4), (9, 4), (channels, 4),
                         (4, 3), (0, 1), (0, 8), (samples - 1, 16)]:
        b.put(value, width)
    b.put(polynomial_crc(b.data(), 8, 7), 8)
    left, right = 1234, -2345
    a, c = {1: (left, right), 8: (left, left - right), 9: (left - right, right),
            10: ((left + right) // 2, left - right)}[channels]
    for value, depth in [(a, 17 if channels == 9 else 16), (c, 17 if channels in (8, 10) else 16)]:
        b.put(0, 8)  # constant subframe, no wasted bits
        b.put(value & ((1 << depth) - 1), depth)
    frame = b.data()
    return frame + polynomial_crc(frame, 16, 0x8005).to_bytes(2, "big") + deflate(bytes(4 * 96))


def main():
    decoder = str(Path(sys.argv[1]).resolve())
    checks = 0
    with tempfile.TemporaryDirectory(prefix="wipeout-chd-") as folder:
        folder = Path(folder)
        image, binary, cue = [folder / name for name in ("disc.chd", "disc.bin", "disc.cue")]

        def run(ok=True, message=None):
            nonlocal checks
            result = subprocess.run([decoder, str(image), str(binary), str(cue)], capture_output=True, text=True)
            assert (result.returncode == 0) == ok, result.stderr
            if message:
                assert message in result.stderr, result.stderr
            if not ok:
                assert not binary.exists() and not cue.exists(), "partial conversion survived"
            checks += 1

        frames = [bytes((j + i * 37) % 256 for j in range(FRAME)) for i in range(12)]
        tracks = ["TRACK:1 TYPE:MODE2_RAW SUBTYPE:NONE FRAMES:4 PREGAP:0 PGTYPE:MODE1 PGSUB:NONE POSTGAP:0",
                  "TRACK:2 TYPE:AUDIO SUBTYPE:NONE FRAMES:5 PREGAP:2 PGTYPE:VAUDIO PGSUB:NONE POSTGAP:1"]
        make_chd(image, frames, tracks=tracks)
        run()
        expected = b"".join(f[:SECTOR] for f in frames[:4])
        for frame in frames[4:9]:
            raw = frame[:SECTOR]
            expected += bytes(v for pair in zip(raw[1::2], raw[::2]) for v in pair)
        expected += bytes(SECTOR)
        assert binary.read_bytes() == expected
        assert "INDEX 00 00:00:04" in cue.read_text() and "INDEX 01 00:00:06" in cue.read_text()
        tracks[1] = tracks[1].replace("VAUDIO", "AUDIO")
        make_chd(image, frames, tracks=tracks)
        run()
        assert "PREGAP 00:00:02" in cue.read_text() and "INDEX 01 00:00:04" in cue.read_text()

        raw_expected = b"".join(f[:SECTOR] for f in frames)
        for strategy in (zlib.Z_FIXED, zlib.Z_DEFAULT_STRATEGY):
            make_chd(image, frames, pack=lambda d: deflate(d, strategy))
            run()
            assert binary.read_bytes() == raw_expected
        # A non-final stored block followed by a compressed block, all in one hunk.
        def stored_then_fixed(data):
            first, rest = data[:100], data[100:]
            return b"\0" + struct.pack("<HH", len(first), len(first) ^ 65535) + first + deflate(rest, zlib.Z_FIXED)
        make_chd(image, frames, pack=stored_then_fixed)
        run()
        assert binary.read_bytes() == raw_expected
        make_chd(image, frames, codec=b"lzma", pack=lambda d: lzma.compress(d, format=lzma.FORMAT_RAW,
                 filters=[{"id": lzma.FILTER_LZMA1, "dict_size": 65536, "lc": 3, "lp": 0, "pb": 2}]))
        run()
        assert binary.read_bytes() == raw_expected
        make_chd(image, [bytes(FRAME)] * 12, codec=b"huff", pack=lambda d: huffman_zeros(len(d)))
        run()
        assert binary.read_bytes() == bytes(12 * SECTOR)
        make_chd(image, frames[:4] * 3, types=[0, 5, 9])
        run()
        assert binary.read_bytes() == b"".join(f[:SECTOR] for f in frames[:4]) * 3
        make_chd(image, frames, types=[4, 4, 4])
        run()
        assert binary.read_bytes() == raw_expected

        # Map RLE starts with an implicit codec 0, before there is a previous entry.
        make_chd(image, frames, map_symbols=[7, 0])
        run()
        assert binary.read_bytes() == raw_expected

        # CD codecs pack the raw sectors and subchannels separately.
        def cd_pack(data, codec):
            raw = b"".join(data[i:i + SECTOR] for i in range(0, len(data), FRAME))
            sub = b"".join(data[i + SECTOR:i + FRAME] for i in range(0, len(data), FRAME))
            packed = deflate(raw) if codec == b"cdzl" else lzma.compress(raw, format=lzma.FORMAT_RAW,
                     filters=[{"id": lzma.FILTER_LZMA1, "dict_size": 65536}])
            return b"\0" + len(packed).to_bytes(2, "big") + packed + deflate(sub)
        for codec in (b"cdzl", b"cdlz"):
            make_chd(image, frames, codec=codec, pack=lambda d: cd_pack(d, codec))
            run()
            assert binary.read_bytes() == raw_expected

        audio = struct.pack(">hh", 1234, -2345) * 588 + bytes(96)
        for channels in (1, 8, 9, 10):
            make_chd(image, [audio] * 4, codec=b"cdfl", pack=lambda d: flac_constant(channels))
            run()
            assert binary.read_bytes() == audio[:SECTOR] * 4

        # An uncompressed v5 map uses hunk numbers, including zero for a sparse hunk.
        make_chd(image, frames)
        original = image.read_bytes()
        metadata_end = 124 + 16 + len("TRACK:1 TYPE:MODE2_RAW SUBTYPE:NONE FRAMES:12") + 1
        hunk = 4 * FRAME
        header = bytearray(original[:124])
        header[16:32] = bytes(16)
        header[40:48] = metadata_end.to_bytes(8, "big")
        plain = header + original[124:metadata_end] + struct.pack(">III", 1, 0, 2)
        plain += bytes(hunk - len(plain)) + b"".join(frames[:4] + frames[8:12])
        image.write_bytes(plain)
        run()
        assert binary.read_bytes() == b"".join(f[:SECTOR] for f in frames[:4]) + bytes(4 * SECTOR) + b"".join(f[:SECTOR] for f in frames[8:])

        # A mode 1 data track: raw as it is, and cooked (2048 bytes of data per frame) made raw with a header.
        make_chd(image, frames[:4], tracks=["TRACK:1 TYPE:MODE1_RAW SUBTYPE:NONE FRAMES:4"])
        run()
        assert "TRACK 01 MODE1/2352" in cue.read_text()
        assert binary.read_bytes() == b"".join(f[:SECTOR] for f in frames[:4])
        make_chd(image, frames[:4], tracks=["TRACK:1 TYPE:MODE1 SUBTYPE:NONE FRAMES:4"])
        run()
        assert "TRACK 01 MODE1/2352" in cue.read_text()

        def bcd(v):
            return v // 10 << 4 | v % 10
        cooked = b""
        for lba, f in enumerate(frames[:4]):
            address = lba + 150
            cooked += (b"\0" + b"\xff" * 10 + b"\0" + bytes([bcd(address // 4500), bcd(address // 75 % 60),
                                                         bcd(address % 75), 1]) + f[:2048] + bytes(288))
        assert binary.read_bytes() == cooked

        # A valid map whose self references loop must fail, too.
        make_chd(image, frames, types=[5, 5, 5])
        run(False)

        for change, message in [((12, b"\0\0\0\4"), "not a version 5"),
                                ((16, b"cdzs"), "codec 'cdzs'"), ((104, b"\1"), "parent CHD"),
                                ((60, struct.pack(">I", 2048)), "not a raw-sector CD")]:
            make_chd(image, frames)
            at, value = change
            data = bytearray(image.read_bytes())
            data[at:at + len(value)] = value
            image.write_bytes(data)
            binary.unlink(missing_ok=True)
            cue.unlink(missing_ok=True)
            run(False, message)
        make_chd(image, frames)
        data = bytearray(image.read_bytes())
        data[-1] ^= 128
        image.write_bytes(data)
        run(False)
        make_chd(image, frames)
        data = image.read_bytes()
        image.write_bytes(data[:-20])
        run(False)
        make_chd(image, frames)
        data = bytearray(image.read_bytes())
        data[300] ^= 255
        image.write_bytes(data)
        run(False)
    print(f"CHD: {checks} generated-image checks passed")


if __name__ == "__main__":
    main()
