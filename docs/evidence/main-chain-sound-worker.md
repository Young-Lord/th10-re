# Main-Chain Sound Worker (`0x0043ccf0`, `0x0043cd30`)

`0x00438c21` creates the sound worker with `CreateThread`, passing the root at
`0x00492590`, then stores the unchecked handle at `0x004977a8`. The thread ID
is written through `0x004977b0`. Before that call, the caller clears the
0x52d0-byte root and publishes the main window to `0x004977b8`.

The native entry `0x0043ccf0` ignores its Win32 thread argument. It invokes
`0x0043cd30` with `ECX = root` and the published window on the stack, waits
while `0x004977b4 == 0`, sets `0x004977bc = 1`, and returns the gate value.
The C++ source therefore separates the Win32 adapter from
`InitializeMainChainSoundRoot`.

`0x0043cd30` first writes `-1` to root `+0x408[128]` and `+0x620[12]`; it does
not clear the sequence count/value arrays at `+0x650` or `+0x680`. It allocates
a four-byte DirectSound holder at `+0x610`. Allocation failure is not checked
before `DirectSoundCreate8`, so the native null dereference is deliberately
retained. Negative results from `DirectSoundCreate8` or
`SetCooperativeLevel(window, 2)` append the DirectSound failure text, release
and free the holder, clear `+0x610`, and return `-1`.

On that success path it performs a temporary format setup whose result is
ignored, creates the 0x8000-byte primary buffer at `+0x608`, clears a fixed
0x8000 bytes at the first Lock region, ignores Unlock/Play/SetTimer failures,
sets both volume inputs at `+0x52c4/+0x52c8` to 100, stores the timer window at
`+0x60c`, and enters the previously recovered 37-item WAV consumer. Later
primary-buffer CreateSoundBuffer or Lock failures return `-1` without the
early holder cleanup.
