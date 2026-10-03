# Tasks: Disco Mode

**Linked Design:** `.sdd/disco-mode/design.md`
**Linked Specification:** `.sdd/disco-mode/specification.md`

### Task 1: A Disco effect on each light, at a fixed tempo

- **Status:** Done
- **Blocked by:** None

**What to build:**

Each of the four lights gains a "Disco" effect in HA. "Mill Door Lamp" offers it as its only effect, and the three interior lights offer it beside "Lamplight". When the operator picks "Disco" on all four lights, the pixels run the staggered chase at a fixed 120 BPM and 1×: each pixel starts one flash per step, a quarter step after the one before, in a fixed order with a fixed colour per pixel. Each flash starts at its peak and fades to a dim glow, not to dark. The chase logic lives in a small pure maths header that the node includes, and a thin glue header connects it to the clock and to the pixel. Every disco write goes through the light's own partition, so the 60% cap and the light's brightness apply, and an HA colour does not change the chase. The check script gains a first step that builds and runs the header tests on the laptop. This task proves that the node can include custom headers and that the firmware still compiles and links on ESPHome 2026.9.1. The handbook and the `spec.md` migration section record the new header level.

**Acceptance criteria:**

**AC-1 (check):**
- **Given:** The example secrets are in place
- **When:** The reviewer runs the check script
- **Then:** The script first builds the header tests with g++ (C++17, all warnings as errors) and runs them, then lints, validates and compiles the firmware with both headers listed under `esphome: includes:` in the node file, and exits 0

**AC-2 (check):**
- **Given:** The check-script test suite
- **When:** The reviewer runs it
- **Then:** All tests pass; the stub run records the g++ call, the test binary and the three existing calls in that order; each interior light has exactly two effects, "Lamplight" as before and "Disco" (an addressable lambda with a 16 ms update interval); "Mill Door Lamp" has exactly one effect, "Disco"; no package holds `includes:` or another node-level key; and no disco lambda names the strip or uses `addressable_set`

**AC-3 (host):**
- **Given:** A disco state at 120 BPM and 1× with beat 0 at 0 ms
- **When:** The header test simulates 60 s of frames 16 ms apart for all four slots
- **Then:** Each slot starts one chase flash in each 500 ms step; exactly one slot starts a flash in each 125 ms quarter step; no two slots start a flash in the same frame; and every gap between two starts on one slot is at least 333 ms

**AC-4 (host):**
- **Given:** The same simulation
- **When:** One frame comes 200 ms late, as after a long loop pass
- **Then:** No slot starts a flash whose scheduled start is more than 60 ms in the past, and each slot flashes again at its next scheduled start

**AC-5 (host):**
- **Given:** The pure header
- **When:** The test reads the envelope, the inverse gamma, the slot colours and a clock anchored 1 s before the 32-bit millisecond counter wraps
- **Then:** The envelope is 1 at phase 0 and 0.10 to 0.20 at phase 1; the inverse gamma undoes gamma 2.8 within 1%; each slot colour has its brightest channel at 255 and the four hues are 90° apart; and the flash starts across the wrap match those of a clock anchored at 0, shifted by the same offset

**AC-6 (device):**
- **Given:** The operator has updated the device over the network with this firmware, HA is connected and all four lights are off
- **When:** The operator opens the effect list of each light, then turns on all four lights at 100% with the effect "Disco" and watches for 30 s
- **Then:** "Mill Door Lamp" offers only "Disco" and each interior light offers "Lamplight" and "Disco"; the four pixels chase, one after another, each flashing twice a second; each flash fades to a dim glow and no pixel goes dark between flashes; HA shows no light state change per flash

**AC-7 (device):**
- **Given:** All four lights run "Disco"
- **When:** The operator sets "Mill Door Glow" to blue and 30% brightness in HA, and then short-presses the Mill Button twice (mill off, then mill on)
- **Then:** "Mill Door Glow" keeps its disco colour and flashes visibly dimmer than the other three; after the second press the mill shows the lamplight look as before, with "Mill Door Lamp" steady

