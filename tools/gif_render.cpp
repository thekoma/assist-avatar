// Offline frame renderer for documentation GIFs.
//
// Renders the avatar (the exact device render code) for one phase into a series
// of PPM frames, which an external tool (ffmpeg) assembles into an animated GIF.
// This produces faithful previews without needing to screen-record the SDL window.
//
// Usage: gif_render <anim> <n_frames> <dt_ms> <out_dir>   (anim = catalogue index)
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include "avatar.h"  // shim esphome.h via -Itest/shim

struct GifDisplay {
  static constexpr int W = 320, H = 240;
  uint8_t buf[H][W][3];

  int get_width() const { return W; }
  int get_height() const { return H; }
  void put(int x, int y, esphome::Color c) {
    if (x < 0 || x >= W || y < 0 || y >= H) return;
    buf[y][x][0] = c.r; buf[y][x][1] = c.g; buf[y][x][2] = c.b;
  }
  void fill(esphome::Color c) {
    for (int y = 0; y < H; ++y)
      for (int x = 0; x < W; ++x) put(x, y, c);
  }
  void draw_pixel_at(int x, int y, esphome::Color c) { put(x, y, c); }
  void line(int x0, int y0, int x1, int y1, esphome::Color c) {  // Bresenham
    int dx = std::abs(x1 - x0), dy = -std::abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, err = dx + dy;
    while (true) {
      put(x0, y0, c);
      if (x0 == x1 && y0 == y1) break;
      int e2 = 2 * err;
      if (e2 >= dy) { err += dy; x0 += sx; }
      if (e2 <= dx) { err += dx; y0 += sy; }
    }
  }
  void write_ppm(const std::string &path) {
    FILE *f = std::fopen(path.c_str(), "wb");
    std::fprintf(f, "P6\n%d %d\n255\n", W, H);
    std::fwrite(buf, 1, sizeof(buf), f);
    std::fclose(f);
  }
};

int main(int argc, char **argv) {
  if (argc != 5) { std::fprintf(stderr, "usage: %s anim n_frames dt_ms out_dir\n", argv[0]); return 2; }
  int anim = std::atoi(argv[1]);
  int n = std::atoi(argv[2]);
  uint32_t dt = (uint32_t) std::atoi(argv[3]);
  std::string dir = argv[4];
  static GifDisplay d;
  for (int i = 0; i < n; ++i) {
    avatar::render_anim(d, anim, (uint32_t) i * dt, avatar::cyan());
    char name[512];
    std::snprintf(name, sizeof(name), "%s/frame_%04d.ppm", dir.c_str(), i);
    d.write_ppm(name);
  }
  return 0;
}
