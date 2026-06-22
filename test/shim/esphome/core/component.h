#pragma once
// Host shim: minimal Component + ESPPreferenceObject so entity headers parse.
#include <cstddef>

namespace esphome {

struct ESPPreferenceObject {
  template<typename T>
  bool load(T *) { return false; }
  template<typename T>
  bool save(T *) { return true; }
};

class Component {
 public:
  virtual void setup() {}
  template<typename T>
  ESPPreferenceObject make_entity_preference() { return {}; }
};

}  // namespace esphome
