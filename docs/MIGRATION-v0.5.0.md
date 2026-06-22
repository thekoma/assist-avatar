# Migration guide: v0.4.0 → v0.5.0

## Breaking changes

### 1. Flattened animation×variation catalogue — variation-select entities removed

In v0.4.0 each orb state had two HA selects: `<State> animation` and a separate
`<State> animation variation`. In v0.5.0 these are merged into a single
`<State> animation` select that lists every animation×variation permutation as a
flat option:

```
Amber Pulse
Breathing Ring
Converging
Dim Ring
Loading Arc
Orb — Agitated
Orb — Calm
Orb — Happy
Orb — Siri
Orb — Sleeping
Orb — Spike
Orb — Wireframe    ← new in v0.5.0
Orbits
Scan Arc
Sonar
Waveform
```

The old `<State> animation variation` entities are **gone**. Home Assistant will
report them as unavailable after the first update; you can delete them from the
HA entity registry. Any saved variation values are discarded — a one-time reset.

### 2. `avatar:` block is now fully optional

You no longer need to declare any `avatar:` block at all. An empty block (or
omitting the key entirely) gives all 9 states their built-in defaults plus the
full HA control set.

Per-state keys accepted in v0.5.0:

| key | type | notes |
|---|---|---|
| `animation` | string | animation id (e.g. `orb`) |
| `variation` | string | variation name (e.g. `calm`) — **string, not `{}`** |
| `speed` | float | first-boot speed multiplier (0.3–10.0) |
| `accent` | string | hex colour, e.g. `"#00FFD9"` — ignored on palette-driven animations |

Old `variation: {}` syntax is **invalid** — replace with `variation: calm` (or
whichever variation name you want as the default).

## New features

### 3. Speed range 0.3×–10×

The `<State> animation speed` number entity now spans 0.3 × to 10 ×
(was 0.1–3×). Existing saved values are clamped into the new range on first boot.

### 4. 2-second preview + banner on control change

Whenever you change an animation, speed, or accent in Home Assistant the display
switches to that state's animation for 2 seconds and shows a brief on-screen
banner, then returns to the current live state. No device restart needed.

### 5. New orb variation: Wireframe

A geometric wireframe-sphere style added as the 7th orb variation. Select it via
the `<State> animation` select: **Orb — Wireframe**.

### 6. Accent rejected on palette-driven animations

If you set an accent colour for a state whose animation uses a fixed colour palette
(`orb`, `amber_pulse`) the engine ignores it and renders the default cyan. The HA
control still exists so you can pre-set a colour for when you switch to an
accent-compatible animation.

## Update checklist

1. Pin to `@v0.5.0` in your device YAML:
   ```yaml
   assist-avatar: github://thekoma/assist-avatar/avatar-remote.yaml@v0.5.0
   ```
2. Remove (or simplify) your `avatar:` block — it is now optional.
3. Replace any `variation: {}` with `variation: <name>` (string).
4. In Home Assistant: delete the stale `<State> animation variation` entities
   from the entity registry (Settings → Devices & services → ESPHome → your device
   → remove unavailable entities).
