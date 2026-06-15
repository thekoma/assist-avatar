#!/usr/bin/env sh
# Reliable emulator dev loop.
#
# ESPHome only re-runs code generation (and re-copies the headers listed under
# `includes:`) when the YAML config_hash changes. Editing only the .h files
# (avatar.h / avatar_draw.h / avatar_math.h) leaves the config_hash unchanged, so
# a plain `esphome run` reports "up to date" and launches the OLD binary.
#
# `esphome clean` forces a fresh build so header edits always take effect. On the
# host platform a clean rebuild is only a few seconds, so this is the safe default
# for iterating on the avatar graphics.
set -e
cd "$(dirname "$0")"
.venv/bin/esphome clean dev-host.yaml
exec .venv/bin/esphome run dev-host.yaml
