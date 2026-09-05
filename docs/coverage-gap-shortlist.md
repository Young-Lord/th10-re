# Coverage Gap Shortlist

Uncovered functions most connected to already-covered code (candidates >=0x100 bytes, ranked by covered callers then total callers).

| Address | Size | Total callers | Covered callers | Hint |
|---|---|---|---|---|
| 0x41BB00 | 793 | 13 | 0x408100 (?), 0x425730 (?) | recursive VM/script runner? calls itself + 0x448DB0 render-object create, 0x404F30 player VM init; 13 callers |
| 0x43BDA0 | 483 | 7 | 0x415800 (?), 0x426360 (?) | calls sub_442F50 (text/ascii stack?); called by 0x415800 RenderAsciiHudBatch and 0x426360 |
| 0x44C350 | 509 | 12 | 0x404610 (?) | calls itself (recursion); called by 0x404610 TickVec3Interpolator; likely string/char formatting for HUD |
| 0x426F70 | 2494 | 6 | 0x425730 (?) | float-heavy: __ftol2 + many D3D/vertex helpers (0x448E30, 0x449950...); called by 0x425730 |
| 0x417C80 | 1105 | 3 | 0x418150 (?) | refs string 'dummy'; calls 0x40C540 StartTimelineContinuation, 0x448D50, free; called by 0x418150 |
| 0x439D20 | 710 | 3 | 0x439890 (?) | self-recursive; called from 0x439890 |
| 0x429B60 | 1681 | 2 | 0x433570 (?) | replay save: strings 'replay/%s','Version %s','Date %.2d/...','Chara %s','Rank %s'; __mkdir,_sprintf |
| 0x43DDF0 | 1419 | 2 | 0x439390 (?) | large engine body calling 0x44D110, 0x44D4E0; called by 0x439390 |
| 0x4250B0 | 1591 | 1 | 0x425730 (?) | float/D3D pipeline calls incl. 0x43E710; called by 0x425730 |
| 0x4269D0 | 796 | 1 | 0x425730 (?) | refs 'Caution!'; calls 0x413790 RefreshLifeIconsEaxStackAbi; called by 0x425730 |
| 0x427E90 | 717 | 1 | 0x428160 (?) | calls 0x44BC70/0x44C5D0; called by 0x428160 |
| 0x401A50 | 570 | 1 | 0x401520 (?) | early-game init called by 0x401520 AsciiManager::OnDrawStrings chain |
| 0x453280 | 829 | 7 | - | calls _memcpy |
| 0x45BFC0 | 829 | 7 | - | calls _memcpy_0 |
| 0x44D110 | 483 | 6 | - | sub_44D110; total callers 6 |
| 0x45D746 | 1039 | 5 | - | calls _isdigit, calls @__security_check_cookie@4 |
| 0x4145F0 | 574 | 5 | - | calls _free, calls ??_M@YGXPAXIHP6EX0@Z@Z |
| 0x40CFB0 | 501 | 5 | - | calls ??2@YAPAXI@Z |
| 0x402440 | 499 | 5 | - | calls _free, calls j__free, calls ??_M@YGXPAXIHP6EX0@Z@Z, calls ___CxxFrameHandler |
| 0x454ED7 | 1946 | 4 | - | calls _write_char, calls _wctomb, calls _malloc, calls __fptrap |
| 0x4465B0 | 1417 | 4 | - | sub_4465B0; total callers 4 |
| 0x40DC80 | 2412 | 3 | - | sub_40DC80; total callers 3 |
| 0x45EE1D | 982 | 3 | - | calls __SEH_prolog, calls __alloca_probe, calls __resetstkoflw, calls _malloc |
| 0x44A190 | 833 | 3 | - | calls @__security_check_cookie@4 |
| 0x4548BE | 764 | 3 | - | calls ___sbh_alloc_block, calls ___sbh_alloc_new_region, calls ___sbh_alloc_new_group |
| 0x45E3A3 | 677 | 3 | - | calls __raise_exc, calls __statfp, calls __clrfp |
| 0x45E648 | 548 | 3 | - | calls __handle_exc, calls __set_statfp, calls __decomp |
| 0x42CB60 | 491 | 3 | - | calls j__free, calls _free |
| 0x415E90 | 4306 | 2 | - | str ' ', str ' ' |
| 0x42F8B0 | 2452 | 2 | - | sub_42F8B0; total callers 2 |
