# Specification: Disco Mode

**Version:** 1.5
**Date:** 2026-10-03
**Status:** Approved
**Author:** Pete Turner (with Claude)

---

## Problem Statement

When music plays in the room, the operator wants the MM02 windmill to join in, but its four pixels can only show steady amber and Lamplight. The dusk visualisation (`docs/mill-at-dusk.html`) already shows the disco look the operator wants, and the real mill cannot show it. The mill has no microphone, so a tempo set by hand drifts against the song, and the operator needs a way to put the lights back on the beat.

## Beneficiaries

**Primary:**
- The operator (the maker), who is in the room with the mill and controls it with the Mill Button and the HA dashboard.

**Secondary:**
- The reviewer, who checks that the safety invariants in the handbook still hold.
- People in the room who see the mill. The operator judges the result on their behalf.

---

## Outcomes

**Must Haves**
- The operator switches the real mill to disco from HA, and the four pixels flash colours in a staggered chase in time with a tempo the operator sets.
- The disco look on the mill matches the dusk visualisation: a quarter-step chase, a new firing order every bar, a new set of colours every phrase, and flashes that fade to a dim glow, not to dark.
- The operator puts the chase on the beat of the music by tapping the Mill Button or the HA "Disco Tap" button, and a run of taps sets the tempo.
- The operator leaves disco with one action, from HA or the Mill Button, and the mill returns to normal lamplight with the sails as they were.
- The Mill Button can still stop the whole mill with no WiFi or HA.
- Disco never survives a restart: the mill boots dark and stopped.
- No pixel exceeds the 60% brightness cap. Each light starts at most 3 chase flashes a second, the white tap flash shows at most 3 times a second, and no chase flash starts within 1/3 s after a white flash.
- Disco does not slow or stutter the sails, and does not fill the HA logbook with a state change per flash.

**Nice-to-haves**
- A tap run sets the tempo for any steady tapping from 60 to 180 BPM, not only for the faster part of the range.

---

## Explicitly Out of Scope

- Any change to the dusk visualisation (`docs/mill-at-dusk.html`). It is the reference for the look and stays as it is.
- A microphone or any audio beat detection.
- Sails that follow the beat. The sails keep their own state, speed and direction.
- Clock sync from HA or from music players (for example Ableton Link or MIDI clock).
- Timestamps for HA taps taken on the HA side. HA taps carry the network delay, and "roughly on the beat" is accepted for them.
- Taps that align the bar or the phrase. Taps align the beat only.
- Colour choice by the operator. The disco colours come from the phrase.
- Disco on one light only. Disco always runs on all four lights together.
- Starting disco from the Mill Button. The button taps and leaves disco; it does not start it.
- The hub migration (`spec.md` Option B). The packages must stay ready for the hub, but the hub work is separate.

---

## Functional Requirements

In every statement, "the system" means the windmill firmware running on the ESP32-C3 together with the repository that holds it. "Bench-firmware FR-xx" means a requirement in `.sdd/windmill-bench-firmware/specification.md`. The four lights keep the names and pixels of bench-firmware FR-10: "Mill Door Lamp" (pixel 0), "Mill Door Glow" (pixel 1), "Mill Stone Floor Window" (pixel 2) and "Mill Bin Floor Window" (pixel 3).

### Disco controls

**FR-01: Disco controls in HA**
- **Statement:** The system shall offer in HA a "Disco Mode" switch, a "Disco BPM" number from 60 to 180 BPM in steps of 0.1 with a default of 120 BPM, a "Disco Rate" select with the options ½×, 1× and 2× with a default of 1×, and a "Disco Tap" button.

**FR-02: Disco effect on each light**
- **Statement:** The system shall offer a "Disco" effect in the effect list of each of the four lights, so that "Mill Door Lamp" offers "Disco" as its only effect and stays a steady light while "Disco Mode" is off, and the three interior lights offer "Disco" beside "Lamplight".

### Starting disco

**FR-03: Disco Mode on starts the chase on all four lights**
- **Statement:** When the operator turns on "Disco Mode" in HA, the system shall turn on any of the four lights that is off, run the "Disco" effect on all four lights together, and start the chase within 1 s, so that at the peak of each flash the brightest channel of that pixel reaches the 60% cap.

