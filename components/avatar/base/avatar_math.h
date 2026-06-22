#pragma once
#include <cmath>
#include <cstdint>

namespace avatar {

// Assistant phases. Integers are part of the contract shared with YAML + avatar.h.
// 0..5 are the voice-assistant states; 6..8 are connection/boot states the device
// derives from wifi/HA status (so the user can see where the link stands).
enum Phase {
  IDLE = 0, LISTENING = 1, THINKING = 2, REPLYING = 3, ERROR = 4, MUTED = 5,
  BOOTING = 6, NO_WIFI = 7, NO_HA = 8
};

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Fraction 0..1 through a period of `period_frames`.
inline float wrap01(uint32_t frame, float period_frames) {
  return std::fmod((float) frame / period_frames, 1.0f);
}

// Smooth 0..1..0 "breath": 0 at t=0, 1 at half period, back to 0.
inline float breath(uint32_t frame, float period_frames) {
  float t = wrap01(frame, period_frames);
  return 0.5f - 0.5f * std::cos(2.0f * (float) M_PI * t);
}

// A radius that gently breathes between `base` and `base + amp`.
inline float breath_radius(float base, float amp, uint32_t frame, float period_frames) {
  return base + amp * breath(frame, period_frames);
}

// Vertical offset of a soft travelling wave at column x. Result in [-amp, amp].
inline float wave_y(float x, uint32_t frame, float k, float amp, float speed) {
  return amp * std::sin(x * k - (float) frame * speed);
}

// i-th of n points on a circle of radius r around (cx,cy), rotated by `phase` radians.
inline void orbit_point(int i, int n, float cx, float cy, float r, float phase,
                        float &out_x, float &out_y) {
  float a = (2.0f * (float) M_PI * (float) i) / (float) n + phase;
  out_x = cx + r * std::cos(a);
  out_y = cy + r * std::sin(a);
}

// i-th of n points starting on a ring of radius r0 and converging to center as
// `progress` goes 0 -> 1.
inline void converge_point(int i, int n, float cx, float cy, float r0, float progress,
                           float &out_x, float &out_y) {
  float a = (2.0f * (float) M_PI * (float) i) / (float) n;
  float r = r0 * (1.0f - progress);
  out_x = cx + r * std::cos(a);
  out_y = cy + r * std::sin(a);
}

// Linear interpolation between two 0..255 channel values.
inline uint8_t lerp_u8(uint8_t a, uint8_t b, float t) {
  if (t < 0.0f) t = 0.0f;
  if (t > 1.0f) t = 1.0f;
  return (uint8_t) (a + (b - a) * t + 0.5f);
}

// Smooth "slow-in / slow-out" easing: clamps t to [0,1] and eases both ends so
// motion accelerates and decelerates instead of moving linearly (robotically).
inline float smoothstep(float t) {
  if (t < 0.0f) t = 0.0f;
  if (t > 1.0f) t = 1.0f;
  return t * t * (3.0f - 2.0f * t);
}

}  // namespace avatar
