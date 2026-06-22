// Standalone preview of a "wireframe particle-sphere" Siri orb (matches the
// teal/purple particle-mesh reference). Self-contained: does NOT touch the
// catalogue. Renders PPM frames for ffmpeg.  Usage: wf <n_frames> <dt_ms> <dir>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>

static constexpr int W = 320, H = 240;
static float acc[H][W][3];

struct Col { float r, g, b; };

// teal -> cyan -> blue -> indigo -> purple -> magenta -> (back to teal)
static Col palette(float u) {
  u -= std::floor(u);
  static const Col k[7] = {
    {0.12f, 0.89f, 0.77f},  // teal
    {0.18f, 0.78f, 0.95f},  // cyan-blue
    {0.20f, 0.45f, 0.98f},  // blue
    {0.42f, 0.28f, 0.95f},  // indigo
    {0.65f, 0.25f, 0.92f},  // purple
    {0.95f, 0.32f, 0.85f},  // magenta
    {0.12f, 0.89f, 0.77f},  // loop
  };
  float f = u * 6.0f; int i = (int) f; float t = f - i;
  t = t * t * (3.0f - 2.0f * t);
  const Col &a = k[i], &b = k[i + 1];
  return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t};
}

static void splat(float px, float py, float rad, Col c, float amp) {
  int ri = (int) (rad + 1.0f);
  float inv = 1.0f / (rad * rad);
  int icx = (int) (px + 0.5f), icy = (int) (py + 0.5f);
  for (int dy = -ri; dy <= ri; ++dy) {
    int y = icy + dy; if (y < 0 || y >= H) continue;
    for (int dx = -ri; dx <= ri; ++dx) {
      int x = icx + dx; if (x < 0 || x >= W) continue;
      float fall = 1.0f - (float) (dx * dx + dy * dy) * inv;
      if (fall <= 0.0f) continue;
      float w = amp * fall * fall;
      acc[y][x][0] += c.r * w; acc[y][x][1] += c.g * w; acc[y][x][2] += c.b * w;
    }
  }
}

int main(int argc, char **argv) {
  if (argc != 4) { std::fprintf(stderr, "usage: %s n dt dir\n", argv[0]); return 2; }
  int n = std::atoi(argv[1]);
  float dt = (float) std::atoi(argv[2]);
  std::string dir = argv[3];

  const float cx = W / 2.0f, cy = H / 2.0f, R = 78.0f;
  const int L = 54;          // meridians (curved bands of dots)
  const int P = 78;          // points per meridian

  for (int f = 0; f < n; ++f) {
    float t = f * dt;
    for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x)
      acc[y][x][0] = acc[y][x][1] = acc[y][x][2] = 0.0f;

    float yaw = t * 0.00045f;
    float cyw = std::cos(yaw), syw = std::sin(yaw);
    float pitch = 0.22f * std::sin(t * 0.00026f);
    float cpt = std::cos(pitch), spt = std::sin(pitch);
    float twist = 1.6f + 0.5f * std::sin(t * 0.0003f);   // swirl of the mesh
    float hue0 = t * 0.00004f;

    for (int m = 0; m < L; ++m) {
      float lon0 = 6.2831853f * m / L;
      for (int p = 0; p <= P; ++p) {
        float lat = -1.5707963f + 3.1415926f * p / P;
        float clat = std::cos(lat), slat = std::sin(lat);
        // swirl: longitude shifts with latitude -> spiralling silk bands
        float lon = lon0 + twist * lat + t * 0.0006f;
        float ux = clat * std::cos(lon), uy = slat, uz = clat * std::sin(lon);
        // rotate: yaw (Y) then pitch (X)
        float x1 = cyw * ux + syw * uz, z1 = -syw * ux + cyw * uz, y1 = uy;
        float y2 = cpt * y1 - spt * z1, z2 = spt * y1 + cpt * z1;
        float depth = (z2 + 1.0f) * 0.5f;               // 0 back .. 1 front
        float px = cx + x1 * R, py = cy + y2 * R;
        Col c = palette(hue0 + (lon * 0.1591549f) + 0.18f * slat);
        float bright = 0.10f + 0.95f * depth * depth;    // front dots much brighter
        float rad = 0.9f + 0.9f * depth;
        splat(px, py, rad, c, bright * 0.55f);
      }
    }

    // bright pearlescent core flare (soft, near centre, slight drift)
    float fx = cx + 6.0f * std::sin(t * 0.0007f), fy = cy + 4.0f * std::cos(t * 0.0009f);
    splat(fx, fy, 26.0f, palette(hue0 + 0.5f), 0.9f);
    splat(fx, fy, 11.0f, {0.85f, 0.95f, 1.0f}, 1.1f);

    // faint rim circle (the sphere's silhouette)
    for (int a = 0; a < 720; ++a) {
      float ang = 6.2831853f * a / 720.0f;
      splat(cx + R * std::cos(ang), cy + R * std::sin(ang), 1.0f,
            palette(hue0 + ang * 0.1591549f), 0.18f);
    }

    // write PPM with a tone map that keeps colour in bright areas
    char name[512];
    std::snprintf(name, sizeof(name), "%s/frame_%04d.ppm", dir.c_str(), f);
    FILE *fp = std::fopen(name, "wb");
    std::fprintf(fp, "P6\n%d %d\n255\n", W, H);
    for (int y = 0; y < H; ++y) {
      for (int x = 0; x < W; ++x) {
        float r = acc[y][x][0], g = acc[y][x][1], b = acc[y][x][2];
        float peak = r > g ? (r > b ? r : b) : (g > b ? g : b);
        if (peak > 1.0f) { float k = 1.0f / peak; r *= k; g *= k; b *= k; }  // preserve hue
        unsigned char px[3] = {
          (unsigned char) (r * 255.0f), (unsigned char) (g * 255.0f), (unsigned char) (b * 255.0f) };
        std::fwrite(px, 1, 3, fp);
      }
    }
    std::fclose(fp);
  }
  return 0;
}
