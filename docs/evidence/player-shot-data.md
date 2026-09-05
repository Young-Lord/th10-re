# TH10 0x00404f30 / 0x0043e710 / 0x00426520 — Player VM Init And Shot Data

Module: `src/PlayerShotData.cpp`. The register ABIs (ESI/ECX/EAX/EBX)
remain thunk boundaries; the bodies are semantic C++.

## 0x00404f30 InitializePlayerMainVmEsiStackAbi

ResetAsciiAnimationVmRecord(vm); zeroes the nine scratch dwords
(`0x340/0x344/0x348/0x334/0x338/0x33c/0x34c/0x350/0x354`); sets
`vm+0x3a0 = vm+0x3a1 = 0x10`; publishes the u16 script index at
`vm+0x38a`; then binds the script through 0x0043e710.

## 0x0043e710 AssignAnmScriptToVmEcxEaxBbxAbi

`script = ((void**)manager[0x11c])[index]`. When the script is missing or
the manager stop flag `manager+0x124` is set, the whole 0x3ac-byte VM
record is wiped with zeros (including fields the reset preserves).
Otherwise: reset again, publish `vm+0x38a = index` and
`vm+0x386 = *(u16*)manager`, back-pointer `vm+0x308 = manager`, clear
flags 9|10 of `vm+0x35c`, set both instruction pointers `vm+0x38c/0x390`
to the script, run the 0x5c timer-block lazy init whose sentinel write is
immediately overwritten by `0xffffffff` (only the flag bit and the rate
pointer survive), clear flag bit 0, run the script interpreter
(`0x43ee30`, ABI still a boundary), and bump the global live-VM counter at
`g_MainChainRenderOwner+0x4c`.

## 0x00426520 LoadPlayerShotDataEsiEaxAbi

Loads the entry file through `LoadPackedResource` (0x0044b360 analog) into
`player+0x45c`; -1 when it fails. Scales the two unit vectors
(`buf+0x10/0x14`) by `sin(pi/4)` from the double constant at `0x470c50`
into `buf+0x18/0x1c` (the native performs each write twice with identical
values; one pass is modeled). Then for each of the `*(u16*)(buf+2)` record
lists, the slot at `buf+0x110 + i*8` is rebased (`*slot += buf`) and its
0x34-stride record chain walked while the signed lead byte is non-negative,
resolving the four handler index tables `0x47476c/0x474778/0x491bf4/
0x491bf8` into `node+0x24/0x28/0x2c/0x30` (no bounds checks — preserved).

## Wiring

`PlayerObjectLifecycle.cpp` now calls these semantic bodies directly;
the extern boundaries for `0x404f30`/`0x426520` are gone. The VM script
interpreter `0x43ee30` remains an extern (one boundary shared by the
dispatcher and this module).
