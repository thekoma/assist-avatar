#pragma once
#include "esphome.h"
#include "avatar_math.h"
#include "avatar_draw.h"

namespace avatar {

// Translate the official ESP32-S3-Box-3 voice_assistant phase ids into our
// avatar::Phase integers. The upstream config uses:
//   idle=1, listening=2, thinking=3, replying=4,
//   not_ready=10, error=11, muted=12, timer_finished=20
// which do NOT line up with avatar::Phase (IDLE=0..MUTED=5), so we map here.
inline int avatar_phase_from(int official) {
  switch (official) {
    case 2:  return LISTENING;  // voice_assist_listening_phase_id
    case 3:  return THINKING;   // voice_assist_thinking_phase_id
    case 4:  return REPLYING;   // voice_assist_replying_phase_id
    case 11: return ERROR;      // voice_assist_error_phase_id
    case 12: return MUTED;      // voice_assist_muted_phase_id
    case 10: return NO_HA;      // not_ready -> "connecting to Home Assistant"
    case 20: return REPLYING;   // timer_finished -> reuse the lively reply look
    case 1:                     // voice_assist_idle_phase_id
    default: return IDLE;
  }
}

// Palette (RGB). Cyan/teal on black.
inline esphome::Color cyan(uint8_t v = 255) { return esphome::Color(0, v, (uint8_t)(v * 0.85f)); }
inline esphome::Color black() { return esphome::Color(0, 0, 0); }

// ---- Animation catalog ------------------------------------------------------
// Each entry is an independent animation "module" drawn by draw_anim(). States
// are mapped to animations separately (default_anim / Home Assistant selects),
// so ANY animation can be assigned to ANY state. To add one: append an enum
// value, a name, and a case in draw_anim().
enum Anim {
  BREATHING_RING = 0, CONVERGING, ORBITS, WAVEFORM, AMBER_PULSE,
  DIM_RING, LOADING_ARC, SONAR, SCAN_ARC,
  ORB, CALM_ORB, SLEEPING_ORB, AGITATED_ORB, SPIKE_ORB, HAPPY_ORB,
  ANIM_COUNT
};

inline const char *anim_name(int a) {
  switch (a) {
    case BREATHING_RING: return "Breathing ring";
    case CONVERGING:     return "Converging particles";
    case ORBITS:         return "Orbits";
    case WAVEFORM:       return "Waveform";
    case AMBER_PULSE:    return "Amber pulse";
    case DIM_RING:       return "Dim ring";
    case LOADING_ARC:    return "Loading arc";
    case SONAR:          return "Sonar";
    case SCAN_ARC:       return "Scanning arc";
    case ORB:            return "Siri orb";
    case CALM_ORB:       return "Calm orb";
    case SLEEPING_ORB:   return "Sleeping orb";
    case AGITATED_ORB:   return "Agitated orb";
    case SPIKE_ORB:      return "Spike orb";
    case HAPPY_ORB:      return "Happy orb";
    default:             return "Breathing ring";
  }
}

// Map a friendly name (e.g. from a Home Assistant select) back to an Anim.
inline int anim_from_name(const char *n) {
  if (n)
    for (int a = 0; a < ANIM_COUNT; ++a)
      if (std::strcmp(n, anim_name(a)) == 0) return a;
  return BREATHING_RING;
}

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
  const float R = p.radius * (1.0f + p.breathe_amp * breath(now_ms, p.breathe_ms));

  static constexpr int BS = 132;                 // box side (~102 KB RGB16)
  static uint16_t acc[BS * BS * 3];
  for (int i = 0; i < BS * BS * 3; ++i) acc[i] = 0;
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
    static uint16_t tmp[BS * BS * 3];
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
      float m = 1.0f - smoothstep((dist - edge0) * einv);   // 1 inside, 0 past edge1
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
  const float R = p.radius * (1.0f + p.breathe_amp * breath(now_ms, p.breathe_ms));
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
        esphome::Color core = scale(prevc, bf);
        esphome::Color halo = scale(prevc, bf * 0.35f);
        it.line((int) prevx, (int) prevy - 1, sx, sy - 1, halo);
        it.line((int) prevx, (int) prevy + 1, sx, sy + 1, halo);
        it.line((int) prevx, (int) prevy, sx, sy, core);
      }
      if (i < p.per_ring)
        glow_disc(it, (float) sx, (float) sy, 0.2f + 0.5f * depth, 0.8f + 0.8f * depth,
                  scale(base, bf));
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

