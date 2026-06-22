#pragma once
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "esphome/components/select/select.h"

namespace avatar {
// A minimal HA-settable select: optimistic publish on control(). Its active_index()
// is read by the display page lambda to choose which animation to render.
// setup() restores the saved index from flash; falls back to the configured default
// index (first-boot seed from the avatar: block) if no saved value is present.
class AvatarSelect : public esphome::select::Select, public esphome::Component {
 public:
  void set_initial_index(size_t i) { this->initial_index_ = i; }
  void setup() override {
    this->pref_ = this->make_entity_preference<size_t>();
    size_t index = this->initial_index_;          // first-boot default from the avatar: block
    size_t saved;
    if (this->pref_.load(&saved) && this->has_index(saved)) index = saved;  // restored if present
    auto v = this->at(index);
    if (v.has_value()) this->publish_state(v.value());
  }
  void control(const std::string &value) override {
    this->publish_state(value);
    auto idx = this->index_of(value);
    if (idx.has_value()) { size_t i = idx.value(); this->pref_.save(&i); }
  }
 protected:
  size_t initial_index_{0};
  esphome::ESPPreferenceObject pref_;
};
}  // namespace avatar
