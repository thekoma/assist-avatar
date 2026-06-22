#pragma once
// Host shim: minimal LightOutput / LightTraits / LightState for avatar_color_output.h.
#include <initializer_list>

namespace esphome {
namespace light {

enum class ColorMode { RGB };

class LightTraits {
 public:
  void set_supported_color_modes(std::initializer_list<ColorMode>) {}
};

class LightState {
 public:
  struct Values {
    void as_rgb(float *r, float *g, float *b) const { *r = rv; *g = gv; *b = bv; }
    float rv{0}, gv{1.0f}, bv{0.85f};
  } current_values;
};

class LightOutput {
 public:
  virtual LightTraits get_traits() = 0;
  virtual void write_state(LightState *) = 0;
  virtual ~LightOutput() = default;
};

}  // namespace light
}  // namespace esphome
