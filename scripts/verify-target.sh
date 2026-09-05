#!/usr/bin/env bash
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
target="$repo_root/resources/th10.exe"
expected=$(sed -n 's/^sha256 = "\([0-9a-f]*\)"$/\1/p' "$repo_root/config/target.toml")

test -n "$expected"
test -f "$target"
actual=$(sha256sum "$target" | awk '{print $1}')
test "$actual" = "$expected"
test "$(stat -c '%s' "$target")" = 487936
objdump -p "$target" | grep -q 'ImageBase[[:space:]]*00400000'
objdump -p "$target" | grep -q 'AddressOfEntryPoint[[:space:]]*000537dc'
objdump -p "$target" | grep -q 'DLL Name: d3d9.dll'
objdump -p "$target" | grep -q 'DLL Name: d3dx9_31.dll'
printf 'TH10 target verification passed: %s\n' "$actual"
