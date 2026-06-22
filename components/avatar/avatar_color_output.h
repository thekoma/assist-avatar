#pragma once
#include "esphome/core/color.h"
#include "esphome/core/component.h"
#include "esphome/components/light/light_output.h"
#include "esphome/components/light/light_state.h"

namespace avatar {
// An RGB light whose latest colour the dispatch reads into a ColorSet slot.
// new_light() wraps this output var in a LightState; the page lambda references
// THIS output by its CONF_OUTPUT_ID (e.g. id(idle_ring)) and calls get(). The
// LightState (not this output) is the registered Component, so this output needs
// no register_component / setup of its own — write_state() is driven by the
// LightState whenever the colour changes.
class AvatarColorOutput : public esphome::light::LightOutput {
 public:
  esphome::light::LightTraits get_traits() override {
    esphome::light::LightTraits t;
    t.set_supported_color_modes({esphome::light::ColorMode::RGB});
    return t;
  }
  void write_state(esphome::light::LightState *state) override {
    float r, g, b;
    // as_rgb folds in state + brightness; an ON light at full brightness yields
    // the chosen RGB, an OFF light yields black.
    state->current_values.as_rgb(&r, &g, &b);
    this->color_ = esphome::Color((uint8_t) (r * 255), (uint8_t) (g * 255), (uint8_t) (b * 255));
  }
  esphome::Color get() const { return this->color_; }
 protected:
  esphome::Color color_{0, 255, 217};  // default cyan; overwritten on first write_state
};
}  // namespace avatar
