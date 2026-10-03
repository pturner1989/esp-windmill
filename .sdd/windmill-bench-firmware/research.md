# Research: windmill-bench-firmware

| Field | Value |
| --- | --- |
| Date | 2026-10-02 |
| Status | Research complete. Ready for requirements. |
| Sources | `spec.md`, `.sdd/handbook.md`, esphome/esphome source (dev `2026.10.0-dev` commit `8617368` of 2026-10-01, and stable `2026.9.1`) |
| Scope | Firmware for the windmill only. Not a multi-module platform. |

## 1. Problem context and why it matters

The Matchmaker MM02 windmill gets motorised sails and four lit pixels. One ESP32-C3 SuperMini runs it. A 28BYJ-48 stepper on a ULN2003 driver turns the sails (GPIO0–3 at research time; on 2026-10-03 the stepper moved to GPIO0, 1, 3, 4 and pixel data to GPIO6, because GPIO2 is a boot-strapping pin). Four SK6812 pixels light the door, two windows and an outside lamp (GPIO4). An optional button on GPIO5 gives local control. Home Assistant (HA) controls the mill through the ESPHome native API.

`spec.md` sets one rule above all others: everything electrical must work on the bench before anything is glued shut. After assembly there is no access to the pixels, the wiring inside the tower, or (without effort) the motor. So the firmware must be correct and tested while the parts still sit on a breadboard. A fault found after Phase 3 means taking the model apart.

This feature delivers that firmware and proves it on the bench. It is done when both of these are true:

1. Our own firmware runs on the C3 and updates over OTA.
2. Every "Phase 1, electronics on the bench" item in `spec.md` that a breadboard can test passes.

| Phase 1 item | How this feature covers it |
| --- | --- |
| Flash the C3, confirm WiFi, API and the fallback AP | Firmware and bench check |
| Stepper turns in both directions | Bench package adds a reverse control |
| Sweep speed 60–320 steps/s, note where steps are missed | Speed number plus bench observation |
| All four pixels address correctly | Bench package adds a pixel scan effect |
| Stepper and pixels together for one hour, no flicker | Bench run |
| 5V rail stays above 4.5V with both running (was: buck holds 5.1V; power changed to USB-C on 2026-10-02) | Multimeter. Not a firmware concern, outside the spec. |
| Boot state is dark and stopped after a power cut | Firmware and bench check |

## 2. Current state

Observed on 2026-10-02:

- The repository holds only `spec.md` and `.sdd/handbook.md`. It is not a git repository. There is no firmware code.
- The user has all parts. They will build on a breadboard. Only the C3 is wired so far.
- The ESPHome web installer (web.esphome.io) flashed the C3 with its default firmware. The C3 is on WiFi.
- USB serial access on the laptop works. A udev rule gives VID `303a` devices to the `plugdev` group and tells ModemManager to ignore them.
- The user has not taken any of the "measure before you buy" measurements in `spec.md`. These do not block bench firmware. But the cap width measurement could change the motor from a 28BYJ-48 to an N20 gearmotor later. That would change the sails package. Section 9 records this as an external risk.
- The user confirmed the handbook assumptions: ESPHome only, the `mill_` id prefix, the package split (`windmill.yaml` plus `packages/mill_*.yaml`), and Conventional Commits.

The draft ESPHome config in `spec.md` ("ESPHome configuration") is the starting point. Section 4 shows it does not validate as written and has two behaviour bugs.

## 3. Domain constraints and terminology

### Constraints

The handbook "Safety invariants" apply to every change. The ones that shape this feature most:

| Constraint | Source | Effect on firmware |
| --- | --- | --- |
| Boot state is dark and stopped | Handbook invariant 1 | No `RESTORE_*` mode or `restore_value` on anything that starts the motor or lights |
| Brightness cap 60% in firmware | Handbook invariant 2 | `color_correct` on the strip and every partition |
| Speed stays in 60–320 steps/s | Handbook invariant 3 | Bounds on the number entity. No other path may set speed outside them |
| Pins match `spec.md` | Handbook invariant 4 | GPIO0, 1, 3, 4 stepper (was GPIO0–3), GPIO6 pixel data, GPIO5 button, all through substitutions |
| Serial logging off on the C3 | Handbook invariant 5 | `logger: baud_rate: 0`. Read logs over the API |
| The 7-pin JST-XH is the module boundary | Handbook invariant 6 | Firmware assumes only four coils and one data line above it |
| Packages hold no node-level config | Handbook "Repository layout" | `esphome:`, `wifi:`, `api:`, `ota:`, `esp32:` stay in `windmill.yaml` |
| The button works without HA | Handbook "Error handling" | Scripts and button act on firmware entities only |

