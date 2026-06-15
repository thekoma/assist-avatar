#pragma once
#include <algorithm>
#include <cstring>
#include <string>
#include <vector>
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

// ---- Text overlay (STT request / TTS response) -----------------------------
// Drawn from the YAML page/display lambda (where fonts + text sensors live), on
// top of the avatar animation. Templated on display + font so the same code runs
// in the SDL emulator and on the device. Not used by the host tests.

// Greedy word-wrap `text` to at most `maxw` pixels per line, up to `max_lines`
// (an ellipsis is appended if it overflows). Width measured with the real font.
template<typename D, typename F>
inline std::vector<std::string> wrap_text(D &it, F *font, const std::string &text,
                                          int maxw, int max_lines) {
  std::vector<std::string> lines;
  std::string line;
  auto width = [&](const std::string &s) {
    int x1, y1, w, h;
    it.get_text_bounds(0, 0, s.c_str(), font, esphome::display::TextAlign::TOP_LEFT,
                       &x1, &y1, &w, &h);
    return w;
  };
  size_t i = 0, n = text.size();
  bool overflow = false;
  while (i < n) {
    while (i < n && text[i] == ' ') ++i;
    size_t j = i;
    while (j < n && text[j] != ' ') ++j;
    std::string word = text.substr(i, j - i);
    i = j;
    if (word.empty()) continue;
    std::string cand = line.empty() ? word : line + " " + word;
    if (line.empty() || width(cand) <= maxw) {
      line = cand;
    } else {
      lines.push_back(line);
      line.clear();
      if ((int) lines.size() >= max_lines) { overflow = true; break; }
      line = word;
    }
  }
  if (!overflow && !line.empty() && (int) lines.size() < max_lines)
    lines.push_back(line);
  else if (i < n || overflow)
    overflow = true;
  if (overflow && !lines.empty()) lines.back() += "\xE2\x80\xA6";  // …
  return lines;
}

// Print text with a soft "phosphor" bloom: a dim copy spread one pixel in each
// direction, bright copy on top.
template<typename D, typename F>
void print_glow(D &it, int x, int y, F *font, esphome::Color c, const char *s) {
  esphome::Color dim((uint8_t) (c.r * 0.32f), (uint8_t) (c.g * 0.32f), (uint8_t) (c.b * 0.32f));
  it.print(x - 1, y, font, dim, s);
  it.print(x + 1, y, font, dim, s);
  it.print(x, y - 1, font, dim, s);
  it.print(x, y + 1, font, dim, s);
  it.print(x, y, font, c, s);
}

// Terminal-style dialog, "typed out" over time like a phosphor CRT. The request
// is drawn near the top and the response near the bottom; each reveals one char
// at a time from its own start time (`req_t0` / `resp_t0`, ms) at `cps`
// characters/second, with a blinking block cursor while it types. A `*_t0` of 0
// means "show that line fully" (already typed).
template<typename D, typename F>
void draw_dialog(D &it, F *font, const char *request, const char *response,
                 esphome::Color color, uint32_t now_ms,
                 uint32_t req_t0, uint32_t resp_t0, float cps) {
  const int W = it.get_width(), H = it.get_height(), margin = 6;
  const int maxw = W - 2 * margin;
  int x1, y1, lw, lh;
  it.get_text_bounds(0, 0, "Ag", font, esphome::display::TextAlign::TOP_LEFT,
                     &x1, &y1, &lw, &lh);
  const int line_h = lh + 2;
  const bool blink = ((now_ms / 350) % 2) == 0;

  auto draw_block = [&](const char *text, uint32_t t0, bool bottom, int max_lines) {
    if (!text || !text[0] || std::strcmp(text, "...") == 0) return;
    std::string full(text);
    size_t reveal = full.size();
    bool typing = false;
    if (t0 != 0) {  // 0 => fully shown
      size_t n = (now_ms <= t0) ? 0
                                : (size_t) ((double) (now_ms - t0) * cps / 1000.0);
      if (n < reveal) { reveal = n; typing = true; }
    }
    auto lines = wrap_text(it, font, full.substr(0, reveal), maxw, max_lines);
    int start_y = bottom ? (H - margin - (int) std::max<int>(1, lines.size()) * line_h)
                         : margin;
    for (size_t k = 0; k < lines.size(); ++k)
      print_glow(it, margin, start_y + (int) k * line_h, font, color, lines[k].c_str());
    if (typing && blink) {  // block cursor at the end of the revealed text
      int cx = margin, cy = start_y + (int) std::max<int>(0, (int) lines.size() - 1) * line_h;
      if (!lines.empty()) {
        int bx, by, bw, bh;
        it.get_text_bounds(0, 0, lines.back().c_str(), font,
                           esphome::display::TextAlign::TOP_LEFT, &bx, &by, &bw, &bh);
        cx = margin + bw + 2;
      }
      it.filled_rectangle(cx, cy, lh / 2, lh, color);
    }
  };

  draw_block(request, req_t0, false, 2);
  draw_block(response, resp_t0, true, 4);
}

}  // namespace avatar
