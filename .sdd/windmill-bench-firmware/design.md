# Design: Windmill Bench Firmware

**Version:** 1.6
**Date:** 2026-10-03
**Status:** Approved
**Linked Specification** `.sdd/windmill-bench-firmware/specification.md`

---

# Design Document

---

## Architecture Overview

### Current Architecture Context
- The repository holds `spec.md`, `.gitignore` and `.sdd/`. It has no firmware, and the C3 runs the web-installer default.
- The draft config in `spec.md` "ESPHome configuration" fails validation (F1). It also has no brightness cap (F3), and its button stops working after one press (F4). This design replaces it.

### Proposed Architecture
- **Structure.** `windmill.yaml` holds the node-level config and all substitutions. It includes three packages: `mill_sails` (stepper, speed, run switch, direction switch), `mill_lights` (strip, four partitions) and `mill_controls` (button, mill scripts, long-press reverse). Dependencies point one way: controls → sails + lights. The controls package uses `mill_sails_turn` and `mill_sails_reverse_switch` from the sails package. There is one configuration and no bench package (spec v1.8).
- **Endless running.** The sails never get a fixed far target. The script `mill_sails_rearm` makes two calls in one action list: `report_position(0)` (re-base), then `set_target(direction × 10,000,000)` (re-arm). It runs at turn-on, at each direction change, and from a 10-minute `interval` while the sails turn.
- **No overflow.** At 320 steps/s the span lasts about 8.7 h, 52 times the re-arm period. Positions stay within ±10.2 million, so neither the int32 position nor `abs(target − current)` in `Stepper::calculate_speed_` can overflow.
- **No hitch.** The ULN2003 takes its coil phase from its own counter (`current_uln_pos_`), not from `current_position`, so a re-base changes no coil output. The script runs in one main-loop callback, so the stepper `loop()` cannot run between the two calls. A reverse is the same re-arm with the opposite sign: the next step goes the other way, with no stop and no change to "Sails Turning".
- **Stop, hold, boot.** Turn-off sets the target to `current_position`, so the next stepper `loop()` takes no step (FR-02). `sleep_when_done: false` keeps the coils energised (FR-04). Every light and switch has `restore_mode: ALWAYS_OFF`, and the direction global does not restore. The template switch runs its turn-off action at setup, and the stepper starts with position = target = 0. So no `on_boot` block is needed (FR-20).
- **Fades with effects.** ESPHome applies the default transition only when a call sets no effect (`light_call.cpp`). So `mill_on` turns the lights on without an effect, which gives the 3 s fade. It then starts Lamplight on each interior light that is still on. An HA turn-on that requests Lamplight starts with no fade, which FR-41 (spec v1.5) allows.
- **Direction.** "Reverse Rotation" is a permanent production switch in the sails package (spec v1.8). It sets the `mill_sails_reverse` global. While the sails turn, it re-arms them, so they reverse at once. While they are stopped, it only sets the direction for the next start. A long press of the Mill Button (1–5 s) toggles this switch while the sails turn (spec v1.9), so HA shows the change and the switch's own actions re-arm the sails. Only the switch sets the global.

### Technology Decisions
- **Firmware:** ESPHome 2026.9.1 YAML on `esp-idf`, board `esp32-c3-devkitm-1`. Only built-in components. No lambda is longer than two lines (handbook).
- **Tools:** the ESPHome CLI and yamllint, pinned in `requirements.txt` and run in a Python venv.
- **Logging:** `logger: baud_rate: 0, level: INFO`. Logs leave the device over the native API only. ESPHome's logger routes ESP-IDF log lines to the API, but lines printed before the logger starts, and early and ISR lines, go straight to the USB-Serial-JTAG console, so `esp32: framework: log_level: NONE` is needed for FR-28 (ESPHome's default is ERROR). The cost: ESP-IDF error lines no longer reach `esphome logs`.
- **Network:** `reboot_timeout: 0s` on WiFi and the API. The default `ap_timeout` (90 s) meets FR-24's 2-minute bound.

