#pragma once
// sonar — concentric rings pulsing outward and fading (no-wifi / searching).
#include "avatar_module.h"

namespace avatar {
namespace mod {
namespace sonar {

// The module contract entry. cs.primary() is the wave colour; speed and
// variation are reserved for a later phase — they are intentionally unused.
template<typename D>
void render(D &it, uint32_t now_ms, const avatar::ColorSet &cs, float speed, uint8_t /*variation*/) {
  now_ms = (uint32_t) ((float) now_ms * (speed > 0.0f ? speed : 1.0f));
  const float cx = it.get_width() / 2.0f;
  const float cy = it.get_height() / 2.0f;
  const int waves = 3;
  for (int k = 0; k < waves; ++k) {
    float p = avatar::wrap01(now_ms + (uint32_t)(k * 1600 / waves), 1600.0f);
    float r = 6.0f + p * 44.0f;
      // uint8 truncation intentional — preserves byte-identical output vs the original ac() path; do not simplify.
    avatar::glow_ring(it, cx, cy, r, 1.5f, 3.0f, avatar::scale(cs.primary(), (uint8_t)(200 * (1.0f - p)) / 255.0f));
  }
}

}  // namespace sonar
}  // namespace mod
}  // namespace avatar