**FR-04: Disco on one light starts Disco Mode**
- **Statement:** While "Disco Mode" is off, when the operator picks the "Disco" effect for any one of the four lights in HA, the system shall turn on "Disco Mode" and run the chase on all four lights as FR-03 states.

**FR-05: Disco leaves the sails alone**
- **Statement:** When "Disco Mode" turns on or off by any means, the system shall leave "Sails Turning", "Reverse Rotation" and "Sail Speed" unchanged, so that turning sails keep turning at the same speed and direction without a pause and stopped sails stay stopped.

### The disco look

**FR-06: One flash per step**
- **Statement:** While "Disco Mode" is on, the system shall start one chase flash on each of the four pixels once per step, apart from chase flashes that the flash limit (FR-25, FR-26) skips, where one step lasts 60 ÷ ("Disco BPM" × "Disco Rate") seconds.

**FR-07: Staggered chase**
- **Statement:** While "Disco Mode" is on, the system shall start the four chase flashes of each step a quarter step apart, so that exactly one pixel starts a chase flash at each quarter step, apart from chase flashes that the flash limit (FR-25, FR-26) skips, and no two pixels start a flash at the same moment except in a tap confirmation flash (FR-24).

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
- **Statement:** While "Disco Rate" is 2×, when "Disco BPM" rises above 90 by any means, including a tap run, the system shall change "Disco Rate" to 1× and show 1× in HA within 5 s.

**FR-15: No automatic return to 2×**
- **Statement:** When "Disco BPM" falls to 90 or below after the rate guard changed "Disco Rate" to 1×, the system shall keep "Disco Rate" at 1× until the operator selects 2× again.

### Taps

**FR-16: A tap puts a beat on the tap**
- **Statement:** While "Disco Mode" is on, when the operator taps, the system shall move the chase so that a beat falls at the tap time and every later beat falls a whole number of beats after it, without starting a new bar or phrase at the tap, even when the move crosses a phrase boundary and so changes the colours.

**FR-17: Button tap time**
- **Statement:** When the operator taps with the Mill Button, the system shall take as the tap time the moment the button goes down, within 30 ms, whatever the press length from 50 ms to 500 ms.

**FR-18: HA tap time**
- **Statement:** When the operator presses "Disco Tap" in HA, the system shall start the tap confirmation flash within 1 s of the press and take as the tap time the start of that flash, within 50 ms, or, when the flash limit (FR-25) skips that flash, the moment the press reaches the device.

**FR-19: A tap run sets the tempo**
- **Statement:** When at least 4 taps of a tap run fall within the 3.5 s window that ends at the latest tap, the system shall set "Disco BPM" from the average spacing of the latest taps in that window, at most 8, and show the new value in HA within 5 s, so that steady tapping at any tempo from 60 to 180 BPM sets "Disco BPM" to that tempo.

**FR-20: A pause starts a new tap run**
- **Statement:** When the operator taps more than 1.2 s after the previous tap, the system shall start a new tap run with that tap and use no earlier tap for the tempo.

**FR-21: Fewer than 4 taps in the window leave the tempo alone**
- **Statement:** When the operator taps and fewer than 4 taps of the tap run fall within the 3.5 s window that ends at that tap, the system shall leave "Disco BPM" unchanged and only move the beat as FR-16 states.

**FR-22: Tapped tempo stays in range**
- **Statement:** If a tap run gives a tempo below 60 BPM or above 180 BPM, then the system shall set "Disco BPM" to 60 or 180, whichever is nearer.

**FR-23: An HA tap starts disco**
- **Statement:** While "Disco Mode" is off, when the operator presses "Disco Tap" in HA, the system shall turn on "Disco Mode" as FR-03 states, with a beat at the tap time of that press.

**FR-24: Tap confirmation flash**
- **Statement:** When the system accepts a tap, the system shall flash all four pixels white together at the 60% cap for 50 ms to 200 ms, unless the flash limit (FR-25) skips the flash, even on a pixel that started a chase flash less than 1/3 s before, starting after the Mill Button is released and no later than 100 ms after the release, or within 1 s of the "Disco Tap" press in HA, and then continue the chase on the new beat.

