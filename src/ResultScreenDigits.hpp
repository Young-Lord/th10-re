#pragma once

#include "Th10Types.hpp"

namespace th10 {

// TH10 0x0042f8b0. Native register ABI: EAX = result-screen state object
// (parent-id slot at +0x2cc, five u16 stat fields at +0x59cc..+0x59d4),
// ECX = dead register input. For each of the twenty glyph entities
// (children of the +0x2cc entity with kinds 67..86) the function resolves
// the child and re-runs its glyph VM init with the matching digit entry
// (tens/units of each stat field; kinds 77..86 repeat the tens/units of
// the same fields). Returns the state object in EAX.
void *UpdateResultScreenStatDigitsEaxAbi(void *state);

} // namespace th10
