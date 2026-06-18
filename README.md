# assist-avatar

An animated avatar for the **ESP32-S3-Box-3** voice assistant (ESPHome + Home
Assistant). A calm, faceless cyan "synthetic mind" in the spirit of the
*Cyberpunk 2077* AIs — neon glow, sub-pixel-smooth motion — that reacts to every
assistant state. Designed to be **child-safe** (no face, no harsh red, slow
organic motion) and to look good at night.

<p align="center">
  <img src="assets/listening.gif" width="320" alt="listening animation">
</p>

It ships as an **ESPHome overlay package**: you keep the official Nabu Casa
voice-assistant firmware exactly as-is (still upgradable) and this package
overrides *only* the display rendering. Wake word, voice pipeline, timers and the
built-in connection screens all keep working — the avatar simply replaces what is
drawn on each screen.

## Gallery

| State | What it means | |
|---|---|---|
| **Idle** | connected and ready, "breathing" | <img src="assets/idle.gif" width="200"> |
| **Listening** | particles stream into a hot core | <img src="assets/listening.gif" width="200"> |
| **Thinking** | counter-rotating orbits with trails | <img src="assets/thinking.gif" width="200"> |
| **Replying** | a horizontal energy burst | <img src="assets/replying.gif" width="200"> |
| **Booting** | loading arc filling up | <img src="assets/booting.gif" width="200"> |
| **No Wi-Fi** | searching the network (sonar waves) | <img src="assets/no_wifi.gif" width="200"> |
| **Connecting to HA** | linking to Home Assistant (scanning arc) | <img src="assets/no_ha.gif" width="200"> |
| **Error** | soft amber pulse | <img src="assets/error.gif" width="200"> |
| **Muted** | dim ring with a slash | <img src="assets/muted.gif" width="200"> |

## How it works

The official S3-Box-3 firmware renders its UI with a `display:` that has one
**page per state** (`idle_page`, `listening_page`, … plus `no_wifi_page`,
`no_ha_page`, `initializing_page`) and switches between them. This package uses
ESPHome's `!extend` directive to replace each page's draw lambda with a call to
`avatar::render(...)`, and bumps the refresh rate so the avatar animates:

```yaml
display:
  - id: !extend s3_box_lcd
    update_interval: 66ms
    pages:
      - id: !extend idle_page
        lambda: 'avatar::render(it, avatar::IDLE, millis());'
      # … one per page
```

Because upstream already decides *which* page to show (based on Wi-Fi / HA / voice
state), the avatar gets every state — including the connection states — for free,
with no extra globals or wiring.

The rendering engine is three small C++ headers:

- **`avatar_math.h`** — pure, dependency-free math (easing, breathing, orbits,
  the `Phase` enum). Unit-tested on the host.
- **`avatar_draw.h`** — neon drawing primitives (`glow_disc`, `glow_ring`,
  `glow_arc`) on ESPHome's display API. Coverage-based and sub-pixel, so motion
  glides instead of crawling, and strokes have an adjustable glow.
- **`avatar.h`** — one `render(display, phase, now_ms)` per state.

Animation is driven by **wall-clock time** (`millis()`), not a frame counter, so
the speed is identical on the desktop emulator and the device (the device is just
less smooth, not slower).

## On-screen text & live colours

- **Terminal text (STT/TTS).** While the assistant is thinking/replying, your
  request and its answer are drawn over the avatar in a phosphor-CRT style
  (VT323 font, soft glow), **typed out** character by character with a blinking
  block cursor. The text comes from the upstream `text_request` / `text_response`
  sensors; `avatar_draw.h::draw_dialog` handles word-wrap and the type-on effect.
- **Live colours from Home Assistant** (no reflash). The package exposes:
  - **`Avatar accent`** (RGB light) — recolours the whole avatar in real time.
  - **`Avatar text`** (RGB light) — the text colour.
  - **`Text colour`** (select) — `Match accent` (text follows the avatar) or
    `Custom` (use the `Avatar text` picker).

  These are virtual RGB lights (they drive no LED); their chosen colour is read
  into a global and passed to `render()` / `draw_dialog()` each frame. The error
  state stays amber regardless (deliberately, for a calm alert).

