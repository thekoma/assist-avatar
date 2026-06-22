#pragma once
#include "esphome/core/color.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"       // esphome::millis() for preview_trigger
#include "esphome/components/light/light_output.h"
#include "esphome/components/light/light_state.h"
#include "avatar_preview.h"         // avatar::preview_trigger

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
  void set_phase_id(int p) { this->phase_id_ = p; }
  void write_state(esphome::light::LightState *state) override {
    float r, g, b;
    // as_rgb folds in state + brightness; an ON light at full brightness yields
    // the chosen RGB, an OFF light yields black.
    state->current_values.as_rgb(&r, &g, &b);
    this->color_ = esphome::Color((uint8_t) (r * 255), (uint8_t) (g * 255), (uint8_t) (b * 255));
    // This write_state ALSO fires at boot via RESTORE_AND_ON (deferred to
    // LightState::loop()), but that lands within the AVATAR_BOOT_SETTLE_MS window,
    // so preview_trigger's time gate suppresses it. Only genuine post-boot HA
    // changes (many seconds later) arm a preview.
    avatar::preview_trigger(this->phase_id_, esphome::millis());
  }
  esphome::Color get() const { return this->color_; }
 protected:
  int phase_id_{-1};
  esphome::Color color_{0, 255, 217};  // default cyan; overwritten on first write_state
};
}  // namespace avatar
