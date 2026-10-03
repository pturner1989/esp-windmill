# Specification: Disco Mode

**Version:** 1.6
**Date:** 2026-10-03
**Status:** Approved
**Author:** Pete Turner (with Claude)

---

## Problem Statement

When music plays in the room, the operator wants the MM02 windmill to join in, but its four pixels can only show steady amber and Lamplight. The dusk visualisation (`docs/mill-at-dusk.html`) already shows the disco look the operator wants, and the real mill cannot show it. The mill has no microphone, so the operator sets the tempo by hand in HA.

## Beneficiaries

**Primary:**
- The operator (the maker), who is in the room with the mill and controls it with the Mill Button and the HA dashboard.

**Secondary:**
- The reviewer, who checks that the safety invariants in the handbook still hold.
- People in the room who see the mill. The operator judges the result on their behalf.

---

## Outcomes

**Must Haves**
- The operator switches the real mill to disco from HA, and the four pixels flash colours in a staggered chase in time with a tempo and rate the operator sets in HA.
- The disco look on the mill matches the dusk visualisation: a quarter-step chase, a new firing order every bar, a new set of colours every phrase, and flashes that fade to a dim glow, not to dark.
- The operator leaves disco with one action, from HA or a long press of the Mill Button, and the mill returns to normal lamplight with the sails as they were.
- One short press of the Mill Button still stops the whole mill, also in disco and also with no WiFi or HA.
- Disco never survives a restart: the mill boots dark and stopped.
- No pixel exceeds the 60% brightness cap, and each light starts at most 3 chase flashes a second.
- Disco does not slow or stutter the sails, and does not fill the HA logbook with a state change per flash.

**Nice-to-haves**
- None.

---

## Explicitly Out of Scope

- Any change to the dusk visualisation (`docs/mill-at-dusk.html`). It is the reference for the look and stays as it is.
- A microphone or any audio beat detection.
- Sails that follow the beat. The sails keep their own state, speed and direction.
- Clock sync from HA or from music players (for example Ableton Link or MIDI clock).
- Tap tempo and tap-to-beat: removed by the user 2026-10-03. No Mill Button press or HA button sets the beat or the tempo.
- Colour choice by the operator. The disco colours come from the phrase.
- Disco on one light only. Disco always runs on all four lights together.
- Starting disco from the Mill Button. The button leaves disco or turns the mill off; it does not start disco.
- The hub migration (`spec.md` Option B). The packages must stay ready for the hub, but the hub work is separate.

---

## Functional Requirements

In every statement, "the system" means the windmill firmware running on the ESP32-C3 together with the repository that holds it. "Bench-firmware FR-xx" means a requirement in `.sdd/windmill-bench-firmware/specification.md`. The four lights keep the names and pixels of bench-firmware FR-10: "Mill Door Lamp" (pixel 0), "Mill Door Glow" (pixel 1), "Mill Stone Floor Window" (pixel 2) and "Mill Bin Floor Window" (pixel 3).

### Disco controls

**FR-01: Disco controls in HA**
- **Statement:** The system shall offer in HA a "Disco Mode" switch, a "Disco BPM" number from 60 to 180 BPM in steps of 0.1 with a default of 120 BPM, and a "Disco Rate" select with the options ½×, 1× and 2× with a default of 1×.

**FR-02: Disco effect on each light**
- **Statement:** The system shall offer a "Disco" effect in the effect list of each of the four lights, so that "Mill Door Lamp" offers "Disco" as its only effect and stays a steady light while "Disco Mode" is off, and the three interior lights offer "Disco" beside "Lamplight".

### Starting disco

**FR-03: Disco Mode on starts the chase on all four lights**
- **Statement:** When the operator turns on "Disco Mode" in HA, the system shall turn on any of the four lights that is off, run the "Disco" effect on all four lights together, and start the chase within 1 s, so that at the peak of each flash the brightest channel of that pixel reaches the 60% cap.

