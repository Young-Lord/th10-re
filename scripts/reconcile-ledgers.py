#!/usr/bin/env python3
"""Reconcile our function ledger against N0zoM1z0/th10's campaign ledger.

Both projects target the same binary (sha256 2f14760b...dc9040), so the ledgers
join on normalized addresses. Produces:

  config/merge-ledger.csv   one row per address with a merge flag
  stdout summary            bucket counts/bytes feeding docs/LEDGER-RECONCILE.md

Buckets, for kind=game (see docs/LEDGER-RECONCILE.md for the adjudication):
  BOTH_VERIFIED        ours object-matched and theirs exact
  ADOPT_EXACT          ours implemented + their authored exact: adopt their unit
  VERIFY_OURS          ours implemented/object-matched, theirs not exact
                       (includes addresses they never ledgered at all)
  COMPILER_EMITTED     MSVC emits these from correct source (scalar-deleting
                       destructors, eh vector dtor iterators). Do not author;
                       they fall out of the surrounding class.
  HEADER_INSTANTIATED  STL template instantiations (std::basic_string members)
                       emitted into our objects from <string>. Not hand-written
                       and not linked from a lib: they need the right source
                       shape plus the right compiler.
  IMPORT_EXACT         their authored exact only: import into our tree
  IMPORT_LEAF          their exact only, origin never classified. Overwhelmingly
                       generated leaf accessors, a few bytes each: cheap to take
                       but near-zero verification value.
  THEIR_DRAFT_ONLY     their non-exact authored draft only: optional import
  UNSOURCED            game function present in neither ledger
  NO_IDA_ENTRY         in our ledger but not an IDA function entry (alias/gadget)

For kind in (library, thunk): LIB_LINKED -- obtained by linking the correct
CRT/DX libraries, never rebuilt.

Byte columns use `owned_size` from config/crt-lib-map.csv, which includes the
trailing alignment + jump table a byte-exact rebuild must reproduce
(scripts/audit-crt-map.py).

Usage: python3 scripts/reconcile-ledgers.py [--theirs /tmp/th10-n0zom1z0]
"""
from __future__ import annotations

import argparse
import csv
from collections import Counter, defaultdict
from pathlib import Path

COMPILER_OWNERS = {"compiler", "compiler_generated"}
GAME_ORDER = [
    "BOTH_VERIFIED", "ADOPT_EXACT", "VERIFY_OURS", "COMPILER_EMITTED",
    "HEADER_INSTANTIATED", "IMPORT_EXACT", "IMPORT_LEAF", "THEIR_DRAFT_ONLY",
    "UNSOURCED",
]


def norm(addr: str) -> int | None:
    try:
        return int(addr, 16)
    except (ValueError, TypeError):
        return None


def read_csv(path: Path) -> list[dict]:
    with path.open(encoding="utf-8-sig", newline="") as fh:
        return list(csv.DictReader(fh))


