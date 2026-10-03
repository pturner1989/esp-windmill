# Specification: Windmill Bench Firmware

**Version:** 1.10
**Date:** 2026-10-03
**Status:** Implemented
**Author:** Pete Turner (with Claude)

---

## Problem Statement

The motorised windmill needs its own firmware on the ESP32-C3 before any part of the model is glued shut, because after assembly there is no access to the pixels, the tower wiring or the motor. The draft configuration in `spec.md` does not validate, does not enforce the 60% brightness cap, and its button stops working after the first press. The operator has no way yet to prove the electronics against the `spec.md` Phase 1 bench checklist.

## Beneficiaries

**Primary:**
- The operator (the maker), who controls and tests the mill through Home Assistant (HA), the physical button, a meter, and by eye on the bench.

**Secondary:**
- The reviewer, who reads the repository and runs the validation commands before a commit.
- People in the room who see the finished mill. The operator judges the result on their behalf.

---

## Outcomes

**Must Haves**
- Our firmware runs on the C3. After the first USB flash, the operator updates it over the network.
- The operator controls the sails, the sail speed and each of the four pixels from HA.
- The button turns the whole mill on and off, with or without WiFi and HA.
- After any reset or power cut, the mill is dark and still until someone asks for something.
- No pixel ever exceeds 60% of its full output, whatever HA requests.
- The operator can reverse the sails from HA at any time, and the sails start forward after every restart.
- Every item in `spec.md` "Phase 1, electronics on the bench" that depends on the firmware and that a breadboard can test passes, except the pixel addressing check and the one-hour run, which the user deferred on 2026-10-03.
- The reviewer can lint, validate and compile the firmware with placeholder secrets, and no real credential reaches the repository.

**Nice-to-haves**
- The speed sweep record gives the operator a measured highest safe speed to use in Phase 4 tuning.

---

## Explicitly Out of Scope

- Soft spin-up and coast-down of the sails. Start and stop stay instant. Deferred to a later feature.
- An on-device web control page, for the bench or for production. HA is the only remote control surface. (The WiFi setup page of the fallback access point is not a control page.)
- Button actions other than the short press and the long press, for example a double press.
- Multi-module platform firmware. This feature covers the windmill only.
- Migration to the main hub (`spec.md` Option B).
- Kit assembly and `spec.md` Phases 2, 3 and 4.
- Final tuning by eye of brightness, colour and sail speed (Phase 4).
- HA scheduling, scenes and automations, other than creating the two light groups.
- A change of motor to an N20 gearmotor if the cap proves too small.
- Lighting the sails or adding a fifth pixel.
- Hardware choices that do not change firmware behaviour: the level shifter versus diode drop, the 330R data resistor, and capacitor values.
- The 5V rail voltage check (the rail stays above 4.5V with the stepper and pixels running). It stays a Phase 1 item in `spec.md`, done with a multimeter, outside this firmware spec.
- Deferred by the user on 2026-10-03: a bench option, that is, a separate bench package with test-only controls that one line includes, and the rule that one change removes it (old NFR-03). "Reverse Rotation" is a permanent production control instead (FR-30).
- Deferred by the user on 2026-10-03: the pixel addressing test, which lights each pixel alone in the order 0, 1, 2, 3 and checks the colour order (old FR-29, AT-22).
- Deferred by the user on 2026-10-03: the check that a build without the bench option offers no test controls in HA (old FR-33, AT-23).
- Deferred by the user on 2026-10-03: the repository checks with and without the bench option (six runs). FR-34 and AT-24 cover the single production configuration (three runs).
- Deferred by the user on 2026-10-03: the pointer in `spec.md` to the firmware files and the handbook update (old FR-38, AT-27). `spec.md` still holds the old draft configuration listing.
- Deferred by the user on 2026-10-03: the one-hour run with the motor and pixels together, with the capacitors fitted (old AT-29). This was the bench proof of FR-14. The firmware half of FR-14 is delivered.

---

## Functional Requirements

In every statement, "the system" means the windmill firmware running on the ESP32-C3 together with the repository that holds it.

### Sails

**FR-01: Turn the sails while Sails Turning is on**
- **Statement:** While "Sails Turning" is on, the system shall keep the sails turning at the current Sail Speed, forward unless "Reverse Rotation" is on (FR-30), with no limit on how long they run.

**FR-02: Stop the sails at once**
- **Statement:** When the operator turns off "Sails Turning" in HA or a button press turns the mill off, the system shall stop the sails in either direction within 1 s of the operator's action, with no coast-down.