**AC-8 (check):**
- **Given:** The handbook and `spec.md`
- **When:** The reviewer reads the Stack, Repository layout, Testing, Commits and Safety invariants sections of the handbook, and the "Migration to the main system" section of `spec.md`
- **Then:** The handbook lists the ladder lambda → script → pure header via `includes:` → external component, the `include/` and `tests/` folders, a "Header tests" level run by the check script, the scope `disco`, and that disco writes only through partition effects; `spec.md` says the hub adds the same `esphome: includes:` line and copies `include/` with `packages/`

**Notes:**

AC-1 to AC-5 and AC-8 are agent-checkable; AC-6 and AC-7 need the operator after an update over the network to 192.168.1.7. The first risk in the design is `includes:`: if the compile or link fails, stop and tell the user, because the fallback (approach B: the pure header moves unchanged into an external component under `components/mill_disco/`) changes the handbook ladder and this task. The fixed order (0, 1, 2, 3) and the fixed colours (phrase 0) are temporary, and Task 2 replaces them; the shared state is a function-local static with the defaults 120 BPM and 1×, and until Task 3 adds a start, beat 0 sits at boot. This task applies two of the three start rules in the design (the per-slot 333 ms gap and the 60 ms late limit); A comment in the header and in the lights package ties the header's gamma constant to the partitions' `gamma_correct`. This task contributes AT-01 (effect lists) and part of AT-16 (FR-02, FR-06, FR-07, FR-10, FR-16, FR-22 in part, FR-26, FR-28, NFR-01, NFR-03, NFR-05).

### Task 2: Firing order per bar and colours per phrase

- **Status:** Done
- **Blocked by:** Task 1

**What to build:**

The chase gets the full look of the dusk visualisation. At the start of each bar of 4 beats, the firing order changes to the next entry of a fixed table of the 24 orders of four pixels, in which each entry differs from the one before by one swap of two neighbours. Each pixel keeps one colour for a whole phrase of 16 beats, the four colours are a quarter of the colour wheel apart, and each new phrase brings a different set of four colours. The flash limit still holds across every bar change.

**Acceptance criteria:**

**AC-1 (host):**
- **Given:** The order table
- **When:** The test reads every bar from −30 to 30
- **Then:** Each order is a permutation of 0–3; each bar differs from the bar before by one swap of two neighbouring positions, including the step from the last table entry back to the first; and negative bars use floored modulo

**AC-2 (host):**
- **Given:** The phrase colours
- **When:** The test reads phrases −2 to 10 for all four slots
- **Then:** In each phrase the four hues are 90° apart and the brightest channel of each colour is 255; each phrase differs from the phrase before; and a negative beat count gives the floored phrase

**AC-3 (host):**
- **Given:** A disco state at 120 BPM and 1×
- **When:** The test simulates 60 s of frames 16 ms apart
- **Then:** Any 8 bars in a row show 8 different orders; exactly one slot starts a flash in each quarter step; every gap between two starts on one slot is at least 333 ms, also across each bar change; and each slot's colour changes only at a phrase start

**AC-4 (device):**
- **Given:** The operator has updated the device, and all four lights run "Disco" at 100% (bar 2 s, phrase 8 s)
- **When:** The operator watches the mill for about 1 minute
- **Then:** The order in which the pixels flash changes every bar; each pixel keeps one colour for each phrase; the four colours look about a quarter of the colour wheel apart (for example red, chartreuse, cyan and violet); each phrase shows a new set; the look matches the dusk visualisation

**Notes:**

AC-1 to AC-3 are agent-checkable; AC-4 needs the operator and is AT-06 by eye at the fixed 120 BPM, because the tempo controls arrive in Task 5. The hue of slot p in phrase n is (n × 137 + 20 + p × 90) mod 360, with HSL saturation 100% and lightness 55%. At 120 BPM and 1× each order change keeps every gap above 333 ms; the skip of one flash at a bar change when BPM × rate is above 135 can only happen once Task 5 allows 180 BPM, and Task 5 tests it. The light meter check in AT-06 is optional; the fade check from Task 1 covers FR-10 by eye. This task contributes AT-06 (FR-08, FR-09, FR-16).

### Task 3: Disco Mode on and off from HA

- **Status:** Done
- **Blocked by:** Task 1, Task 2

**What to build:**

