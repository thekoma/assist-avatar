#pragma once
// orbits — two slowly counter-rotating rings of glowing points with fading trails.
#include "avatar_module.h"

namespace avatar {
namespace mod {
namespace orbits {

// The module contract entry. cs.primary() is the orbit colour; speed and
// variation are RESERVED (ignored) — keeping now_ms unscaled guarantees
// byte-identical output vs. the original case ORBITS:.
template<typename D>
void render(D &it, uint32_t now_ms, const avatar::ColorSet &cs, float speed, uint8_t /*variation*/) {
  now_ms = (uint32_t) ((float) now_ms * (speed > 0.0f ? speed : 1.0f));
  const float cx = it.get_width() / 2.0f;
  const float cy = it.get_height() / 2.0f;

  // Two slowly counter-rotating rings of glowing points with fading trails.
  const int n = 10;
  const int trail = 5;
  float phaseA = (float) now_ms * 0.0008f;
  float phaseB = -(float) now_ms * 0.0005f;
  for (int i = 0; i < n; ++i) {
    for (int t = 0; t < trail; ++t) {
      float decay = 1.0f - 0.8f * (float) t / trail;
      float ax, ay, bx, by;
      avatar::orbit_point(i, n, cx, cy, 34.0f, phaseA - (float) t * 0.06f, ax, ay);
      avatar::orbit_point(i, n, cx, cy, 52.0f, phaseB + (float) t * 0.05f, bx, by);
      // uint8 truncation intentional — preserves byte-identical output vs the original ac() path; do not simplify.
      avatar::glow_disc(it, ax, ay, 2.4f, 4.5f, avatar::scale(cs.primary(), (float)(uint8_t)(255 * decay) / 255.0f));
      avatar::glow_disc(it, bx, by, 2.2f, 4.0f, avatar::scale(cs.primary(), (float)(uint8_t)(215 * decay) / 255.0f));
    }
  }
}

}  // namespace orbits
}  // namespace mod
}  // namespace avatar
