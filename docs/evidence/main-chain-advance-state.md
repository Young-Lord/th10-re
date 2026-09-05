# Main Chain Advance State Evidence

`0x004218d0` receives `g_MainChainContext` in `EAX`. It compares
`previous_state` (`+0x38c`) with `requested_state` (`+0x390`) and returns one
without locking when equal. Otherwise it enters the critical section at
`+0x6c4`, increments `state_update_depth` (`+0x6f9`), saves the previous state
to `+0x394`, and initializes `transition_color` (`+0x780`) to `0xff000000`.

The requested state is dispatched through the table at `0x00421b0c`; several
transitions also use a secondary table at `0x00421b4c` and selector bytes at
`0x00421b5c`. Before normal return, the function writes the requested state
back to `previous_state`, leaves the critical section, decrements update depth,
and returns one. The failure path after `0x0041fd00` returns four.

Function, global, and field names are behavioral. Exact jump-table relocations
and all called function ABIs must be verified from the exported COFF before a
strict reconstruction is added.
