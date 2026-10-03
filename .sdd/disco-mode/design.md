# Design: Disco Mode

**Version:** 1.1
**Date:** 2026-10-03
**Status:** Draft
**Linked Specification** `.sdd/disco-mode/specification.md` (v1.5)

---

# Design Document

---

## Architecture Overview

### Current Architecture Context
- `windmill.yaml` holds the node config and includes three packages: `mill_sails`, `mill_lights` (strip `mill_pixels` and four partition lights with their own 60% four-channel `color_correct`) and `mill_controls` (Mill Button and the scripts `mill_toggle`, `mill_on`, `mill_off`, `mill_reverse_from_button`). Dependencies point one way: controls → sails + lights. `scripts/test_check.sh` asserts this structure.
- `mill_on` holds the lamplight look inline: amber on all four lights with a 3 s fade, then Lamplight on each interior light that is still on.
- The button acts on release (`on_click`): 50–500 ms toggles the mill, 1–5 s reverses the turning sails.
- ESPHome 2026.9.1 source facts: `AddressableLambdaLightEffect` calls its lambda at most once per `update_interval`. `AddressableLight::update_state` sets the light's brightness as the local brightness even while an effect runs, and `ESPColorCorrection` computes `(value × color_correct × brightness) ^ gamma` (gamma 2.8 on a partition). A `LightCall` with no effect keeps the running effect. A call with effect None still gets the default transition (`light_call.cpp`). `get_effect_name()` returns a `StringRef`, which has `operator==(const char*)`. The main loop runs every 16 ms unless a `HighFrequencyLoopRequester` is active. Only `uln2003` requests one, and only while it steps.

### Proposed Architecture
- **Approach A+ (research §5).** Each of the four lights gets a "Disco" `addressable_lambda` effect with a one-line lambda. Two C++ headers, included with `esphome: includes:` in `windmill.yaml`, hold the logic:
  - `include/mill_disco.h`: pure functions. No ESPHome types, no clock reads, no static state. Host-tested with g++.
  - `include/mill_disco_esphome.h`: the one shared state instance and thin glue that reads `millis()`, calls the pure functions and writes the pixel.
- **Package structure.** New `packages/mill_disco.yaml` holds the four disco entities, the disco scripts and a reconcile interval. The lamplight look moves from `mill_on` into a script `mill_lamplight` in `mill_lights.yaml`. Dependencies stay one way: controls → sails + lights + disco; disco → lights; lights → headers. The disco package never names a sails id (FR-05).
- **One shared state, written only in the main loop.** One `mill_disco::Disco` value holds the beat clock, the tap run, the latest white flash, the button-down time and the flash state of slots 0–3. The four effects and every entity action use this one instance. All writers run in the ESPHome main loop, so it needs no locks. It is a function-local static of about 100 bytes, with no heap use. Its defaults are BPM 120 and rate 1×, the same as the number's and the select's initial values. Every BPM or rate change goes through the glue, so the clock and the entities always agree. Nothing restores it.
- **Beat clock with no drift.** `beats(t) = anchor_beat + (t − anchor_ms) × bpm ÷ 60000`, in double. Steps = beats × rate. Each frame computes beats from the anchor, so nothing accumulates (NFR-03). Every time difference is signed `int32_t(a − b)`, which is wrap-safe for spans under 24.8 days. Bar, phrase, table index and hue use floored division and modulo, so negative beats work (a button press can fall before an anchor set during the press). A BPM change re-anchors at "now" with the old BPM first, so the beat count stays continuous (FR-11). A tap re-anchors at the tap time and rounds the beat count there to the nearest whole beat (FR-16). A rate change keeps the beat count, so steps stay aligned to beats (FR-12). Disco start anchors beat 0 at "now".
- **Scheduled flashes, not frame-detected flashes.** For each slot, `frame()` computes the latest scheduled chase start at or before now: step `n` plus the slot's quarter position in the firing order of the bar that contains step `n`. Steps per bar = 4 × rate (2, 4 or 8), so bar boundaries fall on step boundaries. Each quarter step therefore has exactly one slot (FR-07), and each slot has exactly one start in each step (FR-06). A new scheduled start becomes a chase flash only when all three of these hold. Otherwise the start is used up with no flash:
  1. It is at least 333 ms after the latest white flash start (FR-25).
  2. It is at least 333 ms after this slot's previous chase start (FR-26).
  3. It is no more than 60 ms in the past, so a long loop pass or a re-anchor never causes a late catch-up flash.

  The limits compare nominal (scheduled) times.