HA shows a "Disco Mode" switch that is off after every restart. Turning it on turns on all four lights at full brightness with the "Disco" effect, puts beat 0 at that moment, and logs "Disco on". Turning it off sets all four lights to the lamplight look within 4 s, including any light that was off before disco, and logs "Disco off". Disco never touches the sails, so turning sails keep turning at the same speed and direction and stopped sails stay stopped. The lamplight look moves out of the button's mill-on script into one shared script in the lights package that takes a mask of the lights to set; mill-on uses it for all four lights, and mill-off and a disco start cancel any lamplight still pending. The switch shows only the true state: only the disco scripts change it. The new disco package holds the switch and its scripts and names no sails id. The README gains a "Disco" section that starts with the switch.

**Acceptance criteria:**

**AC-1 (check):**
- **Given:** The example secrets are in place
- **When:** The reviewer runs the check script and the check-script test suite
- **Then:** All runs pass; the node includes the disco package; "Disco Mode" (`mill_disco_mode`) is a template switch, not optimistic, with restore mode ALWAYS_OFF; the disco package names no sails id (`mill_sails*`, `mill_sail_speed`); the controls package turns on no light; every light the lights and disco packages turn on is a partition; every lambda has at most two lines; and the controls-scripts test counts only the controls package's `logger.log` calls, which stay at 3, so the new `mill.disco` logs do not change the count

**AC-2 (check):**
- **Given:** The controls, lights and disco packages
- **When:** The reviewer reads the mill-on, mill-off, lamplight and disco start scripts, and runs the check-script test suite
- **Then:** Mill-on turns on the sails and runs the lamplight script with mask 15; mill-off stops the lamplight script after it stops mill-on; the lamplight script turns each selected light on in the existing amber with effect None, waits 3 s, and then starts "Lamplight" on each selected interior light that is still on; the disco start stops the lamplight script before it turns any light on; and the test suite checks these new `mill_on` and `mill_off` expectations, and its amber, `delay 3s` and Lamplight checks move (not deleted) to `script/mill_lamplight`, with `effect=None` in each amber call and the mask bit in each condition

**AC-3 (host):**
- **Given:** A disco state at 150 BPM and ½× in which slot 2 started a chase flash 100 ms ago
- **When:** The test calls the start function and simulates 2 s of frames
- **Then:** The beat count is 0 at the start time; BPM and rate stay 150 and ½×; and slot 2 starts no chase flash less than 333 ms after its previous flash

**AC-4 (device):**
- **Given:** The operator has updated the device, HA is connected, and the mill is dark and stopped
- **When:** The operator turns on "Disco Mode" in HA and watches for 30 s, with the network log open
- **Then:** All four pixels start the chase within 1 s; the brightest moment of each flash looks as bright as that light does steady at 100%; the sails stay still; within 5 s HA shows "Disco Mode" on, all four lights on with the effect "Disco" and "Sails Turning" off; the log shows "Disco on" under the tag `mill.disco`

**AC-5 (device):**
- **Given:** A short press turned the mill on, "Reverse Rotation" is on, Sail Speed is 240, and the operator then turned off "Mill Bin Floor Window" in HA
- **When:** The operator turns on "Disco Mode" in HA, watches for 30 s, turns it off and watches for 10 s
- **Then:** The sails turn clockwise at the same speed without a pause throughout; with disco on, "Mill Bin Floor Window" turns on and all four pixels chase; with disco off, within 4 s all four lights show the lamplight look, including "Mill Bin Floor Window", with "Mill Door Lamp" steady; "Sails Turning", "Reverse Rotation" and "Sail Speed" stay as they were; HA shows each change within 5 s

**AC-6 (device):**
- **Given:** The mill is off
- **When:** The operator short-presses the Mill Button, short-presses it again about 1 s later during the fade, and watches for 10 s
- **Then:** All four pixels end dark and stay dark, with no light coming back on with "Lamplight", and the sails stop

**AC-7 (device):**
- **Given:** "Disco Mode" is on, the sails turn, and the device runs from its USB charger
- **When:** The operator unplugs the charger for 10 s, plugs it back in and watches for five minutes without a command
- **Then:** All four pixels stay dark and the sails do not move; once the device reconnects, HA shows "Disco Mode", "Sails Turning" and all four lights off