### Flash limit

**FR-25: The white flash wins over the chase**
- **Statement:** While less than 1/3 s has passed since the start of the latest tap confirmation flash, the system shall start no chase flash and no further tap confirmation flash on any pixel, but still move the beat for any tap in that time as FR-16 states.

**FR-26: Chase flashes stay 1/3 s apart**
- **Statement:** While "Disco Mode" is on, the system shall never start two chase flashes on one pixel less than 1/3 s apart, including across a "Disco Rate" change by the operator or by the drop from 2× to 1×.

### Mill Button in disco

**FR-27: Short press in disco is a tap**
- **Statement:** While "Disco Mode" is on, when the operator short-presses the Mill Button, the system shall treat the press as a tap and leave the sails and the on or off state of the four lights unchanged.

**FR-28: Long press in disco leaves disco**
- **Statement:** While "Disco Mode" is on, when the operator long-presses the Mill Button, the system shall turn off "Disco Mode" as FR-32 states and leave "Sails Turning" and "Reverse Rotation" unchanged.

**FR-29: Other presses in disco do nothing**
- **Statement:** While "Disco Mode" is on, if the operator holds the Mill Button for more than 500 ms but less than 1 s, or for more than 5 s, then the system shall leave "Disco Mode", the beat, the sails, "Reverse Rotation" and the four lights unchanged.

**FR-30: Disco and the button without the network**
- **Statement:** While "Disco Mode" is on and the device has no WiFi or HA connection, the system shall keep the chase running and act on Mill Button presses as FR-27, FR-28 and FR-29 state.

**FR-31: Two button actions stop the mill without the network**
- **Statement:** While "Disco Mode" is on and the device has no WiFi or HA connection, when the operator long-presses the Mill Button and then short-presses it, the system shall leave disco at the long press and turn the mill off at the short press, so that the sails stop within 1 s of the short press's release and all four pixels are dark within 4 s of it.

### Leaving disco

**FR-32: Disco Mode off returns to lamplight**
- **Statement:** When "Disco Mode" turns off by the operator's action on the HA switch or by a long press of the Mill Button, the system shall set all four lights to the lamplight look within 4 s, including any light that was off before disco started.

### Light changes from HA during disco

**FR-33: Brightness and colour changes keep disco**
- **Statement:** While "Disco Mode" is on, when the operator changes the brightness or colour of one of the four lights in HA and leaves that light on with the effect "Disco", the system shall keep "Disco Mode" on, scale that light's flash peaks with the new brightness with no peak above the 60% cap, and keep that light's flash colours as the phrase sets them.

**FR-34: A change to one light leaves disco**
- **Statement:** While "Disco Mode" is on, when the operator in HA turns off one of the four lights or picks an effect other than "Disco" for it, the system shall turn off "Disco Mode", give that light what the operator asked, and set the other three lights to the lamplight look within 4 s.

**FR-35: All four lights off leaves disco**
- **Statement:** While "Disco Mode" is on, when the operator turns off all four lights from HA within 1 s of each other (for example through the "Mill Lights" group), the system shall end with "Disco Mode" off and all four pixels dark within 4 s of the last of those commands, even if the lights still on begin to change towards the lamplight look before that command arrives.

### State reporting and start-up

**FR-36: HA shows the real disco state**
- **Statement:** While HA is connected, when "Disco Mode", "Disco BPM", "Disco Rate", or the state or effect of any of the four lights changes by any means, including the Mill Button, a tap run, the rate guard and a change to one light, the system shall show the new value in HA within 5 s.

**FR-37: No HA state change per flash**
- **Statement:** While "Disco Mode" is on, the system shall send HA no light state change for a chase flash or a tap confirmation flash, so that HA sees a change to the four lights or "Disco Mode" only when disco starts or stops or an entity value changes.

**FR-38: Disco never survives a restart**
- **Statement:** When the device starts after any reset or power restore, the system shall show "Disco Mode" off and keep all four pixels dark and the sails still, whatever values "Disco BPM" and "Disco Rate" hold, until the operator sends a command or presses the Mill Button.

