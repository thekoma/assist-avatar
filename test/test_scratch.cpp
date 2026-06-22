// Host test for the base library: ColorSet semantics + scratch<T,SLOT> distinctness.
#include <cstdio>
#include <cstdint>
#include "avatar_module.h"  // pulls shim esphome.h via -Itest/shim

int main() {
  int fails = 0;

  // ColorSet: get() never reads out of bounds; primary() == slot 0.
  avatar::ColorSet cs = avatar::ColorSet::single(esphome::Color(10, 20, 30));
  if (cs.n != 1) { std::printf("FAIL: single() n=%d\n", cs.n); fails++; }
  if (cs.primary().g != 20) { std::printf("FAIL: primary g\n"); fails++; }
  if (cs.get(3).g != 20) { std::printf("FAIL: get() OOB should clamp to last\n"); fails++; }
  avatar::ColorSet empty;  // n == 0
  if (empty.get(0).r != 0) { std::printf("FAIL: empty get(0) not (0,0,0)\n"); fails++; }

  // scratch: distinct (T,SLOT) => distinct buffers (the orb acc/tmp aliasing guard).
  const std::size_t N = 1024;
  uint16_t *a = avatar::scratch<uint16_t, 0>(N);
  uint16_t *b = avatar::scratch<uint16_t, 1>(N);
  if (a == nullptr || b == nullptr) { std::printf("FAIL: scratch returned null on host\n"); fails++; }
  if (a == b) { std::printf("FAIL: scratch<uint16_t,0> aliases scratch<uint16_t,1>\n"); fails++; }
  // Same (T,SLOT) is stable across calls.
  if (avatar::scratch<uint16_t, 0>(N) != a) { std::printf("FAIL: scratch<uint16_t,0> not stable\n"); fails++; }
  // Fill a and b fully (IN BOUNDS) with distinct patterns; verify writing b
  // never disturbs a — i.e. scratch<,0> and scratch<,1> are truly distinct memory.
  for (std::size_t i = 0; i < N; ++i) a[i] = 0xAAAA;
  for (std::size_t i = 0; i < N; ++i) b[i] = 0x5555;
  if (a[0] != 0xAAAA || a[N - 1] != 0xAAAA) {
    std::printf("FAIL: writing scratch<uint16_t,1> disturbed scratch<uint16_t,0> (buffers alias)\n");
    fails++;
  }

  if (fails) { std::printf("test_scratch: %d FAIL\n", fails); return 1; }
  std::printf("test_scratch: all passed\n");
  return 0;
}
