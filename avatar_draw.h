#pragma once
#include "esphome.h"
#include "avatar_math.h"

// Neon drawing primitives shared by every avatar state. Shapes are drawn with a
// soft glow/bloom halo (and an optional white-hot centre) so thin geometry reads
// as glowing light rather than clean vector strokes — the cyberpunk look. Float
// centres/radii keep motion smooth (sub-pixel) and let radii breathe without
// popping. Depends on ESPHome's Display API, so it is not unit-tested directly.
namespace avatar {

// Scale an RGB colour's brightness by w (0..1). On black this reads as coverage.
inline esphome::Color scale(esphome::Color c, float w) {
  if (w < 0.0f) w = 0.0f;
  if (w > 1.0f) w = 1.0f;
  return esphome::Color((uint8_t) (c.r * w), (uint8_t) (c.g * w), (uint8_t) (c.b * w));
}

// A pixel of `base` at brightness t, blended toward white by `white` (for the
// overexposed hot core). Both clamped to 0..1.
inline esphome::Color neon_px(esphome::Color base, float t, float white) {
  if (t < 0.0f) t = 0.0f;
  if (t > 1.0f) t = 1.0f;
  if (white < 0.0f) white = 0.0f;
  if (white > 1.0f) white = 1.0f;
  float r = (base.r + (255.0f - base.r) * white) * t;
  float g = (base.g + (255.0f - base.g) * white) * t;
  float b = (base.b + (255.0f - base.b) * white) * t;
  return esphome::Color((uint8_t) r, (uint8_t) g, (uint8_t) b);
}

// Glowing disc: a solid core of radius `core` surrounded by a soft halo that
// fades over `glow` pixels. `white_center` (0..1) blends the core toward white
// for a hot, overexposed energy source.
template<typename D>
void glow_disc(D &it, float cx, float cy, float core, float glow, esphome::Color color,
               float white_center = 0.0f) {
  if (core < 0.0f) core = 0.0f;
  if (glow < 0.5f) glow = 0.5f;
  float outer = core + glow + 0.5f;
  float outer2 = outer * outer;
  int x0 = (int) std::floor(cx - outer), x1 = (int) std::ceil(cx + outer);
  int y0 = (int) std::floor(cy - outer), y1 = (int) std::ceil(cy + outer);
  for (int y = y0; y <= y1; ++y) {
    if (y < 0 || y >= it.get_height()) continue;   // never write off-screen
    for (int x = x0; x <= x1; ++x) {
      if (x < 0 || x >= it.get_width()) continue;
      float dx = (float) x - cx, dy = (float) y - cy;
      float d2 = dx * dx + dy * dy;
      if (d2 > outer2) continue;
      float d = std::sqrt(d2);
      float t, white = 0.0f;
      if (d <= core + 0.5f) {
        t = 1.0f;
        white = white_center * (core > 0.0f ? (1.0f - d / (core + 0.5f)) : 1.0f);
      } else {
        float h = 1.0f - (d - core) / glow;  // linear falloff 1 -> 0 across glow
        if (h <= 0.0f) continue;
        t = h * h;                           // squared = softer neon halo
      }
      it.draw_pixel_at(x, y, neon_px(color, t, white));
    }
  }
}

// Glowing ring (annulus) of mid-radius `r` and stroke `thickness`, with a soft
// halo fading over `glow` pixels on both sides.
template<typename D>
void glow_ring(D &it, float cx, float cy, float r, float thickness, float glow,
               esphome::Color color) {
  float half = thickness * 0.5f;
  float outer = r + half + glow + 0.5f;
  float inner = r - half - glow - 0.5f;
  if (inner < 0.0f) inner = 0.0f;
  float outer2 = outer * outer, inner2 = inner * inner;
  int x0 = (int) std::floor(cx - outer), x1 = (int) std::ceil(cx + outer);
  int y0 = (int) std::floor(cy - outer), y1 = (int) std::ceil(cy + outer);
  for (int y = y0; y <= y1; ++y) {
    if (y < 0 || y >= it.get_height()) continue;   // never write off-screen
    for (int x = x0; x <= x1; ++x) {
      if (x < 0 || x >= it.get_width()) continue;
      float dx = (float) x - cx, dy = (float) y - cy;
      float d2 = dx * dx + dy * dy;
      if (d2 > outer2 || d2 < inner2) continue;
      float dd = std::fabs(std::sqrt(d2) - r);
      float t;
      if (dd <= half + 0.5f) {
        t = 1.0f;
      } else {
        float h = 1.0f - (dd - half) / glow;
        if (h <= 0.0f) continue;
        t = h * h;
      }
      it.draw_pixel_at(x, y, scale(color, t));
    }
  }
}

// Glowing arc: like glow_ring but only the angular slice starting at `a_start`
// (radians, atan2 convention) and spanning `width` radians. Used for loading /
// scanning motifs.
template<typename D>
void glow_arc(D &it, float cx, float cy, float r, float thickness, float glow,
              esphome::Color color, float a_start, float width) {
  float half = thickness * 0.5f;
  float outer = r + half + glow + 0.5f;
  float inner = r - half - glow - 0.5f;
  if (inner < 0.0f) inner = 0.0f;
  float outer2 = outer * outer, inner2 = inner * inner;
  const float TAU = 2.0f * (float) M_PI;
  int x0 = (int) std::floor(cx - outer), x1 = (int) std::ceil(cx + outer);
  int y0 = (int) std::floor(cy - outer), y1 = (int) std::ceil(cy + outer);
  for (int y = y0; y <= y1; ++y) {
    if (y < 0 || y >= it.get_height()) continue;
    for (int x = x0; x <= x1; ++x) {
      if (x < 0 || x >= it.get_width()) continue;
      float dx = (float) x - cx, dy = (float) y - cy;
      float d2 = dx * dx + dy * dy;
      if (d2 > outer2 || d2 < inner2) continue;
      float rel = std::atan2(dy, dx) - a_start;
      rel -= TAU * std::floor(rel / TAU);   // normalise to 0..2pi
      if (rel > width) continue;            // outside the arc slice
      float t;
      float dd = std::fabs(std::sqrt(d2) - r);
      if (dd <= half + 0.5f) {
        t = 1.0f;
      } else {
        float h = 1.0f - (dd - half) / glow;
        if (h <= 0.0f) continue;
        t = h * h;
      }
      it.draw_pixel_at(x, y, scale(color, t));
    }
  }
}

}  // namespace avatar
