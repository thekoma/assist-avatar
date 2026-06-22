#pragma once
// breathing_ring — slow breathing neon ring (~8 s per breath at speed 1.0).
// The reference module: it defines the contract every animation render() follows.
#include "avatar_module.h"

namespace avatar {
namespace mod {
namespace breathing_ring {

// The module contract entry. cs.primary() is the ring colour; speed scales the
// breath period (speed 1.0 == the authored 8 s pace); variation is unused here.
template<typename D>
void render(D &it, uint32_t now_ms, const avatar::ColorSet &cs, float speed, uint8_t /*variation*/) {
  const float cx = it.get_width() / 2.0f;
  const float cy = it.get_height() / 2.0f;
  const float period = 8000.0f / (speed > 0.0f ? speed : 1.0f);
  float r = avatar::breath_radius(36.0f, 6.0f, now_ms, period);
  uint8_t b = avatar::lerp_u8(90, 200, avatar::breath(now_ms, period));
  avatar::glow_ring(it, cx, cy, r, 2.0f, 5.0f, avatar::scale(cs.primary(), b / 255.0f));
}

}  // namespace breathing_ring
}  // namespace mod
}  // namespace avatar