**AC-8 (device):**
- **Given:** "Disco Mode" has been on for 1 minute
- **When:** The operator leaves the mill alone for 10 minutes and then opens the HA logbook and history for the four lights and "Disco Mode"
- **Then:** They show no state change for those 10 minutes

**Notes:**

AC-1 to AC-3 are agent-checkable; AC-4 to AC-8 need the operator. The lamplight script uses effect None, which stops "Disco" at once but still takes the 3 s default fade; every start and every leave cancels any pending lamplight (the start stops it, a leave restarts it, mill-off stops it). Until Task 4, HA does not notice a light that changes during disco (fixed by Task 4). The switch's boot-time turn-off runs the leave, which does nothing while the switch is already off. The handbook layout gains the disco package; this task contributes AT-02 (at the default 120 BPM and 1×), AT-03, AT-15, AT-17 and the peak check of AT-16 (FR-01 in part, FR-03, FR-05, FR-21 for the HA switch, FR-25, FR-26, FR-27, FR-28, NFR-01).

### Task 4: Disco Mode follows changes to the lights

- **Status:** Done
- **Blocked by:** Task 3

**What to build:**

"Disco Mode" stays true to the four lights. A check every 250 ms compares the switch with the lights. When the switch is off and the operator picks "Disco" for any one light in HA, disco starts on all four lights. When the switch is on and the operator turns one light off or picks another effect for it, disco ends: the changed light keeps what the operator asked, and the lights that were still chasing take the lamplight look. When the operator turns off all four lights at once (for example with the "Mill Lights" group), all four end dark and the switch ends off. A brightness or colour change from HA keeps disco on: the light keeps its place in the chase, its peaks scale with the brightness, and its colours stay those of the phrase.

**Acceptance criteria:**

**AC-1 (check):**
- **Given:** The example secrets are in place
- **When:** The reviewer runs the check script and reads the disco package
- **Then:** All runs pass; a 250 ms interval runs the disco start when the switch is off and any light is on with "Disco", and runs the leave for a changed light, with the mask of the lights still on with "Disco" taken before any light changes, when the switch is on and any light is off or has another effect; the leave for a changed light takes an integer mask and passes it to the lamplight script

**AC-2 (device):**
- **Given:** The operator has updated the device, the mill is on in the lamplight look, and "Disco Mode" is off
- **When:** The operator picks "Disco" for "Mill Stone Floor Window" in HA and watches for 10 s
- **Then:** All four pixels start the chase within 1 s, not only the stone floor window, and within 5 s HA shows "Disco Mode" on and all four lights with the effect "Disco"

**AC-3 (device):**
- **Given:** "Disco Mode" is on with all four lights chasing
- **When:** The operator turns off "Mill Door Glow" and watches 10 s; turns "Disco Mode" on again, picks "Lamplight" for "Mill Bin Floor Window" and watches 10 s; turns "Disco Mode" on again, picks "None" for "Mill Door Lamp" and watches 10 s
- **Then:** After each change, within 4 s the changed light shows what the operator asked (dark, flickering with Lamplight, steady), the other lights show the lamplight look, and within 5 s HA shows "Disco Mode" off and the true state of all four lights; each time disco turns on again, all four lights chase, including "Mill Door Glow"

**AC-4 (device):**
- **Given:** The "Mill Lights" group exists, the sails turn, and "Disco Mode" is on
- **When:** The operator turns off "Mill Lights" in HA and watches for 10 s
- **Then:** All four pixels are dark within 4 s and stay dark; within 5 s HA shows "Disco Mode" off and all four lights off; the sails keep turning

**AC-5 (device):**
- **Given:** "Disco Mode" is on with all four lights chasing
- **When:** The operator sets "Mill Door Glow" to 30% brightness and watches 20 s, sets its colour to pure blue and watches past the next phrase change, then sets it back to 100%
- **Then:** "Mill Door Glow" keeps its place in the chase, flashes visibly dimmer at 30%, does not turn blue, changes colour with the others at the phrase change, and flashes as bright as the others at 100%; "Disco Mode" stays on throughout

**Notes:**

