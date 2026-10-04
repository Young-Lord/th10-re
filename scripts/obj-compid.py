#!/usr/bin/env python3
"""Extract the @comp.id symbol value from a MSVC COFF object.

comp.id encodes the compiler build (low 24 bits), so it distinguishes a
13.10.3077 (Toolkit RTM) object from a 13.10.6030 (SP1) one regardless of
code content.

Usage: python3 scripts/obj-compid.py obj [obj...]
"""
import struct
import sys


def compid(path: str) -> list[str]:
    d = open(path, "rb").read()
    machine, nsec, ts, symptr, nsym, optsz, chars = struct.unpack_from("<HHIIIHH", d, 0)
    strtab = symptr + nsym * 18
    out = []
    for i in range(nsym):
        off = symptr + i * 18
        name = d[off:off + 8]
        if name[:4] == b"\x00\x00\x00\x00":
            soff = struct.unpack_from("<I", name, 4)[0]
            end = d.find(b"\x00", strtab + soff)
            if end < 0:
                continue
            nm = d[strtab + soff:end].decode("latin1")
        else:
            nm = name.rstrip(b"\x00").decode("latin1")
        if nm == "@comp.id":
            out.append(hex(struct.unpack_from("<I", d, off + 8)[0]))
    return out


for p in sys.argv[1:]:
    print(p, "comp.id:", *compid(p))