**FR-39: Brightness cap in disco**
- **Statement:** The system shall drive each pixel at no more than 60% of its full output during every chase flash and every tap confirmation flash, whatever brightness or colour HA requests.

---

## Non-Functional Requirements

**NFR-01: Every disco path keeps the safety invariants**
- **Target:** Zero pixel paths for disco (the chase and the tap confirmation flash) that bypass the 60% cap on all four channels. Zero disco entities or settings that can turn on lights or start disco at boot; only "Disco BPM" and "Disco Rate" may keep a value across a restart.
- **Verification:** architectural-only. The reviewer checks the firmware files against handbook invariants 1 and 2 before each commit.

**NFR-02: Disco does not slow the sails**
- **Target:** With "Disco Mode" on at 180 BPM and 1×, the time for 5 full turns of the sails is within 2% of the time for 5 full turns at the same Sail Speed with "Disco Mode" off and all four lights on in the lamplight look. This holds at Sail Speed 170 steps/s and at the highest safe Sail Speed from 60 to 320 steps/s that the bench-firmware speed sweep (bench-firmware AT-05) records.
- **Verification:** platform-observed. At each of the two speeds, the operator times 5 full turns with a stopwatch on the bench, first with "Disco Mode" off and the lights in the lamplight look, then with "Disco Mode" on at 180 BPM and 1× while pressing "Disco Tap" in HA 5 times during the timing, and compares the two times.

**NFR-03: The chase keeps time**
- **Target:** With no taps, the offset between a pixel's on-beat flash and a reference metronome set to the same BPM changes by no more than 50 ms over 60 s.
- **Verification:** platform-observed. With "Disco Mode" on at 60 BPM and 1× and a metronome at 60 BPM playing aloud, the operator records the mill as a slow-motion video with sound for 60 s with no input, and reads the offset between the on-beat flashes and the clicks in the first and the last second of the video.

**NFR-04: The flash limit holds on every path**
- **Target:** Each light starts at most 3 chase flashes a second, the tap confirmation flash shows at most 3 times a second, and no chase flash starts within 1/3 s after the start of a tap confirmation flash. So: zero allowed pairs of "Disco BPM" and "Disco Rate" that give a step shorter than 1/3 s, zero firmware paths that start a chase flash less than 1/3 s after the start of a tap confirmation flash, and zero paths that start a tap confirmation flash less than 1/3 s after the start of the previous one, and zero paths, including a "Disco Rate" change by the operator or by the rate guard, that start two chase flashes on one pixel less than 1/3 s apart.
- **Verification:** architectural-only. The reviewer checks the firmware files, the BPM range and the rate guard before each commit.

**NFR-05: Packages stay ready for the hub**
- **Target:** Zero node-level settings (`esphome:`, `wifi:`, `api:`, `ota:`, `esp32:`) in any `packages/mill_*.yaml` file, and every node-level setting that the feature adds is recorded in the `spec.md` section "Migration to the main system".
- **Verification:** architectural-only. The reviewer checks the package files against the handbook "Repository layout" rule and checks the `spec.md` migration section.

---

## Acceptance Tests

**AT-01: Disco controls in HA** (FR-01, FR-02, FR-36)
- **Given:** The disco firmware runs for the first time after a full flash erase, HA is connected, and "Disco BPM" and "Disco Rate" have never been set.
- **When:** The operator opens the device page in HA, reads the disco entities, and then opens the effect list of each of the four lights.
- **Then:** HA shows "Disco Mode" off, "Disco BPM" at 120 with a range of 60 to 180 in steps of 0.1, "Disco Rate" at 1× with the options ½×, 1× and 2×, and a "Disco Tap" button; "Mill Door Lamp" offers "Disco" as its only effect; each interior light offers "Lamplight" and "Disco".

**AT-02: Disco Mode on from a dark mill** (FR-03, FR-05, FR-06, FR-07, FR-36)
- **Given:** HA is connected, the mill is dark and stopped, "Disco BPM" is 60 and "Disco Rate" is ½×, so one step lasts 2 s.
- **When:** The operator turns on "Disco Mode" on the HA dashboard and watches the pixels and the sails for 30 s.
- **Then:** All four pixels start the chase within 1 s; each pixel flashes once every 2 s; exactly one pixel starts a flash every 0.5 s, and no two pixels start a flash together; the sails stay still; within 5 s HA shows "Disco Mode" on, all four lights on with the effect "Disco", and "Sails Turning" off.

