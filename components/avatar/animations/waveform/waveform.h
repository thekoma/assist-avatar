#pragma once
// waveform — horizontal energy burst: a glowing waveform, brightest at the centre.
#include "avatar_module.h"

namespace avatar {
namespace mod {
namespace waveform {

// The module contract entry. cs.primary() is the wave colour; speed and
// variation are reserved for a later phase and are unused here.
template<typename D>
void render(D &it, uint32_t now_ms, const avatar::ColorSet &cs, float speed, uint8_t /*variation*/) {
  now_ms = (uint32_t) ((float) now_ms * (speed > 0.0f ? speed : 1.0f));
  const float cx = it.get_width() / 2.0f;
  const float cy = it.get_height() / 2.0f;
  float amp = 8.0f + 14.0f * avatar::breath(now_ms, 2000.0f);
  int w = (int) it.get_width(), hgt = (int) it.get_height();
  for (int x = 0; x < w; ++x) {
    float env = std::sin((float) M_PI * x / w);
    float y = cy + avatar::wave_y((float) x, now_ms, 0.05f, amp * env, 0.005f);
    for (int dyi = -5; dyi <= 5; ++dyi) {
      int yy = (int) (y + dyi);
      if (yy < 0 || yy >= hgt) continue;
      float h = 1.0f - std::fabs((float) dyi) / 6.0f;
      if (h <= 0.0f) continue;
      it.draw_pixel_at(x, yy, avatar::scale(avatar::scale(cs.primary(), 235 / 255.0f), h * h * env));
    }
  }
}

}  // namespace waveform
}  // namespace mod
}  // namespace avatar