### Quality Attributes
- **NFR-01:** `color_correct: [60%, 60%, 60%, 60%]` is on the strip and on each partition. Every pixel write goes through a partition: from HA, from Lamplight and from `mill_on`. The strip is `internal`, and no action targets it (no `addressable_set`).
- **NFR-02:** `ALWAYS_OFF` is on all five lights and both switches, and only `Sail Speed` restores a value. Speed stays in 60–320 by three means: the number bounds; rounding, which maps [60,320] onto multiples of 10 inside [60,320]; and, at turn-on, the same half-up rounding followed by a clamp to 60–320, where NaN gives 170. Pins exist only as substitutions in `windmill.yaml`: GPIO0, GPIO1, GPIO3 and GPIO4 for the stepper (GPIO2 unconnected), GPIO6 for pixel data and GPIO5 for the button.
- **Loop timing (single core):** The stepper takes at most one step per `loop()`, so blocking work in a loop pass can delay a step and lower the real speed. Each Lamplight effect returns early until its `update_interval` (50 ms) has passed, so it writes at the same rate whatever the loop rate. The strip's `max_refresh_rate` (20 ms) also limits frames to 50 per second. Each frame blocks for about 0.1–0.2 ms (`rmt_tx_wait_all_done` and a 50 µs delay), so Lamplight blocks for at most about 1% of loop time. Toggles write no flash, because no switch or global restores. `flash_write_interval` (60 s) batches speed saves. The re-arm is O(1) every 10 minutes. The only log lines are one INFO line per button toggle and one per long-press reverse. All objects are allocated at setup.
- **FR-14:** Steady lights with no effect send no frames. When frames are sent, the RMT hardware times the bits, so loop timing cannot change them. The one remaining firmware risk is a delayed RMT refill (see Risks); otherwise flicker comes from the hardware: the capacitors, the 330R resistor and the routing.

---

## API Design

**HA entities (production):**
- **Sails Turning** (switch `mill_sails_turn`, optimistic, boots off). On: sets the speed from Sail Speed, then re-arms. Turning it on while on only re-arms. Off: stops the sails at once. It stays on in either direction.
- **Sail Speed** (number `mill_sail_speed`): 60–320 steps/s, step 10, default 170, restored across restarts. Out of range: ESPHome rejects the value, logs a warning and keeps the state. In range but not a multiple of 10: the firmware rounds half up, and HA only sees the rounded value. A change applies at once, in either direction.
- **Mill Door Glow** (pixel 1), **Mill Stone Floor Window** (2), **Mill Bin Floor Window** (3): RGBW lights with brightness and the "Lamplight" effect. They fade over 3 s by default and boot off. **Mill Door Lamp** (0) is the same, without effects.
- **Reverse Rotation** (switch `mill_sails_reverse_switch`, optimistic, boots off): sets `mill_sails_reverse`, and re-arms if the sails turn. While the sails are stopped, it only sets the direction for the next start. A short press never changes it; a long press toggles it while the sails turn.
- **Mill Button** (binary sensor `mill_button`): on while pressed. Acts on release: a press of 50–500 ms toggles the whole mill; a press of 1–5 s toggles "Reverse Rotation" while "Sails Turning" is on, and does nothing while the sails are stopped. Any other press does nothing. The raw strip `mill_pixels` is `internal` and not in HA.

**Scripts (firmware-internal, no parameters):** `mill_sails_rearm` re-bases and re-arms. `mill_on` and `mill_off` turn the whole mill on or off. `mill_toggle` chooses on or off and logs the choice. `mill_reverse_from_button` toggles "Reverse Rotation" while the sails turn and logs the reversal.

