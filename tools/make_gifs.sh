#!/usr/bin/env bash
# Render avatar previews to animated GIFs in assets/, using the exact device
# render code (via gif_render). Requires ffmpeg.
#   - the 9 per-state DEFAULT animations (the gallery)
#   - the 7 orb variations (siri/calm/sleeping/agitated/spike/happy/wireframe)
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p assets
TMP=$(mktemp -d)
c++ -std=c++17 -Itest/shim -Icomponents/avatar/base -Icomponents/avatar tools/gif_render.cpp -o "$TMP/gif_render"

# Host catalogue indices (components/avatar/base/avatar.h):
#   0 breathing_ring  1 converging  2 orbits  3 waveform  4 amber_pulse  5 dim_ring
#   6 loading_arc  7 sonar  8 scan_arc  9 orb/siri  10 orb/calm  11 orb/sleeping
#   12 orb/agitated  13 orb/spike  14 orb/happy  15 orb/wireframe
render() {  # name anim_index n_frames dt_ms   (fps = 1000/dt)
  local name=$1 phase=$2 n=$3 dt=$4
  local fps=$(( 1000 / dt ))
  local fdir="$TMP/$name"; mkdir -p "$fdir"
  "$TMP/gif_render" "$phase" "$n" "$dt" "$fdir"
  ffmpeg -y -loglevel error -framerate "$fps" -i "$fdir/frame_%04d.ppm" \
    -vf "palettegen=max_colors=64" "$fdir/palette.png"
  ffmpeg -y -loglevel error -framerate "$fps" -i "$fdir/frame_%04d.ppm" -i "$fdir/palette.png" \
    -lavfi "paletteuse" -loop 0 "assets/${name}.gif"
  echo "assets/${name}.gif  ($(du -h "assets/${name}.gif" | cut -f1))"
}

# Per-state default animations (the gallery)
render idle      0 120 66   # breathing_ring (~8s breath, more frames)
render listening 1  90 66   # converging
render thinking  9  90 45   # orb (siri default), a touch smoother
render replying  3  90 66   # waveform
render booting   6  90 66   # loading_arc
render no_wifi   7  90 66   # sonar
render no_ha     8  90 66   # scan_arc
render error     4  90 66   # amber_pulse
render muted     5  90 66   # dim_ring

# Orb variations showcase (the 7 flattened "Orb — …" options)
render orb_siri      9  90 45
render orb_calm      10 90 45
render orb_sleeping  11 110 60
render orb_agitated  12 90 40
render orb_spike     13 90 45
render orb_happy     14 90 45
render orb_wireframe 15 90 45

rm -rf "$TMP"
echo "done."