**AT-03: Disco on and off with the sails turning** (FR-02, FR-03, FR-05, FR-32, FR-36)
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

**AT-07: Rate guard** (FR-12, FR-13, FR-14, FR-15, FR-36)
- **Given:** "Disco Mode" is on, "Disco BPM" is 120, "Disco Rate" is 1×, and HA is connected.
- **When:** On the HA dashboard the operator selects 2× and watches for 10 s; sets "Disco BPM" to 90, selects 2× and watches for 10 s; sets "Disco BPM" to 120 and watches for 10 s; then sets "Disco BPM" to 80 and watches for 10 s.
- **Then:** The first 2× is refused: HA shows 1× within 5 s and the chase speed does not change. At 90 BPM, 2× is accepted, HA shows 2×, and "Mill Door Lamp" flashes 3 times a second. At 120 BPM, HA shows 1× within 5 s and the lamp flashes 2 times a second. At 80 BPM, "Disco Rate" stays at 1×.

**AT-08: A single tap from the button and from HA** (FR-16, FR-18, FR-24, FR-25, FR-27, FR-36)
- **Given:** HA is connected, the sails turn, "Disco Mode" is on at 60 BPM and 1×, the current colours changed about 8 beats ago, and the operator records the mill as a slow-motion video.
- **When:** The operator short-presses the Mill Button once, half-way between two beats, and watches for 10 s; then presses "Disco Tap" on the HA dashboard once and watches for 10 s.
- **Then:** After the button press, all four pixels flash white together for 50 ms to 200 ms, starting after the release and no later than 100 ms after it; no pixel starts a chase flash less than 1/3 s after the white flash starts; then the chase continues with exactly one pixel starting a flash at each quarter step; the colours, "Disco BPM", the sails and the on state of the four lights do not change, and HA still shows "Disco Mode" on. After the HA press, all four pixels flash white together within 1 s of the press and the chase continues in the same way.

**AT-09: Tap timing from the button and from HA** (FR-16, FR-17, FR-18)
- **Given:** "Disco Mode" is on at 60 BPM and 1×, a metronome at 60 BPM plays aloud, and the operator records the mill and the button as a slow-motion video with sound.
- **When:** The operator presses the Mill Button once on a metronome click, holds it for about 400 ms before release, and watches for 10 s; then presses "Disco Tap" on the HA dashboard once and watches for 10 s.
- **Then:** On the video, after the button press, the first on-beat flash after the white flash starts 1.000 s after the frame in which the button went down, within 30 ms, and not about 400 ms late. After the HA press, the first on-beat flash after the white flash starts 1.000 s after the first frame of the white flash, within 50 ms.

**AT-10: Tap runs set the tempo** (FR-14, FR-19, FR-20, FR-21, FR-22, FR-36)
- **Given:** HA is connected, "Disco Mode" is on, "Disco BPM" is 120, "Disco Rate" is 1×, and a metronome app is at hand. The operator waits at least 3 s between the parts of the test.
- **When:** With the Mill Button, the operator taps 3 times along with the metronome at 80 BPM; taps 8 times at 60 BPM; taps 8 times at 55 BPM; taps 8 times at 180 BPM; taps 8 times as fast as possible, at about 4 taps a second; taps 4 times at 100 BPM, pauses 2 s and taps 4 times at 140 BPM; then sets "Disco BPM" to 80 and "Disco Rate" to 2× in HA and taps 8 times at 120 BPM. The operator reads "Disco BPM" and "Disco Rate" in HA after each part.
- **Then:** After the 3 taps, "Disco BPM" stays at 120. After each other run, HA shows the new value within 5 s of the last tap: 57 to 63 for the 60 BPM run; 60 for the 55 BPM run; 177 to 180 for the 180 BPM run; 180 for the fastest run; 137 to 143 for the 100-then-140 run; 117 to 123 for the last run, with "Disco Rate" changed to 1×.

