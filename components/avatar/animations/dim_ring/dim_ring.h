#pragma once
// dim_ring — very dim, static glowing ring with a small slash.
#include "avatar_module.h"

namespace avatar {
namespace mod {
namespace dim_ring {

// The module contract entry. cs.primary() is the ring/slash colour; variation is
// reserved. dim_ring is a STATIC look (no time-based math), so scaling now_ms by
// speed is a genuine no-op kept only for a uniform signature across modules.
template<typename D>
void render(D &it, uint32_t now_ms, const avatar::ColorSet &cs, float speed, uint8_t /*variation*/) {
  now_ms = (uint32_t) ((float) now_ms * (speed > 0.0f ? speed : 1.0f));
  (void) now_ms;  // static animation: now_ms intentionally unread
  const float cx = it.get_width() / 2.0f;
  const float cy = it.get_height() / 2.0f;
  avatar::glow_ring(it, cx, cy, 36.0f, 1.5f, 3.0f, avatar::scale(cs.primary(), 40 / 255.0f));
  it.line((int)(cx - 14), (int)(cy - 14), (int)(cx + 14), (int)(cy + 14), avatar::scale(cs.primary(), 70 / 255.0f));
}

}  // namespace dim_ring
}  // namespace mod
}  // namespace avatar
