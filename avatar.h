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

// Templated on the display type so the same code compiles for SDL and mipi_spi.
//
// Driven by wall-clock time (`now_ms`, e.g. millis()), NOT a per-frame counter,
// so animation speed is independent of the display's refresh rate: the same
// motion plays identically on the 60fps emulator and the slower device. All
// periods/rates below are expressed in real time (milliseconds).
// `accent` recolours every cyan element at runtime (e.g. from a Home Assistant
// RGB light); it defaults to cyan so existing callers/emulator/tests are
// unchanged. The amber error pulse stays amber regardless (child-safe).
template<typename D>
void render(D &it, int phase, uint32_t now_ms, esphome::Color accent = cyan()) {
  it.fill(black());
  const float cx = it.get_width() / 2.0f;
  const float cy = it.get_height() / 2.0f;
  auto ac = [&](uint8_t v) { return scale(accent, v / 255.0f); };  // accent at brightness v

  switch (phase) {
    case LISTENING: {
      // A continuous stream of glowing particles drawn inward toward a hot
      // core. Particles are staggered (the ring is always evenly fed, no global
      // pulse), ease inward (smoothstep) and fade to zero at both ends so the
      // loop has no visible "teleport" reset.
      glow_ring(it, cx, cy, 40.0f, 1.5f, 4.0f, ac(120));
      const int n = 28;
      float base = wrap01(now_ms, 3500.0f);
      for (int i = 0; i < n; ++i) {
        float p = base + (float) i / n;
        if (p >= 1.0f) p -= 1.0f;            // staggered position along the path
        float e = smoothstep(p);             // eased inward travel
        float px, py;
        converge_point(i, n, cx, cy, 56.0f, e, px, py);
        float fade = std::sin((float) M_PI * p);  // 0 at ends -> seamless loop
        uint8_t b = lerp_u8(40, 255, e);
        glow_disc(it, px, py, 1.0f, 3.5f, ac((uint8_t) (b * fade)));
      }
      // Hot core last so it stays brightest, pulsing gently as it "draws in".
      float corePulse = 0.65f + 0.35f * breath(now_ms, 1750.0f);
      glow_disc(it, cx, cy, 2.5f, 10.0f, ac((uint8_t) (255 * corePulse)), 0.85f);
      break;
    }
    case THINKING: {
      // Two slowly counter-rotating rings of glowing points, each with a short
      // fading trail (follow-through) — "processing".
      const int n = 10;
      const int trail = 5;
      float phaseA = (float) now_ms * 0.0008f;   // ~0.8 rad/s
      float phaseB = -(float) now_ms * 0.0005f;   // ~0.5 rad/s, opposite way
      for (int i = 0; i < n; ++i) {
        for (int t = 0; t < trail; ++t) {
          float decay = 1.0f - 0.8f * (float) t / trail;  // head bright, tail faint
          float ax, ay, bx, by;
          orbit_point(i, n, cx, cy, 34.0f, phaseA - (float) t * 0.06f, ax, ay);
          orbit_point(i, n, cx, cy, 52.0f, phaseB + (float) t * 0.05f, bx, by);
          glow_disc(it, ax, ay, 2.4f, 4.5f, ac((uint8_t) (255 * decay)));
          glow_disc(it, bx, by, 2.2f, 4.0f, ac((uint8_t) (215 * decay)));
        }
      }
      break;
    }
    case REPLYING: {
      // Horizontal energy burst: a glowing waveform spreading from a hot core,
      // brightest at the centre and fading at the screen edges.
      float amp = 8.0f + 14.0f * breath(now_ms, 2000.0f);
      int w = (int) it.get_width(), hgt = (int) it.get_height();
      for (int x = 0; x < w; ++x) {                  // x < w: column w is off-screen
        float env = std::sin((float) M_PI * x / w);  // fade at the screen edges
        float y = cy + wave_y((float) x, now_ms, 0.05f, amp * env, 0.005f);
        for (int dyi = -5; dyi <= 5; ++dyi) {  // vertical glow around the curve
          int yy = (int) (y + dyi);
          if (yy < 0 || yy >= hgt) continue;
          float h = 1.0f - std::fabs((float) dyi) / 6.0f;
          if (h <= 0.0f) continue;
          it.draw_pixel_at(x, yy, scale(ac(235), h * h * env));
        }
      }
      break;
    }
    case ERROR: {
      // Soft amber pulse — gentle, never harsh red (kept amber regardless of accent).
      float p = breath(now_ms, 2500.0f);
      esphome::Color amber(255, (uint8_t) (140 + 60 * p), 0);
      glow_ring(it, cx, cy, breath_radius(34.0f, 8.0f, now_ms, 2500.0f), 2.0f, 5.0f, amber);
      break;
    }
    case MUTED: {
      // Very dim, static glowing ring with a small slash to signal "off".
      glow_ring(it, cx, cy, 36.0f, 1.5f, 3.0f, ac(40));
      it.line((int) (cx - 14), (int) (cy - 14), (int) (cx + 14), (int) (cy + 14),
              ac(70));
      break;
    }
    case BOOTING: {
      // "Loading": a faint ring with a bright arc sweeping round to fill it.
      glow_ring(it, cx, cy, 36.0f, 1.5f, 3.0f, ac(45));
      float p = wrap01(now_ms, 1600.0f);
      glow_arc(it, cx, cy, 36.0f, 2.0f, 4.0f, ac(230), -(float) M_PI / 2.0f,
               2.0f * (float) M_PI * p);
      break;
    }
    case NO_WIFI: {
      // Searching for the network: concentric rings pulsing outward and fading.
      const int waves = 3;
      for (int k = 0; k < waves; ++k) {
        float p = wrap01(now_ms + (uint32_t) (k * 1600 / waves), 1600.0f);
        float r = 6.0f + p * 44.0f;
        glow_ring(it, cx, cy, r, 1.5f, 3.0f, ac((uint8_t) (200 * (1.0f - p))));
      }
      break;
    }
    case NO_HA: {
      // Wifi up, linking to Home Assistant: a faint ring with a bright arc
      // scanning around it.
      glow_ring(it, cx, cy, 38.0f, 1.5f, 3.0f, ac(45));
      float rot = (float) now_ms * 0.0035f;
      glow_arc(it, cx, cy, 38.0f, 2.0f, 4.0f, ac(235), rot, 0.9f);
      break;
    }
    case IDLE:
    default: {
      // Slow breathing ring (~8s per breath); the sub-pixel radius grows and
      // shrinks smoothly.
      float r = breath_radius(36.0f, 6.0f, now_ms, 8000.0f);
      uint8_t b = lerp_u8(90, 200, breath(now_ms, 8000.0f));
      glow_ring(it, cx, cy, r, 2.0f, 5.0f, ac(b));
      break;
    }
  }
}

}  // namespace avatar