**AT-11: Flash limit with fast taps and rate changes** (FR-24, FR-25, FR-26)
- **Given:** HA is connected, "Disco Mode" is on at 180 BPM and 1×, so each pixel starts a chase flash every 1/3 s, and the operator records the mill as a slow-motion video.
- **When:** The operator short-presses the Mill Button 8 times as fast as possible, at about 4 taps a second, then gives no input for 5 s. On the HA dashboard the operator then sets "Disco BPM" to 90 and "Disco Rate" to ½× and waits 10 s; selects 2× half-way between two on-beat flashes and waits 10 s; then sets "Disco BPM" to 120, so that the rate guard drops "Disco Rate" to 1×, and waits 10 s. The operator plays the video back slowly.
- **Then:** Each white flash shows on all four pixels together; white flashes start at least 1/3 s apart, so at least one of the taps shows no white flash; no pixel starts a chase flash less than 1/3 s after the start of a white flash, and each pixel's chase flashes start at least 1/3 s apart; after the last tap, the chase continues with exactly one pixel starting a flash at each quarter step; across the change from ½× to 2× and across the drop from 2× to 1×, each pixel's chase flashes still start at least 1/3 s apart.

**AT-12: An HA tap starts disco** (FR-05, FR-23, FR-24, FR-36)
- **Given:** HA is connected, the mill is on in the lamplight look with the sails turning, and "Disco Mode" is off.
- **When:** The operator presses "Disco Tap" on the HA dashboard once and watches the mill for 10 s.
- **Then:** Within 1 s of the press, all four pixels flash white together and then start the chase; within 5 s HA shows "Disco Mode" on and all four lights on with the effect "Disco"; the sails keep turning at the same speed and direction.

**AT-13: Picking Disco on one light** (FR-03, FR-04, FR-36)
- **Given:** HA is connected, the mill is on in the lamplight look, and "Disco Mode" is off.
- **When:** The operator picks the effect "Disco" for "Mill Stone Floor Window" on the HA dashboard and watches the mill for 10 s.
- **Then:** All four pixels start the chase within 1 s, not only the stone floor window; within 5 s HA shows "Disco Mode" on and all four lights with the effect "Disco".

**AT-14: Mill Button presses in disco, and the stop path without WiFi** (FR-27, FR-28, FR-29, FR-30, FR-31, FR-32, FR-36)
- **Given:** HA is connected, the sails turn forward (anticlockwise) and "Disco Mode" is on.
- **When:** The operator holds the Mill Button for about 0.7 s and watches for 5 s; holds it for about 6 s and watches for 5 s; holds it for about 2 s and watches for 10 s; turns "Disco Mode" on again in HA; switches off the WiFi access point; short-presses the button once and watches for 5 s; holds it for about 2 s and watches for 10 s; short-presses it again and watches for 10 s; then switches the access point on again.
- **Then:** The 0.7 s and 6 s holds change nothing: the chase continues and the sails keep turning. After the first 2 s hold, within 4 s all four lights show the lamplight look, the sails keep turning anticlockwise without a pause, and within 5 s HA shows "Disco Mode" off with "Reverse Rotation" unchanged. With the access point off, the short press gives the white confirmation flash and the chase continues; the 2 s hold returns the lights to the lamplight look with the sails still turning anticlockwise; the next short press stops the sails within 1 s of release and the pixels are dark within 4 s of it. Within 5 minutes of the access point returning, HA shows "Disco Mode" off, "Sails Turning" off and all four lights off.

**AT-15: Brightness and colour from HA during disco** (FR-33, FR-36)
- **Given:** HA is connected, "Disco Mode" is on at 60 BPM and ½× with all four lights chasing, and the operator notes the brightness that HA shows for "Mill Door Glow".
- **When:** On the HA dashboard the operator sets "Mill Door Glow" to 30% brightness and watches for 20 s; sets its colour to pure blue and watches for 20 s, past the next phrase change; then sets its brightness back to the noted value and watches for 10 s.
- **Then:** After the brightness change, "Mill Door Glow" keeps its place in the chase with flashes visibly dimmer than the other three pixels, and the other pixels do not change. After the colour change, "Mill Door Glow" does not turn blue: it keeps the colour of the phrase and changes colour with the others at the next phrase. After the brightness returns, its flashes are as bright as those of the other pixels. Throughout, "Disco Mode" stays on, and HA shows "Mill Door Glow" on with the effect "Disco" and the brightness the operator set.

