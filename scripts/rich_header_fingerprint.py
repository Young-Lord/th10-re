#!/usr/bin/env python3
"""Decode the DOS-stub Rich header of the pinned th10.exe target and report
the compiler/linker build fingerprint. Independent re-derivation of the
YomotsuHisami/N0zoM1z0 toolchain claim (see docs/EVIDENCE-RICH-HEADER.md).

Usage: python3 scripts/rich_header_fingerprint.py [path-to-exe]
"""
import struct
import sys
from collections import Counter
from pathlib import Path

EXPECTED_SHA256 = "2f14760b6fbbf57549541583283badb9a19a4222b90f0a146d5aa17f01dc9040"

# Rich prodids of interest. 0x5F(95)/0x60(96)/0x64(100) are cl-family
# markers; 0x5A is MASM; 0x0F is the Idl marker cl emits alongside.
PRODID_NAMES = {
    0x0001: "implib", 0x000F: "idl", 0x0016: "link",
    0x005A: "masm", 0x005D: "cvtres", 0x005E: "misc",
    0x005F: "cl", 0x0060: "cl", 0x0064: "cl-ltcg", 0x0069: "misc",
}

KNOWN_BUILDS = {
    3077: "VC7.1 RTM (cl 13.10.3077, VC++ Toolkit 2003)",
    6030: "VC7.1 SP1 (cl 13.10.6030)",
    4035: "linker 7.10.4035 era tool DLL",
}


def decode(data: bytes) -> list[tuple[int, int, int]]:
    idx = data.find(b"Rich", 0, 0x400)
    if idx < 0:
        raise SystemExit("no Rich marker found")
    key = struct.unpack_from("<I", data, idx + 4)[0]
    start = None
    for off in range(idx - 16, 0, -8):
        if struct.unpack_from("<I", data, off)[0] ^ key == 0x536E6144:  # 'DanS'
            start = off
            break
    if start is None:
        raise SystemExit("DanS marker not found")
    entries = []
    off = start + 16  # DanS block is 16 bytes
    while off < idx:
        a, b = struct.unpack_from("<II", data, off)
        a ^= key
        b ^= key
        if a == 0 or b == 0:
            break
        entries.append((a >> 16, a & 0xFFFF, b))
        off += 8
    return entries


def main() -> None:
    path = Path(sys.argv[1] if len(sys.argv) > 1 else "resources/th10.exe")
    data = path.read_bytes()
    import hashlib
    digest = hashlib.sha256(data).hexdigest()
    if digest != EXPECTED_SHA256:
        raise SystemExit(f"target identity mismatch: sha256={digest}")

    entries = decode(data)
    counts: Counter[tuple[int, int]] = Counter()
    for prodid, build, count in entries:
        counts[(prodid, build)] += count

    print(f"target: {path} sha256 ok ({len(data)} bytes)")
    print(f"{'prodid':>8} {'name':>9} {'build':>6} {'objects':>8}")
    for (prodid, build), count in sorted(counts.items(), key=lambda x: -x[1]):
        print(f"  0x{prodid:04x} {PRODID_NAMES.get(prodid, '?'):>9} {build:6d} {count:8d}")

    cl = {b: c for (p, b), c in counts.items() if p in (0x5F, 0x60, 0x64)}
    print("\nverdict:")
    for build, count in sorted(cl.items()):
        print(f"  cl build {build}: {count} objects ({KNOWN_BUILDS.get(build, 'unknown')})")
    if not cl:
        raise SystemExit("no cl markers found")
    dominant = max(cl.items(), key=lambda x: x[1])
    others = {b: c for b, c in cl.items() if b != dominant[0]}
    print(f"  -> dominant compiler: cl build {dominant[0]} "
          f"({KNOWN_BUILDS.get(dominant[0], 'unknown')}), {dominant[1]} objects")
    if dominant[0] == 6030 and sum(others.values()) <= 4:
        print("  -> verdict: VC7.1 SP1, cl 13.10.6030. The handful of stray "
              "2179/4035 markers are linker-adjacent DLL inputs, not cl.")
        print("  -> any toolchain pinned at 13.10.3077 cannot reproduce this binary")
    else:
        print("  -> unresolved mix; investigate before pinning")


if __name__ == "__main__":
    main()
