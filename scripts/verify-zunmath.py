#!/usr/bin/env python3
"""Standalone ZunMath 0x41F800 verification: symbol lookup + byte compare.

Self-contained on purpose: parses the COFF object directly and prints every
intermediate (sections, symbol keys, bytes) so a mismatch is diagnosable from
one run. No shell pipelines involved.
"""
import struct

OBJ = "build/rewrite/ZunMath.obj"
SYM = "?SetVectorFromAngle@th10@@YIXPAUFloat2@1@MM@Z"
ADDR, SIZE = 0x41F800, 28

obj = open(OBJ, "rb").read()
machine, nsec, ts, symptr, nsym, optsz, chars = struct.unpack_from("<HHIIIHH", obj, 0)
print(f"sections={nsec} syms={nsym} symptr=0x{symptr:x}")

strtab = symptr + nsym * 18
hit = None
i = 0
while i < nsym:
    rec = obj[symptr + i * 18: symptr + i * 18 + 18]
    aux = rec[17]  # NumberOfAuxSymbols; rec[16] is StorageClass
    if rec[:4] == b"\x00\x00\x00\x00":
        soff = struct.unpack_from("<I", rec, 4)[0]
        end = obj.find(b"\x00", strtab + soff)
        name = obj[strtab + soff:end].decode("latin1", "replace")
    else:
        name = rec[:8].rstrip(b"\x00").decode("latin1", "replace")
    if "SetVector" in name or name == SYM:
        val, sec = struct.unpack_from("<IH", rec, 8)
        print(f"  sym[{i}] {name!r} sec={sec} val=0x{val:x} aux={aux}")
        if name == SYM:
            hit = (sec, val)
    i += 1 + aux

if hit is None:
    raise SystemExit("SYMBOL NOT FOUND — dump above shows what the obj has")

sec, val = hit
# section header table
so = 20 + optsz + 40 * (sec - 1)
name = obj[so:so + 8].rstrip(b"\x00").decode("latin1", "replace")
rsz, ptr, relptr = struct.unpack_from("<III", obj, so + 16)
nrel = struct.unpack_from("<H", obj, so + 32)[0]
code = obj[ptr + val: ptr + val + SIZE]
relocs = set()
for k in range(nrel):
    point, symidx, typ = struct.unpack_from("<IIH", obj, relptr + 10 * k)
    if val <= point < val + SIZE:
        relocs.add(point - val)

exe = open("resources/th10.exe", "rb").read()
ref = exe[ADDR - 0x401000 + 0x400: ADDR - 0x401000 + 0x400 + SIZE]

print(f"section '{name}' rawptr=0x{ptr:x} size={rsz} nrel={nrel}")
print(f"reloc sites in body: {sorted(relocs) if relocs else 'none'}")
print("obj:", code.hex(" "))
print("ref:", ref.hex(" "))
diffs = [(k, code[k], ref[k]) for k in range(SIZE) if k not in relocs and code[k] != ref[k]]
if diffs:
    print(f"MISMATCH: {len(diffs)} non-reloc byte(s): {diffs}")
else:
    print("MATCH: all non-relocation bytes identical")