**AT-16: A change to one light leaves disco** (FR-02, FR-03, FR-34, FR-36)
- **Given:** HA is connected and "Disco Mode" is on with all four lights chasing.
- **When:** On the HA dashboard the operator turns off "Mill Door Glow" and watches for 10 s; turns "Disco Mode" on again, picks the effect "Lamplight" for "Mill Bin Floor Window" and watches for 10 s; turns "Disco Mode" on again, picks the effect "None" for "Mill Door Lamp" and watches for 10 s.
- **Then:** After each change, within 4 s the changed light shows what the operator asked ("Mill Door Glow" dark, "Mill Bin Floor Window" flickering with Lamplight, "Mill Door Lamp" steady), the other lights show the lamplight look, and within 5 s HA shows "Disco Mode" off and the true state and effect of all four lights. Each time "Disco Mode" turns on again, all four lights chase, including "Mill Door Glow".

**AT-17: All four lights off leaves disco** (FR-05, FR-35, FR-36)
- **Given:** HA is connected, the "Mill Lights" group exists, the sails turn, and "Disco Mode" is on.
- **When:** The operator turns off "Mill Lights" on the HA dashboard and watches the mill for 10 s.
- **Then:** All four pixels are dark within 4 s and stay dark; within 5 s HA shows "Disco Mode" off and all four lights off; the sails keep turning.

**AT-18: Disco does not survive a power cut** (FR-38)
- **Given:** "Disco Mode" is on at 140 BPM and ½×, the sails turn, and the device is powered from its USB charger.
- **When:** The operator unplugs the USB charger for 10 s, plugs it back in, and watches the mill for five minutes without sending any command.
- **Then:** From power-up onward all four pixels stay dark and the sails do not move; once the device reconnects, HA shows "Disco Mode" off, "Sails Turning" off and all four lights off; "Disco BPM" and "Disco Rate" show values inside their allowed ranges.

**AT-19: Brightness cap and flash peaks in disco** (FR-03, FR-24, FR-33, FR-39)
- **Given:** A meter with a maximum-hold function is in series with the pixel string's 5 V feed. The operator knows the limit from bench-firmware AT-11. HA is connected and the mill is dark.
- **When:** The operator reads the meter with all four lights off. The operator turns on "Disco Mode" at 60 BPM and ½× in HA, sets "Mill Door Lamp", "Mill Stone Floor Window" and "Mill Bin Floor Window" to 1% brightness, resets the maximum hold, watches for 16 s without a tap, and reads the maximum-hold value. The operator then sets all four lights in HA to 100% brightness and white, resets the maximum hold, presses "Disco Tap" in HA 5 times about 2 s apart, and reads the maximum-hold value again.
- **Then:** With three lights at 1%, those three flash visibly dimmer than "Mill Door Glow", and the brightest moment of each "Mill Door Glow" flash looks, by eye, as bright as that light does steady at 100% brightness. Both maximum-hold readings are at or below the bench-firmware AT-11 limit.

**AT-20: No HA state per flash** (FR-37)
- **Given:** HA is connected and "Disco Mode" has been on at 120 BPM and 1× for 1 minute.
- **When:** The operator leaves the mill alone for 10 minutes and then opens the HA logbook and the history of the four lights and "Disco Mode" for that 10 minutes.
- **Then:** The logbook and history show no state changes for the four lights or "Disco Mode" during the 10 minutes.

---

## Open Questions

- The exact flash decay curve and the frame interval. Starting points are the visualisation's curve and about 20 ms.
- Do "Disco BPM" and "Disco Rate" keep their values across a restart? Both are allowed, because they start nothing (FR-38). Design decides.

---

## Appendix

