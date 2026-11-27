import struct
from pathlib import Path

PROGRAM, PROTECTED, APPVAR, LIST, MATRIX, STRING, PICTURE = 0x05, 0x06, 0x15, 0x01, 0x02, 0x04, 0x07
EXTENSIONS = {PROGRAM: "8xp", PROTECTED: "8xp", APPVAR: "8xv", LIST: "8xl", MATRIX: "8xm", STRING: "8xs", PICTURE: "8ci"}
DATA_MAX = 65504
REAL_ZERO = bytes([0x00, 0x80]) + bytes(7)
TOKENS = {" ": 0x29, '"': 0x2A, "\n": 0x3F, ":": 0x3E}


def real(value):
    if not value:
        return REAL_ZERO
    text = str(value).ljust(14, "0")
    return bytes([0x00, 0x80 + len(str(value)) - 1]) + bytes(int(text[i:i + 2], 16) for i in range(0, 14, 2))


def build(var_type, name, raw, archived=False, comment=b"CEagle test"):
    entry = struct.pack("<HHB", 13, len(raw), var_type) + name.ljust(8, b"\0") + bytes([0, 0x80 if archived else 0])
    entry += struct.pack("<H", len(raw)) + raw
    header = b"**TI83F*\x1a\x0a\x00" + comment.ljust(42, b"\0") + struct.pack("<H", len(entry))
    return header + entry + struct.pack("<H", sum(entry) & 0xFFFF)


def sized(body):
    return struct.pack("<H", len(body)) + body


def basic(text):
    out = bytearray()
    i = 0
    while i < len(text):
        if text.startswith("Disp ", i):
            out.append(0xDE)
            i += 5
            continue
        if text.startswith("prgm", i):
            out.append(0x5F)
            i += 4
            continue
        c = text[i]
        out.append(TOKENS.get(c, ord(c)))
        i += 1
    return sized(bytes(out))


def asm(size, libraries=(), fill=0):
    body = bytearray(b"\xef\x7b\xc9")
    for name, version in libraries:
        body += b"\xc0" + name.encode() + b"\0" + bytes([version]) + b"\xc3\0\0\0"
    if libraries:
        body += b"\0"
    body += bytes([fill]) * (size - len(body))
    return sized(bytes(body))


def library(version, size=64):
    body = bytes([0xC0, 0xC1, version]) + bytes(size) + struct.pack("<I", 3)[:3] + bytes(3)
    return sized(body)


def appvar(payload):
    return sized(payload)


def real_list(values):
    return struct.pack("<H", len(values)) + b"".join(real(v) for v in values)


def matrix(rows, cols):
    return bytes([cols, rows]) + b"".join(real(r * cols + c) for r in range(rows) for c in range(cols))


def string(text):
    return basic(text)


def picture():
    return sized(bytes((i * 13) & 0xFF for i in range(21945)))


def raw_of(path):
    data = Path(path).read_bytes()
    hlen, dlen = struct.unpack_from("<HH", data, 55)
    return data[59], data[60:68].rstrip(b"\0"), data[57 + 2 + hlen:57 + 2 + hlen + dlen], data[69] & 0x80 != 0


def write(directory, filename, var_type, name, raw, archived=False):
    path = Path(directory) / filename
    path.write_bytes(build(var_type, name, raw, archived))
    return path