// Draw one animation. Time-based (`now_ms` = millis()) so speed is refresh-rate
// independent; `accent` recolours every element (amber pulse stays amber). Does
// NOT clear the screen, so overlays can be layered on top by the caller.
template<typename D>
void draw_anim(D &it, int anim, uint32_t now_ms, esphome::Color accent) {
  const float cx = it.get_width() / 2.0f;
  const float cy = it.get_height() / 2.0f;
  auto ac = [&](uint8_t v) { return scale(accent, v / 255.0f); };  // accent at brightness v

  switch (anim) {
    case CONVERGING: {
      // A continuous stream of glowing particles drawn inward toward a hot core,
      // staggered + eased + faded so the loop has no visible reset.
      glow_ring(it, cx, cy, 40.0f, 1.5f, 4.0f, ac(120));
      const int n = 28;
      float base = wrap01(now_ms, 3500.0f);
      for (int i = 0; i < n; ++i) {
        float p = base + (float) i / n;
        if (p >= 1.0f) p -= 1.0f;
        float e = smoothstep(p);
        float px, py;
        converge_point(i, n, cx, cy, 56.0f, e, px, py);
        float fade = std::sin((float) M_PI * p);
        uint8_t b = lerp_u8(40, 255, e);
        glow_disc(it, px, py, 1.0f, 3.5f, ac((uint8_t) (b * fade)));
      }
      float corePulse = 0.65f + 0.35f * breath(now_ms, 1750.0f);
      glow_disc(it, cx, cy, 2.5f, 10.0f, ac((uint8_t) (255 * corePulse)), 0.85f);
      break;
    }
    case ORBITS: {
      // Two slowly counter-rotating rings of glowing points with fading trails.
      const int n = 10;
      const int trail = 5;
      float phaseA = (float) now_ms * 0.0008f;
      float phaseB = -(float) now_ms * 0.0005f;
      for (int i = 0; i < n; ++i) {
        for (int t = 0; t < trail; ++t) {
          float decay = 1.0f - 0.8f * (float) t / trail;
          float ax, ay, bx, by;
          orbit_point(i, n, cx, cy, 34.0f, phaseA - (float) t * 0.06f, ax, ay);
          orbit_point(i, n, cx, cy, 52.0f, phaseB + (float) t * 0.05f, bx, by);
          glow_disc(it, ax, ay, 2.4f, 4.5f, ac((uint8_t) (255 * decay)));
          glow_disc(it, bx, by, 2.2f, 4.0f, ac((uint8_t) (215 * decay)));
        }
      }
      break;
    }
    case WAVEFORM: {
      // Horizontal energy burst: a glowing waveform, brightest at the centre.
      float amp = 8.0f + 14.0f * breath(now_ms, 2000.0f);
      int w = (int) it.get_width(), hgt = (int) it.get_height();
      for (int x = 0; x < w; ++x) {
        float env = std::sin((float) M_PI * x / w);
        float y = cy + wave_y((float) x, now_ms, 0.05f, amp * env, 0.005f);
        for (int dyi = -5; dyi <= 5; ++dyi) {
          int yy = (int) (y + dyi);
          if (yy < 0 || yy >= hgt) continue;
          float h = 1.0f - std::fabs((float) dyi) / 6.0f;
          if (h <= 0.0f) continue;
          it.draw_pixel_at(x, yy, scale(ac(235), h * h * env));
        }
      }
      break;
    }
    case AMBER_PULSE: {
      // Soft amber pulse — gentle, never harsh red (stays amber, ignores accent).
      float p = breath(now_ms, 2500.0f);
      esphome::Color amber(255, (uint8_t) (140 + 60 * p), 0);
      glow_ring(it, cx, cy, breath_radius(34.0f, 8.0f, now_ms, 2500.0f), 2.0f, 5.0f, amber);
      break;
    }
    case DIM_RING: {
      // Very dim, static glowing ring with a small slash.
      glow_ring(it, cx, cy, 36.0f, 1.5f, 3.0f, ac(40));
      it.line((int) (cx - 14), (int) (cy - 14), (int) (cx + 14), (int) (cy + 14), ac(70));
      break;
    }
    case LOADING_ARC: {
      // A faint ring with a bright arc sweeping round to fill it.
      glow_ring(it, cx, cy, 36.0f, 1.5f, 3.0f, ac(45));
      float p = wrap01(now_ms, 1600.0f);
      glow_arc(it, cx, cy, 36.0f, 2.0f, 4.0f, ac(230), -(float) M_PI / 2.0f,
               2.0f * (float) M_PI * p);
      break;
    }
    case SONAR: {
      // Concentric rings pulsing outward and fading (searching).
      const int waves = 3;
      for (int k = 0; k < waves; ++k) {
        float p = wrap01(now_ms + (uint32_t) (k * 1600 / waves), 1600.0f);
        float r = 6.0f + p * 44.0f;
        glow_ring(it, cx, cy, r, 1.5f, 3.0f, ac((uint8_t) (200 * (1.0f - p))));
      }
      break;
    }
    case SCAN_ARC: {
      // A faint ring with a bright arc scanning around it (linking).
      glow_ring(it, cx, cy, 38.0f, 1.5f, 3.0f, ac(45));
      float rot = (float) now_ms * 0.0035f;
      glow_arc(it, cx, cy, 38.0f, 2.0f, 4.0f, ac(235), rot, 0.9f);
      break;
    }
    case ORB:          draw_orb(it, now_ms, accent, orb_siri());     break;
    case CALM_ORB:     draw_orb(it, now_ms, accent, orb_calm());     break;
    case SLEEPING_ORB: draw_orb(it, now_ms, accent, orb_sleeping()); break;
    case AGITATED_ORB: draw_orb(it, now_ms, accent, orb_agitated()); break;
    case SPIKE_ORB:    draw_orb(it, now_ms, accent, orb_spike());    break;
    case HAPPY_ORB:    draw_orb(it, now_ms, accent, orb_happy());    break;
    case BREATHING_RING:
    default: {
      // Slow breathing ring (~8s per breath), sub-pixel radius.
      float r = breath_radius(36.0f, 6.0f, now_ms, 8000.0f);
      uint8_t b = lerp_u8(90, 200, breath(now_ms, 8000.0f));
      glow_ring(it, cx, cy, r, 2.0f, 5.0f, ac(b));
      break;
    }
  }
}

// Clear the screen and draw the given animation.
template<typename D>
void render_anim(D &it, int anim, uint32_t now_ms, esphome::Color accent = cyan()) {
  it.fill(black());
  draw_anim(it, anim, now_ms, accent);
}

// Default animation assigned to each assistant phase (overridable from HA).
inline int default_anim(int phase) {
  switch (phase) {
    case LISTENING: return CONVERGING;
    case THINKING:  return ORBITS;
    case REPLYING:  return WAVEFORM;
    case ERROR:     return AMBER_PULSE;
    case MUTED:     return DIM_RING;
    case BOOTING:   return LOADING_ARC;
    case NO_WIFI:   return SONAR;
    case NO_HA:     return SCAN_ARC;
    case IDLE:
    default:        return BREATHING_RING;
  }
}

// Backward-compatible entry point: render a phase using its default animation.
template<typename D>
void render(D &it, int phase, uint32_t now_ms, esphome::Color accent = cyan()) {
  render_anim(it, default_anim(phase), now_ms, accent);
}

}  // namespace avatar
