#pragma once
#include "avatar_module.h"
#include "animations/breathing_ring/breathing_ring.h"
#include "animations/converging/converging.h"
#include "animations/orbits/orbits.h"
#include "animations/waveform/waveform.h"
#include "animations/amber_pulse/amber_pulse.h"
#include "animations/dim_ring/dim_ring.h"
#include "animations/loading_arc/loading_arc.h"
#include "animations/sonar/sonar.h"
#include "animations/scan_arc/scan_arc.h"
#include "animations/orb/orb.h"

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

// Draw one animation. Time-based (`now_ms` = millis()) so speed is refresh-rate
// independent; `accent` recolours every element (amber pulse stays amber). Does
// NOT clear the screen, so overlays can be layered on top by the caller.
template<typename D>
void draw_anim(D &it, int anim, uint32_t now_ms, esphome::Color accent) {
  switch (anim) {
    case CONVERGING:   avatar::mod::converging::render(it, now_ms, avatar::ColorSet::single(accent), 1.0f, 0);   break;
    case ORBITS:       avatar::mod::orbits::render(it, now_ms, avatar::ColorSet::single(accent), 1.0f, 0);       break;
    case WAVEFORM:     avatar::mod::waveform::render(it, now_ms, avatar::ColorSet::single(accent), 1.0f, 0);     break;
    case AMBER_PULSE:  avatar::mod::amber_pulse::render(it, now_ms, avatar::ColorSet::single(accent), 1.0f, 0);  break;
    case DIM_RING:     avatar::mod::dim_ring::render(it, now_ms, avatar::ColorSet::single(accent), 1.0f, 0);     break;
    case LOADING_ARC:  avatar::mod::loading_arc::render(it, now_ms, avatar::ColorSet::single(accent), 1.0f, 0);  break;
    case SONAR:        avatar::mod::sonar::render(it, now_ms, avatar::ColorSet::single(accent), 1.0f, 0);        break;
    case SCAN_ARC:     avatar::mod::scan_arc::render(it, now_ms, avatar::ColorSet::single(accent), 1.0f, 0);     break;
    case ORB:          avatar::mod::orb::render(it, now_ms, avatar::ColorSet::single(accent), 1.0f, 0); break;
    case CALM_ORB:     avatar::mod::orb::render(it, now_ms, avatar::ColorSet::single(accent), 1.0f, 1); break;
    case SLEEPING_ORB: avatar::mod::orb::render(it, now_ms, avatar::ColorSet::single(accent), 1.0f, 2); break;
    case AGITATED_ORB: avatar::mod::orb::render(it, now_ms, avatar::ColorSet::single(accent), 1.0f, 3); break;
    case SPIKE_ORB:    avatar::mod::orb::render(it, now_ms, avatar::ColorSet::single(accent), 1.0f, 4); break;
    case HAPPY_ORB:    avatar::mod::orb::render(it, now_ms, avatar::ColorSet::single(accent), 1.0f, 5); break;
    case BREATHING_RING:
    default: {
      // Delegates to the breathing_ring module (the first extracted module).
      avatar::mod::breathing_ring::render(it, now_ms, avatar::ColorSet::single(accent), 1.0f, 0);
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
