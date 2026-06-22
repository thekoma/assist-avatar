#!/usr/bin/env bash
# Render each assistant state's DEFAULT animation to an animated GIF in assets/,
# using the exact device render code (via gif_render). Requires ffmpeg.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p assets
TMP=$(mktemp -d)
c++ -std=c++17 -Itest/shim -Icomponents/avatar/base -Icomponents/avatar tools/gif_render.cpp -o "$TMP/gif_render"

# name  anim_index  n_frames  dt_ms   (anim index = host catalogue order; fps = 1000/dt)
#   0 breathing_ring  1 converging  3 waveform  4 amber_pulse  5 dim_ring
#   6 loading_arc  7 sonar  8 scan_arc  9 orb (siri variation)
STATES=(
  "idle 0 120 66"        # breathing_ring (~8s breath, so more frames)
  "listening 1 90 66"    # converging
  "thinking 9 90 45"     # orb (siri) — a touch smoother
  "replying 3 90 66"     # waveform
  "booting 6 90 66"      # loading_arc
  "no_wifi 7 90 66"      # sonar
  "no_ha 8 90 66"        # scan_arc
  "error 4 90 66"        # amber_pulse
  "muted 5 90 66"        # dim_ring
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
