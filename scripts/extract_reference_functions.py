#!/usr/bin/env python3
"""Extract per-function reference artifacts from the original th10.exe.

The binary has no symbols, so function boundaries are derived from the
MSVC layout: functions are separated by runs of `int3` padding. The full
`objdump -d` listing is split into instruction groups (a new group starts
at the first non-int3 instruction after an int3 run, and at .text start),
and for every address registered in config/function-status.csv that
begins a group, writes:

  build/reference/<address>_<name>.asm  — objdump disassembly of the group
  build/reference/<address>_<name>.bin  — the raw function bytes

Addresses that do not begin a group (adjacent functions without padding)
are reported to stdout so they can be revisited.
"""
import csv
import os
import re
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXE = os.path.join(REPO, "resources", "th10.exe")
CSV_PATH = os.path.join(REPO, "config", "function-status.csv")
OUT_DIR = os.path.join(REPO, "build", "reference")

# .text: VMA 0x401000, file offset 0x400 (see `objdump -h`).
TEXT_VMA = 0x401000
TEXT_FILEOFF = 0x400

INSN = re.compile(r"^\s*([0-9a-f]+):\t(?:[0-9a-f]{2} )+\s*\t*(.*)$")


def load_implemented():
    entries = []
    with open(CSV_PATH) as f:
        for row in csv.reader(f):
            if len(row) >= 4 and row[0].startswith("0x") and row[3] == "implemented":
                entries.append((int(row[0], 16), row[1].strip()))
    return entries


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    disasm = subprocess.run(
        ["objdump", "-d", "-M", "intel", EXE],
        capture_output=True, text=True, check=True).stdout

    # Parse instruction lines; split into groups on int3 runs.
    groups = []  # list of (start_va, [line, ...])
    current = None
    prev_int3 = True  # .text starts fresh
    for line in disasm.splitlines():
        m = INSN.match(line)
        if not m:
            continue
        addr = int(m.group(1), 16)
        text = m.group(2).strip()
        is_pad = text.startswith("int3")
        if is_pad:
            prev_int3 = True
            continue
        if prev_int3 or current is None:
            current = (addr, [])
            groups.append(current)
        current[1].append(line)
        prev_int3 = False
    if not groups:
        sys.exit("no instruction groups parsed from objdump output")

    group_starts = [g[0] for g in groups]

    with open(EXE, "rb") as f:
        image = f.read()

    def va_to_off(va):
        return va - TEXT_VMA + TEXT_FILEOFF

    implemented = load_implemented()
    by_va = {}
    for va, name in implemented:
        by_va.setdefault(va, name)

    # Sub-split: implemented addresses that sit inside a padded group
    # (adjacent functions without int3 padding) become intra-group splits,
    # so their reference bytes start exactly at the function entry.
    splits = set(group_starts) | set(by_va.keys())
    splits = sorted(splits)
    all_lines = []
    for _, lines in groups:
        all_lines.extend(lines)
    # Map each instruction line to its address once.
    line_addrs = []
    for line in all_lines:
        m = INSN.match(line)
        line_addrs.append(int(m.group(1), 16) if m else 0)

    def addr_at(index):
        # Walk back over padding-free boundaries; groups contain no int3
        # lines, so every line has a valid address.
        return line_addrs[index]

    import bisect
    written = 0
    for idx, va in enumerate(splits):
        name = by_va.get(va)
        if name is None:
            continue
        end_va = splits[idx + 1] if idx + 1 < len(splits) else va + 0x10000
        # Slice the instruction lines belonging to [va, end_va).
        start_i = bisect.bisect_left(line_addrs, va)
        end_i = bisect.bisect_left(line_addrs, end_va)
        body = all_lines[start_i:end_i]
        if not body:
            continue
        safe = re.sub(r"[^A-Za-z0-9_.+-]", "_", name)[:80]
        with open(os.path.join(OUT_DIR, "%08x_%s.asm" % (va, safe)), "w") as f:
            f.write("\n".join(body) + "\n")
        off = va_to_off(va)
        size = max(0, end_va - va)
        with open(os.path.join(OUT_DIR, "%08x_%s.bin" % (va, safe)), "wb") as f:
            f.write(image[off:off + size])
        written += 1

    print("instruction groups: %d, split points: %d, reference functions"
          " written: %d" % (len(groups), len(splits), written))
    uncovered = sum(1 for va in by_va if va not in set(splits))
    if uncovered:
        print("implemented addresses without any group coverage (%d):"
              % uncovered)
        for va in sorted(by_va):
            if va not in set(splits):
                print("  %08x %s" % (va, by_va[va]))


if __name__ == "__main__":
    main()
