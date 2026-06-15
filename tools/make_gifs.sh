#!/usr/bin/env bash
# Render every avatar state to an animated GIF in assets/, using the exact device
# render code (via gif_render). Requires ffmpeg.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p assets
TMP=$(mktemp -d)
c++ -std=c++17 -Itest/shim -I. tools/gif_render.cpp -o "$TMP/gif_render"

# name phase n_frames dt_ms   (fps = 1000/dt, durata = n*dt; scelte per buoni loop)
STATES=(
  "idle 0 80 100"
  "listening 1 70 50"
  "thinking 2 72 55"
  "replying 3 60 50"
  "error 4 50 50"
  "muted 5 8 50"
  "booting 6 32 50"
  "no_wifi 7 32 50"
  "no_ha 8 36 50"
)

for s in "${STATES[@]}"; do
  set -- $s
  name=$1; phase=$2; n=$3; dt=$4
  fps=$(( 1000 / dt ))
  fdir="$TMP/$name"; mkdir -p "$fdir"
  "$TMP/gif_render" "$phase" "$n" "$dt" "$fdir"
  ffmpeg -y -loglevel error -framerate "$fps" -i "$fdir/frame_%04d.ppm" \
    -vf "palettegen=max_colors=64" "$fdir/palette.png"
  ffmpeg -y -loglevel error -framerate "$fps" -i "$fdir/frame_%04d.ppm" -i "$fdir/palette.png" \
    -lavfi "paletteuse" -loop 0 "assets/${name}.gif"
  echo "assets/${name}.gif  ($(du -h "assets/${name}.gif" | cut -f1))"
done
rm -rf "$TMP"
echo "done."