- **Firing order (FR-08).** `bar_order(bar)` reads a fixed table: the 24 orders of four pixels in Steinhaus–Johnson–Trotter sequence, indexed by floored `bar mod 24`. Each bar differs from the one before by one swap of two neighbours, so any 8 bars show 8 different orders. With one neighbour swap, the pixel that moves earlier keeps a 0.75-step gap, which is at least 1/3 s while BPM × rate ≤ 135. Above that, that pixel skips one flash at the bar change. Spec v1.4 AT-04 allows this.
- **Look.** Phrase = floor(beats ÷ 16). Slot `p` takes hue `(phrase × 137 + 20 + p × 90) mod 360`, HSL with saturation 100% and lightness 55%. The brightest channel is 255, so each flash peak reaches the cap (FR-03, FR-09). Envelope (light output) = `0.15 + 0.85 × e^(−5 × phase)`, with phase = time since the slot's last chase start ÷ step length. It is 15.6% at phase 1 and tends to 15% after a skipped flash (FR-10). The partition applies gamma 2.8 after scaling, so the header writes each channel × `envelope^(1/2.8)`, and the light output follows the envelope. The white flash is the W channel at 255 for 100 ms (FR-24).
- **Brightness and colour from HA (FR-33).** The effect ignores `current_color`, so an HA colour does not change the chase. The partition's local brightness scales every write, so an HA brightness change scales the peaks. A call with no effect keeps "Disco" running. The partition's 60% `color_correct` still applies (FR-39).
- **Render timing.** `update_interval: 16ms` matches the loop interval, so each light renders on every loop pass. With the sails stopped, the strip's `max_refresh_rate: 20ms` defers every second show, so the pixels change about every 32 ms. With the sails turning, the stepper's high-frequency loop gives a render every 16 ms and a show at least 20 ms apart. A displayed start lags its nominal time by at most 32 ms. The four lights render in separate effect calls, so they can differ by up to one frame (≤ 32 ms), the white flash included. This is accepted.
- **Tap and white flash timing.** An accepted tap sets the white start to "now".
  - The button: `on_press` stores `millis() − 20` (the 20 ms `delayed_on_off` filter) as the tap time (FR-17). `on_click` fires 20 ms after release, so white shows at most about 52 ms after release (FR-24, ≤ 100 ms).
  - An HA tap: the tap time is the arrival time. White shows typically within 16 ms of it, at worst 32 ms. FR-18 (spec v1.5) allows 50 ms. The 1/3 s white window and the 60 ms late limit suppress the chase starts that a snap moves into the past.
- **Disco Mode stays true to the lights.** "Disco Mode" is a template switch with `optimistic: false`. Only the disco scripts publish its state. A 250 ms `interval` compares the switch with the four lights (target state from `remote_values`, and whether the effect name equals "Disco"):
  - Switch off, and any light on with "Disco" → run `mill_disco_start` (FR-04).
  - Switch on, and any light off or with another effect → compute `disco_mask(lights)` (the lights still on with "Disco") before anything changes. Then run `mill_disco_leave_for_light` with that mask. The changed light keeps what the operator asked (FR-34). When "Mill Lights" turns all four off, the off commands that arrive after the leave turn the remaining lights off, so all four end dark (FR-35).
- **Leaving disco.** `mill_lamplight(mask)` (bit n = pixel n) turns each selected light on in amber with effect None. Effect None stops "Disco" at once, so the interval never sees a stale "Disco". It still takes the 3 s default transition from the last disco frame to amber. After 3 s, the script starts Lamplight on each selected interior light that is still on, within 4 s (FR-32). A user leave (HA switch off, or a long press) and `mill_on` select all four lights. `mill_disco_start` turns the lights on at 100% in the same amber with effect "Disco".
  - **Contract: every start and every leave cancels any pending lamplight.** `mill_disco_start` stops `mill_lamplight` first. A leave re-runs it (`mode: restart`). `mill_off` stops it.