**Substitutions (set in `windmill.yaml`; the Option B hub changes only these):**
- `name`, `friendly_name`; `sails_pin_a`–`sails_pin_d` (GPIO0, GPIO1, GPIO3, GPIO4; GPIO2 stays unconnected because it is a boot-strapping pin), `pixel_pin` (GPIO6), `button_pin` (GPIO5).
- `sails_forward_direction` (`"1"` or `"-1"`): the operator sets it on the bench so that forward is anticlockwise from the sail side (AT-01).

**Commands:**
- Setup and checks: `python3 -m venv .venv && . .venv/bin/activate && pip install -r requirements.txt`, then `cp secrets.example.yaml secrets.yaml`, then `scripts/check.sh` (exits non-zero on the first failure). AT-25 and AT-26: `git check-ignore secrets.yaml`, `git log --all -- secrets.yaml`, `esphome version`, `yamllint --version`.
- Device: `esphome run windmill.yaml --device /dev/ttyACM0` (first flash), `esphome run windmill.yaml --device village-windmill.local` (OTA), `esphome logs windmill.yaml` (logs over the API).

---

## Components

### Modified

None. The `spec.md` pointer to the firmware files is deferred (see Feasibility Review). `.gitignore` keeps its entries, including `secrets.yaml` (FR-35).

### Added

#### Node config — `windmill.yaml`
- **Responsibility:** Holds the node identity, board, network, security, logger and pin substitutions, and includes the packages.
- **Consumers:** ESPHome CLI, `scripts/check.sh`.
- **Location:** `windmill.yaml`
- **Kind:** ESPHome top-level config.
- **Details:**
  ```
  substitutions: name village-windmill, friendly_name Windmill, sails_pin_a..d GPIO0, GPIO1, GPIO3, GPIO4,
                 pixel_pin GPIO6, button_pin GPIO5, sails_forward_direction "1"
  esphome: {name ${name}, friendly_name ${friendly_name}}
  esp32: {board esp32-c3-devkitm-1, framework: {type esp-idf, log_level NONE}}   logger: {baud_rate 0, level INFO}
  api: {encryption.key !secret api_key, reboot_timeout 0s}   ota: [{platform esphome, password !secret ota_password}]
  wifi: {ssid/password !secret, reboot_timeout 0s, ap: {ssid "Windmill Fallback", password !secret ap_password}}   captive_portal:
  packages: {sails/lights/controls: !include packages/mill_*.yaml}
  ```
- **Rationale:** FR-21, FR-22 and FR-23: `reboot_timeout 0s` keeps the node running through a network loss, and WiFi and the API reconnect on their own. FR-24: the fallback AP. FR-25: the encrypted API. FR-26: the OTA password. FR-27: logs over the API at INFO. FR-28: `baud_rate 0` and the ESP-IDF `log_level NONE`.

#### Sails package — `packages/mill_sails.yaml`
- **Responsibility:** Turns, stops, holds and reverses the sails, with no time limit, and offers the direction switch.
- **Consumers:** `mill_controls`, HA.
- **Location:** `packages/mill_sails.yaml`
- **Kind:** ESPHome package.
- **Details:**
  ```
  stepper: uln2003 id mill_sails, pins ${sails_pin_a..d} (pin c: ignore_strapping_warning true),
           FULL_STEP, max_speed 170 steps/s, sleep_when_done false
  globals: mill_sails_reverse bool "false", no restore
  switch: template "Sails Turning" id mill_sails_turn, optimistic, ALWAYS_OFF
    turn_on:  stepper.set_speed clamp(floor((speed + 5) / 10) * 10, 60, 320), NaN → 170; script.execute mill_sails_rearm
    turn_off: stepper.set_target current_position
  switch: template "Reverse Rotation" id mill_sails_reverse_switch, optimistic, ALWAYS_OFF
    on/off: globals.set mill_sails_reverse true/false; if switch.is_on mill_sails_turn → script.execute mill_sails_rearm
  script mill_sails_rearm: report_position 0; set_target ${sails_forward_direction} * (reverse ? -1 : 1) * 10000000
  interval 10min: if switch.is_on mill_sails_turn → script.execute mill_sails_rearm
  ```
