#!/usr/bin/env bash
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
original_object="$repo_root/build/objdiff/orig/GlobalManagerDisableChainsCallback.obj"
target_object="$repo_root/build/strict/GlobalManagerDisableChainsCallback.obj"
report="$repo_root/build/objdiff/global-manager-disable-chains-callback.json"

test -f "$original_object" || {
    printf 'Missing %s; export Ghidra objects first.\n' "$original_object" >&2
    exit 1
}

mkdir -p "$repo_root/build/strict" "$repo_root/build/objdiff"
cd "$repo_root"
nasm -f win32 src/GlobalManagerDisableChainsCallback.asm -o "$target_object"
/home/niko/.local/share/th10-re/objdiff/objdiff-cli diff \
    -1 "$target_object" \
    -2 "$original_object" \
    --relax-reloc-diffs \
    -o "$report"
printf 'Wrote %s\n' "$report"