**FR-03: Show the sails turning in either direction**
- **Statement:** While the sails turn in either direction, the system shall show "Sails Turning" as on in HA.

**FR-04: Hold the sails when stopped**
- **Statement:** While the sails are stopped, the system shall keep the motor energised so that the sails hold their position.

**FR-05: Sail Speed range and default**
- **Statement:** The system shall offer "Sail Speed" in HA with a range of 60 to 320 steps/s in steps of 10 and a default of 170 steps/s that applies until the operator sets another value.

**FR-06: Round Sail Speed to a step of 10**
- **Statement:** When the operator sets a Sail Speed value from 60 to 320 steps/s that is not a multiple of 10, the system shall round it to the nearest multiple of 10 (a value halfway between two multiples rounds up), use the rounded value, and show the rounded value in HA.

**FR-07: Reject Sail Speed outside the range**
- **Statement:** If the operator sets a Sail Speed value below 60 or above 320 steps/s, then the system shall reject the value and keep the current speed unchanged.

**FR-08: Change speed while turning**
- **Statement:** While the sails turn, when the operator changes Sail Speed in HA, the system shall change to the new speed without stopping the sails.

**FR-09: Keep the speed setting across a restart**
- **Statement:** When the device restarts at least 2 minutes after the operator last set Sail Speed, the system shall use that value.

### Lights

**FR-10: Four individual lights**
- **Statement:** The system shall offer four HA lights, "Mill Door Lamp" (pixel 0, outside), "Mill Door Glow" (pixel 1, ground floor door), "Mill Stone Floor Window" (pixel 2) and "Mill Bin Floor Window" (pixel 3), each of which changes the state, colour and brightness of its own pixel only and shows red, green and blue on that pixel as HA requests them.

**FR-11: Independent lamplight on the interior pixels**
- **Statement:** The system shall offer a Lamplight flicker effect on each of the three interior lights that flickers each pixel independently of the other pixels.

**FR-12: Steady door lamp**
- **Statement:** The system shall show "Mill Door Lamp" only as a steady light, with no flicker effect.

**FR-13: Brightness cap**
- **Statement:** The system shall drive each pixel at no more than 60% of its full output, whatever brightness or colour HA or the button requests.

**FR-14: Steady lights send no pixel data**
- **Statement:** While all four lights show steady colours with no effect, the system shall send no new data to the pixels.

**FR-41: Lights fade on and off**
- **Statement:** When any of the four mill lights turns on or off, by the button or from HA without a requested transition time or effect, the system shall fade its pixel to the new state, so that the pixel begins to change within 1 s of the operator's action and finishes the change within 4 s of it.

### Button and state reporting

**FR-15: Short press turns the mill off**
- **Statement:** While the sails turn in either direction or any of the four lights is on, when the operator short-presses the button, the system shall turn the mill off: the sails stop within 1 s of the button's release, and all four lights fade to off as FR-41 states.

**FR-16: Short press turns the mill on**
- **Statement:** While the sails are stopped and all four lights are off, when the operator short-presses the button, the system shall turn the mill on: the sails start turning at the current Sail Speed within 1 s of the button's release, and the lights fade on as FR-41 states, the three interior lights to warm deep amber with Lamplight and "Mill Door Lamp" to a steady warm deep amber.

**FR-17: Button works without the network**
- **Statement:** While the device has no WiFi or HA connection, when the operator short-presses or long-presses the button, the system shall act on the mill exactly as it does when connected.

**FR-18: HA shows the real state**
- **Statement:** While HA is connected, when the sails or any light changes state by any means, including the button, the system shall show the new state in HA within 5 s.

**FR-19: Mill Button state in HA**
- **Statement:** The system shall show in HA, as "Mill Button", whether the button is pressed or released.

**FR-40: Long press reverses the turning sails**
- **Statement:** While the sails turn, when the operator holds the Mill Button for 1 s to 5 s and then releases it, the system shall change "Reverse Rotation" to its other state, so that the sails change direction within 1 s of the release without stopping, "Sails Turning" stays on, the lights stay as they are, and HA shows the new "Reverse Rotation" state within 5 s while HA is connected.

**FR-43: Long press while stopped does nothing**
- **Statement:** While the sails are stopped, when the operator holds the Mill Button for 1 s to 5 s and then releases it, the system shall leave the sails, "Reverse Rotation" and the four lights unchanged.

**FR-44: Other presses do nothing**
- **Statement:** If the operator holds the Mill Button for more than 500 ms but less than 1 s, or for more than 5 s, then the system shall leave the sails, "Reverse Rotation" and the four lights unchanged.

