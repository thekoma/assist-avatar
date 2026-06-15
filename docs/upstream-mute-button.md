# Upstream finding: on-device mute button (GPIO1) is not bridged to firmware state

**Status:** documented, not fixed here (out of scope for the avatar). Candidate
for an issue / PR against
[`esphome/wake-word-voice-assistants`](https://github.com/esphome/wake-word-voice-assistants)
(the `esp32-s3-box-3` config).

This is **not** caused by the assist-avatar overlay — the avatar only re-skins the
display pages. It is an upstream behaviour we hit while testing on real hardware.

## Symptoms (observed on an ESP32-S3-Box-3, firmware built from
`esp32-s3-box-3.factory.yaml@main`, June 2026)

- **Mute from Home Assistant** (toggling the `Mute` switch): the screen changes to
  the muted view and the mic is muted in software — **but the hardware mute LED on
  top of the device does not light**.
- **Pressing the physical mute button**: the **hardware mute LED lights** and the
  mic is cut — **but the screen does not change**, and the audio capture appears to
  desync and **does not recover** afterwards (the firmware still believes the mic
  is active).

The two mute mechanisms are effectively **decoupled**.

## Root cause

The ESP32-S3-Box-3 has a dedicated mute button on **GPIO1** and hardware mute/power
LEDs on top (per the
[Home Assistant S3-Box voice docs](https://www.home-assistant.io/voice_control/s3_box_voice_assistant/)).
The mute button + LED behave as a **hardware** path.

In the current `factory.yaml@main` config, **there is no `binary_sensor` on GPIO1**.
The only GPIO button defined is `left_top_button` on **GPIO0** (short click → stop a
ringing timer; 10 s hold → factory reset). There is also no `output`/`light` driving
a mute LED.

Evidence gathered:
- Merged/expanded config (`esphome config`): only one GPIO `binary_sensor`, on
  `number: 0` (`left_top_button`); no `number: 1`, no mute-button handler, no mute-LED
  output.
- Device logs over the API while pressing the button: **no events logged** — the
  firmware does not see the press at all.
- The `Mute` template switch (`id: mute`) is toggled **only** by Home Assistant. Its
  `on_turn_on` does `microphone.mute` + `voice_assistant_phase = 12` + `draw_display`;
  `on_turn_off` does the inverse. Nothing on the device toggles it.

So: the physical button mutes the mic and lights the LED in hardware, the firmware is
blind to GPIO1, the displayed phase never changes, and the software mic state desyncs
from the hardware mute (the likely cause of the "capture does not recover").

## Proposed fix (for the upstream config)

Read GPIO1 and drive the **same software path** the remote mute uses, so the physical
button and the firmware/UI stay in sync (and the mic is properly muted/unmuted in
software, fixing the stuck capture):

```yaml
binary_sensor:
  - platform: gpio
    id: mute_button
    pin:
      number: GPIO1
      inverted: true          # confirm polarity on hardware
      mode:
        input: true
        pullup: true
    # If GPIO1 is a momentary button:
    on_press:
      - switch.toggle: mute
    # If GPIO1 is a latching/level switch, follow its state instead:
    # on_state:
    #   - if:
    #       condition: { lambda: 'return x;' }
    #       then: [switch.turn_on: mute]
    #       else: [switch.turn_off: mute]
```

**Open question to resolve before wiring:** whether GPIO1 is a **momentary** button
(toggle on press) or a **latching/level** input (follow state), and its exact
polarity. Characterise it first with a logging-only `binary_sensor` on GPIO1, then
pick `on_press` vs `on_state`.

Optionally, if the mute LED is meant to be firmware-driven on this revision, also wire
an output to reflect the mute state.

## References
- HA voice docs: https://www.home-assistant.io/voice_control/s3_box_voice_assistant/
- HA S3-Box-3 customize: https://www.home-assistant.io/voice_control/s3-box-customize/
- Upstream config: https://github.com/esphome/wake-word-voice-assistants