**FR-04: Disco on one light starts Disco Mode**
- **Statement:** While "Disco Mode" is off, when the operator picks the "Disco" effect for any one of the four lights in HA, the system shall turn on "Disco Mode" and run the chase on all four lights as FR-03 states.

**FR-05: Disco leaves the sails alone**
- **Statement:** When "Disco Mode" turns on or off by any means other than a short press of the Mill Button (FR-17), the system shall leave "Sails Turning", "Reverse Rotation" and "Sail Speed" unchanged, so that turning sails keep turning at the same speed and direction without a pause and stopped sails stay stopped.

### The disco look

**FR-06: One flash per step**
- **Statement:** While "Disco Mode" is on, the system shall start one chase flash on each of the four pixels once per step, apart from chase flashes that the flash limit (FR-16) skips, where one step lasts 60 ÷ ("Disco BPM" × "Disco Rate") seconds.

**FR-07: Staggered chase**
- **Statement:** While "Disco Mode" is on, the system shall start the four chase flashes of each step a quarter step apart, so that exactly one pixel starts a chase flash at each quarter step, apart from chase flashes that the flash limit (FR-16) skips, and no two pixels start a flash at the same moment.

**FR-08: New firing order every bar**
- **Statement:** While "Disco Mode" is on, the system shall pick the order in which the four pixels flash at the start of each bar of 4 beats, so that any 8 bars in a row show at least 3 different orders.

**FR-09: Colours change every phrase**
- **Statement:** While "Disco Mode" is on, the system shall give each pixel one colour for a whole phrase of 16 beats, with the four colours a quarter of the colour wheel apart, and change to a different set of four colours at the start of each phrase.

**FR-10: Flashes fade to a dim glow**
- **Statement:** While "Disco Mode" is on, the system shall start each chase flash at its peak and fade it to 15% ± 5% of the peak by the start of the pixel's next chase flash, so that no pixel goes dark while the chase runs.

**FR-11: Tempo change without a jump**
- **Statement:** While "Disco Mode" is on, when the operator changes "Disco BPM" from HA, the system shall change to the new step length within 1 s and keep the chase going without a pause, without a jump in the chase, and without a change of the current colours.

**FR-12: Rate change on the beat**
- **Statement:** While "Disco Mode" is on, when "Disco Rate" changes to an allowed value, the system shall change to the new step length within 1 s, so that an on-beat flash falls on every second beat at ½×, on every beat at 1×, and on every beat and half beat at 2×.

### Rate guard

**FR-13: Refuse 2× above 90 BPM**
- **Statement:** If the operator selects 2× in "Disco Rate" while "Disco BPM" is above 90, then the system shall refuse 2×, run the chase at 1×, and show 1× in HA within 5 s.

**FR-14: Drop from 2× when the tempo rises above 90 BPM**
- **Statement:** While "Disco Rate" is 2×, when "Disco BPM" rises above 90, the system shall change "Disco Rate" to 1× and show 1× in HA within 5 s.

**FR-15: No automatic return to 2×**
- **Statement:** When "Disco BPM" falls to 90 or below after the rate guard changed "Disco Rate" to 1×, the system shall keep "Disco Rate" at 1× until the operator selects 2× again.

### Flash limit

**FR-16: Chase flashes stay 1/3 s apart**
- **Statement:** While "Disco Mode" is on, the system shall never start two chase flashes on one pixel less than 1/3 s apart, including across a "Disco Rate" change by the operator or by the drop from 2× to 1×.

### Mill Button in disco

**FR-17: Short press in disco turns the mill off**
- **Statement:** While "Disco Mode" is on, when the operator short-presses the Mill Button, the system shall turn the mill off as bench-firmware FR-15 states and turn off "Disco Mode", so that the sails stop within 1 s of the button's release and all four pixels are dark within 4 s of it.

**FR-18: Long press in disco leaves disco**
- **Statement:** While "Disco Mode" is on, when the operator long-presses the Mill Button, the system shall turn off "Disco Mode" as FR-21 states and leave "Sails Turning" and "Reverse Rotation" unchanged.