- **Rationale:** FR-01: the periodic re-arm keeps the sails turning with no time limit. FR-02: the sails stop at once. FR-03: the switch state does not depend on direction. FR-04: the coils stay energised. FR-05 and NFR-02: turn-on uses the same half-up rounding to a multiple of 10 as the number, then clamps to 60–320, and NaN gives 170. FR-20: both switches are `ALWAYS_OFF` and the direction global does not restore, so the sails start forward after every restart. FR-30: the re-arm multiplies the forward direction by the reverse sign. FR-31: a direction change while the sails turn is the same re-arm with the opposite sign, with no stop. FR-42: while the sails are stopped, the switch only sets the global, so nothing moves until the next start.

#### Sail Speed number — in `packages/mill_sails.yaml`
- **Responsibility:** Holds, validates, rounds, restores and applies the sail speed.
- **Consumers:** HA, and the turn-on action of `mill_sails_turn`.
- **Location:** `packages/mill_sails.yaml`
- **Kind:** Template number.
- **Details:**
  ```
  number: template "Sail Speed" id mill_sail_speed, min 60, max 320, step 10, initial_value 170,
          unit steps/s, optimistic false, restore_value true
    set_action: r = floor((x + 5) / 10) * 10
      if r == x: publish_state(x); stepper.set_speed x
      else:      delay 0ms; number.set mill_sail_speed r     # one loop pass later
  ```
- **Rationale:** FR-05: the bounds, step and default. FR-07: `NumberCall` rejects out-of-range values before `set_action` runs. FR-06: `TemplateNumber::control` saves the raw value after `set_action`. The deferred set therefore saves the rounded value last. HA only sees multiples of 10, and the stepper never gets an unrounded speed. FR-08: `set_speed` acts at once. The default acceleration is 1e6 steps/s², which is in effect instant, so the next step uses the new rate. FR-09: preferences flush every 60 s.

#### Lights package — `packages/mill_lights.yaml`
- **Responsibility:** Provides four capped, independent single-pixel lights.
- **Consumers:** HA, `mill_controls`.
- **Location:** `packages/mill_lights.yaml`
- **Kind:** ESPHome package.
- **Details:**
  ```
  light: esp32_rmt_led_strip id mill_pixels "Mill Pixels" internal, pin ${pixel_pin}, num_leds 4, chipset SK6812,
         channel_colors GRBW, color_correct [60%,60%,60%,60%], max_refresh_rate 20ms, ALWAYS_OFF
  partition × 4: mill_door_glow | mill_stone_window | mill_bin_window | mill_door_lamp,
         segments [mill_pixels from N to N], color_correct [60%,60%,60%,60%], ALWAYS_OFF, default_transition_length 3s
  interior three only: effects [addressable_flicker name Lamplight, update_interval 50ms, intensity 10%]
  ```
- **Rationale:** FR-10: one pixel per light, with GRBW order. FR-11: each partition is its own `LightState`, and its Lamplight draws its own random values. Lamplight changes the pixel only once per `update_interval`, so it looks the same at any loop rate, and `max_refresh_rate` is a second limit on frames (see Loop timing). The effect writes through the partition's view, so the partition's 60% cap applies to every flicker value (NFR-01). The interval and intensity are Phase 4 starting values. The intensity must stay above 0%, because the effect takes a random value modulo the intensity. FR-12: the door lamp has no effects. FR-13 and NFR-01: the cap is on every partition and on the strip. FR-14: steady lights send no frames. FR-20: `ALWAYS_OFF`. FR-41: the 3 s default fade.