### Start-up and network

**FR-20: Dark and still at start-up**
- **Statement:** When the device starts after any reset or power restore, the system shall keep all four pixels dark and the sails still, and show "Sails Turning", "Reverse Rotation" and all four lights as off in HA, until the operator sends a command or presses the button.

**FR-21: Keep running during network loss**
- **Statement:** If the WiFi or HA connection drops, then the system shall keep the sails and lights in their current state, without restarting, for as long as the loss lasts, unless the operator presses the Mill Button.

**FR-22: Rejoin WiFi**
- **Statement:** When the configured WiFi network returns after a loss, the system shall rejoin it within 5 minutes without operator action.

**FR-23: Accept HA reconnection**
- **Statement:** When HA reconnects after a loss of the HA connection, the system shall accept the reconnection within 5 minutes without operator action.

**FR-24: Fallback access point**
- **Statement:** If the configured WiFi network is unavailable, then the system shall offer, within 2 minutes, a password-protected access point named "Windmill Fallback" through which the operator can enter new WiFi details without a USB cable.

**FR-25: Encrypted HA connection**
- **Statement:** The system shall accept an HA connection only when HA presents the configured encryption key.

**FR-26: Network updates**
- **Statement:** When the operator sends a firmware update over the network, the system shall install it when it carries the configured update password, and reject it otherwise.

**FR-27: Logs over the network**
- **Statement:** While the operator has requested the device log over the network, when a button press turns the mill on or off or reverses the sails, the system shall send a log message over the network that says so.

**FR-28: No logs over USB serial**
- **Statement:** The system shall send no firmware log messages over the USB serial port.

### Sail direction

**FR-30: Reverse rotation**
- **Statement:** While "Reverse Rotation" is on, the system shall turn the sails in the reverse direction, not the forward direction, whenever the sails turn.

**FR-31: Change direction while turning**
- **Statement:** While the sails turn, when the operator turns "Reverse Rotation" on or off, the system shall change the direction of the sails without the operator stopping them first.

**FR-42: Change direction while stopped**
- **Statement:** While the sails are stopped, when the operator turns "Reverse Rotation" on or off, the system shall keep the sails stopped and "Sails Turning" off, and use the new direction the next time the sails start.

**FR-32: Short press leaves the direction alone**
- **Statement:** When the operator short-presses the button, the system shall turn the mill on or off and leave "Reverse Rotation" as it is. Only the long press (FR-40) and the operator's own change of "Reverse Rotation" change the direction.

### Repository

**FR-34: Checks pass with placeholder secrets**
- **Statement:** When the reviewer runs the YAML lint, the configuration validation command and the compile command on the production configuration with only the placeholder secrets in place, the system shall pass all three with no errors.

**FR-35: Secrets file out of version control**
- **Statement:** The system shall keep the real secrets file out of version control.

**FR-36: Example secrets file**
- **Statement:** The system shall hold a committed example secrets file that contains placeholder values only.

**FR-37: Repeatable builds**
- **Statement:** The system shall pin the ESPHome and yamllint versions in the repository, so that the same commit builds with the same tool versions.

**FR-39: HA group instructions**
- **Statement:** The system shall include a short note in the repository that tells the operator how to create the HA light groups "Mill Interior" (pixels 1–3) and "Mill Lights" (all four pixels).

---

## Non-Functional Requirements

**NFR-01: Brightness cap covers every pixel path**
- **Target:** Every path that sets pixel output applies the 60% cap: each of the four lights and the button's mill-on action. Zero paths without the cap.
- **Verification:** architectural-only. The reviewer confirms in the firmware files that each path carries the cap.

**NFR-02: Safety invariants hold in the firmware files**
- **Target:** Zero violations of these rules in review: nothing restores a lit, turning or reversed state at start-up; no path sets Sail Speed outside 60–320 steps/s; stepper outputs IN1–IN4 are GPIO0, GPIO1, GPIO3 and GPIO4, GPIO2 is unconnected, pixel data is GPIO6 and the button is GPIO5, as `spec.md` "Pin allocation" states; serial logging is off; the committed log level is INFO.
- **Verification:** architectural-only. The reviewer checks the firmware files against the handbook "Safety invariants" before each commit.

---

## Acceptance Tests