**FR-19: Other presses in disco do nothing**
- **Statement:** While "Disco Mode" is on, if the operator holds the Mill Button for more than 500 ms but less than 1 s, or for more than 5 s, then the system shall leave "Disco Mode", the chase, the sails, "Reverse Rotation" and the four lights unchanged.

**FR-20: Disco and the button without the network**
- **Statement:** While "Disco Mode" is on and the device has no WiFi or HA connection, the system shall keep the chase running and act on Mill Button presses as FR-17, FR-18 and FR-19 state.

### Leaving disco

**FR-21: Disco Mode off returns to lamplight**
- **Statement:** When "Disco Mode" turns off by the operator's action on the HA switch or by a long press of the Mill Button, the system shall set all four lights to the lamplight look within 4 s, including any light that was off before disco started.

### Light changes from HA during disco

**FR-22: Brightness and colour changes keep disco**
- **Statement:** While "Disco Mode" is on, when the operator changes the brightness or colour of one of the four lights in HA and leaves that light on with the effect "Disco", the system shall keep "Disco Mode" on, scale that light's flash peaks with the new brightness with no peak above the 60% cap, and keep that light's flash colours as the phrase sets them.

**FR-23: A change to one light leaves disco**
- **Statement:** While "Disco Mode" is on, when the operator in HA turns off one of the four lights or picks an effect other than "Disco" for it, the system shall turn off "Disco Mode", give that light what the operator asked, and set the other three lights to the lamplight look within 4 s.

**FR-24: All four lights off leaves disco**
- **Statement:** While "Disco Mode" is on, when the operator turns off all four lights from HA within 1 s of each other (for example through the "Mill Lights" group), the system shall end with "Disco Mode" off and all four pixels dark within 4 s of the last of those commands, even if the lights still on begin to change towards the lamplight look before that command arrives.

### State reporting and start-up

**FR-25: HA shows the real disco state**
- **Statement:** While HA is connected, when "Disco Mode", "Disco BPM", "Disco Rate", or the state or effect of any of the four lights changes by any means, including the Mill Button, the rate guard and a change to one light, the system shall show the new value in HA within 5 s.

**FR-26: No HA state change per flash**
- **Statement:** While "Disco Mode" is on, the system shall send HA no light state change for a chase flash, so that HA sees a change to the four lights or "Disco Mode" only when disco starts or stops or an entity value changes.

**FR-27: Disco never survives a restart**
- **Statement:** When the device starts after any reset or power restore, the system shall show "Disco Mode" off and keep all four pixels dark and the sails still, whatever values "Disco BPM" and "Disco Rate" hold, until the operator sends a command or presses the Mill Button.

**FR-28: Brightness cap in disco**
- **Statement:** The system shall drive each pixel at no more than 60% of its full output during every chase flash, whatever brightness or colour HA requests.

---

## Non-Functional Requirements

**NFR-01: Every disco path keeps the safety invariants**
- **Target:** Zero pixel paths for the chase that bypass the 60% cap on all four channels. Zero disco entities or settings that can turn on lights or start disco at boot; only "Disco BPM" and "Disco Rate" may keep a value across a restart.
- **Verification:** architectural-only. The reviewer checks the firmware files against handbook invariants 1 and 2 before each commit.

**NFR-02: Disco does not slow the sails**
- **Target:** With "Disco Mode" on at 180 BPM and 1×, the time for 5 full turns of the sails is within 2% of the time for 5 full turns at the same Sail Speed with "Disco Mode" off and all four lights on in the lamplight look. This holds at Sail Speed 170 steps/s and at the highest safe Sail Speed from 60 to 320 steps/s that the bench-firmware speed sweep (bench-firmware AT-05) records.
- **Verification:** platform-observed. At each of the two speeds, the operator times 5 full turns with a stopwatch on the bench, first with "Disco Mode" off and the lights in the lamplight look, then with "Disco Mode" on at 180 BPM and 1×, and compares the two times.

