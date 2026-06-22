#pragma once
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "esphome/components/number/number.h"

namespace avatar {
// HA-settable number (optimistic): the page lambda reads ->state and passes it
// to dispatch as the speed multiplier.
// setup() restores the saved float from flash; falls back to the configured default
// value (first-boot seed from the avatar: block) if no saved value is present.
class AvatarNumber : public esphome::number::Number, public esphome::Component {
 public:
  void set_initial_value(float v) { this->initial_value_ = v; }
  void setup() override {
    this->pref_ = this->make_entity_preference<float>();
    float value;
    if (!this->pref_.load(&value)) value = this->initial_value_;   // first-boot default
    this->publish_state(value);
  }
  void control(float value) override {
    this->publish_state(value);
    this->pref_.save(&value);
  }
 protected:
  float initial_value_{1.0f};
  esphome::ESPPreferenceObject pref_;
};
}  // namespace avatar
