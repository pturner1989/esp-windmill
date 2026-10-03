# Research: disco-mode

| Field | Value |
| --- | --- |
| Date | 2026-10-03 |
| Status | Research complete. Ready for requirements. |
| Sources | `notes.md`, `.sdd/handbook.md`, `packages/mill_lights.yaml`, `packages/mill_controls.yaml`, `packages/mill_sails.yaml`, `docs/mill-at-dusk.html` (version 4 script), installed ESPHome `2026.9.1` source |
| Scope | Disco on the real mill. The visualisation already has it and does not change. |

## 1. Problem context and why it matters

Disco mode is a novelty. When music plays, the operator switches the real mill to disco.
The four pixels then flash colours in time with a BPM that the operator sets.
The sails keep turning as they are.

This is a separate feature. It follows windmill-bench-firmware, which is Implemented and merged to `main`.
It does not reopen that spec, but it changes how the Mill Button behaves while disco is on (section 4).

The feature matters because it adds a new light behaviour that writes pixels many times a second.
It must do that without breaking three things the bench firmware already guarantees:
the 60% brightness cap, the dark and stopped boot state, and the sail step timing.

## 2. Current system behaviour

| Area | Behaviour today |
| --- | --- |
| Pixels | Four SK6812 RGBW pixels (GRBW order) on GPIO6. The strip `mill_pixels` is internal, `color_correct` 60% on four channels, `max_refresh_rate: 20ms`. |
| Lights | One HA partition light per pixel: Door Lamp (pixel 0), Door Glow (1), Stone Floor Window (2), Bin Floor Window (3). Each has its own 60% four-channel `color_correct`, a 3 s default fade and `restore_mode: ALWAYS_OFF`. |
| Effects | The three interior lights offer "Lamplight", an `addressable_flicker` at 50 ms. The Door Lamp has no effect and stays steady. |
| Mill Button | GPIO5. A short press of 50–500 ms runs `mill_toggle`, which calls `mill_on` or `mill_off`. A hold of 1–5 s runs `mill_reverse_from_button`, which reverses the sails while they turn. Both act on release (`on_click`). |
| `mill_on` | Starts the sails and fades all four lights to amber. After 3 s it starts Lamplight on each interior light that is still on. |
| Sails | The stepper takes at most one step per main-loop pass. Anything that makes a loop pass long can slow the sails. |
| Network | WiFi and API `reboot_timeout: 0s`. The button works with no network. |

The visualisation (`docs/mill-at-dusk.html`) is the reference for the disco look:

- **Beat clock.** Beats = elapsed time × BPM ÷ 60. Steps = beats × rate.
- **Chase.** Each pixel flashes once per step. Pixel *p* fires at `steps − order[p] ÷ 4`, so the four flashes sit a quarter step apart.
- **Order.** A seeded shuffle of `[0,1,2,3]` from the bar number (bar = beats ÷ 4) gives a new firing order every bar.
- **Colour.** Phrase = beats ÷ 16. Base hue = `(phrase × 137 + 20) mod 360`. Pixel *p* takes hue `base + p × 90`, full saturation, lightness 55%.
- **Envelope.** Each flash decays from full to 15% as `0.15 + 0.85 × e^(−5 × phase)`.
- **Rate guard.** 2× is allowed only while BPM × 2 ÷ 60 ≤ 3. A higher BPM drops the rate to 1×.
- **BPM change.** The beat count stays continuous when the BPM changes.
- **Tap.** Taps older than 2.5 s drop out. A gap over 1.2 s starts a new run. The run keeps at most 8 taps. With 4 or more, BPM = 60 × (taps − 1) ÷ span. Every tap rounds the beat count to the nearest whole beat, so the tap moment becomes a beat.

## 3. Domain constraints and terminology

### Constraints

- **Brightness cap (handbook invariant 2).** Every pixel write must pass through a light with the 60% `color_correct`.
- **Boot state (invariant 1).** The mill boots dark and stopped. Disco must not survive a restart.
- **Loop timing.** The stepper needs short loop passes. An `addressable_lambda` effect defaults to `update_interval: 0ms`, which runs on every loop pass. The effect must set an interval.
- **Flash rate.** BPM 60–180. At 180 BPM and 1×, each light flashes 3 times a second, the common limit in photosensitivity guidance. The mill as a whole changes up to 12 times a second (four lights, a quarter step apart). We accept this for small, separate lights in a novelty. Each flash also decays to 15%, not to off, so the contrast is lower than a full on-off flash.
- **Short lambdas.** The handbook keeps a lambda to a few lines. It treats custom C++ as a last resort.
- **Package boundary.** A package must not contain node-level config such as `esphome:`. The hub must be able to include `packages/mill_*.yaml` unchanged.
- **Stop path.** The button must still be able to stop the motor without the network.

