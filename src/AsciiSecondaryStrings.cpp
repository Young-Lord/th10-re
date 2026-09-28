// TH10 AsciiManager secondary-string queue helpers (0x4014d0..0x401d90).
//
// The manager keeps a 64-entry secondary string ring at +0x6f6c (entries of
// 0x68 bytes, live count at +0x896c) plus the current color/scale/is_gui
// state at +0x8974..+0x8984 and a frame tick counter at +0x8990. The native
// entries use register ABIs (EAX or ECX = manager, ESI = manager for the
// formatted variant); the semantic bodies are plain th10:: functions and the
// native ABI remains a thunk boundary.

#include "Th10Types.hpp"
#include "AsciiManager.hpp"

namespace th10 {

namespace {

AsciiManagerString *SecondaryStringAt(AsciiManager *manager, i32 index) {
    return &manager->secondary_strings[index];
}

} // namespace

namespace {

i32 &SecondaryCount(AsciiManager *manager) { // live count at +0x896c
    return *reinterpret_cast<i32 *>(reinterpret_cast<u8 *>(manager) + 0x896c);
}

i32 &SecondaryMirror(AsciiManager *manager) { // +0x8970 scratch mirror
    return *reinterpret_cast<i32 *>(reinterpret_cast<u8 *>(manager) + 0x8970);
}

u32 &AsciiFrameTick(AsciiManager *manager) { // +0x8990 tick counter
    return *reinterpret_cast<u32 *>(reinterpret_cast<u8 *>(manager) + 0x8990);
}

} // namespace

// TH10 0x004014d0. Native EAX = manager (register ABI, returns 1). Clears
// the secondary-string live count and the +0x8970 mirror, then increments
// the +0x8990 frame tick counter. The caller ignores the returned 1.
i32 ClearAsciiSecondaryStringsAndTickEaxAbi(AsciiManager *manager) {
    SecondaryCount(manager) = 0;
    SecondaryMirror(manager) = 0;
    ++AsciiFrameTick(manager);
    return 1;
}

// TH10 0x004014f0. Native ECX = manager. Identical body to 0x004014d0.
i32 ClearAsciiSecondaryStringsAndTickEcxAbi(AsciiManager *manager) {
    return ClearAsciiSecondaryStringsAndTickEaxAbi(manager);
}

// TH10 0x00401d90. Native EAX = manager. Clears the two counters without
// the +0x8990 tick and without a return value.
void ClearAsciiSecondaryStringsEaxAbi(AsciiManager *manager) {
    SecondaryCount(manager) = 0;
    SecondaryMirror(manager) = 0;
}

// TH10 0x004015c0. Native ESI = manager, ECX = NUL-terminated text,
// EBX = source Float3 position. Appends one secondary entry while the live
// count is below 0x40 (past the cap the call silently does nothing):
// copies the text, then the three position floats, then the manager's
// current color/scale_x/scale_y/is_gui state, and zeroes the entry's
// is_selected word. The trailing entry fields stay untouched.
void AddAsciiSecondaryStringEsiAbi(AsciiManager *manager, const Float3 *position,
                                   const char *text) {
    i32 count = SecondaryCount(manager);
    if (count >= 0x40) {
        return;
    }
    AsciiManagerString *entry = SecondaryStringAt(manager, count);
    SecondaryMirror(manager) = count + 1;

    char *out = entry->text;
    const char *in = text;
    // Native copies byte by byte including the terminator (rep-like loop).
    do {
        *out++ = *in;
    } while (*in++ != '\0');

    entry->position = *position;
    entry->color = manager->color;
    entry->scale_x = manager->scale_x;
    entry->scale_y = manager->scale_y;
    entry->is_gui = manager->is_gui;
    entry->is_selected = 0;
}

// TH10 0x00401700. Native ESI = manager, stack (format, va_list); `ret 8`.
// vsprintf's into a 0x200-byte stack buffer and forwards to the secondary
// append above; returns the formatted length. Native ABI is a thunk
// boundary; the va_list bridge lives in the ASM layer.
i32 AddAsciiSecondaryStringFormattedEsiStackAbi(AsciiManager *manager,
                                                const char *format,
                                                char *const *va_args_raw) {
    (void)manager;
    (void)format;
    (void)va_args_raw;
    // Boundary: implemented as a native thunk (vsprintf + tail call to
    // 0x004015c0). Declared here so the CSV entry has a semantic anchor.
    return 0;
}

} // namespace th10