**AT-01: Forward rotation, start and stop** (FR-01, FR-02, FR-03, FR-18; Phase 1 "rotation")
- **Given:** The ULN2003 and a bare 28BYJ-48 are wired on the breadboard, a mark is drawn on the motor shaft coupler, Sail Speed is 170 steps/s, the sails are stopped, and "Reverse Rotation" is off.
- **When:** The operator turns on "Sails Turning" in HA, watches the mark for one minute from the side where the sails attach, times three full turns with a stopwatch, then turns off "Sails Turning" in HA.
- **Then:** The mark turns steadily anticlockwise (forward) for the whole minute; three turns take between 34.3 s and 38.0 s; the mark stops within 1 s of the switch going off in HA; HA shows "Sails Turning" on while the mark turns and off within 5 s of the stop.

**AT-02: Reverse rotation and direction change** (FR-30, FR-31, FR-42, FR-32, FR-03, FR-02, FR-15, FR-20; Phase 1 "rotation both directions")
- **Given:** The motor and the button are wired as in AT-01, Sail Speed is 170 steps/s, the sails are stopped and "Reverse Rotation" is off. The device is powered from its USB charger.
- **When:** The operator turns on "Reverse Rotation" in HA and watches the mark for 5 s; turns on "Sails Turning", watches the mark for one minute and times three full turns; then, while the sails turn, turns off "Reverse Rotation" and watches for 30 s; turns "Reverse Rotation" on again and watches for 30 s; turns off "Sails Turning"; then turns on "Sails Turning" again, with "Reverse Rotation" still on, and short-presses the button. Last, with "Reverse Rotation" still on, the operator unplugs the USB charger for 10 s, plugs it back in, waits until HA shows the device connected, and turns on "Sails Turning".
- **Then:** While the sails are stopped, turning on "Reverse Rotation" does not move the mark, and "Sails Turning" stays off. With "Reverse Rotation" on, the mark turns steadily clockwise, opposite to AT-01, three turns take a time within the AT-01 window, and HA shows "Sails Turning" on. When "Reverse Rotation" goes off, the mark changes to anticlockwise without the operator stopping it, and "Sails Turning" stays on. When "Reverse Rotation" goes on again, the mark changes back to clockwise. When "Sails Turning" goes off, the mark stops within 1 s of the switch going off in HA. After the button press, the mark stops within 1 s of release, HA shows "Sails Turning" off within 5 s, and "Reverse Rotation" stays on. After the restart, HA shows "Reverse Rotation" off, and when "Sails Turning" goes on the mark turns anticlockwise.

**AT-03: Sails hold when stopped** (FR-04)
- **Given:** The sails have turned and then stopped, and a meter is in series with the ULN2003 board's 5 V feed only.
- **When:** The operator watches the coil LEDs on the ULN2003 board, reads the meter, and gently tries to turn the coupler by hand.
- **Then:** At least one coil LED stays lit, and the meter reads between 50 mA and 300 mA. The operator also notes whether the coupler resists being turned by hand, as information only and not as a pass condition.

**AT-04: Sail Speed range, default and rounding** (FR-05, FR-06, FR-07)
- **Given:** The firmware runs for the first time after a full flash erase, and Sail Speed has never been set.
- **When:** The operator opens "Sail Speed" in HA, then uses HA developer tools to set it to 400, then to 50, then to 245.
- **Then:** HA shows 170 steps/s at first and offers only 60 to 320 in steps of 10; the requests for 400 and 50 are rejected and the value stays at 170; the request for 245 sets the speed to 250 and HA shows 250.

**AT-05: Speed sweep** (FR-05; Phase 1 "sweep the speed number")
- **Given:** The sails turn with a mark on the coupler, and the operator has a stopwatch.
- **When:** The operator sets Sail Speed in HA to 60, 100, 140, 170, 200, 240, 280 and 320 in turn, and at each setting times three full turns of the mark.
- **Then:** The record holds a measured three-turn time for every setting in the sweep from 60 to 320, beside the expected time (3 × 2048 ÷ setting, in seconds), and no measured three-turn time is below 95% of the expected time. The operator also records, as information for Phase 4 and not as a pass condition, the highest setting at which the measured time stays within 5% of the expected time with no stutter or buzzing.

**AT-06: Speed change without stopping** (FR-08)
- **Given:** The sails turn at 170 steps/s.
- **When:** The operator times three full turns, sets Sail Speed to 240 in HA, and times three more full turns.
- **Then:** The mark keeps moving with no pause; the first three turns take between 34.3 s and 38.0 s, and the three turns at 240 steps/s take between 24.3 s and 26.9 s.