def classify(o, t, kind, their_status, their_owner) -> str:
    """Return the merge bucket for one address."""
    exact = their_status == "exact"
    authored = their_owner == "authored"
    our_status = o["status"] if o else ""

    if kind in ("library", "thunk"):
        return "LIB_LINKED"
    if not kind:
        return "NO_IDA_ENTRY" if o else "UNSOURCED"

    # Origin classes that are never hand-authored, whichever ledger saw them.
    if t and their_owner in COMPILER_OWNERS:
        return "COMPILER_EMITTED"
    if t and their_owner == "library":
        # kind=game survived scripts/audit-crt-map.py, so this is not CRT:
        # it is an STL template instantiation landing in our objects.
        return "HEADER_INSTANTIATED"

    if o:
        if our_status == "object-matched" and exact:
            return "BOTH_VERIFIED"
        if exact and authored:
            return "ADOPT_EXACT"
        if our_status in ("implemented", "object-matched", "boundary"):
            return "VERIFY_OURS"
        return "VERIFY_OURS"
    if t:
        if exact and authored:
            return "IMPORT_EXACT"
        if exact:
            return "IMPORT_LEAF"
        if authored:
            return "THEIR_DRAFT_ONLY"
        return "UNSOURCED"
    return "UNSOURCED"


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--theirs", default="/tmp/th10-n0zom1z0")
    ap.add_argument("--out", default="config/merge-ledger.csv")
    args = ap.parse_args()

    ours = {a: r for r in read_csv(Path("config/function-status.csv"))
            if (a := norm(r["address"])) is not None}
    crt = {a: r for r in read_csv(Path("config/crt-lib-map.csv"))
           if (a := norm(r["address"])) is not None}
    theirs_path = Path(args.theirs) / "config" / "functions.csv"
    if not theirs_path.exists():
        raise SystemExit(f"{theirs_path} not found; pass --theirs <clone>")
    theirs = {a: r for r in read_csv(theirs_path)
              if (a := norm(r["address"])) is not None}

    rows = []
    for a in sorted(set(ours) | set(theirs) | set(crt)):
        o, t, c = ours.get(a), theirs.get(a), crt.get(a, {})
        kind = c.get("kind", "")
        their_status = t["status"] if t else ""
        their_owner = (t["owner"] if t else "") or ""
        # One of their rows has shifted fields, spilling notes prose into owner.
        if len(their_owner) > 40:
            their_owner = ""
        flag = classify(o, t, kind, their_status, their_owner)

        code = int(c["size"]) if c.get("size") else None
        owned = int(c["owned_size"]) if c.get("owned_size") else code
        size_t = int(t["size"]) if t and t.get("size") else None
        # Compare their reviewed extent against our physical-ownership extent,
        # not against IDA's code-only size: the tail-table gap is accounted for.
        boundary = ""
        if owned is not None and size_t is not None and owned != size_t:
            boundary = f"ours={owned} theirs={size_t} d={size_t - owned:+d}"

        rows.append({
            "address": f"0x{a:08x}",
            "kind": kind,
            "code_size": code if code is not None else "",
            "owned_size": owned if owned is not None else "",
            "our_status": o["status"] if o else "",
            "our_name": o["name"] if o else c.get("name", ""),
            "their_status": their_status,
            "their_owner": their_owner,
            "their_proposed_name": t["proposed_name"] if t else "",
            "their_module": t["module"] if t else "",
            "their_source_file": t["source_file"] if t else "",
            "their_match_percent": t["match_percent"] if t else "",
            "size_theirs": size_t if size_t is not None else "",
            "flag": flag,
            "boundary_note": boundary,
        })

    out = Path(args.out)
    with out.open("w", encoding="utf-8", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=list(rows[0].keys()))
        w.writeheader()
        w.writerows(rows)
    print(f"wrote {out} ({len(rows)} rows)\n")

    game = [r for r in rows if r["kind"] == "game"]
    gbytes = sum(r["owned_size"] or 0 for r in game)
    n = Counter(r["flag"] for r in game)
    b = defaultdict(int)
    for r in game:
        b[r["flag"]] += r["owned_size"] or 0
    print(f"game functions: {len(game)} / {gbytes} bytes")
    print(f"{'bucket':<22}{'funcs':>7}{'bytes':>10}{'byte share':>12}")
    for k in GAME_ORDER + sorted(set(n) - set(GAME_ORDER)):
        if k in n:
            print(f"{k:<22}{n[k]:>7}{b[k]:>10}{100*b[k]/gbytes:>11.1f}%")

    other = Counter(r["flag"] for r in rows if r["kind"] != "game")
    print("\nnon-game rows: " + ", ".join(f"{k}={v}" for k, v in other.most_common()))

    mm = [r for r in rows if r["boundary_note"]]
    tails = {}
    tp = Path("config/tail-tables.csv")
    if tp.exists():
        tails = {r["address"]: int(r["tail_bytes"]) for r in read_csv(tp)}
    print(f"\nresidual extent disagreements: {len(mm)}")
    cause = Counter()
    for r in mm:
        d = int(r["owned_size"]) - int(r["size_theirs"])
        if r["kind"] == "game" and d == tails.get(r["address"], -1):
            cause["tail table we found, not yet in their ledger"] += 1
        elif r["kind"] == "library" and d > 0:
            cause["library: our COFF body vs their contiguous fragment"] += 1
        else:
            cause[f"unexplained ({r['kind'] or 'no kind'})"] += 1
    for k, v in cause.most_common():
        print(f"  {v:>3}  {k}")
    for r in mm:
        d = int(r["owned_size"]) - int(r["size_theirs"])
        if not (r["kind"] == "game" and d == tails.get(r["address"], -1)) and \
           not (r["kind"] == "library" and d > 0):
            print(f"       {r['address']} {r['kind']:<8}{r['boundary_note']:<28}"
                  f"{(r['their_proposed_name'] or r['our_name'])[:36]}")


if __name__ == "__main__":
    main()
