#pragma once
// orb — the orb family: 6 moods (siri/calm/sleeping/agitated/spike/happy), all
// sharing one ribbon-based renderer and one layered-blob renderer. Each mood is
// a preset; variation selects which preset (0=siri … 5=happy).
#include "avatar_module.h"

namespace avatar {
namespace mod {
namespace orb {

// Apple-Siri-ish cyclic palette: teal -> blue -> purple -> magenta -> (teal).
// `u` wraps; used by orbs whose colour is fixed (not the user accent).
inline esphome::Color palette_siri(float u) {
  static const uint8_t key[4][3] = {
    {40, 220, 200}, {60, 120, 240}, {150, 70, 230}, {230, 80, 200}};
  u -= std::floor(u);
  float f = u * 4.0f;
  int i = (int) f;
  float tt = f - (float) i;
  const uint8_t *a = key[i & 3];
  const uint8_t *b = key[(i + 1) & 3];
  return esphome::Color((uint8_t) (a[0] + (b[0] - a[0]) * tt),
                        (uint8_t) (a[1] + (b[1] - a[1]) * tt),
                        (uint8_t) (a[2] + (b[2] - a[2]) * tt));
}

// ---- Orb family -------------------------------------------------------------
// The "orb" animations share one ribbon-based renderer; each mood is a preset.
// Siri's orb reads as flowing silk because it is a few intertwined great-circle
// ribbons (not scattered dots), each with a travelling perpendicular wobble.
struct OrbParams {
  int rings;          // intertwined ribbons (great circles)
  int per_ring;       // particles per ribbon (dense => continuous silk)
  float radius;       // base radius (px)
  float base_speed;   // travel of particles along the ribbon (rad/ms)
  float flow_speed;   // drift of the silk wobble (rad/ms)
  float wobble_freq;  // waves per ribbon
  float wobble_amp;   // perpendicular "silk" amplitude (px)
  float spike_freq;   // spikes per ribbon
  float spike_amp;    // spike radial amplitude (px)
  float spike_beat;   // spike pulse frequency (rad/ms); 0 = steady
  float yaw_speed;    // global rotation (rad/ms)
  float breathe_amp;  // radius breathing (fraction of radius)
  float breathe_ms;   // breathing period (ms)
  uint8_t bright;     // peak (front) brightness
  bool use_palette;   // true = fixed Siri palette (ignores accent); false = accent
  uint8_t back_bright; // translucent back-face floor brightness (silk shows through)
  bool combo;         // true = Siri "silk in haze" combo renderer (haze + ribbons)
  int haze_n;         // particle count for the haze volume (combo only)
};

// ---- Siri orb: 2D layered liquid blobs (metasidd/Orb-style) -------------------
// Translated from the SwiftUI/Canvas recipes people use to recreate the modern
// Siri orb: a FEW big soft radial-gradient blobs in the Siri palette (teal -> blue
// -> purple -> pink) drift and rotate at different speeds inside a circular field;
// their colours are accumulated additively, box-blurred into one buttery volume,
// then masked to a soft disc so it reads as a single contained liquid orb rather
// than separate shapes. No 3D, no ribbons, no central dot. `p.rings` = blob count.
template<typename D>
void draw_orb_siri(D &it, uint32_t now_ms, esphome::Color accent, const OrbParams &p) {
  const float t = (float) now_ms;
  const int W = it.get_width(), H = it.get_height();
  const float cx = W / 2.0f, cy = H / 2.0f;
  const float R = p.radius * (1.0f + p.breathe_amp * avatar::breath(now_ms, p.breathe_ms));

  static constexpr int BS = 132;                 // box side (~102 KB RGB16)
  static constexpr int NBUF = BS * BS * 3;        // elements per scratch buffer
  // acc + tmp (~204 KB combined) live in PSRAM on device (scratch<> uses
  // RAMAllocator EXTERNAL); a null return means PSRAM is exhausted -> skip frame.
  // SLOT 0 vs 1 guarantees two DISTINCT buffers (aliasing would corrupt the blur).
  uint16_t *acc = avatar::scratch<uint16_t, 0>(NBUF);
  uint16_t *tmp = avatar::scratch<uint16_t, 1>(NBUF);
  if (acc == nullptr || tmp == nullptr) return;
  for (int i = 0; i < NBUF; ++i) acc[i] = 0;
  const int bx0 = (int) cx - BS / 2, by0 = (int) cy - BS / 2;

  // ===== 1) LAYERED BLOBS: a few large soft radial gradients, splatted additively
  // A stable CENTRED base disc gives the round body; smaller drifting tint blobs
  // ride inside it so colour swirls without the silhouette wandering off-centre.
  {
    // Splat one soft radial gradient (smooth quadratic falloff) into the buffer.
    auto splat = [&](float px, float py, float br, esphome::Color col, float a) {
      if (br < 4.0f) br = 4.0f;
      int rB = (int) col.r, gB = (int) col.g, bB = (int) col.b;
      float inv = 1.0f / (br * br);
      int ri = (int) (br + 1.0f);
      int icx = (int) (px + 0.5f), icy = (int) (py + 0.5f);
      for (int dy = -ri; dy <= ri; ++dy) {
        int ay = icy - by0 + dy; if (ay < 0 || ay >= BS) continue;
        for (int dx = -ri; dx <= ri; ++dx) {
          int ax = icx - bx0 + dx; if (ax < 0 || ax >= BS) continue;
          float fall = 1.0f - (float) (dx * dx + dy * dy) * inv;
          if (fall <= 0.0f) continue;
          float w = a * fall * fall * 255.0f;          // smooth gaussian-ish core
          int o = (ay * BS + ax) * 3;
          int vr = acc[o]   + (int) (rB * w / 255.0f);
          int vg = acc[o+1] + (int) (gB * w / 255.0f);
          int vb = acc[o+2] + (int) (bB * w / 255.0f);
          acc[o]   = vr > 65535 ? 65535 : (uint16_t) vr;
          acc[o+1] = vg > 65535 ? 65535 : (uint16_t) vg;
          acc[o+2] = vb > 65535 ? 65535 : (uint16_t) vb;
        }
      }
    };

    const float base_amp = (p.bright / 255.0f) * 0.42f;
    const int NB = p.rings < 2 ? 2 : (p.rings > 8 ? 8 : p.rings);
    const float driftR = R * 0.22f;            // how far tint blobs roam from middle
    const float amp = (p.bright / 255.0f) * 0.46f;

    // centred base body: a broad disc whose hue cycles slowly through the palette
    esphome::Color basec = p.use_palette ? palette_siri(t * 0.00006f) : accent;
    splat(cx, cy, R * 0.96f, basec, base_amp);

    // drifting tint blobs: orbit + lissajous wobble, each a different palette hue
    for (int b = 0; b < NB; ++b) {
      float ph = 6.2831853f * b / NB;
      float sp = p.base_speed * (0.65f + 0.40f * b);          // each blob its own pace
      float ang = t * sp + ph;
      float bxp = cx + driftR * std::cos(ang)
                     + R * 0.09f * std::sin(t * p.flow_speed * 1.7f + ph);
      float byp = cy + driftR * std::sin(ang * 1.13f + 0.5f)
                     + R * 0.09f * std::cos(t * p.flow_speed * 1.3f + ph);
      float br = R * (0.60f + 0.12f * std::sin(t * p.flow_speed * 2.0f + ph * 1.7f));
      esphome::Color col = p.use_palette ? palette_siri((float) b / NB + t * 0.00006f) : accent;
      splat(bxp, byp, br, col, amp);
    }
  }

  // ===== 2) cheap separable 5-tap (1-2-2-2-1) blur for a buttery glow =========
  {
    // tmp is declared at function scope above (heap-backed via scratch<> on host, PSRAM on device).
    auto cl = [](int v) { return v < 0 ? 0 : (v > BS - 1 ? BS - 1 : v); };
    for (int y = 0; y < BS; ++y) {
      int row = y * BS * 3;
      for (int x = 0; x < BS; ++x) {
        int x0 = cl(x - 2) * 3, x1 = cl(x - 1) * 3, x2 = x * 3,
            x3 = cl(x + 1) * 3, x4 = cl(x + 2) * 3;
        for (int c = 0; c < 3; ++c) {
          int s = acc[row + x0 + c] + 2 * acc[row + x1 + c] + 2 * acc[row + x2 + c]
                + 2 * acc[row + x3 + c] + acc[row + x4 + c];
          tmp[row + x2 + c] = (uint16_t) (s / 8);
        }
      }
    }
    for (int x = 0; x < BS; ++x) {
      for (int y = 0; y < BS; ++y) {
        int y0 = cl(y - 2), y1 = cl(y - 1), y3 = cl(y + 1), y4 = cl(y + 2);
        for (int c = 0; c < 3; ++c) {
          int s = tmp[(y0 * BS + x) * 3 + c] + 2 * tmp[(y1 * BS + x) * 3 + c]
                + 2 * tmp[(y  * BS + x) * 3 + c] + 2 * tmp[(y3 * BS + x) * 3 + c]
                + tmp[(y4 * BS + x) * 3 + c];
          acc[(y * BS + x) * 3 + c] = (uint16_t) (s / 8);
        }
      }
    }
  }

  // ===== 3) composite, masked to a soft disc so it reads as ONE liquid orb =====
  // The mask fades the volume to nothing just outside R, giving the clean round
  // silhouette of the Siri orb instead of square-ish blur edges.
  const float edge0 = R * 0.80f, edge1 = R * 1.04f, einv = 1.0f / (edge1 - edge0);
  for (int y = 0; y < BS; ++y) {
    int sy = by0 + y; if (sy < 0 || sy >= H) continue;
    float dyf = (float) sy - cy;
    for (int x = 0; x < BS; ++x) {
      int sx = bx0 + x; if (sx < 0 || sx >= W) continue;
      int o = (y * BS + x) * 3;
      int r = acc[o], g = acc[o+1], b = acc[o+2];
      if (r + g + b <= 28) continue;
      float dxf = (float) sx - cx;
      float dist = std::sqrt(dxf * dxf + dyf * dyf);
      float m = 1.0f - avatar::smoothstep((dist - edge0) * einv);   // 1 inside, 0 past edge1
      if (m <= 0.0f) continue;
      r = (int) (r * m); g = (int) (g * m); b = (int) (b * m);
      if (r + g + b <= 28) continue;
      if (r > 255) r = 255; if (g > 255) g = 255; if (b > 255) b = 255;
      it.draw_pixel_at(sx, sy, esphome::Color((uint8_t) r, (uint8_t) g, (uint8_t) b));
    }
  }
}

// Render a ribbon orb: `rings` tumbling great-circles of particles, each with a
// travelling perpendicular wobble (the "silk"), depth-shaded. No central core.
template<typename D>
void draw_orb(D &it, uint32_t now_ms, esphome::Color accent, const OrbParams &p) {
  if (p.combo) { draw_orb_siri(it, now_ms, accent, p); return; }
  const float t = (float) now_ms;
  const float cx = it.get_width() / 2.0f, cy = it.get_height() / 2.0f;
  const float R = p.radius * (1.0f + p.breathe_amp * avatar::breath(now_ms, p.breathe_ms));
  const float yaw = t * p.yaw_speed;
  const float cyw = std::cos(yaw), syw = std::sin(yaw);
  const float pitch = 0.4f * std::sin(t * 0.0003f);
  const float cpt = std::cos(pitch), spt = std::sin(pitch);
  const float spikeAmp = p.spike_amp * (p.spike_beat > 0.0f
                            ? (0.5f + 0.5f * std::sin(t * p.spike_beat)) : 1.0f);
  const float zdiv = p.radius * 1.3f + p.wobble_amp + p.spike_amp;

  for (int r = 0; r < p.rings; ++r) {
    // Each ribbon's plane is oriented by a normal spread over the sphere
    // (Fibonacci), so the rings cross and WEAVE in 3D instead of stacking as
    // parallel bands.
    float nphi = std::acos(1.0f - 2.0f * (r + 0.5f) / p.rings);
    float ntheta = 2.399963f * r;
    float cnx = std::cos(nphi), snx = std::sin(nphi);
    float cny = std::cos(ntheta), sny = std::sin(ntheta);
    float prevx = 0, prevy = 0;
    esphome::Color prevc;
    for (int i = 0; i <= p.per_ring; ++i) {        // <= : the last step closes the loop
      int ii = i % p.per_ring;
      float alpha = (6.2831853f * ii) / p.per_ring + t * p.base_speed;
      float rad = R + std::sin(alpha * p.spike_freq) * spikeAmp;   // spikes (radial)
      float wob = std::sin(alpha * p.wobble_freq + t * p.flow_speed) * p.wobble_amp;
      float lx = std::cos(alpha) * rad, ly = std::sin(alpha) * rad, lz = wob;
      // orient the ribbon plane: Rx(nphi) then Ry(ntheta)
      float ya = cnx * ly - snx * lz, za = snx * ly + cnx * lz, xa = lx;
      float xb = cny * xa + sny * za, zb = -sny * xa + cny * za, yb = ya;
      // global tumble: yaw (Y) then pitch (X)
      float x2 = cyw * xb + syw * zb, z2 = -syw * xb + cyw * zb;
      float y3 = cpt * yb - spt * z2, z3 = spt * yb + cpt * z2;
      float depth = (z3 / zdiv + 1.0f) * 0.5f;
      if (depth < 0.0f) depth = 0.0f; else if (depth > 1.0f) depth = 1.0f;
      float bf = (18.0f + (p.bright - 18.0f) * depth * depth) / 255.0f;  // back faded heavily
      float u = alpha * 0.159155f + (float) r / p.rings + t * 0.00006f;  // hue along ribbon + drift
      esphome::Color base = p.use_palette ? palette_siri(u) : accent;
      int sx = (int) (cx + x2), sy = (int) (cy + y3);
      if (i > 0) {  // glowing "silk" strand: bright core line + a dim 1px halo
        esphome::Color core = avatar::scale(prevc, bf);
        esphome::Color halo = avatar::scale(prevc, bf * 0.35f);
        it.line((int) prevx, (int) prevy - 1, sx, sy - 1, halo);
        it.line((int) prevx, (int) prevy + 1, sx, sy + 1, halo);
        it.line((int) prevx, (int) prevy, sx, sy, core);
      }
      if (i < p.per_ring)
        avatar::glow_disc(it, (float) sx, (float) sy, 0.2f + 0.5f * depth, 0.8f + 0.8f * depth,
                  avatar::scale(base, bf));
      prevx = sx; prevy = sy; prevc = base;
    }
  }
}

inline OrbParams orb_siri()     { return {5, 60, 46.0f, 0.00050f, 0.0010f, 2.5f, 15.0f, 0.0f,  0.0f, 0.0000f, 0.00030f, 0.05f,  6000.0f, 255, true,  50, true,  520}; }
inline OrbParams orb_calm()     { return {4, 60, 44.0f, 0.00035f, 0.0007f, 3.0f, 12.0f, 0.0f,  0.0f, 0.0000f, 0.00022f, 0.05f,  8000.0f, 220, true,  40, false, 0}; }
inline OrbParams orb_sleeping() { return {3, 56, 42.0f, 0.00018f, 0.0003f, 2.0f,  6.0f, 0.0f,  0.0f, 0.0000f, 0.00010f, 0.14f, 10000.0f, 120, true,  24, false, 0}; }
inline OrbParams orb_agitated() { return {4, 64, 46.0f, 0.00160f, 0.0030f, 5.0f, 22.0f, 0.0f,  0.0f, 0.0000f, 0.00090f, 0.06f,  1800.0f, 255, true,  40, false, 0}; }
inline OrbParams orb_spike()    { return {4, 64, 38.0f, 0.00050f, 0.0011f, 2.0f,  9.0f, 8.0f, 22.0f, 0.0042f, 0.00040f, 0.04f,  3000.0f, 255, true,  30, false, 0}; }
inline OrbParams orb_happy()    { return {4, 64, 44.0f, 0.00090f, 0.0016f, 4.0f, 18.0f, 0.0f,  0.0f, 0.0000f, 0.00060f, 0.16f,  1400.0f, 255, true,  40, false, 0}; }

template<typename D>
void render(D &it, uint32_t now_ms, const avatar::ColorSet &cs, float speed, uint8_t variation) {
  // The orb is palette-driven (use_palette=true on every preset), so it ignores
  // cs for now — byte-identical to the current look. cs is reserved for a future
  // deliberate visual rework (core/glow colours), not wired here.
  // accent is passed through for the non-combo path's API; the presets set
  // use_palette=true so accent is unused, matching today's rendering exactly.
  // Scaling now_ms here makes every mood's draw_orb/draw_orb_siri math honour
  // speed uniformly (no-op at speed=1.0 -> byte-identical).
  now_ms = (uint32_t) ((float) now_ms * (speed > 0.0f ? speed : 1.0f));
  esphome::Color accent = cs.n ? cs.primary() : esphome::Color(0, 255, (uint8_t)(255 * 0.85f));
  switch (variation) {
    case 1: draw_orb(it, now_ms, accent, orb_calm());     break;
    case 2: draw_orb(it, now_ms, accent, orb_sleeping()); break;
    case 3: draw_orb(it, now_ms, accent, orb_agitated()); break;
    case 4: draw_orb(it, now_ms, accent, orb_spike());    break;
    case 5: draw_orb(it, now_ms, accent, orb_happy());    break;
    case 0:
    default: draw_orb(it, now_ms, accent, orb_siri());    break;
  }
}

}  // namespace orb
}  // namespace mod
}  // namespace avatar