### Terminology

| Term | Meaning |
| --- | --- |
| Beat | One beat of the music at the set BPM. |
| Step | One chase cycle: a beat × the rate. Each pixel flashes once per step. |
| Bar | 4 beats. The firing order changes every bar. |
| Phrase | 16 beats. The four colours change every phrase. |
| Disco Rate | ½×, 1× or 2× the tempo for the chase. |
| Beat anchor | The time (ms since boot) at which beat 0 fell. A tap moves it. |
| Slot | A pixel's fixed index (0–3) inside the shared beat maths. |
| Envelope | The brightness curve of one flash, from full down to 15%. |

## 4. Approaches considered

| | Approach | For | Against |
| --- | --- | --- | --- |
| A | An inline `addressable_lambda` "Disco" effect on each light. Each lambda reads shared globals (BPM, beat anchor, rate). | Built-in ESPHome. Shows as an effect in HA, like Lamplight. Writes through each partition, so the cap holds. `update_interval` limits the rate. | The beat maths is 20–30 lines. Four copies break the short-lambda rule. No way to test the maths off the device. |
| A+ | A, with the maths in a small C++ header of pure functions, included through `esphome: includes:`. Each lambda becomes a one- or two-line call. | Keeps lambdas short. One copy of the maths. Host tests with g++. | Adds a C++ file and a test setup. The handbook does not yet describe this pattern. |
| B | A custom ESPHome effect component under `components/`, with host tests. | The cleanest structure. | The most work: Python schema, codegen and C++ classes for one novelty. |
| C | A looping script that flashes the next pixel each quarter beat through `light.turn_on`. | YAML only. | Not an effect, so HA cannot show "Disco". Each call publishes a light state, so HA receives 4 or more updates per beat. |
| D | Built-in effects only, for example `strobe` with fixed colours. | No code. | No BPM, no tap and no phase control. |

## 5. Recommended direction and reasoning

Use **A+**: one "Disco" `addressable_lambda` effect on each of the four lights.
A small header holds the pure beat maths. The lambdas and the tap handler call it.

**Why A+.**
- The user wants Disco as an effect in HA, like Lamplight. A and B give that. C and D do not.
- A+ keeps each lambda to one or two lines. It keeps one copy of the maths, not four.
- Pure functions take time and settings as inputs. So g++ on the laptop can test them with no device.
- `includes:` is supported in ESPHome `2026.9.1`. The installed source (`esphome/core/config.py`) accepts it with no deprecation warning. It copies the file into the build and adds it after the globals.
- The header is the smallest C++ that meets the need. It sits between a lambda and an external component on the handbook's ladder.

**What the header holds.** Functions for the step phase from time, anchor, BPM and rate;
the flash envelope; the firing order from the bar number; the phrase colour for a slot;
the rate guard; tap averaging; and the phase snap. Names and signatures are a design decision.

**How it fits together.**

| Part | Plan |
| --- | --- |
| Shared state | Globals for BPM, beat anchor time, rate and tap history. The beat anchor is not restored. BPM may restore, which is harmless because it starts nothing. |
| Effect | Each light's Disco effect knows its slot (0–3) as a constant. All four derive the order from the same bar number, so they agree on the same "random" order with no shared order state. |
| Rate limit | `update_interval` of about 20 ms, which matches the strip's `max_refresh_rate: 20ms`. |
| Cap | The effect writes through the partition view, so each partition's 60% `color_correct` applies. |
| Restart | The lights are `ALWAYS_OFF`, which clears any effect. The Disco Mode switch is `ALWAYS_OFF`. |
| HA traffic | An effect change is one state update. The frames themselves do not publish state. |

**HA entities.**

| Entity | Type | Behaviour |
| --- | --- | --- |
| Disco Mode | switch, `ALWAYS_OFF` | On: starts Disco on all four lights at once, and turns them on if needed. Off: returns to normal lamplight (amber, Lamplight on the interior three, steady Door Lamp, as `mill_on` sets them). The sails do not change. |
| Disco BPM | number, 60–180 | Sets the tempo. A tap run can also set it. |
| Disco Rate | select, ½× / 1× / 2× | 2× only while BPM × 2 ÷ 60 ≤ 3. |
| Disco Tap | button | Snaps the phase to a beat. 4–8 taps within 2.5 s set the BPM. |

