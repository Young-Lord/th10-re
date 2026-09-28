# Enemy script variable accessors (0x0043ec70, 0x0043eda0)

Module: `src/EnemyScriptVars.cpp`

Id family 10000-10009 over the enemy record:
10000-10003 -> ints +0x30c/+0x310/+0x314/+0x318; 10004-10007 -> floats
+0x31c/+0x320/+0x324/+0x328 converted through the shared `_ftol2` thunk
0x00463b2c (registered boundary; modeled as `static_cast<long long>`);
10008/10009 -> dwords +0x32c/+0x330.

`0x0043eda0` resolves the write address for the same family (0x2710..13 ->
dwords 195..198, 0x2718/19 -> dwords 203/204), gated by the slot bit in
the 16-bit mask; when the bit is clear the id pointer is returned
unchanged.
