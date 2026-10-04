#!/usr/bin/env python3
"""Audit config/crt-lib-map.csv against the target image and N0zoM1z0's ledger.

Two independent corrections, both evidence-backed (see docs/LEDGER-RECONCILE.md):

1. Tail tables. IDA's function `size` stops at the last instruction, so the
   compiler-emitted alignment sled + switch jump table + byte selector table
   that physically belong to the function are not counted. A byte-exact decomp
   must reproduce those bytes, so they belong in the denominator. Detected
   directly from the image: after <=7 bytes of alignment there must be >=3
   consecutive little-endian dwords pointing back into the function body.
   Cross-check: on the 24 functions where N0zoM1z0's independently reviewed
   `size` disagrees with ours, this detector reproduces their extent exactly
   24/24.

2. Origin. FLIRT left ~291 functions above 0x450000 unsigned (noted in
   TH10_PROGRESS_METRIC.md 5.2). N0zoM1z0's ledger names 46 of our `kind=game`
   rows as CRT internals (`_realloc`, `_calloc`, `__input`, `__output`,
   `___libm_error_support`, ...) with module=CRT. Structural confirmation:
   those 46 sit next to a known `kind=library` function 91% of the time
   against a 7% base rate for other game functions in the same address span.

Outputs:
  config/tail-tables.csv             one row per detected tail table
  config/crt-origin-corrections.csv  the kind=game -> library reclassifications
  --apply                            rewrite config/crt-lib-map.csv

`--apply` is idempotent and never edits the `size` column: it adds
`tail_bytes` / `owned_size` / `origin_note` so existing `size` readers keep
working, while `owned_size` carries the physical ownership extent.

Usage: python3 scripts/audit-crt-map.py [--theirs /tmp/th10-n0zom1z0] [--apply]
"""
from __future__ import annotations

import argparse
import bisect
import csv
import struct
from pathlib import Path

ALIGN_MAX = 7       # longest alignment sled seen: lea ecx,[ecx+0] + nop
MIN_TABLE = 3       # fewer than 3 in-range dwords is not a switch table
TABLE_MAX = 4096    # sanity cap on how far past the function we look


class Image:
    """Minimal PE reader: virtual address -> file offset."""

    def __init__(self, path: Path):
        self.d = path.read_bytes()
        pe = struct.unpack_from("<I", self.d, 0x3C)[0]
        if self.d[pe:pe + 4] != b"PE\0\0":
            raise SystemExit(f"{path}: not a PE image")
        nsec = struct.unpack_from("<H", self.d, pe + 6)[0]
        optsz = struct.unpack_from("<H", self.d, pe + 20)[0]
        self.base = struct.unpack_from("<I", self.d, pe + 24 + 28)[0]
        self.secs = []
        for i in range(nsec):
            o = pe + 24 + optsz + 40 * i
            vsz, va, rsz, ptr = struct.unpack_from("<IIII", self.d, o + 8)
            self.secs.append((va, max(vsz, rsz), ptr))

    def off(self, va: int) -> int | None:
        r = va - self.base
        for va0, span, ptr in self.secs:
            if va0 <= r < va0 + span:
                return ptr + (r - va0)
        return None

    def read(self, lo: int, hi: int) -> bytes:
        o0, o1 = self.off(lo), self.off(hi)
        return b"" if o0 is None or o1 is None or o1 <= o0 else self.d[o0:o1]


def read_csv(path: Path) -> list[dict]:
    with path.open(encoding="utf-8-sig", newline="") as fh:
        return list(csv.DictReader(fh))


def by_addr(rows: list[dict], key: str = "address") -> dict[int, dict]:
    out = {}
    for r in rows:
        try:
            out[int(r[key], 16)] = r
        except (ValueError, TypeError, KeyError):
            pass
    return out


