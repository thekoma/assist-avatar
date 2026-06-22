#pragma once
// Host shim: minimal Number base for avatar_number.h.
namespace esphome {
namespace number {
class Number {
 public:
  float state{1.0f};
  void publish_state(float v) { state = v; }
  virtual void control(float) {}
};
}  // namespace number
}  // namespace esphome
