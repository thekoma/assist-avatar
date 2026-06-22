#pragma once
// Host shim: minimal Select base for avatar_select.h.
#include <string>
#include <optional>
#include <vector>
#include <cstring>

namespace esphome {
namespace select {
class Select {
 public:
  // active_index: returns the current index if a state is published.
  std::optional<size_t> active_index() const {
    if (!has_state_) return std::nullopt;
    for (size_t i = 0; i < options_.size(); ++i)
      if (options_[i] == state_) return i;
    return std::nullopt;
  }
  std::optional<std::string> at(size_t i) const {
    if (i < options_.size()) return options_[i];
    return std::nullopt;
  }
  bool has_index(size_t i) const { return i < options_.size(); }
  std::optional<size_t> index_of(const std::string &v) const {
    for (size_t i = 0; i < options_.size(); ++i)
      if (options_[i] == v) return i;
    return std::nullopt;
  }
  void publish_state(const std::string &v) { state_ = v; has_state_ = true; }
  void traits_set_options(const std::vector<std::string> &o) { options_ = o; }
  virtual void control(const std::string &) {}
  std::string state_;
  bool has_state_{false};
  std::vector<std::string> options_;
};
}  // namespace select
}  // namespace esphome