**NFR-03: The chase keeps time**
- **Target:** With no change to "Disco BPM" or "Disco Rate", the offset between a pixel's on-beat flash and a reference metronome set to the same BPM changes by no more than 50 ms over 60 s.
- **Verification:** platform-observed. With "Disco Mode" on at 60 BPM and 1× and a metronome at 60 BPM playing aloud, the operator records the mill as a slow-motion video with sound for 60 s with no input, and reads the offset between the on-beat flashes and the clicks in the first and the last second of the video.

**NFR-04: The flash limit holds on every path**
- **Target:** Each light starts at most 3 chase flashes a second. So: zero allowed pairs of "Disco BPM" and "Disco Rate" that give a step shorter than 1/3 s, and zero paths, including a "Disco Rate" change by the operator or by the rate guard, that start two chase flashes on one pixel less than 1/3 s apart.
- **Verification:** architectural-only. The reviewer checks the firmware files, the BPM range and the rate guard before each commit.

**NFR-05: Packages stay ready for the hub**
- **Target:** Zero node-level settings (`esphome:`, `wifi:`, `api:`, `ota:`, `esp32:`) in any `packages/mill_*.yaml` file, and every node-level setting that the feature adds is recorded in the `spec.md` section "Migration to the main system".
- **Verification:** architectural-only. The reviewer checks the package files against the handbook "Repository layout" rule and checks the `spec.md` migration section.

---

## Acceptance Tests

**AT-01: Disco controls in HA** (FR-01, FR-02, FR-25)
- **Given:** The disco firmware runs for the first time after a full flash erase, HA is connected, and "Disco BPM" and "Disco Rate" have never been set.
- **When:** The operator opens the device page in HA, reads the disco entities, and then opens the effect list of each of the four lights.
- **Then:** HA shows "Disco Mode" off, "Disco BPM" at 120 with a range of 60 to 180 in steps of 0.1, and "Disco Rate" at 1× with the options ½×, 1× and 2×, and no other disco entity; "Mill Door Lamp" offers "Disco" as its only effect; each interior light offers "Lamplight" and "Disco".

**AT-02: Disco Mode on from a dark mill** (FR-03, FR-05, FR-06, FR-07, FR-25)
- **Given:** HA is connected, the mill is dark and stopped, "Disco BPM" is 60 and "Disco Rate" is ½×, so one step lasts 2 s.
- **When:** The operator turns on "Disco Mode" on the HA dashboard and watches the pixels and the sails for 30 s.
- **Then:** All four pixels start the chase within 1 s; each pixel flashes once every 2 s; exactly one pixel starts a flash every 0.5 s, and no two pixels start a flash together; the sails stay still; within 5 s HA shows "Disco Mode" on, all four lights on with the effect "Disco", and "Sails Turning" off.

**AT-03: Disco on and off with the sails turning** (FR-02, FR-03, FR-05, FR-21, FR-25)
- **Given:** HA is connected, a short press of the Mill Button turned the mill on, "Reverse Rotation" is on, Sail Speed is 240 steps/s, and the operator then turned off "Mill Bin Floor Window" in HA.
- **When:** The operator turns on "Disco Mode" on the HA dashboard, watches the mill for 30 s, then turns off "Disco Mode" on the HA dashboard and watches for 10 s.
- **Then:** The sails turn clockwise at the same speed without a pause for the whole test; with "Disco Mode" on, "Mill Bin Floor Window" turns on and all four pixels chase within 1 s; with "Disco Mode" off, within 4 s all four lights show the lamplight look, including "Mill Bin Floor Window", with "Mill Door Lamp" steady; within 5 s of each change HA shows the new "Disco Mode" and light states, and "Sails Turning", "Reverse Rotation" and "Sail Speed" stay as they were.

