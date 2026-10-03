#!/usr/bin/env python3
"""Reconcile our function ledger against N0zoM1z0/th10's campaign ledger.

Both projects target the same binary (sha256 2f14760b...dc9040), so ledgers
join on normalized addresses. Produces:

  config/merge-ledger.csv   one row per address with merge flags
  stdout summary            bucket counts feeding docs/LEDGER-RECONCILE.md

Buckets (flag column):
  BOTH_MATCHED           our object-matched and their exact
  ADOPT_EXACT            our implemented + their exact(authored): adopt their verified unit
  VERIFY_OURS            our implemented, their side not exact: verify ourselves
  CONFLICT_EXCLUDED      our implemented but their status=excluded: arbitrate
  IMPORT_EXACT           their exact(authored) only: import into our tree
  THEIR_DRAFT_ONLY       their non-exact authored draft only: optional import
  LIB_FREE               library/compiler/thunk functions: come from linked libs
  UNSOURCED              game function present in neither ledger
  BOUNDARY_MISMATCH      both ledgers have the address but sizes disagree

Usage: python3 scripts/reconcile-ledgers.py [--theirs /tmp/th10-n0zom1z0]
"""
from __future__ import annotations

import argparse
import csv
from collections import Counter
from pathlib import Path


def norm(addr: str) -> int | None:
    try:
        return int(addr, 16)
    except (ValueError, TypeError):
        return None


def read_csv(path: Path) -> list[dict]:
    with path.open(encoding="utf-8-sig", newline="") as fh:
        return list(csv.DictReader(fh))


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--theirs", default="/tmp/th10-n0zom1z0")
    ap.add_argument("--out", default="config/merge-ledger.csv")
    args = ap.parse_args()
    theirs_dir = Path(args.theirs)

    ours = {}
    for r in read_csv(Path("config/function-status.csv")):
        a = norm(r["address"])
        if a is not None:
            ours[a] = r
    crt = {}
    for r in read_csv(Path("config/crt-lib-map.csv")):
        a = norm(r["address"])
        if a is not None:
            crt[a] = r
    theirs = {}
    for r in read_csv(theirs_dir / "config" / "functions.csv"):
        a = norm(r["address"])
        if a is not None:
            theirs[a] = r

    rows = []
    all_addrs = sorted(set(ours) | set(theirs) | set(crt))
    for a in all_addrs:
        o = ours.get(a)
        t = theirs.get(a)
        c = crt.get(a, {})
        kind = c.get("kind", "")
        size_crt = int(c["size"], 0) if c.get("size") not in (None, "") else None
        size_t = int(t["size"], 0) if t and t.get("size") not in (None, "") else None
        our_status = o["status"] if o else ""
        their_status = t["status"] if t else ""
        their_owner = t["owner"] if t else ""
        their_exact = their_status == "exact"
        their_authored = their_owner == "authored"

        flag = ""
        if o and t and our_status == "object-matched" and their_exact:
            flag = "BOTH_MATCHED"
        elif o and t and their_exact and their_authored:
            flag = "ADOPT_EXACT"
        elif o and t and our_status == "implemented" and their_status == "excluded":
            flag = "CONFLICT_EXCLUDED"
        elif o and t and our_status in ("implemented", "object-matched"):
            flag = "VERIFY_OURS"
        elif t and not o and their_exact and their_authored:
            flag = "IMPORT_EXACT"
        elif t and not o and not their_exact and their_authored:
            flag = "THEIR_DRAFT_ONLY"
        elif not o and not t and kind == "game":
            flag = "UNSOURCED"
        elif t and not o:
            flag = "LIB_FREE"
        elif kind in ("library", "thunk") and not o and not t:
            flag = "LIB_FREE"

        boundary = "" if size_crt is None or size_t is None else (
            "" if size_crt == size_t else f"crt={size_crt} theirs={size_t}")
        if boundary and flag not in ("UNSOURCED", "LIB_FREE"):
            flag = (flag + "+BOUNDARY_MISMATCH") if flag else "BOUNDARY_MISMATCH"

        rows.append({
            "address": f"0x{a:08x}",
            "kind": kind,
            "our_status": our_status,
            "our_name": o["name"] if o else c.get("name", ""),
            "their_status": their_status,
            "their_owner": their_owner,
            "their_proposed_name": t["proposed_name"] if t else "",
            "their_module": t["module"] if t else "",
            "their_source_file": t["source_file"] if t else "",
            "their_match_percent": t["match_percent"] if t else "",
            "size_theirs": size_t or "",
            "flag": flag,
            "boundary_note": boundary,
        })

    out = Path(args.out)
    with out.open("w", encoding="utf-8", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=list(rows[0].keys()))
        w.writeheader()
        w.writerows(rows)

    game = [r for r in rows if r["kind"] == "game"]
    print(f"wrote {out} ({len(rows)} rows)\n")
    print("全部行 flag 分布:")
    for k, v in Counter(r["flag"] for r in rows).most_common():
        print(f"  {k or '(none)'}: {v}")
    print(f"\n游戏函数 (kind=game) 桶分布, 共 {len(game)} 个:")
    for k, v in Counter(r["flag"] for r in game).most_common():
        print(f"  {k or '(none)'}: {v}")
    mm = [r for r in rows if "BOUNDARY_MISMATCH" in r["flag"]]
    print(f"\n边界不一致: {len(mm)} 个 (前 10)")
    for r in mm[:10]:
        print(f"  {r['address']}  {r['boundary_note']}  ours={r['our_name'][:40]} theirs={r['their_proposed_name'][:40]}")


if __name__ == "__main__":
    main()
