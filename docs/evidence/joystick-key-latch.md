# Joystick key latch (0x0044a950)

Module: `src/JoystickKeyLatch.cpp`

Records at `DAT_00474e30`, stride 0x6a bytes (53 u16s): +0x00 current
mask, +0x02 previous mask, +0x04 auto-repeat mask, +0x06 pressed mask,
+0x08 released mask, +0x0a..+0x29 sixteen per-bit hold counters.

Native AX = new mask, ECX = record index. The previous mask is latched,
the current mask stored, then for each of the 16 bits (LSB first): held
bits age their counter, and once it reaches 26 the bit joins the
auto-repeat mask and the counter drops by 8 (8-frame repeat cadence);
released bits zero the counter. Finally pressed = current & (current ^
previous) and released = (current ^ previous) & ~current.