The level shifter choice (74AHCT125 or diode drop) is a hardware choice. It does not change the firmware.

### Terminology

| Term | Meaning here |
| --- | --- |
| Strip | The `esp32_rmt_led_strip` light that drives all four pixels on GPIO6 (was GPIO4) |
| Partition | A `partition` light. It shows a range of strip pixels as its own light entity with its own state |
| `color_correct` | A per-channel scale factor on an addressable light. ESPHome applies it to every pixel write |
| `channel_colors` | The current option that sets pixel colour order (for example GRB). It replaces `rgb_order` |
| Endless target | The run model: the sails turn because the stepper target is set to 2,000,000,000 steps, far past any reachable position |
| Bench package | `packages/mill_bench.yaml`. Test-only controls, included during bench work and removed to ship |
| HA light group | A Home Assistant helper that groups several light entities into one. It lives in HA, not in this repo |
| Lamplight | The `flicker` effect in the spec config that imitates a lamp flame |

## 4. Findings: spec config checked against ESPHome source

We checked the `spec.md` config against a clone of esphome/esphome at dev `2026.10.0-dev` (commit `8617368`, 2026-10-01) and against stable `2026.9.1`. Both versions agree on every point below. Paths are under `esphome/components/`.

### F1. `rmt_channel` fails validation, `rgb_order` is deprecated

The `esp32_rmt_led_strip` schema (`esp32_rmt_led_strip/light.py`) no longer has `rmt_channel`. So `esphome config` rejects the spec config. `rgb_order` still works but is deprecated in favour of `channel_colors`. `migrate_channel_colors` in `light/__init__.py` handles the old option, and the plan is to remove it in 2027.3.0. The firmware must drop `rmt_channel` and use `channel_colors` with GRB order. The bench must also show whether the bullet pixels are RGB or RGBW SK6812 parts, because that changes the `channel_colors` value.

### F2. `restore_value` on `sails_running` does no harm, but has no use

We first suspected that `restore_value: true` on the `sails_running` global could make the switch show the wrong state after a reboot. The source disproves this. Globals set up at priority HARDWARE (800). The template switch sets up later, at HARDWARE−2 (`globals/globals_component.h`, `template/switch/template_switch.cpp`). With `restore_mode: ALWAYS_OFF`, `Switch::get_initial_state_with_restore_mode()` (`switch/switch.cpp`) returns off, and `TemplateSwitch::setup()` calls `control(false)`. That runs `turn_off_action`, which sets the global back to false. So the restore has no effect on behaviour. It does cost one flash write each time the switch toggles. Decision: remove `restore_value: true`.

### F3. Nothing in the spec config enforces the 60% cap

The spec scripts request 60% and 50% brightness, but any HA command can still ask for 100%. `color_correct` exists on addressable lights and limits every write. One detail matters: `PartitionLightOutput::get_view_internal` (`partition/light_partition.h`) calls `view.raw_set_color_correction(&this->correction_)`. A partition uses its own correction, not the strip's. So `color_correct: [60%, 60%, 60%]` must be on the strip and on every partition. A partition without it would drive its pixels at full output. The handbook already states this.

### F4. Overlapping partitions break the button

Each partition is its own `LightState`. The spec defines "Mill Interior" (pixels 0–2) and "Mill Lights" (pixels 0–3), so they overlap. `mill_off` turns off only "Mill Lights". "Mill Interior" stays on, and its Lamplight effect keeps writing pixels 0–2 on every loop. The toggle script checks `light.is_on: mill_interior`, sees "on", and calls `mill_off` again. So every short press runs `mill_off`. The button never turns the mill on again, and the interior probably stays lit. This result comes from reading the source. The bench should confirm it if the spec config is ever flashed, but the chosen light model (section 6) removes the overlap.

### F5. Step timing is "at most", not "exactly"