**AT-07: Speed setting survives a restart** (FR-09)
- **Given:** The operator set Sail Speed to 240 at least 2 minutes before the power cut, and the sails turn. The device is powered from its USB charger.
- **When:** The operator unplugs the USB charger for 10 s, plugs it back in, waits until HA shows the device connected, then turns on "Sails Turning" in HA and times three full turns.
- **Then:** HA shows Sail Speed 240, and three turns take between 24.3 s and 26.9 s.

**AT-08: Each light controls only its own pixel and shows the requested colour** (FR-10, FR-41)
- **Given:** Four pixels are wired on the bench and all four lights are off.
- **When:** The operator turns on each light in HA one at a time, sets it to red, then green, then blue, then sets its brightness to 20% and then to 100%, and then turns it off before moving to the next light.
- **Then:** Each light turns on, changes and turns off only the pixel at its stated position ("Mill Door Glow" 0, "Mill Stone Floor Window" 1, "Mill Bin Floor Window" 2, "Mill Door Lamp" 3); on each pixel red shows red, green shows green and blue shows blue; at 20% the pixel is dimmer than at 100%, and each brightness change changes only that light's own pixel; the other pixels do not change. Each time a light turns on or off, its pixel begins to change within 1 s of the command and finishes the change within 4 s.

**AT-09: Interior pixels flicker independently** (FR-11)
- **Given:** All four lights are off.
- **When:** The operator turns on the three interior lights in HA with the Lamplight effect and the same colour, and watches them for two minutes.
- **Then:** All three pixels flicker, and the flicker of each pixel does not rise and fall in step with the others.

**AT-10: Door lamp is steady** (FR-12)
- **Given:** "Mill Door Lamp" is off.
- **When:** The operator opens "Mill Door Lamp" in HA, looks for effects, then turns it on and watches it for two minutes.
- **Then:** HA offers no flicker effect for "Mill Door Lamp", and the pixel stays at a steady brightness.

**AT-11: Brightness cap by supply current** (FR-13, FR-16)
- **Given:** A meter is in series with the pixel string's 5 V feed. The operator has recorded from the pixel datasheet the full-white current I of one pixel, which is the sum of the currents of its colour channels at full output.
- **When:** The operator reads the meter with all four lights off, then sets all four lights in HA to 100% brightness, white, no effect, and reads it again, then short-presses the button twice (off, then mill on) and, 5 s after the second press, reads it a third time.
- **Then:** The first reading is information only and not a pass value. The second reading is at or below 0.6 × 4 × I, rounded up to the next 10 mA to allow for meter error and the pixels' idle current; with the `spec.md` figure of about 60 mA per pixel, this limit is 150 mA. The third reading is at or below the second.

**AT-12: Button turns the mill off** (FR-15, FR-41, FR-02, FR-18)
- **Given:** HA is connected, the sails turn and all four lights are on.
- **When:** The operator short-presses the button. After HA shows everything off, the operator turns on only "Mill Door Lamp" in HA, leaves the sails stopped, waits 5 s, and short-presses the button again.
- **Then:** After the first press, the sails stop within 1 s of release, all four pixels begin to dim within 1 s of release and are dark within 4 s of it, and within 5 s HA shows "Sails Turning" off and all four lights off. After the second press, the door lamp pixel begins to dim within 1 s of release and is dark within 4 s of it, and within 5 s HA shows "Mill Door Lamp" off.

**AT-13: Button turns the mill on** (FR-16, FR-41, FR-18)
- **Given:** HA is connected, the sails are stopped and all four lights are off.
- **When:** The operator short-presses the button and watches for one minute, then short-presses the button again.
- **Then:** After the first press, the sails start to turn at the current Sail Speed within 1 s of release; the pixels begin to brighten within 1 s of release, and within 4 s of it the three interior pixels show warm deep amber (not white) and flicker independently and the door lamp pixel shows a steady warm deep amber; within 5 s HA shows all four lights on with the interior lights on Lamplight and "Sails Turning" on. After the second press, the sails stop within 1 s of release, all four pixels are dark within 4 s of it, and within 5 s HA shows "Sails Turning" off and all four lights off.

**AT-14: Button and state without WiFi** (FR-17, FR-21, FR-22, FR-23)
- **Given:** The mill is on after a button press, and HA shows the device connected.
- **When:** The operator turns off the WiFi access point for 20 minutes and watches the mill without a break for the whole 20 minutes, short-presses the button once at about minute 17 and again at about minute 18, then turns the access point back on.
- **Then:** Until the first press the sails keep turning and the lights stay on; the first press turns everything off and the second turns the mill on again, judged for the mill only (the sails stop or start within 1 s of release, and the pixels finish their change within 4 s); at no point do the pixels go dark or the sails stop other than by the first press, so the device does not restart; within 5 minutes of the access point returning, HA shows the device connected again and its entities match what the mill is doing.

