// Game-manager state-2 menu controller (TH10 0x0042d420): the eight-row
// mode-selection menu update dispatched by the calculation controller
// 0x0042cdf0 (case 2). Its sibling state bodies live in
// GameManagerStateBodies.cpp; this one is kept separate because it carries
// the unlock-bank probe (0x0042c850) and the difficulty hand-off into the
// state-6 practice menu.
#ifndef TH10_SCORESCREENUPDATE_HPP
#define TH10_SCORESCREENUPDATE_HPP

// Declared at global scope with plain int so it matches the extern
// declaration in TitleGameManagerLifecycle.cpp (i32 == int).
int RunManagerStateBody2(void *game_manager);

// TH10 0x0042d420. Native EBX = game manager, plain retn, always returns 1
// in EAX. Sub-state at +0x20 drives the machine:
//   0: arm the +0x24 cursor record with the +0x2c maximum (8); when the
//      score-save unlock bank (0x0042c850 over DAT_0047783c) is empty,
//      append one 1 to the pending-flag list at +0xb4 (index word +0xf8);
//      when DAT_00474ca0 carries bit 0x10, seed the cursor value toward 2
//      and clear the bit; spawn script 0x58 when the +0x424 handle is
//      unresolved, always spawn script 0, and set sub-state 1. Native case
//      0 falls through into the case 1 body.
//   1: once the +0x2b4 timer exceeds 10, set sub-state 2, run the +0x2c4
//      entity with stop word 3 and queue the slot (cursor value + 17) stop
//      word 0.
//   2: menu cursor handling over the record at +0x24 (0x474e36/0x474e34
//      masks), the slot-(cursor+7) stop-word refresh on change, the 0xa
//      exit/confirm channel with the seven-row clamp, and the 0x1001 accept
//      dispatch that routes rows 0..5 to sub-state 4 through the 90/91
//      stop-word pair, row 6 through the 0x42c750 side effects, and row 7
//      straight to sub-state 4.
//   4: once the +0x2b4 timer reaches 20, act on the accepted row: rows
//      0/1/2 toggle DAT_00474ca0 bit 0x10 and continue into state 6 (row 1
//      parks the difficulty in +0x58f0, sets it to 4 and re-clamps the
//      cursor; rows 0/2 clamp DAT_00474c74 below 4 and push the value into
//      the cursor record through 0x0040ad20), rows 3/4/5 hand off to states
//      12/11/14, row 6 to state 4 and row 7 to state 3, each finalizing the
//      cursor record (0x0044be20) except row 7.
int RunManagerStateBody2(void *game_manager);

#endif // TH10_SCORESCREENUPDATE_HPP
