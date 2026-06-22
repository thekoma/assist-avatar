#pragma once
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "esphome/core/hal.h"       // esphome::millis() for preview_trigger
#include "esphome/components/number/number.h"
#include "avatar_preview.h"         // avatar::preview_trigger

namespace avatar {
// HA-settable number (optimistic): the page lambda reads ->state and passes it
// to dispatch as the speed multiplier.
// setup() restores the saved float from flash; falls back to the configured default
// value (first-boot seed from the avatar: block) if no saved value is present.
class AvatarNumber : public esphome::number::Number, public esphome::Component {
 public:
  void set_initial_value(float v) { this->initial_value_ = v; }
  void set_phase_id(int p) { this->phase_id_ = p; }
  void setup() override {
    this->pref_ = this->make_entity_preference<float>();
    float value;
    if (!this->pref_.load(&value)) value = this->initial_value_;   // first-boot default
    this->publish_state(value);
  }
  void control(float value) override {
    this->publish_state(value);
    this->pref_.save(&value);
    // Genuine HA-initiated change: arm a 2s preview of this state (no-op during
    // the boot-settle window — see avatar_preview.h's time gate).
    avatar::preview_trigger(this->phase_id_, esphome::millis());
  }
 protected:
  float initial_value_{1.0f};
  int phase_id_{-1};
  esphome::ESPPreferenceObject pref_;
};
}  // namespace avatar
