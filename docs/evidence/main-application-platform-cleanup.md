# Main Application Platform Cleanup Evidence

## Scope

This document covers the straight-line cleanup block in `0x00438ad0` from
`0x00438dda` through `0x00438e86`. The enclosing function also owns retry,
logging, and final-exit branches; those are intentionally outside this scope.

## Ordered Effects

1. Store `2` to `g_MainChainResourceGate` at `0x004977b4`.
2. Stop BGM worker controls for `g_TransitionRoot` through `0x0043cc40`.
3. Fully tear down its sound resources through `0x0043d120`.
4. If `g_MainChainRenderOwner` at `0x00491c10` is non-null, invoke its
   stack-argument in-place destructor `0x00446220`, outer-free the same
   pointer through `0x004524a1`, then clear the global. The global is cleared
   even when it was initially null.
5. If D3D9 device `0x00491c30` is non-null, call vtable slot `+0x40` with
   `(device, &0x00491d0c)`, ignore its result, reread the global, release it
   through slot `+0x08`, and clear it.
6. Release and clear the Direct3D9 root interface at `0x00491c2c` through
   vtable slot `+0x08`.
7. If window `0x004924f0` is non-null, call `ShowWindow(hwnd, 0)`,
   `MoveWindow(hwnd, 0, 0, 0, 0, 0)`, `DestroyWindow(hwnd)`, then clear it.
8. Call `ShowCursor(1)` unconditionally.

`0x0043d120` is a complete root sound teardown: it releases the BGM descriptor,
two 128-slot DirectSound arrays, raw images, then conditionally tears down
stream/device/control/holder state and track allocations. It must not be
reduced to producer-image cleanup.

`0x00446220` separately drains two intrusive lists whose owner fields hold
the `node + 4` link address, then clears 20 tail elements, an owned pointer at
`+0x3ad488`, and all 4096 pool elements in reverse order. It never frees the
owner allocation itself.

The matching constructor `0x00445900` constructs both node arrays, clears the
entire owner, resets all 4096 pool nodes, registers two calculation and 18 draw
callbacks at fixed priorities, then calls `SetVertexShader(NULL)`. Scheduler
record allocation failure intentionally has no recovery path.

## Verification

* `resources/th10.exe`, `0x00438dda-0x00438e86`.
* `0x0043cc40-0x0043ccee` and `0x0043d120-0x0043d24c`.
* `0x00446220-0x004462ef` for the render-owner in-place boundary.

## Final Exit Branch

When the enclosing function's local status is not `2`, `0x00438efa` writes
the fixed 0x34-byte configuration block, performs the two distinct MIDI
output teardown stages, optionally flushes the error buffer to `./log.txt`,
deletes all seven critical sections at `0x00492274`, restores three system
parameters, enables IME, and releases the 0xa004 pointer table. The pointer
table's global is intentionally not cleared after its outer allocation is
released.

When local status is `2`, the final branch is skipped. Instead, it resets the
error cursor and base byte, appends the fixed retry message, conditionally
enables IME, runs exactly 60 nonblocking `PeekMessageA` iterations (dispatching
only retrieved messages), then jumps to the enclosing startup loop.

## Frame Loop

The stable message/device loop processes at most one pending message per pass.
Without a message it tests D3D9 cooperative level, advances one frame only on
`S_OK`, handles only `D3DERR_DEVICENOTRESET` with render-slot release/reset,
then sets runtime flag `0x10`. Nonzero frame results are retained verbatim: a
frame result of `2` selects the startup retry path, while `1`, `-1`, or an
unsuccessful cooperative Reset HRESULT proceed to final cleanup. A successful
callback registration always enters this loop, even if the exit flag was
already set; its first loop-condition check then skips processing and performs
normal main-chain shutdown.

The reset preparation helper `0x00438a30` receives the render owner in EAX
and releases/clears 32 COM slots starting at `+0x3ad4e0`, in ascending order.

After `EndScene`, `0x004391f0` calls Present with four null arguments. A
negative result releases those render targets, attempts Reset with ignored
result, restores states even after failed Reset, and writes recovery state 2.
It then invokes `0x00438a60` on both outcomes before optional diagnostics.
That entry is not the ordinary `0x00442f50` vertex flush: it first tests the
signed deferred-batch sentinel at render-owner `+0x4`, calls the corresponding
surface/batch submission path, and clears the sentinel to `-1`; it then does
the same for the distinct sentinel at `+0x0`. The underlying submit paths own
their own conditional vertex flushes. An unconditional `0x00442f50` call here
would therefore introduce a draw when both sentinels are inactive.

The `+0x4` branch resolves a source surface from the owner group table, then
only for a non-null source flushes pending vertices, obtains the back buffer,
and performs an ignored-result D3DX surface copy before releasing the back
buffer. The `+0x0` branch flushes, releases a preexisting primary/shadow cache
pair if present, creates a primary cache and a system-memory shadow, checks the
back-buffer-to-primary copy, ignores the primary-to-shadow copy result, and
does not roll back partially created surfaces after later failure. The exact
source group descriptor and cache-dimension field meanings remain surface
adapter boundaries.