AC-1 is agent-checkable; AC-2 to AC-5 need the operator, and AC-4 needs the "Mill Lights" group from the bench README. The check reads each light's target state from `remote_values` and compares its effect name with "Disco"; the leave publishes the switch off first, so the next check does not run it again. With the group, the lights still chasing may start to fade to amber before their own off command arrives; those later off commands win, so all four end dark (FR-24). After this task, a short press during disco turns all lights off, and the check then turns the switch off. This task contributes AT-09, AT-12, AT-13 and AT-14 (FR-04, FR-05, FR-22, FR-23, FR-24, FR-25).

### Task 5: Tempo and rate from HA, with the rate guard

- **Status:** Done (agent checks); device ACs pending user
- **Blocked by:** Task 2, Task 3, Task 4

**What to build:**

HA shows a "Disco BPM" number from 60 to 180 in steps of 0.1 and a "Disco Rate" select with ½×, 1× and 2×. Both start at 120 and 1× after every restart and keep no value. A tempo change takes effect within 1 s with no pause and no jump in the chase, and the colours do not change at the change. A rate change keeps the beat, so an on-beat flash falls on every second beat at ½×, every beat at 1×, and every beat and half beat at 2×. The rate guard keeps every step at 1/3 s or longer: 2× is refused above 90 BPM, a tempo above 90 BPM drops 2× to 1×, and the rate never returns to 2× by itself. HA shows each accepted value. The per-pixel 1/3 s gap holds across every rate change. The README "Disco" section adds the two controls.

**Acceptance criteria:**

**AC-1 (check):**
- **Given:** The example secrets are in place
- **When:** The reviewer runs the check script and reads the disco package
- **Then:** All runs pass; "Disco BPM" (`mill_disco_bpm`) has minimum 60, maximum 180, step 0.1, initial value 120, is not optimistic and does not restore; "Disco Rate" (`mill_disco_rate`) has the options ½×, 1× and 2×, initial option 1×, is not optimistic and does not restore; each set action passes the value to the glue and publishes the accepted values

**AC-2 (host):**
- **Given:** A disco state at 120 BPM and 1× that has run for 9.3 s
- **When:** The test changes the tempo to 100 BPM, and then changes the rate to ½× and to 2×
- **Then:** The beat count is the same just before and just after each change (within 0.001 beat); the phrase does not change at either change; and at ½× an on-beat start falls on every second beat, at 1× on every beat, and at 2× on every beat and half beat

**AC-3 (host):**
- **Given:** A new disco state
- **When:** The test reads its defaults, asks for 2× at 90 BPM and at 90.1 BPM, raises the tempo from 90 to 120 at 2×, lowers it to 80, and sets the tempo to 200 and to 40
- **Then:** The defaults are 120 BPM and 1×; 2× is kept at 90 BPM and refused at 90.1 BPM; the raise to 120 drops the rate to 1× and reports the drop; the rate stays 1× at 80 BPM; and the tempo clamps to 180 and to 60

**AC-4 (host):**
- **Given:** The simulation with frames 16 ms apart
- **When:** It runs 30 s at each of 120 BPM 1×, 120 BPM ½×, 90 BPM 2× and 180 BPM 1×; and then runs 90 BPM ½×, selects 2× half-way between two on-beat starts, and after 10 s raises the tempo to 120 BPM
- **Then:** "Mill Door Lamp" (slot 0) starts 60 ± 1, 30 ± 1, 84 to 90 and 84 to 90 flashes; at every setting no quarter step has more than one start; every gap between two starts on one slot is at least 333 ms, also across the rate change and the guard drop; no start is more than 60 ms late; and the test prints the skip count per slot

**AC-5 (device):**
- **Given:** The operator has updated the device, HA is connected, and the device runs from its USB charger
- **When:** The operator opens the device page in HA, sets "Disco BPM" to 140 and "Disco Rate" to ½×, then unplugs the charger for 10 s and plugs it back in
- **Then:** Before the power cut HA shows "Disco BPM" at 120 with a range of 60 to 180 in steps of 0.1, "Disco Rate" at 1× with the options ½×, 1× and 2×; after the device reconnects HA shows 120 and 1× again, "Disco Mode" off and the mill dark and still

