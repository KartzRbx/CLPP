"""Build assets/brand/clpp.ico (used by src/cli/clpp.rc) from assets/brand/clpp.png.

The decagon is cropped from the transparent background to a square and scaled down with an
area filter (premultiplied alpha, so edges stay smooth) to the usual icon sizes. Pure Python
(zlib only), no imaging library.

Usage: python tools/scripts/make_icon.py
"""
import os
import struct
import zlib

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SOURCE = os.path.join(ROOT, "assets", "brand", "clpp.png")
TARGET = os.path.join(ROOT, "assets", "brand", "clpp.ico")
SIZES = [256, 128, 64, 48, 32, 24, 16]


def read_png(path):
    data = open(path, "rb").read()
    assert data[:8] == b"\x89PNG\r\n\x1a\n", "not a PNG"
    pos, idat = 8, b""
    while pos < len(data):
        length, kind = struct.unpack(">I4s", data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + length]
        if kind == b"IHDR":
            width, height, depth, color = struct.unpack(">IIBB", body[:10])
            assert depth == 8 and color in (2, 6), "expected 8-bit RGB/RGBA"
        elif kind == b"IDAT":
            idat += body
        pos += 12 + length
    channels = 4 if color == 6 else 3
    raw = zlib.decompress(idat)
    stride = width * channels
    rows, previous = [], bytearray(stride)
    for y in range(height):
        filter_type = raw[y * (stride + 1)]
        line = bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        for x in range(stride):
            a = line[x - channels] if x >= channels else 0
            b = previous[x]
            c = previous[x - channels] if x >= channels else 0
            if filter_type == 1:
                line[x] = (line[x] + a) & 255
            elif filter_type == 2:
                line[x] = (line[x] + b) & 255
            elif filter_type == 3:
                line[x] = (line[x] + (a + b) // 2) & 255
            elif filter_type == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[x] = (line[x] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        rows.append(line)
        previous = line
    pixels = [[(row[x * channels], row[x * channels + 1], row[x * channels + 2],
                (row[x * channels + 3] if channels == 4 else 255) / 255.0) for x in range(width)] for row in rows]
    return width, height, pixels


def bounding_box(width, height, pixels):
    xs, ys = [], []
    for y in range(0, height, 2):
        for x in range(0, width, 2):
            if pixels[y][x][3] > 0.5:
                xs.append(x)
                ys.append(y)
    return min(xs), min(ys), max(xs), max(ys)


def scaled(square, side, size):
    """Area-average `square` (side x side, straight RGBA) down to size x size, premultiplied."""
    out = []
    step = side / size
    for oy in range(size):
        y0, y1 = int(oy * step), max(int((oy + 1) * step), int(oy * step) + 1)
        row = []
        for ox in range(size):
            x0, x1 = int(ox * step), max(int((ox + 1) * step), int(ox * step) + 1)
            r = g = b = a = 0.0
            count = 0
            for y in range(y0, y1):
                for x in range(x0, x1):
                    pr, pg, pb, pa = square[y][x]
                    r += pr * pa
                    g += pg * pa
                    b += pb * pa
                    a += pa
                    count += 1
            if a > 0:
                row.append((round(r / a), round(g / a), round(b / a), round(255 * a / count)))
            else:
                row.append((0, 0, 0, 0))
        out.append(row)
    return out


def write_png(rows):
    size = len(rows)
    raw = b"".join(b"\x00" + bytes(channel for pixel in row for channel in pixel) for row in rows)

    def chunk(kind, body):
        return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body) & 0xFFFFFFFF)

    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0)) +
            chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def main():
    width, height, pixels = read_png(SOURCE)
    left, top, right, bottom = bounding_box(width, height, pixels)
    side = max(right - left, bottom - top) + 8
    cx, cy = (left + right) // 2, (top + bottom) // 2
    square = []
    for y in range(cy - side // 2, cy - side // 2 + side):
        row = []
        for x in range(cx - side // 2, cx - side // 2 + side):
            row.append(pixels[y][x] if 0 <= x < width and 0 <= y < height else (0.0, 0.0, 0.0, 0.0))
        square.append(row)
    images = [write_png(scaled(square, side, size)) for size in SIZES]
    header = struct.pack("<HHH", 0, 1, len(images))
    offset = 6 + 16 * len(images)
    entries, blobs = b"", b""
    for size, image in zip(SIZES, images):
        entries += struct.pack("<BBBBHHII", size % 256, size % 256, 0, 0, 1, 32, len(image), offset)
        offset += len(image)
        blobs += image
    with open(TARGET, "wb") as f:
        f.write(header + entries + blobs)
    print(TARGET, offset, "bytes")


if __name__ == "__main__":
    main()