#### Controls package — `packages/mill_controls.yaml`
- **Responsibility:** Turns the whole mill on and off from the button, and reverses the turning sails with a long press, with no network needed.
- **Consumers:** The operator, through the button.
- **Location:** `packages/mill_controls.yaml`
- **Kind:** ESPHome package (binary sensor and four scripts).
- **Details:**
  ```
  binary_sensor: gpio "Mill Button" id mill_button, ${button_pin} input pullup inverted, delayed_on_off 20ms,
    on_click {min 50ms, max 500ms} → script.execute mill_toggle
             {min 1s,   max 5s}    → script.execute mill_reverse_from_button   # no other handler
  mill_reverse_from_button: if switch.is_on mill_sails_turn → switch.toggle mill_sails_reverse_switch;
               logger.log level INFO tag mill.controls "Button: sails reversed"     # stopped: nothing
  mill_toggle: sails on or any of 4 lights on → mill_off; logger.log level INFO tag mill.controls "Button: mill off"
               else → mill_on; logger.log level INFO tag mill.controls "Button: mill on"
  mill_off: script.stop mill_on; switch.turn_off mill_sails_turn; light.turn_off × 4 (3 s fade, stops effect)
  mill_on (mode restart): switch.turn_on mill_sails_turn; interior × 3 at 100%, lamp at 85%, rgb 100/76/52 (gives 255/120/40 after gamma 2.8);
           delay 3s; each interior light still on → light.turn_on effect Lamplight
  ```
- **Rationale:** FR-15 and FR-16: the toggle checks all five outputs, which removes F4. FR-17: the scripts use only firmware entities, so they work without a network. FR-18: every change publishes to HA at once. FR-19: the button state shows in HA. FR-40: the second click range (1–5 s, on release) toggles the "Reverse Rotation" switch, not the global, so HA sees the change and the switch's own actions re-arm the sails with no stop. FR-43: the condition on `mill_sails_turn` makes a long press while stopped do nothing. FR-44: no handler covers 500 ms–1 s or more than 5 s. FR-27: all three `logger.log` calls set level INFO, because the default is DEBUG, and all use the tag `mill.controls`. FR-32: `mill_toggle`, `mill_on` and `mill_off` never name the direction switch, and the package never names the `mill_sails_reverse` global. FR-41: lights fade first, then the effect starts. The colour values are Phase 4 starting values.

#### Example secrets — `secrets.example.yaml`
- **Responsibility:** Holds placeholders that validate and compile.
- **Consumers:** The reviewer, who copies it to `secrets.yaml`.
- **Location:** `secrets.example.yaml`
- **Kind:** Repository file.
- **Details:** `wifi_ssid`, `wifi_password`, `ap_password` and `ota_password` hold "example-…" values of 8 or more characters. `api_key` holds a valid 44-character base64 placeholder that decodes to "example-api-key-placeholder-only" (ESPHome 2026.9.1 rejects the all-zero key).
- **Rationale:** FR-36: placeholders only. FR-34: the values pass ESPHome's checks.

#### Tool pins — `requirements.txt`
- **Responsibility:** Pins the tool versions.
- **Consumers:** The venv setup.
- **Location:** `requirements.txt`
- **Kind:** Repository file.
- **Details:** `esphome==2026.9.1` and `yamllint==1.38.0`.
- **Rationale:** FR-37 (AT-26).

#### Lint config — `.yamllint`
- **Responsibility:** Limits `yamllint -s .` to project YAML, with rules that suit ESPHome.
- **Consumers:** The reviewer, `scripts/check.sh`.
- **Location:** `.yamllint`
- **Kind:** Repository file.
- **Details:** `extends: default`; ignore `.esphome/` and `.venv/`; `line-length` max 120; `document-start` disabled.
- **Rationale:** FR-34. Without this file, strict mode lints the YAML inside `.venv` and fails at the 80-column default.

