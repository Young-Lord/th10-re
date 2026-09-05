#!/usr/bin/env bash
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
source /home/niko/.local/share/th10-re/env.sh

target_object="$repo_root/build/strict/AsciiManagerStrings.obj"
original_object="$repo_root/build/objdiff/orig/AsciiManagerStrings.obj"
report="$repo_root/build/objdiff/ascii-manager-strings.json"

test -f "$original_object" || {
    printf 'Missing %s; export Ghidra objects first.\n' "$original_object" >&2
    exit 1
}

mkdir -p "$repo_root/build/strict" "$repo_root/build/objdiff"
cd "$repo_root"
th10_cl /nologo /c /TP /I src src/AsciiManager.cpp /Fobuild\\strict\\AsciiManager.obj
nasm -f win32 src/AsciiManagerStrings.asm -o "$target_object"
"$TH10_RE_ROOT/objdiff/objdiff-cli" diff \
    -1 "$target_object" \
    -2 "$original_object" \
    --relax-reloc-diffs \
    -o "$report"
printf 'Wrote %s\n' "$report"
