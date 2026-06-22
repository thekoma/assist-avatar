#pragma once
// Base library for modular animations: colour carrier, capability descriptor,
// and the PSRAM scratch helper. Pulls esphome.h (real on device, shim on host)
// plus the math + neon primitives, so an animation module includes ONLY this.
#include "esphome.h"
#include "avatar_math.h"
#include "avatar_draw.h"
#include <cstddef>

// On device, scratch<T,SLOT> places big buffers in PSRAM via RAMAllocator.
// The host shim has no helpers.h, so this is guarded out and host uses std::vector.
#ifdef USE_ESP32
#include "esphome/core/helpers.h"
#else
#include <vector>
#include <cassert>
#endif

namespace avatar {

inline constexpr uint8_t AVATAR_MAX_COLORS = 4;

// One declared colour slot: a stable role id, a display name, and a default RGB.
struct ColorRole {
  const char *role;
  const char *name;
  uint8_t r, g, b;
};

// Capability descriptor (the C++ view of a manifest). Used by later phases'
// codegen/registry; defined here so module headers can carry one.
struct ModuleCaps {
  const char *id;
  const char *name;
  uint8_t n_colors;
  const ColorRole *roles;
  float speed_min, speed_max, speed_default;
  uint8_t n_variations;
  const char *const *variation_names;
};

// Fixed-capacity, allocation-free colour set passed by const-ref into render().
struct ColorSet {
  esphome::Color c[AVATAR_MAX_COLORS];
  uint8_t n = 0;
  esphome::Color get(uint8_t i) const { return c[i < n ? i : (n ? (uint8_t)(n - 1) : 0)]; }
  esphome::Color primary() const { return get(0); }
  static ColorSet single(esphome::Color a) {
    ColorSet s;
    s.c[0] = a;
    s.n = 1;
    return s;
  }
};

// Lazily-allocated scratch buffer of n elements. Each distinct (T, SLOT) is a
// distinct buffer (so e.g. an accumulator and a blur temp never alias). On
// device the buffer lives in PSRAM (ALLOC_EXTERNAL) and may be nullptr if PSRAM
// is exhausted — the caller MUST null-check and skip the frame. n must be
// constant for a given (T, SLOT). NOT thread-safe; the display runs in one task.
template<typename T, int SLOT>
T *scratch(std::size_t n) {
#ifdef USE_ESP32
  static esphome::RAMAllocator<T> alloc{esphome::RAMAllocator<T>::ALLOC_EXTERNAL};
  static T *p = alloc.allocate(n);
  return p;
#else
  // Host: size the buffer on first use; assert later calls don't ask for more
  // (the device path sizes on first allocate() too, so this guards the shared
  // contract: n must be constant for a given (T, SLOT)).
  static const std::size_t cap = n;
  static std::vector<T> buf(n);
  assert(n <= cap && "scratch<T,SLOT>: n must be constant for a given (T, SLOT)");
  return buf.data();
#endif
}

}  // namespace avatar