#### Check script — `scripts/check.sh`
- **Responsibility:** Runs lint, validate and compile on `windmill.yaml`.
- **Consumers:** The reviewer, before each commit.
- **Location:** `scripts/check.sh`
- **Kind:** Bash script (`set -euo pipefail`).
- **Details:**
  ```
  require esphome and yamllint on PATH, secrets.yaml present and not staged in git, else exit 1 with a message
  yamllint -s . ; esphome config windmill.yaml ; esphome compile windmill.yaml     # stops at the first failure
  ```
- **Rationale:** FR-34 (AT-24): the script makes the three runs on the one configuration. FR-35: the script fails if `secrets.yaml` is staged, so a forced `git add` shows up before the commit.

#### Operator note — `README.md`
- **Responsibility:** Explains setup, checks, flashing, the sail direction and creating the HA groups.
- **Consumers:** The operator and the reviewer.
- **Location:** `README.md`
- **Kind:** Repository document.
- **Details:** Sections: Setup, Checks, First USB flash and OTA, Forward direction, Sail direction, HA light groups. "First USB flash and OTA" warns that `esphome upload` sends the last build in the shared build directory, so the operator compiles the wanted config (or uses `esphome run`, which compiles) before an upload. The last section uses HA Settings → Helpers → Group → Light group to create "Mill Interior" (pixels 1–3) and "Mill Lights" (all four).
- **Rationale:** FR-39 (AT-28). The note also holds the commands that AT-19, AT-21 and AT-26 use.

### Used

#### ESPHome 2026.9.1 built-in components
- **Location:** The `esphome` package in `.venv`.
- **Provides:** `uln2003`, `esp32_rmt_led_strip`, `partition`, `addressable_flicker`, template switch/number, `gpio` binary sensor, `script`, `interval`, `globals`, `logger`, `api`, `ota`, `wifi`, `captive_portal`.
- **Used by:** `windmill.yaml` and all packages.
- **Rationale:** The handbook prefers built-ins. The behaviour this design relies on was checked in source.

#### ESPHome CLI and yamllint
- **Location:** `.venv/bin`.
- **Provides:** Lint, config, compile, USB and OTA upload, and logs over the API.
- **Used by:** `scripts/check.sh`, `README.md`.
- **Rationale:** One pinned toolchain for every check (research 5.1, option B).

#### Home Assistant
- **Location:** The operator's HA instance.
- **Provides:** The native API client, the entity UI and the light group helper.
- **Used by:** All entities, `README.md`.
- **Rationale:** HA is the only remote control surface. The light groups live in HA (F4).

---

## Feasibility Review

- **No design blocker.** Every FR maps to built-in behaviour that was checked in source.
- **Settled (FR-41).** An HA turn-on that requests Lamplight starts the effect with no fade. The user accepted this, and spec v1.5 excludes that case from FR-41. The design adds no workaround.
- **Deferred (user, 2026-10-03).** The bench package and its include line, the derived `windmill_nobench.yaml` check and the one-line removal rule, the pixel addressing test, the `spec.md` pointer and handbook sync, and the one-hour run with the capacitors fitted. Spec v1.8 lists them in "Explicitly Out of Scope". `spec.md` still holds the old draft configuration listing.
- **Operator actions.** Most device ATs need the operator: first USB flash (AT-19), wiring, meters, the forward direction, HA groups or network changes. The tasks list them for each AT.

---

## Risks and Dependencies