`Stepper::should_step_()` (`stepper/stepper.cpp`) takes at most one step per `loop()` call. It sets `last_step_ = now`, so it does not catch up missed time. The ULN2003 component (`uln2003/uln2003.cpp`) requests high-frequency looping while the motor moves. Any work that blocks the loop (WiFi, API traffic, flash writes) delays the next step. So the real speed is at or below the configured steps/s, with small pauses. At 5 RPM a 30 ms stall moves the sails less than 1°, so we expect nobody to see it. The RPM table in `spec.md` is an upper bound. This is acceptable because the user sets the speed by eye.

### F6. Start and stop are instant

`acceleration` and `deceleration` default to `inf`, so the stepper starts and stops at once. The spec stop path sets the target to the current position, so the sails stop dead. We considered a soft spin-up and coast-down. The user deferred it ("later").

### F7. The device restarts after 15 minutes without WiFi or HA

Both the WiFi and the native API components default `reboot_timeout` to `15min` (`wifi/__init__.py`, `api/__init__.py`, `api/api_server.cpp`). If WiFi or Home Assistant is away for 15 minutes, the C3 restarts. A restart brings the mill back dark and stopped (handbook invariant 1), so a running mill would go dark during a long network outage. The firmware must set `reboot_timeout: 0s` on both, so the mill keeps its state and the button keeps working without a network. Found during requirements (spec FR on network loss).

### Other checks

The stepper position is a signed int. At 170 steps/s the 2,000,000,000 target lasts about 136 days of continuous running. The endless-target model is fine.

### Summary

| # | Spec item | Result | Action |
| --- | --- | --- | --- |
| F1 | `rmt_channel: 0`, `rgb_order: GRB` | Validation fails; option deprecated | Remove `rmt_channel`; use `channel_colors` |
| F2 | `restore_value: true` on `sails_running` | No behaviour effect; extra flash writes | Remove it |
| F3 | Brightness 60% in scripts only | Cap not enforced | `color_correct` on strip and every partition |
| F4 | Overlapping partitions | Button stops working; interior stays lit | Non-overlapping partitions (section 6) |
| F5 | Steps/s in RPM table | Real speed is at or below the setting | Accept; set speed by eye |
| F6 | Instant stop | Works as written | Keep; soft start/stop deferred |

## 5. Approaches considered

### 5.1 Toolchain

| Option | Description | Tradeoff |
| --- | --- | --- |
| **A** | ESPHome CLI compiles; web.esphome.io flashes | Two tools for one job, with no gain |
| **B** | ESPHome CLI in a Python venv, version pinned in `requirements.txt` | One tool for validate, compile, USB flash, OTA and logs. The agent can run the pre-commit checks. **Chosen.** |
| **C** | Local ESPHome dashboard in Docker | One more service to run. Harder for the agent to run validation |
| **D** | HA ESPHome Device Builder | Compiles away from the repo, so the agent cannot run `esphome config` or `compile` before a commit |

web.esphome.io stays available for logs and fallback flashing, but it cannot compile YAML. The first compile downloads the ESP-IDF toolchain. That takes several GB of disk and several minutes.

### 5.2 Light model

The four pixels need HA entities. The spec's overlapping partitions fail (F4), so we compared four models.

| Option | Description | Tradeoff |
| --- | --- | --- |
| **a** | Two firmware lights only (interior and lamp) | No control of a single pixel |
| **b** | Four non-overlapping single-pixel partitions in firmware; grouping in HA | Full control, no overlap, no conflicting states. Grouping lives outside the repo. **Chosen.** |
| **c** | Keep overlapping partitions; scripts turn all of them off | HA still shows conflicting states when a user acts on one entity |
| **d** | One light entity with effects for each scene | Loses separate control of the door lamp |

### 5.3 Bench testing

Phase 1 needs controls that the shipped firmware should not have: reverse rotation and a pixel addressing test.

| Option | Description | Tradeoff |
| --- | --- | --- |
| **x** | Bench-only package `packages/mill_bench.yaml`, included from `windmill.yaml` during bench work | Repeatable. One line to remove. **Chosen.** |
| **y** | Test controls stay in production | Extra entities and code in the shipped firmware |
| **z** | Edit the config by hand, flash, test, revert | Not repeatable. Easy to ship a test edit by mistake |

### 5.4 Brightness cap

Script values alone do not stop an HA command at 100% (F3). `color_correct` limits every write, from every source. Its cost is that HA's brightness scale changes meaning: HA 100% gives 60% physical output.