**AT-04: Step length and rate** (FR-06, FR-07, FR-12)
- **Given:** "Disco Mode" is on, "Disco BPM" is 120 and "Disco Rate" is 1×, and the operator records the mill as a slow-motion video.
- **When:** The operator counts the flashes of "Mill Door Lamp" on the video over 30 s at each of these settings, set in turn on the HA dashboard: 120 BPM at 1×; "Disco Rate" ½×; "Disco BPM" 90 and then "Disco Rate" 2×; "Disco Rate" 1× and then "Disco BPM" 180.
- **Then:** The counts are 60 and 30, each within 1, and at the two settings with a 1/3 s step (90 BPM at 2×, 180 BPM at 1×) between 84 and 90, because the flash limit may skip a flash when the firing order changes; after each change the new flash rate starts within 1 s; at every setting exactly one pixel starts a flash at each quarter step.

**AT-05: Tempo change without a jump** (FR-11)
- **Given:** "Disco Mode" is on at 120 BPM and 1×, so one step lasts 0.5 s, the current colours have just changed, and the operator records the mill as a slow-motion video.
- **When:** The operator sets "Disco BPM" to 100 on the HA dashboard, so one step lasts 0.6 s, and watches the pixels for 10 s.
- **Then:** The chase slows within 1 s; on the video, each pixel's gap between the last flash before the change and the first flash after it lies between 0.5 s and 0.6 s, within 30 ms; the four colours do not change at the change.

**AT-06: Firing order, colours and fade** (FR-08, FR-09, FR-10)
- **Given:** "Disco Mode" is on at 60 BPM and ½×, so a bar lasts 4 s and a phrase lasts 16 s; the room is dark; the operator records the mill as a video and has a light meter with a maximum-hold function.
- **When:** The operator records 64 s of the chase without any input and then plays the video back slowly; then the operator holds the light meter against "Mill Door Glow", resets the maximum hold, and for 3 of its flashes reads the live value just before the next flash and the maximum-hold value.
- **Then:** In every 8 bars in a row the pixels flash in at least 3 different orders; each pixel keeps one colour for each 16 s phrase; the four colours of a phrase are hues about 90° apart on the colour wheel (for example red, chartreuse, cyan and violet); each new phrase shows a different set of four colours; just before each flash, the live light meter value is 10% to 20% of the maximum-hold value, and no pixel goes dark between flashes.

**AT-07: Rate guard** (FR-12, FR-13, FR-14, FR-15, FR-25)
- **Given:** "Disco Mode" is on, "Disco BPM" is 120, "Disco Rate" is 1×, and HA is connected.
- **When:** On the HA dashboard the operator selects 2× and watches for 10 s; sets "Disco BPM" to 90, selects 2× and watches for 10 s; sets "Disco BPM" to 120 and watches for 10 s; then sets "Disco BPM" to 80 and watches for 10 s.
- **Then:** The first 2× is refused: HA shows 1× within 5 s and the chase speed does not change. At 90 BPM, 2× is accepted, HA shows 2×, and "Mill Door Lamp" flashes 3 times a second. At 120 BPM, HA shows 1× within 5 s and the lamp flashes 2 times a second. At 80 BPM, "Disco Rate" stays at 1×.

**AT-08: Flash limit at the fastest step and across rate changes** (FR-16)
- **Given:** HA is connected, "Disco Mode" is on at 180 BPM and 1×, so each pixel starts a chase flash every 1/3 s, and the operator records the mill as a slow-motion video.
- **When:** The operator watches for 10 s. On the HA dashboard the operator then sets "Disco BPM" to 90 and "Disco Rate" to ½× and waits 10 s; selects 2× half-way between two on-beat flashes and waits 10 s; then sets "Disco BPM" to 120, so that the rate guard drops "Disco Rate" to 1×, and waits 10 s. The operator plays the video back slowly.
- **Then:** At 180 BPM, each pixel's chase flashes start at least 1/3 s apart; across the change from ½× to 2× and across the drop from 2× to 1×, each pixel's chase flashes still start at least 1/3 s apart; after each change the chase continues with exactly one pixel starting a flash at each quarter step.