```
Button down ─20 ms filter─► on_press: note_press(now − 20)
Button up   ─20 ms filter─► on_click 50–500 ms ─► disco on? ─ yes ─► mill_disco_tap(press time) ─► tap(): run, BPM, snap, guard, white start = now
                                                             └ no ──► mill_toggle (unchanged)
Next render (≤16 ms): frame() per slot ─► white for 100 ms, then the chase from the new anchor
```

### Technology Decisions
- **ESPHome 2026.9.1, built-ins plus two headers.** `includes:` lists both headers under `esphome:` in `windmill.yaml`, because a package must not hold node-level config (NFR-05). ESPHome emits the includes after the global declarations, so no `globals:` entry uses a header type. Headers use C++17 and `<cstdint>`, `<cmath>`, `<array>` only.
- **Frame interval 16 ms** (`update_interval`). The strip keeps `max_refresh_rate: 20ms`. Decay curve: the visualisation's `0.15 + 0.85 × e^(−5 × phase)`, applied to light output through the inverse gamma.
- **"Disco BPM" and "Disco Rate" do not restore.** They boot at 120 and 1×, as the shared state does. A tempo from the last session rarely matches the next song. A tap run sets BPM through `publish_state`, which writes no preference, so restore would need an extra save path. NFR-01 allows either choice.
- **Host tests:** one file, `tests/mill_disco_test.cpp`, with a few local `CHECK` / `CHECK_NEAR` macros and no third-party framework. `scripts/check.sh` builds it with `g++ -std=c++17 -Wall -Wextra -Werror -Iinclude` into `.esphome/host-tests/` (already git-ignored) and runs it before lint.
- **Logging:** `logger.log` at level INFO with tag `mill.disco`: "Disco on", "Disco off" and "Tap: BPM x". There is no line per flash or per tap. The button logs its own "Button: disco off" line (tag `mill.controls`).

### Quality Attributes
- **NFR-01 (cap and boot).** The only disco pixel writes are `it[0] = …` inside the four partition effects. So every chase and white write passes through that partition's 60% four-channel `color_correct`. No disco path names `mill_pixels` or uses `addressable_set`. Boot: the lights and "Disco Mode" are `ALWAYS_OFF`. The switch's boot-time turn-off action runs `mill_disco_leave`, which does nothing while the switch is off. The number and select do not restore (FR-38).
- **NFR-02 (sails).** Each effect call costs roughly 30–60 µs of soft-float work (double beats, `expf`, `powf`; the C3 has no FPU). At 4 lights and up to 62 renders/s, that is under 1.5% of loop time. Each strip show blocks for about 0.1–0.2 ms. No call blocks, allocates or loops more than 8 times (the tap run).
- **NFR-03 (timing).** Beats come from the anchor plus `millis()`, with no accumulated increments.
- **NFR-04 (flash limit).** The BPM bounds (60–180) and the rate guard (2× only at ≤ 90 BPM) keep every step at 1/3 s or longer. `frame()` applies the white window and the per-slot gap on every path. `tap()` marks a white flash only when the previous one started at least 333 ms before. A host test simulates AT-11 and checks every gap.
- **NFR-05 (hub).** `includes:` is only in `windmill.yaml`. The `spec.md` migration section records it.

---

## API Design

**HA entities (added):**
- **Disco Mode** (switch `mill_disco_mode`, `optimistic: false`, `ALWAYS_OFF`). On: runs `mill_disco_start`. Off: runs `mill_disco_leave`. Each does nothing if the switch already has the requested state.
- **Disco BPM** (number `mill_disco_bpm`): 60–180, step 0.1, initial 120, `optimistic: false`, no restore. ESPHome rejects out-of-range values. A set re-anchors the clock, applies the rate guard and publishes the value. If the guard dropped 2×, it also publishes "1×" on "Disco Rate".
- **Disco Rate** (select `mill_disco_rate`): options "½×", "1×", "2×", initial "1×", `optimistic: false`, no restore. A set publishes the accepted rate. 2× while BPM > 90 publishes "1×" (FR-13).
- **Disco Tap** (button `mill_disco_tap_button`). With disco off, it starts disco and then taps. With disco on, it taps.
- **Lights:** each light's effect list gains "Disco". "Mill Door Lamp" offers only "Disco". The interior lights offer "Lamplight" and "Disco".