## Install

### Option A — remote, one line (recommended; works from the HA ESPHome dashboard)

Add **one package line** to your device's YAML and flash. Nothing to copy:

```yaml
packages:
  # the line your device already has (keep it):
  esphome.voice-assistant: github://esphome/wake-word-voice-assistants/esp32-s3-box-3/esp32-s3-box-3.factory.yaml@main
  # add the avatar overlay (must come AFTER the line above):
  assist-avatar: github://thekoma/assist-avatar/avatar-remote.yaml@main
```

Keep your existing `name` / `api:` / `wifi:` exactly as they are, then **Install
→ Wirelessly**. Pin a release by replacing `@main` with a tag. Done.

> **Why `avatar-remote.yaml` and not `avatar-package.yaml`?** ESPHome resolves the
> C++ headers in `includes:` relative to *your* YAML, not to a remote package
> ([esphome#14583](https://github.com/esphome/esphome/issues/14583)), so a plain
> github package can't ship its own headers. `avatar-remote.yaml` pulls this repo
> in as a PlatformIO **library** (`esphome.libraries`) so the headers land on the
> compiler include path; `library.json` keeps it header-only so nothing else is
> built. If your first compile can't find `avatar.h`, use Option B.

### Option B — local files

Prerequisites: ESPHome (e.g. `uv venv && uv pip install esphome`).

1. Clone this repo and enter it (or copy `avatar-package.yaml`, `_avatar-body.yaml`
   and the `src/` folder next to your device YAML — e.g. into HA's `/config/esphome/`).
2. `cp secrets.yaml.example secrets.yaml` and fill it in (2.4 GHz Wi-Fi; generate
   an API key with `openssl rand -base64 32`).
3. `cp esp32-s3-box-3.example.yaml my-box.yaml` and set `name` / `friendly_name`.
4. First flash over USB, then over the air: `esphome run my-box.yaml`.

Then in Home Assistant: add the auto-discovered **ESPHome** device, assign it a
**Voice assistant pipeline**, keep **Wake word engine location = On device**, and
say **"Okay Nabu"** — the screen switches to the listening animation.

## Develop (desktop emulator — no flashing)

ESPHome's SDL backend runs the display on your computer, so you can iterate on the
graphics in seconds. Requires `brew install sdl2 libsodium` (macOS) and ESPHome
installed natively.

```bash
./dev.sh          # = esphome clean + run dev-host.yaml  (a window opens)
```

Click the window to cycle through all states. Edit any `.h` and re-run `./dev.sh`.

> **Why `./dev.sh` and not a bare `esphome run`?** ESPHome only re-runs code
> generation (and re-copies the `includes:` headers) when the YAML changes.
> Editing only the `.h` files leaves the config hash unchanged, so a plain run
> launches the *old* binary. `dev.sh` does a `clean` first to avoid that.

## Tests

The pure math and the render bounds are tested on the host (no hardware):

```bash
c++ -std=c++17 -Isrc test/test_avatar_math.cpp -o /tmp/t && /tmp/t
c++ -std=c++17 -Itest/shim -Isrc test/test_render.cpp -o /tmp/t && /tmp/t
```

`test_render.cpp` runs every state across a time sweep with a mock display that
fails on any out-of-bounds draw — it catches the kind of off-screen write that can
crash the device, on the host, before flashing.

## Regenerate the GIFs

```bash
./tools/make_gifs.sh    # renders every state to assets/<state>.gif (needs ffmpeg)
```

## Credits

Built on the official [ESPHome wake-word voice assistants](https://github.com/esphome/wake-word-voice-assistants).
Avatar engine and packaging by [@thekoma](https://github.com/thekoma). MIT licensed.
