// TH10 0x00432690 — paginated list refresh of the game-manager state-B
// screen (called from RunManagerStateBodyB, TH10 0x00431ee0, on page
// shifts).
#ifndef TH10_UNLOCKLISTREFRESH_HPP
#define TH10_UNLOCKLISTREFRESH_HPP

namespace th10 {

// TH10 0x00432690. Native ECX = game manager. Re-resolves the ten entity
// handle slots at manager+0x5d4 for the visible rows of the paginated
// selection list, drawing each row through the 0x00447bb0 text-entity
// entry, and blanks the rows past the last entry. Stale handles (no longer
// resolvable through the render owner's two entity lists) are zeroed.
// Always returns 0.
void RefreshStateBSelection(void *game_manager);

} // namespace th10

#endif // TH10_UNLOCKLISTREFRESH_HPP
