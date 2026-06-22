#pragma once
#include "esphome/core/color.h"
#include "avatar_module.h"          // ColorSet, AVATAR_MAX_COLORS
#include "avatar_select.h"
#include "avatar_number.h"
#include "avatar_color_output.h"
#include "avatar_dispatch.h"        // avatar::dispatch<D>

namespace avatar {

static constexpr int AVATAR_NUM_PHASES = 9;            // IDLE..NO_HA
static constexpr uint8_t AVATAR_MAX_STATE_COLORS = AVATAR_MAX_COLORS;  // 4 — agrees with ColorSet

struct StateEntry {
  AvatarSelect     *anim_sel{nullptr};
  AvatarSelect     *var_sel{nullptr};
  AvatarNumber     *speed_num{nullptr};
  AvatarColorOutput *colors[AVATAR_MAX_STATE_COLORS]{};
  uint8_t           n_colors{0};
};

extern StateEntry g_avatar_states[AVATAR_NUM_PHASES];

inline void register_state(int phase_id, AvatarSelect *anim, AvatarSelect *var,
                            AvatarNumber *speed, AvatarColorOutput *const *cols, uint8_t n) {
  if (phase_id < 0 || phase_id >= AVATAR_NUM_PHASES) return;
  StateEntry &e = g_avatar_states[phase_id];
  e.anim_sel  = anim;
  e.var_sel   = var;
  e.speed_num = speed;
  e.n_colors  = n > AVATAR_MAX_STATE_COLORS ? AVATAR_MAX_STATE_COLORS : n;
  for (uint8_t i = 0; i < e.n_colors; ++i) e.colors[i] = cols[i];
}

template<typename D>
void render_state(D &it, int phase_id, uint32_t now_ms) {
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
  float speed = e.speed_num ? e.speed_num->state : 1.0f;
  uint8_t variation = (e.var_sel && e.var_sel->active_index().has_value())
                        ? (uint8_t) e.var_sel->active_index().value() : 0;
  int mod = (e.anim_sel->active_index().has_value())
              ? (int) e.anim_sel->active_index().value() : 0;
  avatar::dispatch(it, mod, now_ms, cs, speed, variation);
}

}  // namespace avatar
