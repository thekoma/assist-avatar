#pragma once
// scan_arc — a faint ring with a bright arc scanning around it (linking / no-HA phase).
#include "avatar_module.h"

namespace avatar {
namespace mod {
namespace scan_arc {

// The module contract entry. cs.primary() is the arc colour; speed and variation
// are reserved for a later phase — they are intentionally unused here.
template<typename D>
void render(D &it, uint32_t now_ms, const avatar::ColorSet &cs, float speed, uint8_t /*variation*/) {
  now_ms = (uint32_t) ((float) now_ms * (speed > 0.0f ? speed : 1.0f));
  const float cx = it.get_width() / 2.0f;
  const float cy = it.get_height() / 2.0f;
  // Faint background ring.
  avatar::glow_ring(it, cx, cy, 38.0f, 1.5f, 3.0f, avatar::scale(cs.primary(), 45 / 255.0f));
  // Bright arc rotating around the ring.
  float rot = (float) now_ms * 0.0035f;
  avatar::glow_arc(it, cx, cy, 38.0f, 2.0f, 4.0f, avatar::scale(cs.primary(), 235 / 255.0f), rot, 0.9f);
}

}  // namespace scan_arc
}  // namespace mod
}  // namespace avatar