### Glossary
- **Operator:** The maker, who is in the room with the mill and controls it with the Mill Button and the HA dashboard.
- **Reviewer:** The person who reads the repository and checks the handbook safety invariants before a commit.
- **Disco Mode:** The HA switch that runs the chase on all four lights. It is off after every restart.
- **Chase:** The disco look: each pixel flashes once per step, a quarter step after the one before, apart from chase flashes that the flash limit skips (FR-06, FR-25, FR-26).
- **Flash:** A chase flash or a tap confirmation flash. The Lamplight flicker is not a flash.
- **Chase flash:** One pixel's flash in the chase: it starts at the peak and fades to a dim glow.
- **Flash peak:** The brightest point of a chase flash. At the brightness that "Disco Mode" sets when it starts, the brightest channel of the pixel is at the 60% cap at the peak.
- **Flash limit:** The rules that the white tap flash wins over the chase (FR-25) and that chase flashes stay 1/3 s apart (FR-26): a tap confirmation flash shows on all four pixels unless the previous one started less than 1/3 s before, and no pixel starts a chase flash less than 1/3 s after a tap confirmation flash starts. A chase flash that started just before a tap may sit closer than 1/3 s to the white flash. No pixel starts two chase flashes less than 1/3 s apart (FR-26).
- **Beat:** One beat of the music at "Disco BPM". The beat count runs on without a jump when "Disco BPM" changes from HA.
- **Step:** One cycle of the chase, of 60 ÷ ("Disco BPM" × "Disco Rate") seconds. Each pixel flashes once per step, apart from chase flashes that the flash limit skips (FR-06, FR-25, FR-26).
- **On-beat flash:** The flash that the first pixel in the firing order starts on a step.
- **Bar:** 4 beats. The firing order changes every bar.
- **Phrase:** 16 beats. The four colours change every phrase.
- **Firing order:** The order in which the four pixels flash within a step.
- **Disco Rate:** ½×, 1× or 2× the tempo for the chase. At ½× a step lasts two beats; at 2× it lasts half a beat. 2× is allowed only up to 90 BPM.
- **Rate guard:** The rule that keeps 2× to 90 BPM or below, so that no step is shorter than 1/3 s.
- **Tap:** A short press of the Mill Button while "Disco Mode" is on, or a press of "Disco Tap" in HA.
- **Tap time:** For the Mill Button, the moment the button goes down. For HA, the start of the tap confirmation flash, or the moment the press reaches the device when the flash limit skips that flash; both include the network delay.
- **Tap run:** A series of taps in which each tap comes no more than 1.2 s after the one before.
- **Confirmation flash:** The short white flash of the four pixels that shows the system accepted a tap. It shows on all four pixels, unless the flash limit skips the whole flash.
- **Lamplight look:** The lights as a button "mill on" sets them (bench-firmware FR-16): the three interior lights in warm deep amber with Lamplight and "Mill Door Lamp" steady in warm deep amber. Leaving disco sets this look on all four lights but does not change the sails.
- **Short press, long press:** As in the bench-firmware glossary: a hold of 50 ms to 500 ms, or of 1 s to 5 s.

### References
- `.sdd/disco-mode/research.md`: sections 2 (visualisation behaviour), 3 (constraints), 5 (recommended direction) and 9 (open questions).
- `.sdd/disco-mode/notes.md`: decisions with the user.
- `.sdd/windmill-bench-firmware/specification.md`: FR-10, FR-12, FR-13, FR-15, FR-16, FR-17, FR-18, FR-20, FR-21, FR-40, FR-41, FR-43, FR-44, AT-11 and the speed sweep (AT-05).
- Bench-firmware requirements this spec changes:
  - FR-12: "Mill Door Lamp" now offers "Disco" as its only effect; outside disco it stays steady (FR-02).
  - FR-13: the 60% cap also covers every chase flash and tap confirmation flash (FR-39).
  - FR-15 and FR-16: while disco is on, a short press is a tap, not mill off or mill on (FR-27).
  - FR-17 and FR-21: without the network, the chase keeps running and the button acts as it does in disco (FR-30, FR-31).
  - FR-18: HA also shows the disco entities and the effect of each light (FR-36).
  - FR-20 and handbook invariant 1: disco is off and the mill is dark and still after a restart (FR-38).
  - FR-40 and FR-43: while disco is on, a long press leaves disco instead (FR-28).
  - FR-44: still applies while disco is on (FR-29).
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