**AT-15: Button and state without HA** (FR-17, FR-21, FR-23)
- **Given:** The mill is on after a button press, HA shows the device connected, and the WiFi access point stays on for the whole test.
- **When:** The operator stops HA for 20 minutes and watches the mill without a break for the whole 20 minutes, short-presses the button once at about minute 17 and again at about minute 18, then starts HA again.
- **Then:** Until the first press the sails keep turning and the lights stay on; the first press turns everything off and the second turns the mill on again, judged for the mill only (the sails stop or start within 1 s of release, and the pixels finish their change within 4 s); at no point do the pixels go dark or the sails stop other than by the first press, so the device does not restart; within 5 minutes of HA starting, HA shows the device connected again and its entities match what the mill is doing.

**AT-16: Mill Button state in HA and long press** (FR-19, FR-40, FR-43, FR-44, FR-17)
- **Given:** The motor and the button are wired as in AT-01, with a mark on the coupler. HA is connected and shows "Mill Button" and "Reverse Rotation". The mill is off and "Reverse Rotation" is off.
- **When:** The operator holds the button for about 2 s, releases it, and watches the mill and HA for 10 s. Next the operator short-presses the button to turn the mill on, waits until the mark turns, holds the button for about 2 s, releases it, and watches for 10 s. Next the operator holds the button for about 7 s, releases it, and watches for 10 s. Last, the operator switches off the WiFi access point, holds the button for about 2 s, releases it, watches for 10 s, and switches the access point on again.
- **Then:** HA shows "Mill Button" as pressed while the button is held and as released after each release, while HA is connected. After the first hold, with the sails stopped, the mark does not move, all four lights stay off, and "Reverse Rotation" and "Sails Turning" stay off. After the second hold, the mark changes from anticlockwise to clockwise within 1 s of the release without stopping, HA shows "Reverse Rotation" on within 5 s, "Sails Turning" stays on, and the lights do not change. After the 7 s hold, the mark keeps turning clockwise, and "Reverse Rotation", "Sails Turning" and the lights do not change. With the access point off, the hold changes the mark back to anticlockwise within 1 s of the release without stopping; when HA reconnects, it shows "Reverse Rotation" off.

**AT-17: Dark and still after a power cut** (FR-20; Phase 1 "boot state")
- **Given:** The sails turn and all four lights are on. The device is powered from its USB charger.
- **When:** The operator unplugs the USB charger for 10 s, plugs it back in, and watches the mill for five minutes without sending any command.
- **Then:** From power-up onward all four pixels stay dark and the mark on the coupler does not move; once the device reconnects, HA shows "Sails Turning" off and all four lights off.

**AT-18: Fallback access point** (FR-24; Phase 1 "fallback AP")
- **Given:** The device runs connected to the configured WiFi network.
- **When:** The operator switches off the configured WiFi network, scans for WiFi networks on a phone 2 minutes later, joins "Windmill Fallback" with its password, and enters the details of a working WiFi network that HA can reach on the page that opens.
- **Then:** "Windmill Fallback" appears in the scan and asks for a password; after the operator enters the new details, the device joins that network and HA connects to it, with no USB cable used.

**AT-19: First flash and encrypted HA connection** (FR-25; Phase 1 "flash the C3, confirm WiFi and API")
- **Given:** The C3 is powered from the laptop's USB cable, with no charger connected. The operator has flashed our firmware over USB once, replacing the web-installer firmware, and has removed the old device from HA.
- **When:** HA discovers the device, and the operator first enters a wrong encryption key and then the correct key.
- **Then:** HA rejects the wrong key and does not connect; with the correct key HA connects and shows the four lights, "Sails Turning", "Reverse Rotation", "Sail Speed" and "Mill Button".

**AT-20: Network update with and without the password** (FR-26, FR-20)
- **Given:** The device runs our firmware on the network, the mill is on after a button press, and HA shows the firmware build date. The device is powered from its USB charger, with no laptop connected.
- **When:** The operator sends a newly compiled build over the network with a wrong update password, and then sends it again with the correct password.
- **Then:** The first attempt fails with an authentication error and HA still shows the old build date; the second attempt installs, the device restarts with all pixels dark and the sails still, and HA shows the new build date.

