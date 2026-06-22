#pragma once
// loading_arc — a faint ring with a bright arc sweeping round to fill it.
#include "avatar_module.h"

namespace avatar {
namespace mod {
namespace loading_arc {

// The module contract entry. cs.primary() is the arc colour; speed and
// variation are reserved for a later phase and are unused here.
template<typename D>
void render(D &it, uint32_t now_ms, const avatar::ColorSet &cs, float speed, uint8_t /*variation*/) {
  now_ms = (uint32_t) ((float) now_ms * (speed > 0.0f ? speed : 1.0f));
  const float cx = it.get_width() / 2.0f;
  const float cy = it.get_height() / 2.0f;
  // Faint background ring.
  avatar::glow_ring(it, cx, cy, 36.0f, 1.5f, 3.0f, avatar::scale(cs.primary(), 45 / 255.0f));
  // Bright arc sweeping round to fill the ring.
  float p = avatar::wrap01(now_ms, 1600.0f);
  avatar::glow_arc(it, cx, cy, 36.0f, 2.0f, 4.0f, avatar::scale(cs.primary(), 230 / 255.0f),
                   -(float) M_PI / 2.0f, 2.0f * (float) M_PI * p);
}

}  // namespace loading_arc
}  // namespace mod
}  // namespace avatar