- **RGBW pixels (spec Open Question 1).** The bench showed that the pixels are RGBW. `channel_colors: GRBW` sets the four-byte pixel format on its own: ESPHome 2026.9.1 rejects `is_rgbw` next to `channel_colors`. The strip and every partition use four `color_correct` values, because with three values ESPHome leaves the white channel at 100%. Only `mill_lights.yaml` changed. The AT-11 limit must still be recomputed from the RGBW datasheet.
- **ULN2003 counter wrap.** `current_uln_pos_` is an int32. It wraps after about 77 days of continuous running at 320 steps/s (146 days at 170). The coil phase survives the wrap, because 2³² is a multiple of 4. Signed overflow is undefined behaviour by the C++ standard. It is benign with current code generation (GCC on RISC-V wraps signed adds). A restart clears the counter. Accepted.
- **RMT refill under WiFi load.** On the C3, `rmt_symbols` defaults to 96, which is exactly one 4-pixel frame. If a later one-hour run (deferred) shows flicker after the hardware fixes, the first firmware fix is to raise `rmt_symbols`.
- **ESP-IDF output on USB (FR-28).** ESP-IDF writes its own log lines to the USB-Serial-JTAG console even with `baud_rate: 0`. `log_level: NONE` turns them off. The boot ROM banner and the bootloader lines still print, and AT-21 allows them. Crash data still reaches the operator: ESPHome's crash handler replays it when `esphome logs` connects; the ESP-IDF panic dump still prints on USB during a crash, is not a log line, and is accepted.
- **WiFi retry with the AP up.** ESPHome keeps retrying the configured network while the fallback AP is up. AT-14 and AT-18 confirm this.
- **Speed save window.** A power cut within about 60 s of a rounded set can restore the raw value. FR-09's 2-minute bound excludes this case, and the turn-on rounding and clamp keep the speed in range.
- **External.** The real speed is at most the setting (F5), and the AT windows allow for this. An N20 motor would change `mill_sails.yaml` only.

---

## Documentation

- `README.md` is Added, as described in Components. Each package starts with a comment that lists the substitutions it needs and the ids it exposes.
- The `spec.md` pointer and the `.sdd/handbook.md` updates ("Repository layout", "Pre-commit validation", "Assumptions and gaps") are deferred by the user (2026-10-03).

---

## Appendix

### Glossary
- **Re-base:** setting the stepper's reported position to 0 without moving the motor.
- **Re-arm:** setting the target one run span (10,000,000 steps) from the current position, in the current direction.

### References
- `specification.md` v1.8 and `research.md` F1–F7.
- ESPHome source: `stepper/stepper.cpp`, `uln2003/uln2003.cpp`, `template/number/template_number.cpp`, `light/light_call.cpp`, `light/addressable_light_effect.h`, `esp32_rmt_led_strip/light.py`, `esp32_rmt_led_strip/led_strip.cpp`, `esp32/__init__.py`, `wifi/__init__.py`.

### Change History
| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0 | 2026-10-02 | Pete Turner (with Claude) | Initial design. |
| 1.1 | 2026-10-02 | Pete Turner (with Claude) | Review fixes: Lamplight as `addressable_flicker` with a rate limit, ESP-IDF log level NONE, check order and staged-secrets check, spec v1.5. |
| 1.2 | 2026-10-03 | Pete Turner (with Claude) | Stepper on GPIO0, 1, 3, 4 with GPIO2 unconnected; pixel data on GPIO6; spec v1.6. |
| 1.3 | 2026-10-03 | Pete Turner (with Claude) | Pixels are SK6812 RGBW (GRBW order), found at the bench; four-value colour correction. |
| 1.4 | 2026-10-03 | Pete Turner (with Claude) | Pixel order changed for wiring: 0 door lamp, 1 door glow, 2 stone floor window, 3 bin floor window. |
| 1.5 | 2026-10-03 | Pete Turner (with Claude) | Scope reduced by the user (spec v1.8): no bench package, include line, derived check file or one-line rule; no pixel addressing test; the check script runs three checks on `windmill.yaml`; the `spec.md` and handbook updates are deferred. "Reverse Rotation" and `mill_sails_reverse` move to the sails package as production entities. |
| 1.6 | 2026-10-03 | Pete Turner (with Claude) | Long press (user request 2026-10-03, spec v1.9): a second `on_click` range (1–5 s) runs `mill_reverse_from_button`, which toggles "Reverse Rotation" while the sails turn and logs one INFO line. The controls package now uses `mill_sails_reverse_switch`; it still never sets the global. |

---