### 5.5 Soft start and stop

Setting `acceleration` and `deceleration` would give a heavier, more realistic motion (F6). The user deferred this. It is out of scope for this feature.

## 6. Decisions and reasoning

| Decision | Reasoning |
| --- | --- |
| Toolchain option B. Pin the current stable ESPHome (2026.9.1) and yamllint in `requirements.txt` | One tool covers every step. The agent can run validation and compile locally, as the handbook requires |
| HA is the primary controller. The button still works without HA | Handbook fail-safe rule. The mill must start and stop with no network |
| Four single-pixel partitions, always visible in HA: "Mill Door Glow" (0), "Mill Stone Floor Window" (1), "Mill Bin Floor Window" (2), "Mill Door Lamp" (3) | No overlap, so no conflicting states (F4). Each pixel runs its own Lamplight effect, so the windows flicker apart from each other, which looks more real |
| Raw strip stays `internal` in production | Users act on the named lights only |
| HA groups: "Mill Interior" (pixels 0–2) and "Mill Lights" (all four) | Gives the spec's two grouped controls without overlapping firmware lights |
| Scripts and the button act on the four firmware lights, never on HA groups | The button must work without HA |
| `color_correct: [60%, 60%, 60%]` on the strip and on all four partitions | Enforces the cap for every command source (F3) |
| Scripts request about 100% (interior) and about 85% (lamp) | The cap multiplies HA brightness before the gamma curve, so with the cap the spec values 60% and 50% would act like 36% and 30% brightness without it. (Physical LED output is lower still because of gamma 2.8: HA 100% under the cap is about 24% duty.) These values give the look the spec intends. Final values are set by eye in Phase 4 |
| Toggle: if the sails or any light is on, turn everything off; else turn everything on | Removes the dependence on one entity's state, which caused F4 |
| Bench package `packages/mill_bench.yaml` with: raw strip exposed with an addressable scan effect (strip also has `color_correct`), a reverse-direction control for the sails. (`web_server` was dropped during requirements: HA is the only remote control surface.) | Covers the Phase 1 items that production firmware should not carry. Removing the include is a checklist item |
| Speed number bounds stay 60–320 steps/s | Handbook invariant 3 |
| Remove `restore_value: true` from `sails_running` | No behaviour effect, and it costs flash writes (F2) |
| Drop `rmt_channel`; use `channel_colors` | Required to validate (F1) |
| Keep instant start and stop | Soft motion deferred (F6) |

## 7. Ruled out

| Item | Why |
| --- | --- |
| Multi-module platform firmware | The user confirmed scope: just the windmill |
| Toolchain A, C, D | See 5.1. Two tools, an extra service, or compiling outside the repo |
| Light models a, c, d | See 5.2. No per-pixel control, conflicting HA states, or no separate lamp |
| Bench options y, z | See 5.3. Test code in production, or a process nobody can repeat |
| Cap in scripts only | HA can bypass it (F3) |
| `color_correct` on the strip only | A partition applies its own correction, not the strip's (F3) |
| `restore_value` on `sails_running` | Useless and adds flash wear (F2). The earlier fear of a wrong switch state was disproved |
| Soft start and stop now | Deferred by the user |
| Custom C++ or an external component | Handbook: last resort. Built-in components cover every need found |

## 8. Prerequisites and disposition

| Prerequisite | Disposition |
| --- | --- |
| Repo setup: `git init`, Python venv, `requirements.txt` (esphome, yamllint), `.gitignore` (`secrets.yaml`, `.esphome/`, `.venv/`, `__pycache__/`), `secrets.example.yaml` | Mechanical. **Bundled into this feature** as its first work, not a separate spec |
| Taking over the C3 | Part of this feature. The first flash of our firmware replaces the web-installer default. Use USB with the CLI |
| HA side of the take-over | The node name changes to `village-windmill`. HA sees a new device and asks for the API encryption key. The user should remove the old discovered device from HA |
| USB serial access | Done (udev rule) |
| Hardware on the breadboard | User task. Only the C3 is wired now. Bench checks need the ULN2003, motor and pixels wired, powered from USB-C |

## 9. Scope check and external risks

This is one spec. It has one coherent outcome: firmware on the C3, verified on the bench against Phase 1. The repo setup is small and only exists to serve that outcome. A roadmap is not needed.

