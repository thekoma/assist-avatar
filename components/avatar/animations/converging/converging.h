#pragma once
// converging — a continuous stream of glowing particles drawn inward toward a hot core.
#include "avatar_module.h"

namespace avatar {
namespace mod {
namespace converging {

// The module contract entry. cs.primary() is the particle/core colour;
// speed and variation are reserved and unused in this phase.
template<typename D>
void render(D &it, uint32_t now_ms, const avatar::ColorSet &cs, float speed, uint8_t /*variation*/) {
  now_ms = (uint32_t) ((float) now_ms * (speed > 0.0f ? speed : 1.0f));
  const float cx = it.get_width() / 2.0f;
  const float cy = it.get_height() / 2.0f;

  avatar::glow_ring(it, cx, cy, 40.0f, 1.5f, 4.0f, avatar::scale(cs.primary(), 120 / 255.0f));
  const int n = 28;
  float base = avatar::wrap01(now_ms, 3500.0f);
  for (int i = 0; i < n; ++i) {
    float p = base + (float) i / n;
    if (p >= 1.0f) p -= 1.0f;
    float e = avatar::smoothstep(p);
    float px, py;
    avatar::converge_point(i, n, cx, cy, 56.0f, e, px, py);
    float fade = std::sin((float) M_PI * p);
    uint8_t b = avatar::lerp_u8(40, 255, e);
      // uint8 truncation intentional — preserves byte-identical output vs the original ac() path; do not simplify.
    avatar::glow_disc(it, px, py, 1.0f, 3.5f, avatar::scale(cs.primary(), (uint8_t)(b * fade) / 255.0f));
  }
  float corePulse = 0.65f + 0.35f * avatar::breath(now_ms, 1750.0f);
      // uint8 truncation intentional — preserves byte-identical output vs the original ac() path; do not simplify.
  avatar::glow_disc(it, cx, cy, 2.5f, 10.0f, avatar::scale(cs.primary(), (uint8_t)(255 * corePulse) / 255.0f), 0.85f);
}

}  // namespace converging
}  // namespace mod
}  // namespace avatar