def find_tail_table(img: Image, fn: int, code_size: int, next_fn: int | None):
    """Return (tail_bytes, align, n_dwords, sel_bytes) or None.

    The dwords must target the *code* body [fn, fn+code_size); a switch table
    never jumps into its own table. The byte selector that MSVC emits after
    the dword table runs until the 0xCC inter-function padding.
    """
    end = fn + code_size
    if next_fn is None or next_fn <= end:
        return None
    blob = img.read(end, min(next_fn, end + TABLE_MAX))
    for align in range(ALIGN_MAX + 1):
        body = blob[align:]
        n = len(body) // 4
        if n < MIN_TABLE:
            continue
        dwords = struct.unpack_from(f"<{n}I", body, 0)
        run = 0
        for v in dwords:
            if fn <= v < end:
                run += 1
            else:
                break
        if run < MIN_TABLE:
            continue
        sel = 0
        for b in body[run * 4:]:
            if b == 0xCC:
                break
            sel += 1
        return align + run * 4 + sel, align, run, sel
    return None


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--theirs", default="/tmp/th10-n0zom1z0")
    ap.add_argument("--image", default="resources/th10.exe")
    ap.add_argument("--map", default="config/crt-lib-map.csv")
    ap.add_argument("--apply", action="store_true", help="rewrite the map in place")
    args = ap.parse_args()

    img = Image(Path(args.image))
    crt_rows = read_csv(Path(args.map))
    crt = by_addr(crt_rows)
    addrs = sorted(crt)
    ours = by_addr(read_csv(Path("config/function-status.csv")))
    theirs_path = Path(args.theirs) / "config" / "functions.csv"
    theirs = by_addr(read_csv(theirs_path)) if theirs_path.exists() else {}
    if not theirs:
        print(f"note: {theirs_path} absent; origin audit will be skipped")

    # ---- 1. origin corrections ------------------------------------------
    # Load previously recorded corrections first and reason against the
    # *pre-correction* view throughout. After --apply those rows read
    # kind=library, so re-deriving them from scratch would find nothing and
    # the adjacency statistic would count the corrected rows as established
    # library neighbours of each other. Carrying the record forward keeps
    # --apply idempotent, keeps the reported numbers stable across runs, and
    # keeps the correction reproducible once the N0zoM1z0 clone is gone.
    rec_path = Path("config/crt-origin-corrections.csv")
    recorded = {r["address"]: r for r in read_csv(rec_path)
                if r.get("address")} if rec_path.exists() else {}
    rec_addrs = {int(k, 16) for k in recorded}

    def base_kind(a: int) -> str:
        """kind as it stood before any origin correction was applied."""
        return "game" if a in rec_addrs else crt[a]["kind"]

    def library_neighbour(a: int) -> bool:
        i = bisect.bisect_left(addrs, a)
        for j in (i - 1, i + 1):
            if 0 <= j < len(addrs) and base_kind(addrs[j]) == "library":
                return True
        return False

    fixes = []
    for a in addrs:
        t = theirs.get(a)
        if (base_kind(a) == "game" and t and t.get("owner") == "library"
                and a not in ours):
            fixes.append({
                "address": f"0x{a:08x}",
                "size": crt[a]["size"],
                "our_name": crt[a]["name"],
                "crt_symbol": t["proposed_name"],
                "their_module": t["module"],
                "their_status": t["status"],
                "library_neighbour": "yes" if library_neighbour(a) else "no",
                "was_kind": "game",
                "now_kind": "library",
            })
    seen = {f["address"] for f in fixes}
    fixes += [r for k, r in recorded.items() if k not in seen]
    fixes.sort(key=lambda f: int(f["address"], 16))
    if fixes:
        with rec_path.open("w", encoding="utf-8", newline="") as fh:
            w = csv.DictWriter(fh, fieldnames=list(fixes[0].keys()))
            w.writeheader()
            w.writerows(fixes)
        picked = {int(f["address"], 16) for f in fixes}
        nb = sum(1 for f in fixes if f["library_neighbour"] == "yes")
        # Control group: the other kind=game functions interleaved in the same
        # address span as the known library run. Comparing against all game
        # functions would stack the deck -- 0x401xxx has no library neighbour
        # to begin with.
        libs = [a for a in addrs if base_kind(a) == "library"]
        lo, hi = min(libs), max(libs)
        others = [a for a in addrs if base_kind(a) == "game"
                  and lo <= a <= hi and a not in picked]
        base = sum(1 for a in others if library_neighbour(a))
        print(f"origin corrections: {len(fixes)} game->library "
              f"({sum(int(f['size']) for f in fixes)} bytes)")
        print(f"  library-adjacent: {nb}/{len(fixes)} = {100*nb/len(fixes):.0f}% "
              f"vs base rate {base}/{len(others)} = {100*base/len(others):.0f}% "
              f"(control: other kind=game in [0x{lo:08x},0x{hi:08x}])")

    # ---- 2. tail-table census -------------------------------------------
    fixed = {int(f["address"], 16) for f in fixes}

    def final_kind(a: int) -> str:
        return "library" if a in fixed else base_kind(a)

    tails = []
    for a in addrs:
        i = bisect.bisect_right(addrs, a)
        nxt = addrs[i] if i < len(addrs) else None
        hit = find_tail_table(img, a, int(crt[a]["size"]), nxt)
        if hit:
            total, align, run, sel = hit
            tails.append({
                "address": f"0x{a:08x}",
                "kind": final_kind(a),
                "name": crt[a]["name"],
                "code_size": crt[a]["size"],
                "align_bytes": align,
                "table_entries": run,
                "selector_bytes": sel,
                "tail_bytes": total,
                "owned_size": int(crt[a]["size"]) + total,
            })

    # Cross-check: on the functions where their independently reviewed extent
    # disagrees with IDA's, does the detector land on exactly their boundary?
    agree = disagree = 0
    for t in tails:
        tr = theirs.get(int(t["address"], 16))
        if not tr or not tr.get("size") or int(tr["size"]) <= int(t["code_size"]):
            continue
        if int(tr["size"]) == t["owned_size"]:
            agree += 1
        else:
            disagree += 1
            print(f"  tail mismatch {t['address']}: ours={t['owned_size']} theirs={tr['size']}")

    with open("config/tail-tables.csv", "w", encoding="utf-8", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=list(tails[0].keys()))
        w.writeheader()
        w.writerows(tails)

    game_tails = [t for t in tails if t["kind"] == "game"]
    print(f"tail tables: {len(tails)} ({sum(t['tail_bytes'] for t in tails)} bytes); "
          f"kind=game {len(game_tails)} ({sum(t['tail_bytes'] for t in game_tails)} bytes)")
    print(f"  cross-check vs N0zoM1z0 reviewed extents: {agree} agree, {disagree} disagree")

    # ---- denominator ----------------------------------------------------
    tail_of = {int(t["address"], 16): t["tail_bytes"] for t in tails}

    def denom(corrected: bool):
        n = b = 0
        for a in addrs:
            kind = final_kind(a) if corrected else base_kind(a)
            if kind != "game":
                continue
            n += 1
            b += int(crt[a]["size"]) + (tail_of.get(a, 0) if corrected else 0)
        return n, b
    n0, b0 = denom(False)
    n1, b1 = denom(True)
    print(f"game denominator: {n0} funcs / {b0} bytes  ->  {n1} funcs / {b1} bytes "
          f"({n1-n0:+d} funcs, {b1-b0:+d} bytes)")

    # ---- 3. apply -------------------------------------------------------
    if not args.apply:
        print("\n(dry run; pass --apply to rewrite the map)")
        return
    fields = ["address", "size", "kind", "name", "tail_bytes", "owned_size", "origin_note"]
    for r in crt_rows:
        a = int(r["address"], 16)
        tb = tail_of.get(a, 0)
        notes = []
        if a in fixed:
            sym = next(f["crt_symbol"] for f in fixes if int(f["address"], 16) == a)
            r["kind"] = "library"
            notes.append(f"origin-corrected game->library: CRT {sym}")
        if tb:
            notes.append(f"+{tb}B tail table (not in IDA size)")
        r["tail_bytes"] = str(tb) if tb else ""
        r["owned_size"] = str(int(r["size"]) + tb) if tb else r["size"]
        r["origin_note"] = "; ".join(notes)
    with open(args.map, "w", encoding="utf-8", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=fields, extrasaction="ignore")
        w.writeheader()
        w.writerows(crt_rows)
    print(f"\nrewrote {args.map}: size column untouched, added "
          f"tail_bytes/owned_size/origin_note")


if __name__ == "__main__":
    main()