**AC-6 (device):**
- **Given:** "Disco Mode" is on at 120 BPM and 1×
- **When:** The operator sets "Disco BPM" to 100 in HA and watches for 10 s
- **Then:** The chase slows within 1 s with no pause and no jump, and the four colours do not change at the change

**AC-7 (device):**
- **Given:** "Disco Mode" is on at 120 BPM and 1×
- **When:** The operator selects 2× and watches 10 s; sets 90 BPM, selects 2× and watches 10 s; sets 120 BPM and watches 10 s; sets 80 BPM and watches 10 s
- **Then:** The first 2× is refused and HA shows 1× within 5 s; at 90 BPM 2× is accepted and "Mill Door Lamp" flashes 3 times a second; at 120 BPM HA shows 1× within 5 s and the lamp flashes twice a second; at 80 BPM the rate stays 1×

**AC-8 (device):**
- **Given:** "Disco Mode" is on at 60 BPM and 1×, and a metronome app plays 60 BPM aloud
- **When:** The operator turns on "Disco Mode" in HA close to a click, notes by eye how far the on-beat flash sits from the click, and watches for 60 s with no input
- **Then:** The gap between the on-beat flash and the click stays the same for the whole minute, with no drift the operator can see

**Notes:**

AC-1 to AC-4 are agent-checkable; AC-5 to AC-8 need the operator. AC-8 checks drift, not the offset, because an HA command carries the network delay; NFR-03's slow-motion video check stays optional. A tempo change re-anchors the clock at "now" with the old tempo first, so the beat count runs on; a rate change keeps the beat count. At a 1/3 s step (90 BPM 2×, 180 BPM 1×) one pixel may skip one flash at a bar change, which spec v1.4 AT-04 allows. If HA shows "½×" or "×" wrong, the design's fallback labels are "1/2x", "1x" and "2x", which is a spec text change to raise with the user. This task contributes AT-01 (entities), AT-04 and AT-08 (by simulation), AT-05, AT-07, NFR-03 by eye and the restore half of AT-15 (FR-01 in part, FR-11, FR-12, FR-13, FR-14, FR-15, FR-16, FR-25, FR-27, NFR-04).


### Task 6: The Mill Button in disco

- **Status:** Backlog
- **Blocked by:** Task 5

**What to build:**

While "Disco Mode" is on, a long press of the Mill Button turns "Disco Mode" off, so the four lights return to the lamplight look within 4 s and the sails keep their state, speed and direction, with no reversal; the button logs one line that it left disco. A short press does what it does outside disco: it turns the mill off, so the sails stop and all four lights fade off, and the disco check then turns "Disco Mode" off. Other press lengths do nothing. All of this works without WiFi or HA, so one short press always stops the whole mill. Outside disco, the button acts as before. The README, the `spec.md` button description and the wiring guide describe the button in disco and the disco bench checks by eye. The task ends with the final review of the safety invariants.

**Acceptance criteria:**

**AC-1 (check):**
- **Given:** The example secrets are in place
- **When:** The reviewer runs the check script and the check-script test suite
- **Then:** All runs pass; "Mill Button" still has `on_click` as its only handler, with no `on_press`, and exactly two click ranges; the 50–500 ms range runs only the mill toggle, as before; the 1–5 s range holds an `if` on `mill_disco_mode` that turns "Disco Mode" off and logs "Button: disco off" while it is on, and runs the reverse otherwise; mill-off names no disco id; the controls-scripts test now expects 4 `logger.log` calls in the controls package (3 before this task), all at INFO with the tag `mill.controls`; and the button test's header word list adds `mill_disco_mode` and `mill_lamplight`

**AC-2 (device):**
- **Given:** The operator has updated the device, HA is connected, the sails turn forward (anticlockwise), "Disco Mode" is on, and the network log is open
- **When:** The operator holds the button about 0.7 s, then about 6 s, then about 2 s, and watches for 10 s after each
- **Then:** The 0.7 s and 6 s holds change nothing; after the 2 s hold all four lights show the lamplight look within 4 s, with "Mill Door Lamp" steady; the sails keep turning anticlockwise without a pause; within 5 s HA shows "Disco Mode" off with "Sails Turning" and "Reverse Rotation" unchanged; the log shows "Button: disco off" under the tag `mill.controls`

