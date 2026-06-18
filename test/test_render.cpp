// Host-side reproduction test for the device render path.
//
// On the SDL backend, draw_pixel_at silently clips out-of-bounds writes, so a
// shape drawn partly off-screen looks fine. On the device's mipi_spi backend an
// out-of-bounds framebuffer write can corrupt memory and crash (bootloop). This
// test runs avatar::render() for every phase across a sweep of timestamps using
// a mock display that flags ANY draw outside the 320x240 bounds — catching that
// class of bug on the Mac, before flashing.
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include "avatar.h"  // pulls the shim esphome.h (via -Itest/shim) + avatar_draw.h + avatar_math.h

struct MockDisplay {
  static constexpr int W = 320, H = 240;
  long draws = 0, oob = 0;
  int min_x = 1 << 30, max_x = -(1 << 30), min_y = 1 << 30, max_y = -(1 << 30);

  int get_width() const { return W; }
  int get_height() const { return H; }
  void fill(esphome::Color) {}

  void track(int x, int y) {
    draws++;
    min_x = std::min(min_x, x); max_x = std::max(max_x, x);
    min_y = std::min(min_y, y); max_y = std::max(max_y, y);
    if (x < 0 || x >= W || y < 0 || y >= H) oob++;
  }
  void draw_pixel_at(int x, int y, esphome::Color) { track(x, y); }
  void line(int x0, int y0, int x1, int y1, esphome::Color) { track(x0, y0); track(x1, y1); }
};

int main() {
  MockDisplay d;
  for (int phase = 0; phase < 9; ++phase) {  // all phases incl. boot/no-wifi/no-ha
    for (uint32_t t = 0; t < 30000; t += 13) {
      avatar::render(d, phase, t);
    }
  }
  for (int a = 0; a < avatar::ANIM_COUNT; ++a) {  // every animation in the catalogue
    for (uint32_t t = 0; t < 30000; t += 13) {
      avatar::render_anim(d, a, t);
    }
  }
  std::printf("render sweep: draws=%ld out_of_bounds=%ld\n", d.draws, d.oob);
  std::printf("x range [%d, %d], y range [%d, %d] (bounds 0..%d, 0..%d)\n",
              d.min_x, d.max_x, d.min_y, d.max_y, MockDisplay::W - 1, MockDisplay::H - 1);
  if (d.oob != 0) {
    std::printf("FAIL: %ld out-of-bounds writes (would corrupt the device framebuffer)\n", d.oob);
    return 1;
  }
  std::printf("all render-bounds checks passed\n");
  return 0;
}
