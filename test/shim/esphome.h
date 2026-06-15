#pragma once
// Minimal stand-in for ESPHome's esphome.h, ONLY for host-side testing of the
// avatar render code. Provides just enough of esphome::Color for avatar.h /
// avatar_draw.h to compile against a mock display on the host.
#include <cstdint>

namespace esphome {
struct Color {
  uint8_t r, g, b;
  Color(uint8_t r = 0, uint8_t g = 0, uint8_t b = 0) : r(r), g(g), b(b) {}
};
namespace display {
// Minimal stand-in so avatar_draw.h parses on the host. The text helpers are
// templates only instantiated by real ESPHome lambdas, never by the host tests.
enum class TextAlign { TOP_LEFT };
}  // namespace display
}  // namespace esphome
