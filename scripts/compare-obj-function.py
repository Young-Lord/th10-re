#!/usr/bin/env python3
"""Relocation-aware byte compare of a function in a MSVC COFF object against
the target image.

Usage:
  python3 scripts/compare-obj-function.py <file.obj> <mangled-symbol> <target-addr> [size]

If size is omitted it is taken from config/adopt-plan.csv (their_size) or
falls back to the run of non-padding bytes. Non-relocation bytes must match
exactly; relocation sites are checked separately (the obj holds placeholders,
the image holds resolved addresses — report both values for human review).

COFF layout notes (why the offsets are what they are):
  file header at 0:   Machine<u16> NSections<u16> TimeStamp<u32>
                      SymPtr<u32> NSyms<u32> OptHdrSize<u16> Chars<u16>
  section header:     Name[8] VirtualSize@+8 VirtualAddress@+12
                      RawSize@+16 RawPtr@+20 RelPtr@+24 ... NRelocs@+32
  symbol record:      Name[8] (or /4-byte zero + string-table offset)
                      Value<u32>@+8 SectionNumber<u16>@+12
  relocation:         Point<u32> SymIdx<u32> Type<u16>, 10 bytes each
"""
import csv
import glob
import os
import struct
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXE = os.path.join(REPO, "resources", "th10.exe")
TEXT_VMA = 0x401000
TEXT_FILEOFF = 0x400  # .text raw pointer in th10.exe


def read_ref(addr: int, size: int) -> bytes:
    exe = open(EXE, "rb").read()
    off = addr - TEXT_VMA + TEXT_FILEOFF
    return exe[off:off + size]


def load_obj_symbols(obj: bytes):
    machine, nsec, ts, symptr, nsym, optsz, chars = struct.unpack_from("<HHIIIHH", obj, 0)
    secs = []
    for i in range(nsec):
        o = 20 + optsz + 40 * i
        name = obj[o:o + 8].rstrip(b"\x00").decode("latin1")
        vsz, va, rsz, ptr, relptr, nln, nrel = struct.unpack_from("<IIIIIIH", obj, o + 8)
        secs.append({"name": name, "ptr": ptr, "size": rsz, "relptr": relptr, "nrel": nrel})
    strtab_off = symptr + nsym * 18
    syms = {}
    i = 0
    while i < nsym:
        o = symptr + i * 18
        rec = obj[o:o + 18]
        if rec[:4] == b"\x00\x00\x00\x00":
            soff = struct.unpack_from("<I", rec, 4)[0]
            end = obj.find(b"\x00", strtab_off + soff)
            name = obj[strtab_off + soff:end].decode("latin1")
        else:
            name = rec[:8].rstrip(b"\x00").decode("latin1")
        aux = rec[16]
        if aux == 0:
            value, secnum = struct.unpack_from("<IH", rec, 8)
            syms[name] = (secnum, value)
        i += 1 + aux
    return secs, syms


def main() -> int:
    if len(sys.argv) < 4:
        print(__doc__)
        return 2
    obj_path, symbol, addr_s = sys.argv[1], sys.argv[2], sys.argv[3]
    addr = int(addr_s, 16)
    size = int(sys.argv[4], 0) if len(sys.argv) > 4 else None
    if size is None:
        with open(os.path.join(REPO, "config", "adopt-plan.csv"), encoding="utf-8") as fh:
            for r in csv.DictReader(fh):
                if int(r["address"], 16) == addr:
                    size = int(r["their_size"])
                    break
    if size is None:
        raise SystemExit(f"no size for {addr_s}: pass it or add to adopt-plan.csv")

    obj = open(obj_path, "rb").read()
    secs, syms = load_obj_symbols(obj)
    if symbol not in syms:
        raise SystemExit(f"symbol {symbol!r} not in {obj_path}")
    secnum, value = syms[symbol]
    sec = secs[secnum - 1]
    code = obj[sec["ptr"] + value: sec["ptr"] + value + size]

    relocs = set()
    for i in range(sec["nrel"]):
        point, symidx, typ = struct.unpack_from("<IIH", obj, sec["relptr"] + 10 * i)
        if value <= point < value + size:
            relocs.add(point - value)

    ref = read_ref(addr, size)
    diffs = [(i, code[i], ref[i]) for i in range(size)
             if i not in relocs and code[i] != ref[i]]

    print(f"{symbol} @ 0x{addr:08X} ({size}B) in {obj_path}")
    print(f"  section {secnum} value +0x{value:X}, relocation sites: "
          f"{sorted(relocs) if relocs else 'none'}")
    if diffs:
        print(f"  MISMATCH: {len(diffs)} non-relocation byte(s) differ:")
        for i, got, want in diffs[:12]:
            print(f"    +0x{i:02X}: obj={got:02X} ref={want:02X}")
        return 1
    print("  MATCH: all non-relocation bytes identical")
    if relocs:
        print("  relocation sites (obj placeholder -> image value):")
        for i in sorted(relocs):
            print(f"    +0x{i:02X}: {code[i:i+4].hex(' ')} -> {ref[i:i+4].hex(' ')}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
