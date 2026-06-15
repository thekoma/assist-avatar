#include <cassert>
#include <cmath>
#include <cstdio>
#include "avatar_math.h"

static bool approx(float a, float b, float eps = 1e-3f) { return std::fabs(a - b) < eps; }

int main() {
  using namespace avatar;

  // Phase enum is stable and starts at 0.
  assert(IDLE == 0 && LISTENING == 1 && THINKING == 2 && REPLYING == 3 && ERROR == 4 && MUTED == 5);
  assert(BOOTING == 6 && NO_WIFI == 7 && NO_HA == 8);

  // breath(): smooth 0..1, ~0 at start of period, ~1 at half period.
  assert(approx(breath(0, 100.0f), 0.0f));
  assert(approx(breath(50, 100.0f), 1.0f));
  for (uint32_t f = 0; f < 200; ++f) {
    float v = breath(f, 100.0f);
    assert(v >= -1e-4f && v <= 1.0f + 1e-4f);
  }

  // breath_radius(): stays within [base, base+amp].
  for (uint32_t f = 0; f < 200; ++f) {
    float r = breath_radius(40.0f, 8.0f, f, 100.0f);
    assert(r >= 40.0f - 1e-3f && r <= 48.0f + 1e-3f);
  }

  // wave_y(): bounded by amplitude.
  for (int x = 0; x < 320; ++x) {
    float y = wave_y((float) x, 30, 0.06f, 12.0f, 0.15f);
    assert(y >= -12.0f - 1e-3f && y <= 12.0f + 1e-3f);
  }

  // orbit_point(): lands on a circle of the given radius around (cx,cy).
  float ox, oy;
  orbit_point(0, 6, 100.0f, 80.0f, 30.0f, 0.0f, ox, oy);
  assert(approx(std::hypot(ox - 100.0f, oy - 80.0f), 30.0f, 1e-2f));

  // converge(): progress 0 -> on ring (radius r0), progress 1 -> at center.
  float px, py;
  converge_point(0, 8, 100.0f, 80.0f, 50.0f, 0.0f, px, py);
  assert(approx(std::hypot(px - 100.0f, py - 80.0f), 50.0f, 1e-2f));
  converge_point(0, 8, 100.0f, 80.0f, 50.0f, 1.0f, px, py);
  assert(approx(std::hypot(px - 100.0f, py - 80.0f), 0.0f, 1e-2f));

  // lerp_u8(): endpoints exact, midpoint rounded, and correct when descending.
  assert(lerp_u8(0, 200, 0.0f) == 0);
  assert(lerp_u8(0, 200, 1.0f) == 200);
  assert(lerp_u8(0, 200, 0.5f) == 100);
  assert(lerp_u8(200, 0, 0.0f) == 200);
  assert(lerp_u8(200, 0, 1.0f) == 0);
  assert(lerp_u8(200, 0, 0.5f) == 100);

  // smoothstep(): clamped 0..1, eased endpoints, symmetric midpoint.
  assert(approx(smoothstep(0.0f), 0.0f));
  assert(approx(smoothstep(1.0f), 1.0f));
  assert(approx(smoothstep(0.5f), 0.5f));
  assert(approx(smoothstep(-3.0f), 0.0f));   // clamps below 0
  assert(approx(smoothstep(3.0f), 1.0f));    // clamps above 1
  // eased: near the ends it moves slower than linear (slow-in / slow-out).
  assert(smoothstep(0.1f) < 0.1f);
  assert(smoothstep(0.9f) > 0.9f);

  std::printf("all avatar_math tests passed\n");
  return 0;
}