**AT-09: Picking Disco on one light** (FR-03, FR-04, FR-25)
- **Given:** HA is connected, the mill is on in the lamplight look, and "Disco Mode" is off.
- **When:** The operator picks the effect "Disco" for "Mill Stone Floor Window" on the HA dashboard and watches the mill for 10 s.
- **Then:** All four pixels start the chase within 1 s, not only the stone floor window; within 5 s HA shows "Disco Mode" on and all four lights with the effect "Disco".

**AT-10: A short press in disco turns the mill off** (FR-17, FR-20, FR-25)
- **Given:** HA is connected, the sails turn and "Disco Mode" is on.
- **When:** The operator short-presses the Mill Button once and watches for 10 s; turns on "Sails Turning" and "Disco Mode" in HA; switches off the WiFi access point; short-presses the button once and watches for 10 s; then switches the access point on again.
- **Then:** After each short press, the sails stop within 1 s of the release, all four pixels are dark within 4 s of it and stay dark, and no light comes back on in the lamplight look. After the first press, within 5 s HA shows "Disco Mode" off, "Sails Turning" off and all four lights off. Within 5 minutes of the access point returning, HA shows the same.

**AT-11: Long and other presses in disco** (FR-18, FR-19, FR-20, FR-21, FR-25)
- **Given:** HA is connected, the sails turn forward (anticlockwise) and "Disco Mode" is on.
- **When:** The operator holds the Mill Button for about 0.7 s and watches for 5 s; holds it for about 6 s and watches for 5 s; holds it for about 2 s and watches for 10 s; turns "Disco Mode" on again in HA; switches off the WiFi access point and watches for 10 s; holds the button for about 0.7 s and watches for 5 s; holds it for about 2 s and watches for 10 s; then switches the access point on again.
- **Then:** The 0.7 s and 6 s holds change nothing: the chase continues and the sails keep turning. After the first 2 s hold, within 4 s all four lights show the lamplight look, the sails keep turning anticlockwise without a pause, and within 5 s HA shows "Disco Mode" off with "Sails Turning" and "Reverse Rotation" unchanged. With the access point off, the chase keeps running, the 0.7 s hold changes nothing, and the 2 s hold returns the lights to the lamplight look within 4 s with the sails still turning anticlockwise. Within 5 minutes of the access point returning, HA shows "Disco Mode" off and "Sails Turning" on.

**AT-12: Brightness and colour from HA during disco** (FR-22, FR-25)
- **Given:** HA is connected, "Disco Mode" is on at 60 BPM and ½× with all four lights chasing, and the operator notes the brightness that HA shows for "Mill Door Glow".
- **When:** On the HA dashboard the operator sets "Mill Door Glow" to 30% brightness and watches for 20 s; sets its colour to pure blue and watches for 20 s, past the next phrase change; then sets its brightness back to the noted value and watches for 10 s.
- **Then:** After the brightness change, "Mill Door Glow" keeps its place in the chase with flashes visibly dimmer than the other three pixels, and the other pixels do not change. After the colour change, "Mill Door Glow" does not turn blue: it keeps the colour of the phrase and changes colour with the others at the next phrase. After the brightness returns, its flashes are as bright as those of the other pixels. Throughout, "Disco Mode" stays on, and HA shows "Mill Door Glow" on with the effect "Disco" and the brightness the operator set.

**AT-13: A change to one light leaves disco** (FR-02, FR-03, FR-23, FR-25)
- **Given:** HA is connected and "Disco Mode" is on with all four lights chasing.
- **When:** On the HA dashboard the operator turns off "Mill Door Glow" and watches for 10 s; turns "Disco Mode" on again, picks the effect "Lamplight" for "Mill Bin Floor Window" and watches for 10 s; turns "Disco Mode" on again, picks the effect "None" for "Mill Door Lamp" and watches for 10 s.
- **Then:** After each change, within 4 s the changed light shows what the operator asked ("Mill Door Glow" dark, "Mill Bin Floor Window" flickering with Lamplight, "Mill Door Lamp" steady), the other lights show the lamplight look, and within 5 s HA shows "Disco Mode" off and the true state and effect of all four lights. Each time "Disco Mode" turns on again, all four lights chase, including "Mill Door Glow".

