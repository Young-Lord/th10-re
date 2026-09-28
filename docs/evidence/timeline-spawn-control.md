# Timeline stage-effect spawns (0x00448e80-0x00449590)

Module: `src/TimelineSpawnControl.cpp`

All six spawn variants: allocate the preset node through
`AllocatePoolVmEsiAbi` (0x00449950, ESI = owner, node in EAX), set the
0x40000000 flag of +0x35c, zero +0x20, seed the position triple at
+0x340..+0x348 (the positioned twins add 224.0f from 0x00470b4c and 16.0f
from 0x00470b48 first), bind the ANM script through
`AssignAnmScriptToVmEcxEaxBbxAbi` (ECX = ANM work, EAX = node, EBX =
script index) and link:

| address | link | position |
|---|---|---|
| 0x00448e80 | list-A front (0x00448a50) | raw triple |
| 0x00448ee0 | list-A front | +224/+16 |
| 0x00448fb0 | list B (0x00448ac0) | raw triple |
| 0x00449010 | list B | +224/+16 |
| 0x004490e0 | list-B front (0x00448b40) | raw triple |
| 0x00449140 | list-B front | +224/+16 |

`0x00449590 StopTimelineEntityTreeEaxAbi`: resolves the timeline handle
(0x004491c0, registered), sets bit 2 of +0x35c and, when the child-list
head (slot 6) is empty, walks slot 5 setting bit 2 on every child.