**Mill Button while disco is on.** A short press is a Disco Tap.
A long press (1–5 s) returns to normal lamplight and leaves the sails as they are.
Outside disco, the button behaves as today.
The controls package must read the disco state (for example the Disco Mode switch) to route each press.
This changes FR-15, FR-16 and FR-40 of windmill-bench-firmware while disco is on.
The button can still stop the sails with no network: a long press leaves disco, then a short press turns the mill off.

**Tap latency.** A tap from an HA dashboard reaches the C3 tens to a few hundred ms late, and the delay varies.
The user accepts "roughly on the beat" for HA taps. The doorbell has no network delay, so it is the tighter tap.

## 6. Ruled out and why

- **C, looping script.** It is not an effect, so it contradicts the user's choice. It sends HA 4 or more light state updates per beat.
- **D, built-in effects only.** No built-in effect follows a BPM or a tap.
- **B, custom component.** Not ruled out for good. It is the fallback if `includes:` stops working or the header grows past pure maths.
- **Plain A without the header.** Four copies of 20–30 lines break the short-lambda rule and cannot be tested off the device.

## 7. Prerequisites and disposition

| Prerequisite | Disposition |
| --- | --- |
| Host-side C++ tests for the header. g++ 15.2 is installed (`/usr/bin/g++`). The framework (plain `assert` or a single-header library) is a design choice. | Bundled into this spec. Mechanical. |

There is no split-out work.

## 8. Scope check

One spec, one outcome: disco on the real mill.
The work touches the lights and controls packages, one new package for the disco entities, one header and its tests.
It does not need a roadmap.

## 9. Open questions for requirements and design

| # | Question | Note |
| --- | --- | --- |
| 1 | Does `includes:` compile and link as expected in `2026.9.1`? | The source accepts it. Prove it with a compile early. If it fails, fall back to B or ask for a handbook exception for A. |
| 2 | Where does `includes:` live? | It is node-level (`esphome:`), so it goes in `windmill.yaml`, not in a package. The hub must add the same line when it takes the packages (migration Option B). |
| 3 | What `update_interval` and envelope decay? | About 20 ms and the visualisation's `e^(−5 × phase)` are the starting points. |
| 4 | Does Disco BPM restore across restarts? | Harmless either way. |
| 5 | What does HA show for each light's effect when Disco Mode is off? | For example "Lamplight" or "None". |
| 6 | What if the operator picks "Disco" on one light from HA? | Presumably it chases in its own slot with the shared clock. Does Disco Mode then show on? |
| 7 | Does a short-press tap give visible feedback? | For example a brief flash on all four lights. |
| 8 | How does "back to lamplight" treat lights that were off before disco? | Turn on all four, as `mill_on` does, or restore the earlier on/off state. |
| 9 | Should the button tap use the press time, not the release time? | `on_click` fires on release, so the press length (50–500 ms) adds to the tap delay. An `on_press` timestamp would make the doorbell tap tighter. |
| 10 | Does an HA tap while disco is off turn disco on? | `notes.md` proposed this and the visualisation does it. Confirm. |
| 11 | Tap tempo below 72 BPM. | With a 2.5 s window, 4 taps need 3 gaps of 0.83 s or less, so a tap run cannot set a BPM under 72. Slower taps still snap the phase. Accept or widen the window. |
| 12 | How does Disco Mode stay true to the lights (FR-18)? | For example if HA turns all four lights off while Disco Mode is on. |
| 13 | Does the light's brightness scale the effect output? | Disco Mode should set brightness so the flashes reach the 60% cap. Check in design. |
| 14 | Handbook update. | Record the header and host-test pattern, and where it sits against "custom C++ is a last resort". |

## 10. Integration points

| File | Change |
| --- | --- |
| `windmill.yaml` | Add `esphome: includes:` for the header. |
| New header (for example `include/mill_disco.h`) | Pure beat maths. No ESPHome types in the testable functions. |
| New host tests (for example `tests/`) | g++ tests for the header. |
| New `packages/mill_disco.yaml` | Globals, Disco Mode switch, Disco BPM number, Disco Rate select, Disco Tap button, tap handler. Uses `mill_` ids. |
| `packages/mill_lights.yaml` | Add the "Disco" effect to all four lights. The Door Lamp gains its first effect. |
| `packages/mill_controls.yaml` | Route short and long press by disco state. Move the lamplight look out of `mill_on` into a script that both `mill_on` and "back to lamplight" use. |
| `spec.md`, `docs/wiring-guide.html` | Describe the button in disco and add bench checks for disco. |
| `.sdd/handbook.md` | Header and host-test pattern, and the new test level. |
| `docs/mill-at-dusk.html` | Reference only. No change. |
