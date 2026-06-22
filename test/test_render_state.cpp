// Host smoke test for avatar::render_state (Phase 4c Task 2).
//
// Verifies two properties of the runtime table + render_state glue:
//   1. An unconfigured phase (anim_sel == nullptr) fills the display black
//      and does NOT draw any pixels beyond that — no stale frames.
//   2. No out-of-bounds pixel writes occur when a configured phase is rendered
//      (the table dispatch ultimately calls avatar::dispatch, same bounds
//      guarantee as test_render.cpp; here we spot-check one state).
//
// avatar_dispatch.h resolution: the real components/avatar/avatar_dispatch.h is
// present on the host (it is generated once and checked in for the device).
// Since -Icomponents/avatar finds it and the animation module headers it pulls
// in also resolve via -Icomponents/avatar/base -Icomponents/avatar -Itest/shim,
// we use the real file directly. Build:
//   c++ -std=c++17 -Itest/shim -Icomponents/avatar/base -Icomponents/avatar \
//     test/test_render_state.cpp components/avatar/avatar_render_state.cpp -o /tmp/trs
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cassert>
#include <string>
#include <vector>

// Pull in the shim so esphome::Color etc. resolve before the component headers.
#include "esphome.h"

// Include the render-state header (and transitively avatar_dispatch.h,
// avatar_select.h, avatar_number.h, avatar_color_output.h).
#include "avatar_render_state.h"

// ---------------------------------------------------------------------------
// MockDisplay — mirrors test_render.cpp but also tracks fill() calls
// ---------------------------------------------------------------------------
struct MockDisplay {
  static constexpr int W = 320, H = 240;
  long fills = 0, draws = 0, oob = 0;
  esphome::Color last_fill{255, 255, 255};  // sentinel: non-black at start

  int get_width()  const { return W; }
  int get_height() const { return H; }

  void fill(esphome::Color c) {
    fills++;
    last_fill = c;
  }

  void draw_pixel_at(int x, int y, esphome::Color) {
    draws++;
    if (x < 0 || x >= W || y < 0 || y >= H) oob++;
  }
  void line(int x0, int y0, int x1, int y1, esphome::Color c) {
    draw_pixel_at(x0, y0, c);
    draw_pixel_at(x1, y1, c);
  }
};

// ---------------------------------------------------------------------------
// Helper: build a configured AvatarSelect with one option selected at index i
// ---------------------------------------------------------------------------
static avatar::AvatarSelect make_select(size_t active) {
  avatar::AvatarSelect s;
  std::vector<std::string> opts{"anim0", "anim1", "anim2"};
  for (auto &o : opts) s.traits_set_options(opts);  // set once is enough
  // Manually publish the active option (bypasses ESPHome infra on host).
  if (active < opts.size()) s.publish_state(opts[active]);
  return s;
}

