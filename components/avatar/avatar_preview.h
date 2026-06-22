#pragma once
// Leaf header for the 2-second per-state PREVIEW subsystem.
//
// Include direction: the three per-state control headers (avatar_select.h,
// avatar_number.h, avatar_color_output.h) are INCLUDED BY avatar_render_state.h,
// not the reverse. So the trigger/guard primitives that the controls must call
// live here, in a header that depends on nothing avatar-specific (only <cstdint>).
// Both the control headers and avatar_render_state.h #include this. No cycle.
//
// Boot guard is a TIME GATE, not a "first-frame" flag. ESPHome's App::setup() is
// non-blocking, so loop() (and therefore the light's RESTORE_AND_ON boot
// write_state, which is deferred to LightState::loop()) starts at very small
// millis(). A fixed boot-settle window (AVATAR_BOOT_SETTLE_MS) robustly swallows
// every boot/restore/seed write — including multi-step light transitions — with
// no dependence on loop-phase ordering. Genuine user control changes happen many
// seconds after boot, well past the gate, so they preview normally.
#include <cstdint>

namespace avatar {

// Must match AVATAR_NUM_PHASES in avatar_render_state.h (static_assert there).
static constexpr int      AVATAR_NUM_PHASES_PREVIEW = 9;  // IDLE..NO_HA
static constexpr uint32_t AVATAR_PREVIEW_MS = 2000;       // 2s preview window
// Ignore any trigger for the first 3s after boot. Covers the deferred
// RESTORE_AND_ON write and any multi-step transition that lands during boot.
static constexpr uint32_t AVATAR_BOOT_SETTLE_MS = 3000;

struct PreviewState {
  bool     active{false};   // a preview is currently showing
  int      phase_id{-1};    // which state's animation to override-render
  uint32_t until_ms{0};     // millis() deadline (wrap-safe signed compare)
};
extern PreviewState g_preview;   // ODR definition in avatar_render_state.cpp

// Arm a 2s preview of `phase_id`. No-op during the boot-settle window (so no
// boot/restore/seed write can arm a spurious preview) and for an out-of-range
// phase. Called only from genuine HA control()/write_state paths.
inline void preview_trigger(int phase_id, uint32_t now_ms) {
  if (now_ms < AVATAR_BOOT_SETTLE_MS) return;
  if (phase_id < 0 || phase_id >= AVATAR_NUM_PHASES_PREVIEW) return;
  g_preview.active   = true;
  g_preview.phase_id = phase_id;
  g_preview.until_ms = now_ms + AVATAR_PREVIEW_MS;
}

}  // namespace avatar