**AT-14: All four lights off leaves disco** (FR-05, FR-24, FR-25)
- **Given:** HA is connected, the "Mill Lights" group exists, the sails turn, and "Disco Mode" is on.
- **When:** The operator turns off "Mill Lights" on the HA dashboard and watches the mill for 10 s.
- **Then:** All four pixels are dark within 4 s and stay dark; within 5 s HA shows "Disco Mode" off and all four lights off; the sails keep turning.

**AT-15: Disco does not survive a power cut** (FR-27)
- **Given:** "Disco Mode" is on at 140 BPM and ½×, the sails turn, and the device is powered from its USB charger.
- **When:** The operator unplugs the USB charger for 10 s, plugs it back in, and watches the mill for five minutes without sending any command.
- **Then:** From power-up onward all four pixels stay dark and the sails do not move; once the device reconnects, HA shows "Disco Mode" off, "Sails Turning" off and all four lights off; "Disco BPM" and "Disco Rate" show values inside their allowed ranges.

**AT-16: Brightness cap and flash peaks in disco** (FR-03, FR-22, FR-28)
- **Given:** A meter with a maximum-hold function is in series with the pixel string's 5 V feed. The operator knows the limit from bench-firmware AT-11. HA is connected and the mill is dark.
- **When:** The operator reads the meter with all four lights off. The operator turns on "Disco Mode" at 60 BPM and ½× in HA, sets "Mill Door Lamp", "Mill Stone Floor Window" and "Mill Bin Floor Window" to 1% brightness, resets the maximum hold, watches for 16 s, and reads the maximum-hold value. The operator then sets all four lights in HA to 100% brightness and white, resets the maximum hold, watches for 16 s, and reads the maximum-hold value again.
- **Then:** With three lights at 1%, those three flash visibly dimmer than "Mill Door Glow", and the brightest moment of each "Mill Door Glow" flash looks, by eye, as bright as that light does steady at 100% brightness. Both maximum-hold readings are at or below the bench-firmware AT-11 limit.

**AT-17: No HA state per flash** (FR-26)
- **Given:** HA is connected and "Disco Mode" has been on at 120 BPM and 1× for 1 minute.
- **When:** The operator leaves the mill alone for 10 minutes and then opens the HA logbook and the history of the four lights and "Disco Mode" for that 10 minutes.
- **Then:** The logbook and history show no state changes for the four lights or "Disco Mode" during the 10 minutes.

---

## Open Questions

- The exact flash decay curve and the frame interval. Starting points are the visualisation's curve and about 20 ms.
- Do "Disco BPM" and "Disco Rate" keep their values across a restart? Both are allowed, because they start nothing (FR-27). Design decides.

---

## Appendix

