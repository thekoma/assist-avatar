#pragma once
#include <cstdio>                   // snprintf (draw_preview_banner)
#include "esphome/core/color.h"
#include "avatar_module.h"          // ColorSet, AVATAR_MAX_COLORS
#include "avatar_preview.h"         // PreviewState, g_preview, preview_* primitives
#include "avatar_select.h"
#include "avatar_number.h"
#include "avatar_color_output.h"
#include "avatar_dispatch.h"        // avatar::dispatch<D>

namespace avatar {

static constexpr int AVATAR_NUM_PHASES = 9;            // IDLE..NO_HA
// Keep the preview-subsystem phase count locked to ours (they live in separate
// headers; avatar_preview.h can't see this constant, so we cross-check here).
static_assert(AVATAR_NUM_PHASES == AVATAR_NUM_PHASES_PREVIEW,
              "AVATAR_NUM_PHASES must match AVATAR_NUM_PHASES_PREVIEW");
static constexpr uint8_t AVATAR_MAX_STATE_COLORS = AVATAR_MAX_COLORS;  // 4 — agrees with ColorSet

// Human display names, indexed by avatar::Phase (IDLE..NO_HA == 0..8). Mirrors
// STATE_DISPLAY in __init__.py. Used only by draw_preview_banner.
static const char *const AVATAR_STATE_NAMES[AVATAR_NUM_PHASES] = {
  "Idle", "Listening", "Thinking", "Replying", "Error",
  "Muted", "Booting", "No Wi-Fi", "No HA"
};

struct StateEntry {
  AvatarSelect     *anim_sel{nullptr};
  AvatarNumber     *speed_num{nullptr};
  AvatarColorOutput *colors[AVATAR_MAX_STATE_COLORS]{};
  uint8_t           n_colors{0};
};

extern StateEntry g_avatar_states[AVATAR_NUM_PHASES];

inline void register_state(int phase_id, AvatarSelect *anim,
                            AvatarNumber *speed, AvatarColorOutput *const *cols, uint8_t n) {
  if (phase_id < 0 || phase_id >= AVATAR_NUM_PHASES) return;
  StateEntry &e = g_avatar_states[phase_id];
  e.anim_sel  = anim;
  e.speed_num = speed;
  e.n_colors  = n > AVATAR_MAX_STATE_COLORS ? AVATAR_MAX_STATE_COLORS : n;
  for (uint8_t i = 0; i < e.n_colors; ++i) e.colors[i] = cols[i];
}

template<typename D>
void render_state(D &it, int phase_id, uint32_t now_ms) {
  // Preview override: while a 2s preview is active, render the AFFECTED state's
  // animation instead of this page's own phase. When no preview is active (the
  // .bss-zero default and the steady state outside any window) this whole block
  // is skipped and the body below runs byte-identically to before.
  if (g_preview.active) {
    if ((int32_t)(now_ms - g_preview.until_ms) >= 0) {
      g_preview.active = false;          // expired -> revert to the page's own phase
    } else {
      phase_id = g_preview.phase_id;     // override: render the affected state's anim
    }
  }
  if (phase_id < 0 || phase_id >= AVATAR_NUM_PHASES) {
    it.fill(esphome::Color(0, 0, 0));
    return;
  }
  const StateEntry &e = g_avatar_states[phase_id];
  if (e.anim_sel == nullptr) {
    // Unconfigured phase: clear to black — do NOT leave a stale frame.
    // (The page lambda's !extend flag REPLACES the upstream lambda, so a bare
    // no-op would leave whatever was drawn last.)
    it.fill(esphome::Color(0, 0, 0));
    return;
  }
  ColorSet cs;
  cs.n = e.n_colors;  // already clamped <= AVATAR_MAX_COLORS in register_state
  for (uint8_t i = 0; i < cs.n; ++i)
    cs.c[i] = e.colors[i] ? e.colors[i]->get() : esphome::Color(0, 255, 217);
  if (cs.n == 0) {
    // State's home animation declares no colour role (orb/amber_pulse), so no
    // accent light was minted. The home anim ignores cs, BUT any OTHER animation
    // selected for this state reads cs.primary() — an empty set returns black,
    // rendering it invisible. Seed a default cyan so every animation is visible
    // regardless of which state it's assigned to. (Palette anims still ignore it,
    // so their output is unchanged.)
    cs.c[0] = esphome::Color(0, 255, 217);
    cs.n = 1;
  }
  float speed = e.speed_num ? e.speed_num->state : 1.0f;
  // Flattened index: identifies both the module and the baked-in variation.
  int idx = (e.anim_sel->active_index().has_value())
              ? (int) e.anim_sel->active_index().value() : 0;
  avatar::dispatch(it, idx, now_ms, cs, speed);
}

// Reads the displayed option label off a select pointer. Templated so the
// `->state` member lookup is DEPENDENT (resolved only at instantiation), never
// parsed/type-checked on host. Real ESPHome select::Select exposes a public
// `std::string state`; the host shim names it `state_`, but this is only ever
// instantiated by the device/SDL lambdas (via draw_preview_banner), so the host
// build never sees `->state`. A non-template helper here would be type-checked
// at definition and break the host compile.
template<typename S>
const char *preview_select_label(S *sel) {
  return sel->state.c_str();
}

// Drawn from the page lambda (the VT323 font id(ui_font) is YAML-scoped, not
// visible here). Templated on display + font like draw_dialog, so it is only
// instantiated by the real device/SDL lambdas and never compiled by host tests.
//
// No-op unless a preview is active; reads the previewed state straight from the
// runtime table. Outside an active preview it returns before touching the
// framebuffer, so zero pixels are written (byte-identical to before).
template<typename D, typename F>
void draw_preview_banner(D &it, F *font, uint32_t now_ms) {
  if (!g_preview.active || (int32_t)(now_ms - g_preview.until_ms) >= 0) return;
  int p = g_preview.phase_id;
  if (p < 0 || p >= AVATAR_NUM_PHASES) return;
  const StateEntry &e = g_avatar_states[p];
  if (e.anim_sel == nullptr) return;                       // unconfigured phase

  const char *anim_label = preview_select_label(e.anim_sel);  // displayed option string
  const char *state_name = AVATAR_STATE_NAMES[p];
  float speed = e.speed_num ? e.speed_num->state : 1.0f;

  // ASCII-only text (no UTF-8 middle dot — VT323 GF_Latin_Core may lack it and
  // render a tofu box). States with an accent colour (n_colors > 0) get the
  // "accent #RRGGBB" clause; colourless states (e.g. orb) omit it.
  char buf[128];
  if (e.n_colors > 0 && e.colors[0]) {
    esphome::Color c = e.colors[0]->get();
    std::snprintf(buf, sizeof(buf),
                  "%s selected for %s, accent #%02X%02X%02X at speed %.1fx",
                  anim_label, state_name, c.r, c.g, c.b, speed);
  } else {
    std::snprintf(buf, sizeof(buf),
                  "%s selected for %s at speed %.1fx",
                  anim_label, state_name, speed);
  }

  // Wrap to the screen width and draw top-aligned, exactly like draw_dialog's top
  // block: print_centered neither wraps nor clips, so a long label would overflow
  // both edges of the 320px screen and be unreadable.
  const int W = it.get_width(), margin = 6;
  const int maxw = W - 2 * margin;
  int bx, by, bw, bh;
  it.get_text_bounds(0, 0, "Ag", font, esphome::display::TextAlign::TOP_LEFT,
                     &bx, &by, &bw, &bh);
  const int line_h = bh + 2;
  const esphome::Color color(130, 255, 160);  // phosphor green, legible on the anim
  auto lines = avatar::wrap_text(it, font, std::string(buf), maxw, 3);
  for (size_t k = 0; k < lines.size(); ++k)
    avatar::print_glow(it, margin, margin + (int) k * line_h, font, color, lines[k].c_str());
}

}  // namespace avatar