## Present Diagnostics

The tail is gated by bit `0x800` of `DAT_00474e36`, which is distinct from
the runtime window options at `0x491d78`. It attempts to create `snapshot`
and ignores that result. It then probes `snapshot/th000.bmp` through
`snapshot/th999.bmp` in ascending order using the locked file-existence helper
at `0x44b4d0`; the first non-existing path is passed to the screenshot entry
`0x420670` with `ESI = g_MainChainContext` and `EAX = path`. If all one
thousand paths exist, no screenshot call is made. The capture entry remains a
separate semantic boundary because its image conversion and background-thread
ownership are not yet recovered.

## System Settings

`0x4392e0` has no branches. It saves screen-saver, low-power, and power-off
settings through `SystemParametersInfoA` actions `0x10`, `0x53`, and `0x54`,
then sends the corresponding disable actions `0x11`, `0x55`, and `0x56` with
flag `2`. It then calls `QueryPerformanceFrequency` and
`QueryPerformanceCounter` into the frame clock globals. All eight results are
ignored; it neither clears the QPC globals nor rolls back any earlier setting
on failure.

## Host Environment

`0x439ff0` creates the named `Touhou 10 App` mutex and returns `-1` when
`GetLastError()` is `ERROR_ALREADY_EXISTS`, regardless of the handle result;
otherwise it returns zero only when the mutex handle is non-null. It reads the
module path, console title, and `STARTUPINFOA`. A null startup title sets
runtime flag `0x40`; a non-null title clears it and, when it names a readable
path with an extension, copies that path or resolves its `.lnk` chain into the
console-title buffer. A bytewise case-sensitive mismatch with the module path
sets `DAT_00492518`, which is intentionally not cleared by this function.

## Configuration

`0x420870` is invoked with `EBX = "th10.cfg"` and a stack `MainChainContext*`;
the semantic C++ boundary therefore takes both inputs explicitly. It defaults,
loads, validates, and writes a 52-byte configuration blob. Missing or invalid
input reverts to defaults and does not fail startup; only the final write
failure returns `-1`. Valid input requires exact size, magic, and six bounded
option bytes, then publishes its 18-byte input-binding block.

## Window Procedure

`0x4390e0` handles `WM_CLOSE` by setting runtime flag `0x80` and consuming the
message, consumes `WM_ENDSESSION`, and synchronizes `WM_ACTIVATEAPP`'s wParam
to the input gate at `0x4924fc` while storing its inverse at `0x492500`. For
`WM_SETCURSOR`, normal-window mode with the inverse gate clear hides the cursor
and sets it to null; all other cases show the standard arrow cursor. Mouse
button down foregrounds the window. The MIDI notification message `0x3c9`
delegates only when the MIDI output global exists, then still forwards to
`DefWindowProcA`.

## Startup Frame Timing

After construction of the render owner, normal-window mode calls
`WINNLSEnableIME(0, 0)`, `ShowCursor(0)`, and `SetCursor(NULL)`. Startup then
sets the frame-clock epoch to zero, obtains one `0x439540` sample, and copies
that sample into `0x492540`, `0x492538`, `0x492528`, and `0x492530` in order.

## D3D Device Startup

`0x439890` builds a 56-byte `D3DPRESENT_PARAMETERS` block, reads adapter mode
without checking its result, and uses a native retry sequence: HAL hardware VP,
HAL software VP, then REF software VP. An all-failure pass either retries with
zero refresh rate or, when the fallback mode is set, retries once after changing
interval from `0x80000000` to `1` and swap effect to `3`; a final failure
releases the Direct3D9 root and returns `1`. Device creation success is any
nonnegative HRESULT. It publishes presentation parameters, fixed startup view
and projection transforms, viewport and caps, performs its narrow
`CheckDeviceFormat == S_OK` test, restores render states, then invokes the
fixed viewport/two-clear-Present initializer.

`0x43bcd0` has a stack color argument. It flushes the render owner if present,
sets viewport `{0,0,640,480,0,1}`, then repeats Clear with flags `3` and a
four-null Present exactly twice. Each negative Present attempts Reset with the
published presentation parameters; no result is checked and no states are
restored in this helper.

## Scheduler Lifecycle

`0x449aa0` performs sparse in-place construction of two callback sentinels:
priority and callback fields are cleared, flags lose only bit `0x1`, and each
link owner points at its own sentinel. `0x438880` first stops the secondary
thread control, drains calculation then draw chains through `0x449f60` while
holding the scheduler lock and updating activity byte `0x49231c`, clears the
three callback slots of draw then calculation sentinel, and leaves outer
allocation release to its caller. The top-level path performs that free before
clearing `g_CallbackScheduler`.
