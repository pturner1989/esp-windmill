# Project Handbook — esp-windmill

ESP-based light and motor platform for the Matchmaker MM02 matchstick windmill.
One ESP32-C3 node drives a 28BYJ-48 stepper (the sails) and four SK6812 pixels.
It runs ESPHome and joins Home Assistant.

## Source of truth

- `spec.md` is the build spec: hardware, wiring, pin allocation, power chain, test phases, and migration paths.
  Read it before research, requirements, or design work.
- If a design or task contradicts `spec.md`, raise it with the user. Do not silently diverge.
  When the user agrees a change, update `spec.md` in the same change.

## Stack

| Concern | Choice |
| --- | --- |
| Firmware | ESPHome YAML, `esp-idf` framework |
| Board | ESP32-C3 SuperMini (`board: esp32-c3-devkitm-1`) |
| Integration | Home Assistant native API, encrypted |
| Updates | USB for the first flash, OTA after that |
| Tooling | Python venv, versions pinned in `requirements.txt` |

Custom C++ is a last resort. Prefer built-in ESPHome components and actions.
When logic outgrows one level, move it to the next:

1. **Lambda.** Keep it to a few lines.
2. **ESPHome `script`.**
3. **Pure header via `esphome: includes:`** in `windmill.yaml`. Put the maths in a header under `include/`
   with no ESPHome types, no clock reads and no static state, so it can be tested on the laptop.
   One thin glue header connects it to ESPHome (`millis()`, the light, the shared state).
4. **External component** under `components/`.

## Repository layout

```
windmill.yaml              # Node config: board, wifi, api, ota, logger. Includes the packages.
packages/
  mill_sails.yaml          # Stepper, speed number, run switch, the 20 ms ramp tick
  mill_lights.yaml         # Pixel strip and partition lights
  mill_controls.yaml       # Button and scripts
  mill_disco.yaml          # Disco Mode switch and its scripts
include/
  mill_disco.h             # Disco maths: pure functions, host-tested
  mill_disco_esphome.h     # Disco glue: shared state, millis(), pixel write
  mill_ramp.h              # Sails ramp maths: rpm to steps/s, the speed ramp. Host-tested
  mill_ramp_esphome.h      # Sails ramp glue: the shared ramp, millis()
tests/
  mill_disco_test.cpp      # Header tests for include/mill_disco.h
  mill_ramp_test.cpp       # Header tests for include/mill_ramp.h
scripts/
  check.sh                 # Pre-commit checks: header tests, lint, config, compile
  test_check.sh            # Tests for check.sh, the node settings and the packages
secrets.example.yaml       # Committed. Placeholder values only.
secrets.yaml               # Git-ignored. Real credentials.
requirements.txt           # Pinned esphome and yamllint versions
spec.md
.sdd/
```

The package split serves migration Option B in `spec.md`. The hub must be able to include
`packages/mill_*.yaml` unchanged, with only substitutions (pins, prefixes) changed.
So a package must not contain node-level config (`esphome:`, `wifi:`, `api:`, `ota:`, `esp32:`).
This includes `esphome: includes:`, so the headers are listed in `windmill.yaml`, and the hub copies that line.

## Naming

| Thing | Convention | Example |
| --- | --- | --- |
| Node name | kebab-case, `village-` prefix | `village-windmill` |
| Component `id` | snake_case, `mill_` prefix | `mill_sails`, `mill_sail_speed` |
| Entity `name` | Title Case, no node name (ESPHome adds `friendly_name`) | `Sail Speed` |
| Package files | snake_case, `mill_` prefix | `mill_lights.yaml` |
| Substitutions | snake_case | `sails_pin_a`, `pixel_pin` |
| Secrets | snake_case | `wifi_ssid`, `api_key` |

The `mill_` id prefix prevents id collisions when the packages fold into the hub.
Use substitutions for all GPIO numbers. Do not hard-code a pin inside a package.

## Safety invariants

Every change must keep these true. A review must check them.

1. **Boot state is dark and stopped.** After any reset or power cut, lights are off and the sails do not turn.
   Never use `restore_mode: RESTORE_*` or `restore_value` on anything that would start the motor or lights at boot.
2. **Brightness cap is 60%.** Enforce it in firmware with `color_correct: [60%, 60%, 60%, 60%]`
   (the pixels are RGBW, so the cap has four channels),
   not only in scripts, so a Home Assistant command cannot exceed it.
   A partition light applies its own correction, not the strip's, so set it on the strip and on every partition.
   Disco writes only through the partition lights' own effects (`it[0]` in the "Disco" effect),
   never to the strip `mill_pixels` and never with `addressable_set`.