| External risk | Effect | Response |
| --- | --- | --- |
| "Measure before you buy" not done. The cap may be too narrow for the 28BYJ-48 | Motor could change to an N20 gearmotor, which needs a different sails package | Does not block this feature. The package split keeps the change local to `mill_sails.yaml` |
| SK6812 variant unknown (RGB or RGBW) | Wrong `channel_colors` gives wrong colours | Check on the bench (Q1) |

## 10. Open questions for requirements and design

| # | Question | Notes |
| --- | --- | --- |
| Q1 | Are the bullet pixels RGB or RGBW SK6812? | Sets `channel_colors` and may change the light type |
| Q2 | Which effect does the pixel scan test use? | It must light one pixel at a time so the user can see the order. The raw strip needs `color_correct` and `ALWAYS_OFF` too |
| Q3 | How does the reverse control work with the endless-target run model? | For example a negative target. It must respect the 60–320 bounds and the stop path |
| Q4 | ~~Is `web_server` in the bench package worth it?~~ | **Resolved in requirements: no.** HA is enough |
| Q5 | Exact script brightness values under the cap | Start near 100% and 85%. Tune by eye in Phase 4 |
| Q6 | Flicker parameters for each pixel. Does the Door Lamp flicker? | The spec gives the lamp no effect |
| Q7 | Does `sail_speed` with `restore_value: true` fire `on_value` at boot while the sails are stopped? | Verify. `set_speed` while stopped should be harmless, and it must not start the motor |
| Q8 | The HA groups live in HA config, outside this repo. Is creating them part of done? | At least document how to create them |
| Q9 | Logger settings | Handbook: `baud_rate: 0` and `INFO`. Logs over the API. `DEBUG` only during bench work, not committed |
| Q10 | Should each partition set `restore_mode: ALWAYS_OFF` explicitly? | The spec sets it only on the strip. Setting it on every light makes invariant 1 visible in review, whatever the default is |
| Q11 | What replaces the spec's `on_boot` call `light.turn_off: mill_lights`? | `mill_lights` no longer exists in firmware. Boot must turn off all four lights |
| Q12 | How does the one-hour test tell data-line flicker from the Lamplight effect? | Run the test with steady colours and no effect, so any flicker points to noise |
| Q13 | How does the user see missed steps in the speed sweep? | For example a mark on the shaft and a count of turns against the expected time. Decide the method in requirements |
| Q14 | Does this feature update the ESPHome config block in `spec.md`? | The handbook says to update `spec.md` in the same change when the user agrees a divergence. Sections 4 and 6 diverge from it |

## 11. Integration points

| Location | Relevance |
| --- | --- |
| `spec.md` "ESPHome configuration" | Starting config. Diverges per sections 4 and 6 |
| `spec.md` "Pin allocation" | GPIO0, 1, 3, 4 (stepper), GPIO6 (pixels), GPIO5 (button); GPIO2 unconnected |
| `spec.md` "Build and test order", Phase 1 | The acceptance checklist for done |
| `spec.md` "Speed", "Sleep behaviour", "Brightness" | 170 steps/s default, `sleep_when_done: false`, amber colour about 255/120/40 |
| Handbook "Safety invariants" | Checked by every review |
| Handbook "Repository layout" | `windmill.yaml`, `packages/mill_sails.yaml`, `mill_lights.yaml`, `mill_controls.yaml`, plus `mill_bench.yaml` |
| Handbook "Testing" and "Pre-commit validation" | `yamllint -s .`, `esphome config`, `esphome compile`, bench checklist |
| `esp32_rmt_led_strip/light.py` | Strip schema: no `rmt_channel` (F1) |
| `light/__init__.py` | `migrate_channel_colors`, `rgb_order` deprecation (F1) |
| `partition/light_partition.h` | Partition applies its own colour correction (F3) |
| `template/switch/template_switch.cpp`, `switch/switch.cpp` | `ALWAYS_OFF` calls `control(false)` at setup (F2) |
| `globals/globals_component.h` | Globals set up at priority HARDWARE (F2) |
| `stepper/stepper.cpp` | One step per loop, no catch-up; acceleration defaults (F5, F6) |
| `uln2003/uln2003.cpp` | High-frequency loop request while moving (F5) |
| Home Assistant | Native API with encryption; two light groups; removal of the old web-installer device |