**AT-21: Logs over the network, none over USB serial** (FR-27, FR-28)
- **Given:** The C3 is powered from the laptop's USB cable, with no charger connected. The device runs on the network, and the laptop has a serial monitor open on that USB connection.
- **When:** The operator opens the device log over the network with the log command, keeps the serial monitor open, short-presses the Mill Button so that the sails turn, and then holds the Mill Button for about 2 s and releases it.
- **Then:** The log command connects to the device by name; within 2 minutes of the short press, the network log shows a message that the mill turned on or off; within 2 minutes of the long press, it shows a message that the sails reversed; and the serial monitor shows no firmware log lines. Output that the chip prints before the firmware starts (the boot ROM banner and the bootloader lines) is allowed.

**AT-24: Repository checks** (FR-34)
- **Given:** A fresh clone of the repository in which the example secrets file has been copied to the secrets file, and no real credential is present.
- **When:** The reviewer runs the YAML lint, the validation command and the compile command on the production configuration from the command line.
- **Then:** All three runs finish with no errors.

**AT-25: No real credentials committed** (FR-35, FR-36)
- **Given:** The repository with its full commit history.
- **When:** The reviewer asks git whether the secrets file is ignored, searches the history for the secrets file, and reads the example secrets file.
- **Then:** Git reports the secrets file as ignored, no commit contains it, and the example file holds only placeholder values.

**AT-26: Pinned tool versions** (FR-37)
- **Given:** The repository at a given commit.
- **When:** The reviewer creates a fresh Python environment from the pinned requirements file and asks ESPHome and yamllint for their versions.
- **Then:** Both report exactly the versions pinned in the repository.

**AT-28: HA light groups** (FR-39)
- **Given:** The four lights show in HA and the repository note on light groups exists.
- **When:** The operator follows the note to create "Mill Interior" and "Mill Lights" in HA, turns on "Mill Interior", then turns off "Mill Lights".
- **Then:** "Mill Interior" turns on pixels 1, 2 and 3 only; "Mill Lights" turns off all four pixels.

---

## Open Questions

- **Answered at the bench, 2026-10-03:** the pixels are SK6812 RGBW, in GRBW byte order. AT-11's limit must use the RGBW datasheet's full-white current, which includes the white LED. (Original question: Are the bullet pixels RGB or RGBW SK6812 parts? This affects whether colours show correctly. It also changes the full-white current. AT-11 takes its limit from the datasheet full-white current of the parts in use. The 150 mA figure in AT-11 holds only for RGB pixels: `spec.md` gives about 60 mA per pixel at full white, so 240 mA for four, and 60% of that is 144 mA. If the pixels prove to be RGBW, the limit must be recalculated from their datasheet. The bench answers this.)
- Will Phase 4 tuning by eye want a default Sail Speed other than 170 steps/s? This does not block this feature.
- Design must make sure the sails never stop by themselves after long cumulative running. The `spec.md` approach uses a fixed far target that the motor reaches after about 136 days at 170 steps/s.

---

## Appendix

### Glossary
- **Operator:** The maker who controls and tests the mill through HA, the button, a meter and by eye.
- **Reviewer:** The person who reads the repository and runs the validation commands before a commit.
- **Reverse Rotation:** The on/off switch in HA that sets the sail direction. While it is on, the sails turn in reverse whenever they turn. It is a permanent production control, and it is off after every restart.
- **Production configuration:** The one firmware configuration in the repository. There is no bench option (deferred by the user on 2026-10-03).
- **Forward:** Anticlockwise as seen from the sail side (the front) of the mill, as on traditional English windmills. Reverse is the opposite direction, clockwise as seen from the sail side.
- **Mill on:** The three interior lights in warm deep amber with Lamplight, "Mill Door Lamp" steady in warm deep amber, and the sails turning at the current Sail Speed.
- **Mill Button:** The physical push button on the mill, also called "the button". HA shows whether it is pressed or released as an entity of the same name. A short press turns the mill on or off. A long press reverses the sails while they turn. Any other press does nothing.
- **Short press:** A press of the button held for 50 ms to 500 ms, then released. It turns the mill on or off and never changes the direction.
- **Long press:** A press of the button held for 1 s to 5 s, then released. The system acts on the release. While the sails turn, it changes "Reverse Rotation" to its other state; while they are stopped, it does nothing. A hold of more than 500 ms but less than 1 s, or of more than 5 s, is neither a short nor a long press.
- **Fade:** A gradual change of a pixel's output between off and its set colour and brightness, of about 3 s, as in `spec.md`. The sails do not fade; they start and stop at once.
- **Lamplight:** A flicker effect that imitates a lamp flame.
- **Warm deep amber:** The colour in `spec.md` "Brightness", roughly 255/120/40 red/green/blue. It must read as amber, not white. Phase 4 sets the exact value by eye.
- **Full output:** The light a pixel gives with every channel at maximum. `spec.md` puts the current for full white at about 60 mA per pixel. With the cap, HA's 100% gives the same light as 60% brightness would without the cap. Physical LED output is lower than 60% because of the brightness curve.
- **Steps/s:** Motor steps per second. One output turn of the 28BYJ-48 is nominally 2048 steps, so at the exact setting one turn takes 2048 ÷ Sail Speed seconds. Most 28BYJ-48 gearboxes have a 63.684:1 ratio, so the real number of steps per turn is about 2038, about 0.5% below 2048, and a turn can take slightly less than the nominal time. The real speed can also fall slightly below the setting, so the firmware does not promise an exact RPM. The acceptance tests accept a three-turn time within 5% above or below the nominal time at 170 and 240 steps/s.
- **Pixel feed:** The 5 V supply to the pixel string. The 3.3 V data from the C3 must read as a valid high, so the data-level fix is either a diode drop on the feed or a level shifter, as `spec.md` allows.
- **Placeholder secrets:** The values in the committed example secrets file. They let the configuration validate and compile but connect to nothing.

