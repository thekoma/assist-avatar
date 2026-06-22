# Migrating to v0.4.0

v0.4.0 is a clean breaking change. The avatar is now a self-contained ESPHome
**`external_components` component** (`components/avatar/`), replacing the v0.3.x
header-only library + hand-written `_avatar-body.yaml` page lambdas.

## What changed

- **Consumption model.** Instead of bundling a PlatformIO library and writing
  page lambdas by hand, you now:
  1. add the component via `external_components`,
  2. include `avatar-pages.yaml` *after* the voice-assistant factory package
     (its `!extend` targets must already exist), and
  3. declare a minimal `avatar:` block.

- **Minimal `avatar:` block.** One line per state:
  ```yaml
  avatar:
    idle:      { animation: breathing_ring }
    listening: { animation: converging }
    thinking:  { animation: orb, variation: {} }   # variation: {} requests the variation control
    replying:  { animation: waveform }
    error:     { animation: amber_pulse }
    muted:     { animation: dim_ring }
    booting:   { animation: loading_arc }
    no_wifi:   { animation: sonar }
    no_ha:     { animation: scan_arc }
  ```

- **Auto-generated controls.** Each state mints Home-Assistant entities with
  consistent, unambiguous names (ids in parentheses):
  - anim select — `<State> animation` (`<state>_anim`)
  - speed number — `<State> animation speed` (`<state>_speed`)
  - variation select — `<State> animation variation` (`<state>_variation`, only where the animation has variations)
  - colour light(s) — `<State> accent`[ N] (`<state>_<role>`)
  No per-control `id:`/`name:` boilerplate; override any block only if needed.

- **Settings persist across reboot.** The animation, speed, variation and accent
  colour you pick in Home Assistant survive a device restart. The value in the
  `avatar:` block is the **first-boot default only**.

- **Speed works on every animation.** Previously only the breathing ring honoured
  the speed control; all animations now respond to it.

## Breaking / one-time effects

- **Entity ids changed** vs v0.3.x (notably the orb). Any HA automations or
  dashboards referencing old entity ids must be repointed.
- **Saved values reset once.** Auto-naming sets each entity's `object_id_hash`
  from its new name; the first flash on v0.4.0 resets the restored per-state
  values once. Re-pick your settings once — they persist from then on.

## Engine layout (for contributors)

The render engine source-of-truth lives under the component:
`components/avatar/base/` (base headers + the host catalogue `avatar.h`) and
`components/avatar/animations/<id>/{<id>.h, manifest.yaml}` (per-animation
drop-in folders). The component copies the headers flat into its own dir at
build time (ESPHome cannot ship a user component's subdirectories), so the
component is self-contained and no longer reaches outside `components/avatar/`.
Host tests build with `-Icomponents/avatar/base -Icomponents/avatar`.
