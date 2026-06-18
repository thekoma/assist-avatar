#!/usr/bin/env bash
# Render every avatar state to an animated GIF in assets/, using the exact device
# render code (via gif_render). Requires ffmpeg.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p assets
TMP=$(mktemp -d)
c++ -std=c++17 -Itest/shim -Isrc tools/gif_render.cpp -o "$TMP/gif_render"

# name anim_index n_frames dt_ms   (anim = catalogue index; fps = 1000/dt)
STATES=(
  "orb_siri 9 90 45"
  "orb_calm 10 90 45"
  "orb_sleeping 11 100 60"
  "orb_agitated 12 90 40"
  "orb_spike 13 90 45"
  "orb_happy 14 90 45"
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