3. **Sail speed stays in 1–9 rpm (about 34–307 steps/s).** Set the bounds on the number entity. Do not allow a path that sets speed outside this range.
   The ramp passes through lower speeds on its way to and from a stop; that is allowed.
4. **Pin allocation matches `spec.md`.** GPIO0, GPIO1, GPIO3, GPIO4 drive stepper IN1–IN4; GPIO6 pixel data; GPIO5 button. GPIO2 stays unconnected (boot-strapping pin).
5. **Serial logging stays off on the C3** (`logger: baud_rate: 0`) in the shipped config.
6. **The two mill connectors are the module boundary** (the motor's 5-pin JST-XH and a 4-pin JST for 5V, GND, data and button). Firmware must not assume anything above them except four coils, one data line and the button.

## Error handling and fail-safe behaviour

ESPHome has no exception model. Handle failure through state and safe defaults:

- On loss of WiFi or the API, the node keeps its current state and the local button still works.
  The node must not need Home Assistant to start or stop the mill.
- Use the fallback AP and `captive_portal` so a bad WiFi config is recoverable without USB.
- Any action that energises the motor must have a matching stop path that works from the button.
- In a lambda, check for `NAN` and out-of-range values before use (for example `sail_speed.state` before restore).

## Logging

- Default log level `INFO` in the shipped config. Use `DEBUG` only during bench work, and do not commit it.
- Use `ESP_LOGx` with a tag that names the area (`"mill.sails"`, `"mill.lights"`) inside lambdas.
- Do not log secrets.
- Read logs over the API (`esphome logs windmill.yaml`) because serial logging is off.

## Testing

There is no unit-test framework for ESPHome YAML. Testing has four levels:

| Level | Command or method | When |
| --- | --- | --- |
| Header tests | `scripts/check.sh` (g++) | Every change to `include/` |
| Config validation | `esphome config windmill.yaml` | Every change |
| Compile | `esphome compile windmill.yaml` | Every change to firmware |
| Bench acceptance | The phase checklists in `spec.md` (Build and test order) | Each hardware milestone |

- To validate without real credentials, copy `secrets.example.yaml` to `secrets.yaml`.
- A task that changes behaviour must state which bench-checklist items verify it.
  Write its acceptance criteria as observable behaviour (for example "sails stop within one step when the button is held for 1s").
- Header tests use no framework. `tests/mill_disco_test.cpp` and `tests/mill_ramp_test.cpp` have local `CHECK` and
  `CHECK_NEAR` macros and exit 1 on any failed check. `scripts/check.sh` builds each with
  `g++ -std=c++17 -Wall -Wextra -Werror -Iinclude` into `.esphome/host-tests/` and runs them first.
  Test only the pure headers. The glue headers need ESPHome.
- `scripts/test_check.sh` checks the check script, the node settings and the packages. Run it with the venv active.
- If an external component under `components/` is written, it needs host-side tests. Use the same approach.

## Commits

- The repo is not yet a git repository. Run `git init` before the first implementation task.
- Use Conventional Commits: `type(scope): summary`.
  - Types: `feat`, `fix`, `refactor`, `docs`, `chore`, `test`.
  - Scopes: `sails`, `lights`, `controls`, `disco`, `node`, `spec`, `tooling`.
  - Example: `feat(sails): add speed number with 1-9 rpm bounds`.
- Put one logical change in each commit. Never commit `secrets.yaml`, `.esphome/`, or build output.

## Pre-commit validation

Run these before every commit. All must pass.

`scripts/check.sh` runs the first four.

```bash
for t in mill_disco_test mill_ramp_test; do
  g++ -std=c++17 -Wall -Wextra -Werror -Iinclude tests/$t.cpp -o .esphome/host-tests/$t && .esphome/host-tests/$t
done
yamllint -s .
esphome config windmill.yaml
esphome compile windmill.yaml   # when firmware YAML changed
git diff --cached --name-only | grep -q '^secrets.yaml$' && echo "secrets staged" && exit 1
```

`.gitignore` must contain `secrets.yaml`, `.esphome/`, `.venv/`, and `__pycache__/`.

## Assumptions and gaps

These are defaults chosen without user input. Confirm or correct them.

- ESPHome is the firmware for the whole project. No custom Arduino or ESP-IDF application.
- The `mill_` id prefix and package split. This diverges from the bare ids in `spec.md` (`sails`, `sail_speed`).
- Conventional Commits and the scope list above.
- There is no CI. Validation runs locally.
- ESPHome version is not chosen yet. Pin the current release in `requirements.txt` on first install.
- Spec config items to check on first compile: `rmt_channel` on `esp32_rmt_led_strip`
  may no longer be accepted by current ESPHome releases, and nothing in the spec config enforces the 60% brightness cap.
