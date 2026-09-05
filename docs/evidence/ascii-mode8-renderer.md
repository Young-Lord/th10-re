# ASCII Mode-8 Renderer Evidence

`0x00444760` is the VM mode-8 3D textured-quad path. It repeats the enabled
and visible gate, flushes the pending 2D batch when needed, conditionally
rebuilds VM matrix `+0x27c`, positions an anchor-selected world matrix, and
updates blend/color/sampler caches.

Unlike the normal glyph paths it does not call `0x442670`. It sets world and
texture transforms directly, binds glyph texture, configures a 20-byte vertex
buffer with FVF `0x102`, and calls `DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2)`.
All D3D and D3DX return values are ignored. The source is
`src/AsciiMode8Renderer.cpp`; native EAX plus stack ABI remains isolated.
