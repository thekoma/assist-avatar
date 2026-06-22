#pragma once
// Host shim: minimal esphome::millis() so the control headers parse/link on host.
// The control()/write_state() paths that call it are never exercised by host
// tests, so the return value is irrelevant — it only needs to parse/link. The
// real esphome/core/hal.h declares `uint32_t millis();`; this inline definition
// is fine since the host build never links the real one.
#include <cstdint>
namespace esphome {
inline uint32_t millis() { return 0; }
}  // namespace esphome