**AC-3 (device):**
- **Given:** HA is connected, the sails turn and "Disco Mode" is on
- **When:** The operator short-presses the button once and watches for 10 s
- **Then:** The sails stop within 1 s of the release; all four pixels are dark within 4 s and stay dark, with no light coming back in the lamplight look; within 5 s HA shows "Disco Mode", "Sails Turning" and all four lights off

**AC-4 (device):**
- **Given:** HA is connected, the sails turn anticlockwise and "Disco Mode" is on
- **When:** The operator switches off the WiFi access point and watches 10 s; holds the button about 0.7 s; holds it about 2 s and watches 10 s; switches the access point on, waits for HA to reconnect and turns "Disco Mode" on in HA; switches the access point off; short-presses once and watches 10 s; then switches the access point on again
- **Then:** With the access point off, the chase keeps running, the 0.7 s hold changes nothing, and the 2 s hold returns the lamplight look within 4 s with the sails still turning anticlockwise; the short press stops the sails within 1 s of release and the pixels are dark within 4 s; within 5 minutes of the access point returning, HA shows "Disco Mode", "Sails Turning" and all four lights off

**AC-5 (device):**
- **Given:** "Disco Mode" is off and the mill is off
- **When:** The operator short-presses the button, holds it about 2 s, and short-presses it again
- **Then:** The mill turns on in the lamplight look, the sails reverse without a stop, and the mill turns off, as before this feature

**AC-6 (device):**
- **Given:** "Disco Mode" is off and all four lights are in the lamplight look, with the sails turning at 170 steps/s
- **When:** The operator times 5 full turns with a stopwatch, then turns on "Disco Mode" at 180 BPM and 1× and times 5 more turns; and then repeats both timings at the highest safe Sail Speed from the bench-firmware speed sweep
- **Then:** At each speed the two times are within 2% of each other

**AC-7 (check):**
- **Given:** The README, `spec.md` and the wiring guide
- **When:** The reviewer reads their button, disco and bench-check sections
- **Then:** Each says that in disco a short press turns the mill off and ends disco, a long press leaves disco to the lamplight look without reversing the sails, other presses do nothing, the button does not start disco, and the button acts the same with no network; `spec.md` and the wiring guide list the disco bench checks by eye; none mentions a tap or a "Disco Tap" button

**AC-8 (check):**
- **Given:** All disco files: the node file, the lights, disco and controls packages, and both headers
- **When:** The reviewer checks them against handbook invariants 1 and 2 and the package and header comments
- **Then:** Every disco pixel write is `it[0]` inside a partition effect; no disco file names the strip or uses `addressable_set`; "Disco Mode" and the four lights are off after every restart, and "Disco BPM" and "Disco Rate" do not restore; the BPM range and the rate guard allow no step shorter than 1/3 s; `includes:` appears only in the node file; and each new file starts with the ids it exposes and uses

**Notes:**

AC-1, AC-7 and AC-8 are agent-checkable; AC-2 to AC-6 need the operator, and AC-4 needs the WiFi access point switched off and on. The short-press range does not change: while disco is on all four lights are on, so the toggle always runs mill-off; the next 250 ms check then finds all four lights off and runs the disco leave with mask 0, which publishes "Disco Mode" off and sets no light. This keeps HA true well within 5 s, and mill-off needs no disco id. The long press only turns the switch off, so the switch's own leave runs with mask 15 and touches nothing on the sails. The 500 ms to 1 s and over 5 s gaps need no handler, because no click range covers them (FR-19). The controls package log count becomes 4 because the long press in disco logs "Button: disco off"; a short press in disco logs "Button: mill off" as before. `include/` has no tap code to remove: `mill_disco.h` and `mill_disco_esphome.h` hold no tap, tap run, white flash or button-press time (checked 2026-10-03), and the white channel of the RGBW output is always 0. This task contributes AT-10, AT-11, NFR-02 and the final NFR-01 review (FR-17, FR-18, FR-19, FR-20, FR-21 for the long press, FR-25, NFR-01, NFR-02, NFR-04, NFR-05).