**Pure header `mill_disco.h` (namespace `mill_disco`).** Every function takes time as `uint32_t` ms and the state and settings as arguments, and returns a new value. No function has side effects. BPM is clamped to 60–180, and rate to ½×, 1× or 2×. No function reports errors.
- `beats_at(clock, t)` → beats at `t`. `retime(clock, now, bpm)` → the same beat count at `now`, with the new BPM.
- `guard_rate(rate, bpm)` → 1× when 2× is requested above 90 BPM, else the requested rate. `set_bpm(state, bpm, now)` → state and whether the guard dropped 2×. `set_rate(state, rate)` → state and the accepted rate.
- `start(state, now)` → beat 0 at `now` and an empty tap run, with BPM and rate kept. The white time and the slot times become the later of their current value and `now − 10 s`, so a quick off then on cannot flash again within 1/3 s.
- `tap(state, tap_ms, now)` → state, `bpm_changed` and `rate_dropped`. It applies FR-16 and FR-19 to FR-22 and the rate guard. It sets the white start to `now` only if the latest white start is at least 333 ms before (FR-25).
- `frame(state, slot, now)` → the slot's next flash state and its RGBW output. It applies the three start rules on nominal times. While less than 100 ms has passed since the white start, the output is white.
- Helpers, tested on their own: `bar_order(bar)`, `phrase_colour(phrase, slot)`, `envelope(phase)`, `level(fraction)` (inverse of gamma 2.8; `kGamma` must match the partitions' `gamma_correct`).

**Glue header `mill_disco_esphome.h`:** `shared()`, `render(it, slot)`, `note_press(down_ms)`, `start_now()`, `tap_now(tap_ms)`, `last_tap()`, `set_bpm_now(bpm)` → `rate_dropped`, `set_rate_now(rate)` → the accepted rate. `any_left(lights)`, `any_disco(lights)` and `disco_mask(lights)` → int take the four `LightState*`. Each wrapper reads `millis()` and `shared()`, calls one pure function and stores the result.

**Scripts:** `mill_lamplight(mask: int)` is in the lights package. `mill_disco_start`, `mill_disco_leave`, `mill_disco_leave_for_light(mask: int)` and `mill_disco_tap(tap_ms: uint32_t)` are in the disco package.

---

## Components

### Modified

#### Node config — `windmill.yaml`
- **Change:** No includes → `esphome: includes:` lists both headers. Adds the `disco` package.
- **Dependants:** `scripts/test_check.sh`. The hub (Option B) must copy the line (Documentation).
- **Kind:** ESPHome top-level config.
- **Details:**
  ```
  esphome: {name, friendly_name, includes: [include/mill_disco.h, include/mill_disco_esphome.h]}
  packages: {sails, lights, disco: !include packages/mill_disco.yaml, controls}
  ```
- **Rationale:** Plumbing for FR-02 and FR-06 to FR-26. The effect lambdas and the entity actions call the headers. It is node-level because of NFR-05.

#### Lights package — `packages/mill_lights.yaml`
- **Change:** Each partition gains a "Disco" effect, so the door lamp gets its first effect. The new script `mill_lamplight` holds the lamplight look moved from `mill_on`, now with effect None.
- **Dependants:** `mill_controls` (`mill_on`, `mill_off`), `mill_disco` (start and leave scripts), `scripts/test_check.sh`.
- **Kind:** ESPHome package.
- **Details:**
  ```
  each partition, slot = pixel index: effects += addressable_lambda {name Disco, update_interval 16ms,
                                          lambda: "mill_disco::render(it, <slot>);"}
  door lamp effects: [Disco]; interior effects: [Lamplight, Disco]
  script mill_lamplight (mode restart, parameters {mask: int}; bit n = pixel n):
    each light whose bit is set → light.turn_on amber as mill_on had it (lamp 85%), effect None → 3 s fade
    delay 3s; each interior light with its bit set and remote_values on → light.turn_on effect Lamplight
  ```
- **Rationale:** FR-02: every light has "Disco", and it is the door lamp's only effect. FR-06 to FR-10 and FR-24: the effect renders the chase and the white flash. FR-33 and FR-39: the effect writes through the partition view, so the partition's brightness and 60% cap apply, and `current_color` is ignored. FR-37: frames publish no state. FR-32 and FR-34: `mill_lamplight` stops "Disco" and gives the lamplight look to exactly the selected lights.

#### Controls package — `packages/mill_controls.yaml`
- **Change:** The button stores the press time and routes each press by the disco state. `mill_on` calls `mill_lamplight`. `mill_off` also stops `mill_lamplight`.
- **Dependants:** `scripts/test_check.sh`.
- **Kind:** ESPHome package.
- **Details:**
  ```
  mill_button on_press: lambda "mill_disco::note_press(millis() - 20);"        # 20 ms = delayed_on_off
  on_click 50ms–500ms: if switch.is_on mill_disco_mode → script mill_disco_tap {tap_ms: press time}
                       else → script mill_toggle
  on_click 1s–5s:      if switch.is_on mill_disco_mode → switch.turn_off mill_disco_mode; INFO "Button: disco off"
                       else → script mill_reverse_from_button
  mill_on: switch.turn_on mill_sails_turn; script.execute mill_lamplight {mask: 15}
  mill_off: script.stop mill_on; script.stop mill_lamplight; (rest unchanged)
  ```
- **Rationale:** FR-17: the tap time is the button-down time. FR-27: a short press in disco taps and changes no light or sail. FR-28: a long press turns the switch off, which runs the user leave. FR-29: no handler covers 500 ms–1 s or more than 5 s. FR-30 and FR-31: every path uses firmware entities only, so it works with no network; after a long press, the next short press runs the existing `mill_off`. FR-32: `mill_on` and the leave share one lamplight look.

#### Check script — `scripts/check.sh`
- **Change:** Three steps → four. A new first step builds and runs the header tests with `g++`.
- **Dependants:** `scripts/test_check.sh`.
- **Kind:** Bash script.
- **Details:** `g++ -std=c++17 -Wall -Wextra -Werror -Iinclude tests/mill_disco_test.cpp -o .esphome/host-tests/mill_disco_test && .esphome/host-tests/mill_disco_test`. The script then runs yamllint, `esphome config` and `esphome compile` as before, and stops at the first failure.
- **Rationale:** Runs the host tests for FR-06 to FR-26 before each commit.

#### Check tests — `scripts/test_check.sh`
- **Change:** The expectations that this design changes:
  - **Stub run:** `run_check` also stubs `g++`. The stub logs its arguments and writes a logging stub at its `-o` path. The expected calls are the g++ line, the test binary, then the three existing calls. `new_repo` also copies `include/` and `tests/`. `clean_path` keeps the real g++. The refusal tests need no change, because check.sh refuses before the g++ step.
  - **Button:** the handlers become `on_press` then `on_click`. There are still two click ranges, but each now holds an `if` on `mill_disco_mode`, with the tap or the leave in `then` and `mill_toggle` or `mill_reverse_from_button` in `else`.
  - **Controls logs:** 4 log calls, not 3. The header comments list the new ids (`mill_disco_mode`, `mill_disco_tap`, `mill_lamplight`).
  - **Scripts:** `mill_on` = `switch.turn_on` + `script.execute mill_lamplight mask=15`. `mill_off` gains `script.stop mill_lamplight` after `script.stop mill_on`. The amber, `delay 3s` and Lamplight checks move to `script/mill_lamplight`, with `effect=None` in each amber call and the mask bit in each condition.
  - **Effects:** each interior light has exactly two effects: Lamplight (as before) and Disco (`addressable_lambda`, 16 ms). The door lamp has exactly one: Disco.
  - **Boundaries:** "every light the controls package turns on is a partition" becomes "the controls package turns on no light". The partition-only check moves to the lights and disco packages.
- **Dependants:** None.
- **Kind:** Bash test script.
- **Details:** One new check: `mill_disco.yaml` names no sails id (`mill_sails*`, `mill_sail_speed`). `long_lambdas`, `strip_writes` and `package_violations` already search `packages/*.yaml`, so they cover the new package with no change.
- **Rationale:** Keeps the pre-commit suite true to the new structure. FR-02 gets the effect checks, and FR-05 gets the sails-id check. The boundary checks keep NFR-01 visible to the reviewer.

### Added

#### Disco maths — `include/mill_disco.h`
- **Responsibility:** Holds every disco decision as pure functions of time, settings and state.
- **Consumers:** `mill_disco_esphome.h`, `tests/mill_disco_test.cpp`.
- **Location:** `include/mill_disco.h`
- **Kind:** C++17 header (no ESPHome types, no I/O, no statics).
- **Details:** These are contracts. The implementer chooses the field layout.
  ```
  Disco state: beat clock (defaults BPM 120, rate 1×), tap run, white flash, button-down time, 4 slot states
  beats_at(clock, t) → beats          retime(clock, now, bpm) → clock          guard_rate(rate, bpm) → rate
  start(state, now) → state           set_bpm(state, bpm, now) → state, rate_dropped     set_rate(state, rate) → state, rate
  tap(state, tap_ms, now) → state, bpm_changed, rate_dropped
  frame(state, slot, now) → slot state, RGBW
  bar_order(bar) → 4 slots   phrase_colour(phrase, slot) → RGB   envelope(phase) → 0.15–1   level(fraction) → byte
  times: uint32_t ms in, differences int32_t(a − b); bar, phrase, hue: floored division and modulo
  constants: 333 ms gap, 100 ms white, 60 ms late limit, 1200 ms run gap, 3500 ms window, gamma 2.8
  ```
- **Rationale:** FR-06 and FR-07: scheduling, with one slot per quarter step. FR-08: the order table. FR-09: phrase colour. FR-10: the envelope. FR-11 and FR-12: `retime` and steps = beats × rate. FR-13 to FR-15: `guard_rate`, applied by `set_bpm`, `set_rate` and `tap`, which never raise the rate. FR-16: the snap. FR-19 to FR-22: the tap run (4 or more taps in the window, a new run after a 1.2 s gap (FR-20), no BPM change under 4 taps (FR-21), clamp to 60–180). FR-24 to FR-26: the white output, the white window and the per-slot gap. FR-03: the peak channel is 255.

#### Disco glue — `include/mill_disco_esphome.h`
- **Responsibility:** Holds the one shared disco state and connects the pure functions to `millis()`, the pixel and the lights.
- **Consumers:** The lambdas in `mill_lights`, `mill_disco` and `mill_controls`.
- **Location:** `include/mill_disco_esphome.h`
- **Kind:** C++17 header (uses `esphome::light` types). Each function has 1–3 lines.
- **Details:**
  ```
  Disco &shared();                                   // one function-local static, defaults as above, never restored
  void render(AddressableLight &it, int slot);       // frame(), then writes it[0] only
  void note_press(uint32_t down_ms);  void start_now();  void tap_now(uint32_t tap_ms);  last_tap();
  bool set_bpm_now(float bpm);  float set_rate_now(float rate);
  bool any_left(lights);  bool any_disco(lights);  int disco_mask(lights);   // lights: the four LightState*
  ```
- **Rationale:** FR-24 and FR-25: one white flash is shared by all four effects, so the lights cannot disagree. FR-17: it keeps the button-down time. FR-37: it writes only to `it` and publishes nothing. FR-38: the state is not restored. FR-39: it writes only through a partition view. FR-04, FR-34 and FR-35 use it through the reconcile checks and the mask.

#### Disco package — `packages/mill_disco.yaml`
- **Responsibility:** Offers the disco entities and keeps "Disco Mode" true to the four lights.
- **Consumers:** HA, `mill_controls`.
- **Location:** `packages/mill_disco.yaml`
- **Kind:** ESPHome package (switch, number, select, button, four scripts, one interval). Ids use the `mill_disco_` prefix. Every lambda has at most 2 lines.
- **Details:**
  ```
  entities as in API Design; number/select set_action: set_bpm_now / set_rate_now, then publish the accepted values
  mill_disco_tap(tap_ms: uint32_t): tap_now(tap_ms); last_tap().bpm_changed → publish BPM + INFO; rate_dropped → publish "1×"
  Disco Tap on_press: if switch off → script mill_disco_start; script mill_disco_tap {tap_ms: millis()}
  mill_disco_start: if off → script.stop mill_lamplight; start_now(); publish on; 4× light.turn_on {100%, amber, effect Disco}; INFO
  mill_disco_leave: if on → publish off; mill_lamplight {mask: 15}; INFO "Disco off"
  mill_disco_leave_for_light(mask: int): publish off; mill_lamplight {mask}; INFO "Disco off"
  interval 250ms: on and any_left → mill_disco_leave_for_light {mask: disco_mask(lights)};  off and any_disco → mill_disco_start
  ```
- **Rationale:** FR-01: the entities, ranges and defaults. FR-03: start turns on all four at 100% with "Disco" in one action list. FR-04: the interval. FR-05: no sails id appears. FR-13 and FR-14: the publishes after the guard. FR-18 and FR-23: the HA tap path. FR-32: the user leave. FR-34 and FR-35: the leave for a changed light, with the mask taken before any light changes. FR-36: every change publishes at once. FR-37: entities publish only on value changes. FR-38: `ALWAYS_OFF`, no restore, and a guarded leave at boot.

#### Header tests — `tests/mill_disco_test.cpp`
- **Responsibility:** Checks the pure header on the laptop.
- **Consumers:** `scripts/check.sh`.
- **Location:** `tests/mill_disco_test.cpp`
- **Kind:** C++17 test program (exit code 1 on any failed check).
- **Details:** Cases:
  - `retime` keeps `beats_at` continuous. A snap gives a whole beat at the tap and moves the beat count by at most 0.5. A tap before the anchor (negative beats) snaps correctly with floored bar and phrase. A clock anchored just before the `millis()` wrap gives the same schedule after the wrap.
  - Each `bar_order` entry is a permutation of 0–3, and neighbouring bars differ by one neighbour swap. Phrase hues are 90° apart with the brightest channel at 255. `envelope(1)` is 0.10–0.20. `level` inverts gamma within 1%.
  - `guard_rate` returns 1× for 2× at 90.1 BPM and keeps 2× at 90 BPM. A new state has BPM 120 and rate 1×. Tap runs follow the AT-10 sequences.
  - A 60 s simulation with 16 ms frames runs the AT-02, AT-04 and AT-11 scenarios. It checks one slot per quarter step, every 333 ms gap and no catch-up flash, and it prints the skip count per slot.
- **Rationale:** Verifies FR-06 to FR-16, FR-19 to FR-22 and FR-24 to FR-26 off the device, and gives the reviewer evidence for NFR-04.

### Used

#### ESPHome 2026.9.1 built-ins
- **Location:** The `esphome` package in the venv.
- **Provides:** `partition` light (own `color_correct`, brightness scaling of effect writes); `addressable_lambda`; template switch, number, select and button; `script` with `parameters`; `interval`; `esphome: includes:`.
- **Used by:** All Modified and Added YAML components.
- **Rationale:** Built-ins carry the cap and the rate limit. The headers hold only the maths and the glue.

#### g++ 15.2
- **Location:** `/usr/bin/g++`.
- **Provides:** The host compiler for the header tests.
- **Used by:** `scripts/check.sh`, `tests/mill_disco_test.cpp`.
- **Rationale:** Already installed. No framework needed.

#### Home Assistant
- **Location:** The operator's HA instance.
- **Provides:** The entity UI and the "Mill Lights" group (bench README).
- **Used by:** `mill_disco`, the lights.
- **Rationale:** The remote control surface. AT-17 uses the existing group.

---

## Feasibility Review

- **No design blocker.** Every FR maps to a component above.
- **First task proves `includes:`.** A stub header with one function, called from one "Disco" lambda, must pass `esphome compile`. If it fails, fall back to approach B: move `mill_disco.h` unchanged into `components/mill_disco/` and turn the glue into a small external component. The tests do not change.
- **Spec conflict 1 (AT-04 vs FR-08 and FR-26), resolved in spec v1.4.** At a step of 1/3 s (180 BPM 1×, 90 BPM 2×), any order change moves some pixel at least one quarter-step earlier, so FR-26 skips its next flash. With the neighbour-swap table, this happens once per bar for one pixel, and only when BPM × rate > 135. AT-04 now allows 84–90 flashes on those rows.
- **Spec conflict 2 (AT-19 threshold), resolved in spec v1.4.** ESPHome applies gamma 2.8 after the 60% `color_correct`, so a channel at the cap runs at about 0.6^2.8 ≈ 24% duty. AT-19 now checks by eye that a disco peak matches the light steady at 100%. The cap still holds (FR-39).
- **User-executed actions:** An OTA flash of the disco firmware comes before every device AT (AT-01 to AT-20). AT-01 needs a full flash erase and a USB flash (`esptool erase_flash`, then `esphome run --device /dev/ttyACM0`). AT-14 needs the WiFi access point switched off and on. AT-17 needs the "Mill Lights" group. AT-18 needs the USB charger unplugged. Per the user's guidance, bench checks are by eye.

---

## Risks and Dependencies

- **Closed by source check.** `get_effect_name()` returns a `StringRef`, which compares with "Disco" through `operator==(const char*)`. A `LightCall` with no effect keeps the running effect, so an HA brightness or colour change keeps "Disco" (FR-33).
- **Script parameter types.** `uint32_t` script parameters are passed to the generated template as written. If the first compile rejects `uint32_t`, use `int` and cast in the glue.
- **Gamma coupling.** `level()` assumes gamma 2.8. If a partition's `gamma_correct` changes, `kGamma` must change too. A comment in both places records this.
- **Soft-float cost.** The estimate is under 1.5% of loop time. NFR-02's stopwatch check confirms it. If the sails slow, the first fix is a 32-entry table for the envelope and gamma.
- **`millis()` wrap.** Signed differences hold for spans under 24.8 days. Each start, tap and BPM change re-anchors, so only one disco session of more than 24 days with no input could fail. Accepted.
- **UTF-8 option labels.** "½×" and "×" pass through YAML and the API as UTF-8. If HA shows them wrong, use "1/2x", "1x" and "2x" instead (a spec text change).
- **External:** ESPHome stays pinned at 2026.9.1 in `requirements.txt`.

---

## Documentation

- **`.sdd/handbook.md`:** the Stack ladder becomes lambda → script → pure header via `includes:` (maths only, no ESPHome types in the tested part, one thin glue header) → external component. Repository layout adds `include/`, `tests/` and `packages/mill_disco.yaml`. Testing adds the level "Header tests | `scripts/check.sh` (g++) | every change to `include/`" and records the framework choice. Commits add the scope `disco`. Safety invariant 2 notes that disco writes only through partition effects.
- **`spec.md`:** "Migration to the main system" says the hub adds the same `esphome: includes:` line and copies `include/` with `packages/`. Describe the button in disco (short press = tap, long press = leave disco) and add bench checks by eye.
- **`README.md`:** a "Disco" section with the four entities, the button in disco, and the stop path with no network (long press, then short press).
- **`docs/wiring-guide.html`:** the button in disco and the bench checks by eye.
- **Package and header comments:** each new file starts with the ids it exposes and uses, as the existing packages do.

---

## Appendix

### Glossary
- **Slot:** A pixel index 0–3 inside the header. It equals the pixel number of the light.
- **Nominal start:** The scheduled time of a chase flash, from the beat clock. The flash limits compare nominal starts.
- **Shared state:** The one `mill_disco::Disco` instance in the glue header.
- **Mask:** The `mill_lamplight` parameter. Bit n selects pixel n.
- **SJT order:** The Steinhaus–Johnson–Trotter sequence of the 24 orders of four items. Neighbouring entries differ by one swap of two neighbours.

### References
- `specification.md` v1.5. `research.md` §2, §5 and §9.
- `.sdd/windmill-bench-firmware/design.md` v1.6.
- ESPHome source: `light/addressable_light_effect.h`, `light/addressable_light.cpp`, `light/esp_color_correction.h`, `light/light_call.cpp`, `light/light_state.h`, `core/application.h`, `script/__init__.py`.

### Change History
| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0 | 2026-10-03 | Pete Turner (with Claude) | Initial design. |
| 1.1 | 2026-10-03 | Pete Turner (with Claude) | Review round 1: `test_check.sh` as a Modified component; BPM and rate defaults in the shared state; lamplight cancel contract; signed time differences and floored maths; 16 ms frames with the worst-case tap lag stated; header contracts instead of layouts; `disco_mask` and effect None in `mill_lamplight`; `start` keeps recent flash times; spec v1.4; risks closed by source check. |

---
