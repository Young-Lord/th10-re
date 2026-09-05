# ASCII Mode-9 Renderer Evidence

`0x00444ce0` directly submits a prebuilt 28-byte vertex sequence rather than
generating a scratch quad. It repeats the enabled/visible gate, flushes pending
2D vertices, binds the glyph texture, configures FVF `0x144` and direct state,
then calls `DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, vertex_count - 2, vertices,
0x1c)`. Count arithmetic intentionally wraps and all HRESULTs are ignored.
