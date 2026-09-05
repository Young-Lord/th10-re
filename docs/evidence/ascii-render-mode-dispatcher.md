# ASCII Render Mode Dispatcher Evidence

`0x004451c0` receives VM in `EAX` and owner in `ECX`. It requires VM flags
bits zero and one plus nonzero byte `+0x2ff`, returning `-1` otherwise. It
dispatches `(flags >> 22) & 15`: modes 0 through 9 now have semantic C++
targets, while modes 10 through 15 return zero. The original tail-call and
mixed-register ABI mechanics are deliberately left to a thin entry thunk.
