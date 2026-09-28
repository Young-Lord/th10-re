# Script label map loader (0x0043e9d0)

Module: `src/ScriptMapLoader.cpp`

Native ESI = script record, stack = script path. Copies the path, rewrites
the last three characters to `map`, and when `DoesMainChainFileExist`
accepts it, loads the file (mode 1) into record +0x12c and rebases every
dword of the first `+0x110 + +0x114` entries against the buffer base
(pointer-table relocation). When the map file is missing, +0x12c is zeroed.
