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
Keep a lambda to a few lines. If logic needs more, write an ESPHome `script`.
If that is still not enough, write an external component under `components/`.

## Repository layout

```
windmill.yaml              # Node config: board, wifi, api, ota, logger. Includes the packages.
packages/
  mill_sails.yaml          # Stepper, speed number, run switch
  mill_lights.yaml         # Pixel strip and partition lights
  mill_controls.yaml       # Button and scripts
secrets.example.yaml       # Committed. Placeholder values only.
secrets.yaml               # Git-ignored. Real credentials.
requirements.txt           # Pinned esphome and yamllint versions
spec.md
.sdd/
```

The package split serves migration Option B in `spec.md`. The hub must be able to include
`packages/mill_*.yaml` unchanged, with only substitutions (pins, prefixes) changed.
So a package must not contain node-level config (`esphome:`, `wifi:`, `api:`, `ota:`, `esp32:`).

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
2. **Brightness cap is 60%.** Enforce it in firmware with `color_correct: [60%, 60%, 60%]`,
   not only in scripts, so a Home Assistant command cannot exceed it.
   A partition light applies its own correction, not the strip's, so set it on the strip and on every partition.
3. **Sail speed stays in 60–320 steps/s.** Set the bounds on the number entity. Do not allow a path that sets speed outside this range.
4. **Pin allocation matches `spec.md`.** GPIO0, GPIO1, GPIO3, GPIO4 drive stepper IN1–IN4; GPIO6 pixel data; GPIO5 button. GPIO2 stays unconnected (boot-strapping pin).
5. **Serial logging stays off on the C3** (`logger: baud_rate: 0`) in the shipped config.
6. **The 8-pin JST-XH is the module boundary.** Firmware must not assume anything above it except four coils, one data line and the button.

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

There is no unit-test framework for ESPHome YAML. Testing has three levels:

| Level | Command or method | When |
| --- | --- | --- |
| Config validation | `esphome config windmill.yaml` | Every change |
| Compile | `esphome compile windmill.yaml` | Every change to firmware |
| Bench acceptance | The phase checklists in `spec.md` (Build and test order) | Each hardware milestone |

- To validate without real credentials, copy `secrets.example.yaml` to `secrets.yaml`.
- A task that changes behaviour must state which bench-checklist items verify it.
  Write its acceptance criteria as observable behaviour (for example "sails stop within one step when the button is held for 1s").
- If an external component under `components/` is written, it needs host-side tests. Decide the framework at that point and record it here.

## Commits

- The repo is not yet a git repository. Run `git init` before the first implementation task.
- Use Conventional Commits: `type(scope): summary`.
  - Types: `feat`, `fix`, `refactor`, `docs`, `chore`, `test`.
  - Scopes: `sails`, `lights`, `controls`, `node`, `spec`, `tooling`.
  - Example: `feat(sails): add speed number with 60-320 steps/s bounds`.
- Put one logical change in each commit. Never commit `secrets.yaml`, `.esphome/`, or build output.

## Pre-commit validation

Run these before every commit. All must pass.

```bash
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