### Glossary
- **Operator:** The maker, who is in the room with the mill and controls it with the Mill Button and the HA dashboard.
- **Reviewer:** The person who reads the repository and checks the handbook safety invariants before a commit.
- **Disco Mode:** The HA switch that runs the chase on all four lights. It is off after every restart.
- **Chase:** The disco look: each pixel flashes once per step, a quarter step after the one before, apart from chase flashes that the flash limit skips (FR-06, FR-16).
- **Flash:** A chase flash. The Lamplight flicker is not a flash.
- **Chase flash:** One pixel's flash in the chase: it starts at the peak and fades to a dim glow.
- **Flash peak:** The brightest point of a chase flash. At the brightness that "Disco Mode" sets when it starts, the brightest channel of the pixel is at the 60% cap at the peak.
- **Flash limit:** The rule that no pixel starts two chase flashes less than 1/3 s apart (FR-16), so each light starts at most 3 chase flashes a second. A scheduled chase flash that would break the rule is skipped.
- **Beat:** One beat of the music at "Disco BPM". The beat count runs on without a jump when "Disco BPM" changes from HA.
- **Step:** One cycle of the chase, of 60 ÷ ("Disco BPM" × "Disco Rate") seconds. Each pixel flashes once per step, apart from chase flashes that the flash limit skips (FR-06, FR-16).
- **On-beat flash:** The flash that the first pixel in the firing order starts on a step.
- **Bar:** 4 beats. The firing order changes every bar.
- **Phrase:** 16 beats. The four colours change every phrase.
- **Firing order:** The order in which the four pixels flash within a step.
- **Disco Rate:** ½×, 1× or 2× the tempo for the chase. At ½× a step lasts two beats; at 2× it lasts half a beat. 2× is allowed only up to 90 BPM.
- **Rate guard:** The rule that keeps 2× to 90 BPM or below, so that no step is shorter than 1/3 s.
- **Mill off:** What a short press does while the mill is on (bench-firmware FR-15): the sails stop and all four lights fade off. In disco it also turns "Disco Mode" off (FR-17).
- **Lamplight look:** The lights as a button "mill on" sets them (bench-firmware FR-16): the three interior lights in warm deep amber with Lamplight and "Mill Door Lamp" steady in warm deep amber. Leaving disco sets this look on all four lights but does not change the sails.
- **Short press, long press:** As in the bench-firmware glossary: a hold of 50 ms to 500 ms, or of 1 s to 5 s.

### References
- `.sdd/disco-mode/research.md`: sections 2 (visualisation behaviour), 3 (constraints), 5 (recommended direction) and 9 (open questions).
- `.sdd/disco-mode/notes.md`: decisions with the user. The tap decisions there are superseded by the user decision of 2026-10-03 (version 1.6).
- `.sdd/windmill-bench-firmware/specification.md`: FR-10, FR-12, FR-13, FR-15, FR-16, FR-17, FR-18, FR-20, FR-21, FR-40, FR-41, FR-43, FR-44, AT-11 and the speed sweep (AT-05).
- Bench-firmware requirements this spec changes:
  - FR-12: "Mill Door Lamp" now offers "Disco" as its only effect; outside disco it stays steady (FR-02).
  - FR-13: the 60% cap also covers every chase flash (FR-28).
  - FR-15: while disco is on, a short press also turns "Disco Mode" off (FR-17).
  - FR-17 and FR-21: without the network, the chase keeps running and the button acts as it does in disco (FR-20).
  - FR-18: HA also shows the disco entities and the effect of each light (FR-25).
  - FR-20 and handbook invariant 1: disco is off and the mill is dark and still after a restart (FR-27).
  - FR-40 and FR-43: while disco is on, a long press leaves disco instead (FR-18).
  - FR-44: still applies while disco is on (FR-19).
- `.sdd/handbook.md`: "Safety invariants", "Repository layout", "Testing".
- `spec.md`: "Migration to the main system".
- `docs/mill-at-dusk.html`: the reference disco look (version 4 script).

### Change History
| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0 | 2026-10-03 | Pete Turner (with Claude) | Initial specification |
| 1.1 | 2026-10-03 | Pete Turner (with Claude) | Review round 1 fixes and user decisions D1–D4 |
| 1.2 | 2026-10-03 | Pete Turner (with Claude) | D1 revised: white tap flash wins over the chase |
| 1.3 | 2026-10-03 | Pete Turner (with Claude) | Review round 2 fixes |
| 1.4 | 2026-10-03 | Pete Turner (with Claude) | AT-04 allows flash-limit skips at a 1/3 s step; AT-19 checks flash peaks by eye (design findings) |
| 1.5 | 2026-10-03 | Pete Turner (with Claude) | FR-18 and AT-09: HA tap time within 50 ms (design: frames run every 16–32 ms) |
| 1.6 | 2026-10-03 | Pete Turner (with Claude) | Taps removed by user decision; short press in disco turns the mill off |