### References
- `spec.md`: "Build and test order" Phase 1, "Speed", "Sleep behaviour", "Brightness", "Pin allocation", "ESPHome configuration".
- `.sdd/handbook.md`: "Safety invariants", "Error handling and fail-safe behaviour", "Testing", "Pre-commit validation".
- `.sdd/windmill-bench-firmware/research.md`: findings F1–F6, sections 6 (decisions) and 7 (ruled out).

### Change History
| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0 | 2026-10-02 | Pete Turner (with Claude) | Initial specification |
| 1.1 | 2026-10-02 | Pete Turner (with Claude) | Applied specification review fixes |
| 1.2 | 2026-10-02 | Pete Turner (with Claude) | Applied second-round review fixes |
| 1.3 | 2026-10-02 | Pete Turner (with Claude) | Applied third-round review fixes (AT-20, AT-03, AT-23, FR-41, FR-27) |
| 1.4 | 2026-10-02 | Pete Turner (with Claude) | Power changed to USB-C into the C3 (spec.md updated); AT-07, AT-17, AT-19, AT-20, AT-21 and the out-of-scope voltage check reworded |
| 1.5 | 2026-10-02 | Pete Turner (with Claude) | FR-41 excludes HA turn-ons that request an effect (the platform starts an effect without a fade); accepted by the user during design |
| 1.6 | 2026-10-03 | Pete Turner (with Claude) | Pins changed so GPIO2 (boot-strapping) is unconnected: stepper on GPIO0, 1, 3, 4, pixel data on GPIO6 (NFR-02); AT-29 no longer requires the pixel-entry capacitor unless a run shows flicker |
| 1.7 | 2026-10-03 | Pete Turner (with Claude) | Pixel order changed for wiring (0 door lamp, 1 door glow, 2 stone floor window, 3 bin floor window); pixels found to be RGBW at the bench |
| 1.8 | 2026-10-03 | Pete Turner (with Claude) | Scope reduced by the user: no bench option. Deferred and removed: FR-29, FR-33, FR-38, NFR-03, AT-22, AT-23, AT-27, AT-29 (listed in Explicitly Out of Scope). "Reverse Rotation" is now a permanent production switch: FR-30, FR-31, FR-32 and AT-02 rewritten, FR-42 added, FR-20 and AT-19 include it. FR-14 keeps its firmware half. FR-34 and AT-24 cover one configuration (three runs) |
| 1.9 | 2026-10-03 | Pete Turner (with Claude) | User request 2026-10-03: a long press (1–5 s, on release) reverses the sails while they turn, through "Reverse Rotation". FR-40 rewritten; FR-43 (long press while stopped does nothing) and FR-44 (other presses do nothing) added; FR-17, FR-27 and FR-32 updated; AT-16 and AT-21 test the long press; "Mill Button", "Short press" and "Long press" glossary entries updated; the out-of-scope line about the long press reworded |
| 1.10 | 2026-10-03 | Pete Turner (with Claude) | Status set to Implemented; feature closed by the operator. Unreported bench checks are recorded as accepted in bench-log.md |