int main() {
  // ---- Test 1: unconfigured phase (table zero-initialised) ------------------
  // g_avatar_states is zero-initialised (.bss), so anim_sel == nullptr for all.
  {
    MockDisplay d;
    avatar::render_state(d, 0, 0);

    assert(d.fills >= 1 && "unconfigured phase must call fill()");
    assert(d.last_fill.r == 0 && d.last_fill.g == 0 && d.last_fill.b == 0 &&
           "unconfigured phase must fill black");
    assert(d.draws == 0 && "unconfigured phase must draw no pixels");
    assert(d.oob   == 0 && "no out-of-bounds writes");
    std::printf("Test 1 passed: unconfigured phase fills black, 0 pixel draws\n");
  }

  // ---- Test 2: out-of-range phase_id -----------------------------------------
  {
    MockDisplay d;
    avatar::render_state(d, -1, 0);
    assert(d.fills >= 1 && d.last_fill.r == 0 && d.last_fill.g == 0 && d.last_fill.b == 0);
    avatar::render_state(d, avatar::AVATAR_NUM_PHASES, 1000);
    assert(d.fills >= 2);
    std::printf("Test 2 passed: out-of-range phase_id fills black\n");
  }

  // ---- Test 3: register_state + render (bounds check) -----------------------
  {
    // Build one AvatarSelect pointing at animation index 0 (amber_pulse in dispatch).
    avatar::AvatarSelect anim_sel;
    {
      std::vector<std::string> opts{"anim0"};
      anim_sel.traits_set_options(opts);
      anim_sel.publish_state("anim0");
    }
    // speed_num, colors all null — render_state must handle gracefully.
    avatar::AvatarColorOutput *no_colors[1] = {nullptr};
    avatar::register_state(0, &anim_sel, nullptr, no_colors, 0);

    MockDisplay d;
    // Sweep timestamps — same idea as test_render.cpp.
    for (uint32_t t = 0; t < 10000; t += 17) {
      avatar::render_state(d, 0, t);
    }
    if (d.oob != 0) {
      std::printf("FAIL: %ld out-of-bounds writes in configured-phase sweep\n", d.oob);
      return 1;
    }
    std::printf("Test 3 passed: configured phase renders with 0 OOB writes "
                "(draws=%ld fills=%ld)\n", d.draws, d.fills);
  }

  // ---- Test 4: preview boot-gate / arm / override / self-clear ---------------
  // Does NOT instantiate draw_preview_banner (template-only, never compiled on
  // host). Exercises the boot-settle time gate + render_state override prologue.
  //
  // Boot guard is now a TIME GATE (preview_trigger ignores now_ms <
  // AVATAR_BOOT_SETTLE_MS) instead of a "ready" flag: a boot/restore write_state
  // lands at small millis() and so can never arm a preview.
  {
    // Register a configured phase at index 0 so the override path renders it.
    avatar::AvatarSelect anim_sel;
    {
      std::vector<std::string> opts{"anim0"};
      anim_sel.traits_set_options(opts);
      anim_sel.publish_state("anim0");
    }
    avatar::AvatarColorOutput *no_colors[1] = {nullptr};
    avatar::register_state(0, &anim_sel, nullptr, no_colors, 0);

    // Boot gate: a trigger at t=0 (and anywhere inside the settle window) is a
    // hard no-op — this is exactly the boot/restore write_state case.
    avatar::g_preview = avatar::PreviewState{};  // reset
    avatar::preview_trigger(0, 0);
    assert(!avatar::g_preview.active && "trigger at boot (t=0) must be a no-op");
    avatar::preview_trigger(0, avatar::AVATAR_BOOT_SETTLE_MS - 1);
    assert(!avatar::g_preview.active &&
           "trigger inside the boot-settle window must be a no-op");

    // At/after the gate, a genuine change arms a 2s window for the given phase.
    const uint32_t t_arm = avatar::AVATAR_BOOT_SETTLE_MS;  // first allowed instant
    avatar::preview_trigger(0, t_arm);
    assert(avatar::g_preview.active && "trigger at/after the boot gate must arm");
    assert(avatar::g_preview.phase_id == 0);
    assert(avatar::g_preview.until_ms == t_arm + avatar::AVATAR_PREVIEW_MS);

    // A later trigger overrides (re-arms with the new phase and deadline).
    const uint32_t t_arm2 = t_arm + 500;
    avatar::preview_trigger(0, t_arm2);
    assert(avatar::g_preview.active && avatar::g_preview.phase_id == 0);
    assert(avatar::g_preview.until_ms == t_arm2 + avatar::AVATAR_PREVIEW_MS &&
           "a later trigger must override the deadline");

    // Render inside the window: stays active and overrides the page's own phase.
    MockDisplay d;
    avatar::render_state(d, 5, t_arm2 + 100);   // page phase 5, preview overrides to 0
    assert(avatar::g_preview.active && "preview must persist within the 2s window");
    assert(d.oob == 0);

    // Render at/after the deadline: self-clears.
    avatar::render_state(d, 5, t_arm2 + avatar::AVATAR_PREVIEW_MS + 1);
    assert(!avatar::g_preview.active && "preview must self-clear after 2s");
    assert(d.oob == 0);

    // Reset so we leave global state clean.
    avatar::g_preview = avatar::PreviewState{};
    std::printf("Test 4 passed: preview boot-gates, arms, overrides, self-clears\n");
  }

  std::printf("all render_state smoke tests passed\n");
  return 0;
}
