#pragma once
// amber_pulse — soft amber pulse (~2.5 s per breath). Ignores accent; builds
// esphome::Color amber directly so the ring stays amber regardless of theme.
#include "avatar_module.h"

namespace avatar {
namespace mod {
namespace amber_pulse {

// The module contract entry. cs is intentionally ignored (amber_pulse builds
// its own colour from the breath phase). speed and variation are reserved.
template<typename D>
void render(D &it, uint32_t now_ms, const avatar::ColorSet & /*cs*/, float speed, uint8_t /*variation*/) {
  // NOTE: this animation ignores cs (hardcoded amber). Scaling now_ms below keeps breath() and breath_radius() locked to the SAME period, so the pulse stays in sync at any speed.
  now_ms = (uint32_t) ((float) now_ms * (speed > 0.0f ? speed : 1.0f));
  const float cx = it.get_width() / 2.0f;
  const float cy = it.get_height() / 2.0f;
  float p = avatar::breath(now_ms, 2500.0f);
  esphome::Color amber(255, (uint8_t)(140 + 60 * p), 0);
  avatar::glow_ring(it, cx, cy, avatar::breath_radius(34.0f, 8.0f, now_ms, 2500.0f), 2.0f, 5.0f, amber);
}

}  // namespace amber_pulse
}  // namespace mod
}  // namespace avatar
