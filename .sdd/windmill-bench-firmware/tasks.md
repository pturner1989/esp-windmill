# Tasks: Windmill Bench Firmware

**Linked Design:** `.sdd/windmill-bench-firmware/design.md`
**Linked Specification:** `.sdd/windmill-bench-firmware/specification.md`

### Task 1: Minimal node that checks, compiles and joins HA

- **Status:** Backlog
- **Blocked by:** None

**What to build:**

The reviewer can set up the pinned tools, copy the placeholder secrets and run one check script that lints, validates and compiles the firmware. The firmware at this point is the node alone: the village-windmill identity on the ESP32-C3 with the ESP-IDF framework, the encrypted HA API, password-protected network updates, logs over the network only at INFO level with ESP-IDF log output turned off, and the "Windmill Fallback" access point with its setup page. The check script refuses to run when the tools are missing or when the secrets file is absent or staged. A first README gives the setup, check, first USB flash and network update commands. After the operator's first USB flash, HA finds the device as "Windmill", accepts only the right encryption key, and the operator can update it over the network from then on.

**Acceptance criteria:**

**AC-1:**
- **Given:** A fresh Python venv created from the pinned requirements file
- **When:** The reviewer asks ESPHome and yamllint for their versions
- **Then:** ESPHome reports 2026.9.1 and yamllint reports 1.38.0

**AC-2:**
- **Given:** The example secrets file copied to the secrets file, with no real credential present
- **When:** The reviewer runs the check script
- **Then:** The YAML lint, the config validation and the compile all pass, the script notes that the bench option is not included, and it exits 0

**AC-3:**
- **Given:** The tools are installed and the secrets file is missing
- **When:** The reviewer runs the check script
- **Then:** The script exits non-zero with a message that says the secrets file is missing, and it runs no ESPHome command

**AC-4:**
- **Given:** The tools are installed, the secrets file exists, and the reviewer has force-added it to the git index
- **When:** The reviewer runs the check script
- **Then:** The script exits non-zero with a message that says the secrets file is staged, and it runs no ESPHome command

**AC-5:**
- **Given:** The secrets file is in place, but the venv is not active, so ESPHome and yamllint are not on the path
- **When:** The reviewer runs the check script
- **Then:** The script exits non-zero with a message that names the missing tool, and it runs no lint, validation or compile step

**AC-6:**
- **Given:** The repository with its full commit history
- **When:** The reviewer asks git whether the secrets file is ignored, searches the history for it, and reads the example secrets file
- **Then:** Git reports the file as ignored, no commit contains it, and the example file holds only placeholder values

**AC-7:**
- **Given:** The node config as validated by ESPHome
- **When:** The reviewer reads the logger, framework, API, update and WiFi settings
- **Then:** Serial logging is off (baud rate 0), the log level is INFO, the ESP-IDF log level is NONE, the API key, update password and access point password all come from secrets, and the access point is named "Windmill Fallback"

**AC-8:**
- **Given:** The C3 is powered from the laptop's USB cable with no charger connected, the operator has flashed this firmware over USB, and the old web-installer device is removed from HA
- **When:** HA discovers the device, and the operator first enters a wrong encryption key and then the correct key
- **Then:** HA rejects the wrong key and does not connect; with the correct key HA connects and shows the device "Windmill"

**AC-9:**
- **Given:** The device runs this firmware on the network and HA shows its firmware build date
- **When:** The operator sends a newly compiled build over the network with a wrong update password, and then with the correct password
- **Then:** The first attempt fails with an authentication error and HA still shows the old build date; the second attempt installs and HA shows the new build date

**AC-10:**
- **Given:** The C3 is powered from the laptop's USB cable with a serial monitor that reconnects on its own open on it, and the device runs on the network
- **When:** The operator opens the device log over the network with the log command, then presses the C3's reset button
- **Then:** The log command reports that it connected to village-windmill and shows the device log; the serial monitor shows it reconnected and shows no firmware log line for 60 s after the reset (the boot ROM banner and bootloader lines are allowed)

**AC-11:**
- **Given:** The device runs connected to the configured WiFi network
- **When:** The operator switches off that network, scans with a phone 2 minutes later, joins "Windmill Fallback" with its password, and enters the details of a working network that HA can reach
- **Then:** "Windmill Fallback" appears in the scan and asks for a password, and after the new details are entered the device joins that network and HA connects to it, with no USB cable used

**Notes:**

AC-1 to AC-7 are agent-checkable, and the reviewer unstages the secrets file straight after the staged-file test in AC-4, so it never reaches a commit; AC-8 to AC-11 need the operator at the bench with only the C3 wired. The first compile downloads the ESP-IDF toolchain (several GB, several minutes), so allow for that before judging the check script slow or broken; the repository is already a git repository on branch `feature/windmill-bench-firmware`, so the handbook's `git init` step is already done. A real API key for the operator's own secrets file is a fresh 32-byte base64 value, and the README says how to make one. After AC-11 the device keeps the network entered on the fallback page across network updates until a flash erase, so the operator re-enters the configured network on the fallback page, or notes that the full flash erase in Task 3 clears it. The WiFi and API reboot timeouts stay at their defaults here, because Task 8 changes them and proves the change with the mill running; this task contributes to AT-18, AT-19, AT-20, AT-21, AT-24 (bench-free runs), AT-25 and AT-26 (FR-24, FR-25, FR-26, FR-27, FR-28, FR-34, FR-35, FR-36, FR-37, NFR-02).

### Task 2: Sails turn and stop from HA, and boot stopped

- **Status:** Backlog
- **Blocked by:** Task 1

**What to build:**

The operator turns the sails on and off from HA with the "Sails Turning" switch. When the switch goes on, the sails turn forward at a fixed 170 steps/s by re-basing the stepper position and setting a far target in the forward direction. When it goes off, the sails stop at once with no coast-down, and the motor stays energised so the sails hold. After any reset or power cut the sails stay still and HA shows "Sails Turning" off. The forward direction is a substitution that the operator sets on the bench so forward is anticlockwise from the sail side, and the README explains how. The stepper pins come only from substitutions in the node file.

**Acceptance criteria:**

**AC-1:**
- **Given:** The example secrets in place
- **When:** The reviewer runs the check script
- **Then:** All runs pass and the config holds a ULN2003 stepper on the four sail pin substitutions (GPIO0–3) that does not sleep when done, and an optimistic "Sails Turning" switch with restore mode ALWAYS_OFF

**AC-2:**
- **Given:** The packages folder
- **When:** The reviewer searches every package for node-level keys (`esphome:`, `esp32:`, `wifi:`, `api:`, `ota:`, `logger:`) and for literal GPIO numbers outside comments
- **Then:** No package holds either

**AC-3:**
- **Given:** The ULN2003 and a bare 28BYJ-48 are wired on the breadboard with a mark on the coupler, and the sails are stopped
- **When:** The operator turns on "Sails Turning" in HA, watches the mark for one minute from the sail side, times three full turns, then turns the switch off
- **Then:** The mark turns steadily anticlockwise, three turns take 34.3 s to 38.0 s, the mark stops within 1 s of the switch going off, and HA shows "Sails Turning" on while it turns and off within 5 s of the stop

**AC-4:**
- **Given:** The sails have turned and then stopped, with a meter in series with the ULN2003 board's 5 V feed only
- **When:** The operator watches the coil LEDs, reads the meter and gently tries to turn the coupler by hand
- **Then:** At least one coil LED stays lit and the meter reads 50 mA to 300 mA; the operator notes whether the coupler resists, as information only

**AC-5:**
- **Given:** The sails turn and the device is powered from its USB charger
- **When:** The operator unplugs the charger for 10 s, plugs it back in and watches for five minutes without sending a command
- **Then:** The mark does not move from power-up onward, and once the device reconnects HA shows "Sails Turning" off

**Notes:**

AC-1 and AC-2 are agent-checkable; AC-3 to AC-5 need the operator, after the ULN2003 and motor are wired to the C3, and until Task 7 the only stop paths are HA or unplugging the supply, which is accepted for bench work only. If AC-3 shows clockwise turning, the operator flips the forward-direction substitution and updates over the network, as the README section "Forward direction" says. Turning the switch on while it is already on only re-arms, and the re-arm script stays a two-call action list (re-base, then set target) with no lambda over two lines. Speed is fixed at the stepper's 170 steps/s until Task 3 adds Sail Speed, and the reverse factor arrives in Task 9. This task contributes AT-01, AT-03 and the sails half of AT-17 (FR-01, FR-02, FR-03, FR-04, FR-18, FR-20, NFR-02); the sails package starts with a comment that lists the substitutions it needs and the ids it exposes.

### Task 3: Sail Speed set, rounded, rejected and restored

- **Status:** Backlog
- **Blocked by:** Task 2

**What to build:**

The operator sets "Sail Speed" in HA from 60 to 320 steps/s in steps of 10, with a default of 170. A value out of range is rejected and the speed stays as it was. A value in range that is not a multiple of 10 is rounded half up, the stepper only ever gets the rounded value, and HA shows the rounded value. A change takes effect at once, even while the sails turn, with no stop. When "Sails Turning" goes on, the sails start at the current Sail Speed, rounded and clamped to 60–320, with 170 used if the value is unknown. The value survives a restart once it has been saved.

**Acceptance criteria:**

**AC-1:**
- **Given:** The example secrets in place
- **When:** The reviewer runs the check script and reads the Sail Speed number
- **Then:** All runs pass, and the number has minimum 60, maximum 320, step 10, initial value 170, restores its value and is not optimistic

**AC-2:**
- **Given:** The sails package
- **When:** The reviewer reads the Sail Speed set action and the "Sails Turning" turn-on action
- **Then:** Both round half up to a multiple of 10, the turn-on action also clamps to 60–320 and uses 170 for an unknown value, and no lambda is longer than two lines

**AC-3:**
- **Given:** The operator has erased the whole flash over USB and flashed this firmware, so Sail Speed has never been set
- **When:** The operator opens "Sail Speed" in HA, then uses HA developer tools to set it to 400, then 50, then 245
- **Then:** HA shows 170 at first and offers only 60 to 320 in steps of 10; 400 and 50 are rejected and the value stays 170; 245 sets the speed to 250 and HA shows 250

**AC-4:**
- **Given:** The sails turn at 170 steps/s
- **When:** The operator times three full turns, sets Sail Speed to 240, and times three more
- **Then:** The mark never pauses; the first three turns take 34.3 s to 38.0 s and the three at 240 take 24.3 s to 26.9 s

**AC-5:**
- **Given:** The sails turn with a mark on the coupler and the operator has a stopwatch
- **When:** The operator sets Sail Speed to 60, 100, 140, 170, 200, 240, 280 and 320 in turn and times three full turns at each
- **Then:** The record holds a measured time for every setting beside the expected time (3 × 2048 ÷ setting), no measured time is below 95% of the expected time, and the operator records the highest setting that stays within 5% with no stutter, as information for Phase 4

**AC-6:**
- **Given:** Sail Speed was set to 240 at least 2 minutes earlier, the sails turn, and the device is powered from its USB charger
- **When:** The operator unplugs the charger for 10 s, plugs it back in, waits for HA to show the device connected, turns on "Sails Turning" and times three turns
- **Then:** HA shows Sail Speed 240 and three turns take 24.3 s to 26.9 s

**AC-7:**
- **Given:** The sails are stopped
- **When:** The operator changes Sail Speed in HA
- **Then:** The mark does not move and "Sails Turning" stays off

**Notes:**

AC-1 and AC-2 are agent-checkable; AC-3 to AC-7 need the operator with the motor wired. AC-3 needs a full flash erase over USB from the laptop with the charger unplugged, then a USB flash; HA reconnects with the same key because the key is in the firmware. The rounded value is set one loop pass later so the saved preference ends as the rounded value, and preferences flush every 60 s, which is why AC-6 waits 2 minutes. The real speed can sit slightly below the setting, so AC-5 only bounds the fast side as a pass condition. This task contributes AT-04, AT-05, AT-06 and AT-07 (FR-05, FR-06, FR-07, FR-08, FR-09, NFR-02).

### Task 4: Sails run with no time limit

- **Status:** Backlog
- **Blocked by:** Task 3

**What to build:**

While "Sails Turning" is on, the firmware re-runs the re-arm every 10 minutes: it re-bases the stepper position to 0 and sets the target one full span (10,000,000 steps) ahead in the current direction. So the sails never reach their target and never stop by themselves, however long they run, and positions stay far from integer overflow. The re-arm causes no pause or change of coil phase, and it does nothing while the sails are stopped.

**Acceptance criteria:**

**AC-1:**
- **Given:** The example secrets in place
- **When:** The reviewer runs the check script and reads the sails package
- **Then:** All runs pass, a 10-minute interval runs the re-arm script only while "Sails Turning" is on, and the re-arm script re-bases to 0 before it sets a target of 10,000,000 steps in the forward direction

**AC-2:**
- **Given:** Sail Speed is 170 and the operator has just turned on "Sails Turning"
- **When:** The operator watches the mark without a break for at least 11 minutes after turn-on and times three-turn blocks back to back (any 11-minute window contains at least one re-arm)
- **Then:** The mark never pauses or stutters, and every block takes 34.3 s to 38.0 s

**AC-3:**
- **Given:** The sails turn at 320 steps/s
- **When:** The operator leaves them for 35 minutes and then turns off "Sails Turning"
- **Then:** The sails turn for the whole 35 minutes, and the mark stops within 1 s of the switch going off

**Notes:**

AC-1 is agent-checkable; AC-2 and AC-3 need the operator with the motor wired. The agent-checkable AC-1 proves the interval exists; AC-2 and AC-3 show nothing got worse. The ULN2003 takes its coil phase from its own counter, so the re-base changes no coil output, and the script runs in one main-loop callback, so the stepper cannot step between the two calls. Without this task, the turn-on re-arm alone would stop the sails after about 8.7 h at 320 steps/s, which a bench session cannot show; AC-3 instead proves several re-arms in a row. This task contributes to FR-01 and to the sails half of AT-29, which runs in Task 12.

### Task 5: Four capped lights with fades, dark at boot

- **Status:** Backlog
- **Blocked by:** Task 2

**What to build:**

HA shows four lights, "Mill Door Glow" (pixel 0), "Mill Stone Floor Window" (1), "Mill Bin Floor Window" (2) and "Mill Door Lamp" (3). Each one controls only its own SK6812 pixel, with correct red, green and blue, and fades on and off over 3 s by default. Every pixel is capped at 60% of full output in firmware, on the internal strip and on each of the four lights, so no HA command can exceed it. The whole strip is internal and has no HA entity. After any reset or power cut all four lights are off and dark. The README gains a section on creating the HA light groups "Mill Interior" (pixels 0–2) and "Mill Lights" (all four).

**Acceptance criteria:**

**AC-1:**
- **Given:** The example secrets in place
- **When:** The reviewer runs the check script and reads the lights package
- **Then:** All runs pass; the strip is internal, uses the pixel pin substitution (GPIO4), four SK6812 pixels and GRB order; and the strip and all four partition lights each carry a 60% colour correction on every channel and restore mode ALWAYS_OFF

**AC-2:**
- **Given:** The node file and the packages
- **When:** The reviewer searches them for direct strip writes and for the old config items
- **Then:** No action targets the strip itself, no `addressable_set` exists, and no `rmt_channel` exists

**AC-3:**
- **Given:** Four pixels are wired on the bench and all four lights are off
- **When:** The operator turns on each light in HA one at a time, sets it to red, green, then blue, sets brightness to 20% then 100%, and turns it off before the next light
- **Then:** Each light changes only the pixel at its stated position, each colour shows as requested, 20% is dimmer than 100%, the other pixels do not change, and each turn-on and turn-off begins within 1 s of the command and finishes within 4 s

**AC-4:**
- **Given:** A meter is in series with the pixel string's 5 V feed, and the operator has the datasheet full-white current I of one pixel
- **When:** The operator reads the meter with all four lights off, then sets all four lights to 100% white with no effect and reads it again
- **Then:** The first reading is information only, and the second is at or below 0.6 × 4 × I rounded up to the next 10 mA (150 mA for RGB pixels at about 60 mA each)

**AC-5:**
- **Given:** The sails turn, all four lights are on, and the device is powered from its USB charger
- **When:** The operator unplugs the charger for 10 s, plugs it back in and watches for five minutes without a command
- **Then:** All four pixels stay dark and the mark does not move from power-up onward, and once the device reconnects HA shows "Sails Turning" and all four lights off

**AC-6:**
- **Given:** The four lights show in HA and the README section on light groups exists
- **When:** The operator follows it to create "Mill Interior" and "Mill Lights", turns on "Mill Interior", then turns off "Mill Lights"
- **Then:** "Mill Interior" turns on pixels 0, 1 and 2 only, and "Mill Lights" turns off all four

**Notes:**

AC-1 and AC-2 are agent-checkable; AC-3 to AC-6 need the operator after the four pixels are wired to GPIO4 through the 330R with the data-level fix, and AC-5 also needs the motor wired as in Task 2. AC-3 first answers whether the pixels are RGB or RGBW: if the colours are wrong because they are RGBW, only the lights package changes (RGBW flag, GRBW order, four correction values on the strip and every light), and the AT-11 limit is recomputed from the datasheet. Steady lights with no effect send no frames, which is the firmware half of FR-14. This task contributes AT-08, AT-17 (completed here), AT-28 and the first two readings of AT-11 (FR-10, FR-13, FR-14, FR-18, FR-20, FR-39, FR-41, NFR-01); the lights package starts with a comment that lists its substitutions and ids.

### Task 6: Lamplight on the interior lights, steady door lamp

- **Status:** Backlog
- **Blocked by:** Task 4, Task 5

**What to build:**

The three interior lights each offer a "Lamplight" flicker effect that imitates a lamp flame. Each light draws its own random values, so the three pixels flicker independently, and each writes at a fixed rate whatever the main-loop rate, through its own 60% cap. "Mill Door Lamp" offers no effect and stays steady.

**Acceptance criteria:**

**AC-1:**
- **Given:** The example secrets in place
- **When:** The reviewer runs the check script and reads the lights package
- **Then:** All runs pass; each of the three interior lights has exactly one effect, a flicker named "Lamplight" with a 50 ms update interval and an intensity above 0%; "Mill Door Lamp" has no effects

**AC-2:**
- **Given:** All four lights are off
- **When:** The operator turns on the three interior lights in HA with Lamplight and the same colour, and watches for two minutes
- **Then:** All three pixels flicker, and no pixel rises and falls in step with the others

**AC-3:**
- **Given:** "Mill Door Lamp" is off
- **When:** The operator opens it in HA, looks for effects, turns it on and watches for two minutes
- **Then:** HA offers no effect for it, and the pixel stays at a steady brightness

**Notes:**

AC-1 is agent-checkable; AC-2 and AC-3 need the operator with the pixels wired. An HA turn-on that requests Lamplight starts with no fade, which FR-41 (spec v1.5) allows, and the interval and intensity are Phase 4 starting values. The one-hour run with the capacitors fitted (AT-29) is Task 12, so this task and the tasks after it do not wait for those parts. This task contributes AT-09 and AT-10 (FR-11, FR-12, NFR-01).

### Task 7: Button turns the whole mill on and off

- **Status:** Backlog
- **Blocked by:** Task 4, Task 6

**What to build:**

A short press of the Mill Button (50–500 ms) toggles the whole mill. If the sails turn or any of the four lights is on, the press turns the mill off: the sails stop at once and all four lights fade off. Otherwise the press turns the mill on: the sails start at the current Sail Speed, the three interior lights fade on to warm deep amber at full brightness and then start Lamplight, and the door lamp fades on to a steady warm deep amber at 85%. A long press does nothing. HA shows "Mill Button" as pressed or released, and every change the button makes shows in HA at once. Each toggle writes one INFO log line, under the tag mill.controls, that says whether the mill went on or off. The scripts use only firmware entities, so they need no network.

**Acceptance criteria:**

**AC-1:**
- **Given:** The example secrets in place
- **When:** The reviewer runs the check script and reads the controls package
- **Then:** All runs pass; "Mill Button" reads the button pin substitution (GPIO5) with pull-up, inverted, with a 20 ms debounce; it has exactly one click handler, limited to 50–500 ms, and no long-press or multi-click handler; the toggle checks the sails and all four lights; both log calls use level INFO and tag `mill.controls`; the mill-off script stops the mill-on script

**AC-2:**
- **Given:** The controls package
- **When:** The reviewer searches it for the direction global, for calls to Home Assistant and for direct strip writes
- **Then:** It names none of them, and every light it turns on is one of the four capped partition lights

**AC-3:**
- **Given:** The button is wired, HA is connected, and the sails are stopped and all four lights off
- **When:** The operator short-presses the button, watches for one minute, then short-presses it again
- **Then:** After the first press the sails start at the current Sail Speed within 1 s of release; the pixels begin to brighten within 1 s and within 4 s the three interior pixels show warm deep amber (not white) flickering independently and the door lamp a steady warm deep amber; within 5 s HA shows all four lights on, the interior lights on Lamplight, and "Sails Turning" on. After the second press the sails stop within 1 s, all four pixels begin to dim within 1 s of release and are dark within 4 s, and within 5 s HA shows everything off

**AC-4:**
- **Given:** HA is connected and the mill is off
- **When:** The operator turns on only "Mill Door Lamp" in HA, leaves the sails stopped, waits 5 s and short-presses the button
- **Then:** The door lamp pixel begins to dim within 1 s of release and is dark within 4 s, and within 5 s HA shows "Mill Door Lamp" off

**AC-5:**
- **Given:** HA is connected and shows "Mill Button"
- **When:** The operator holds the button for about two seconds and releases it
- **Then:** HA shows "Mill Button" pressed while held and released after, and the sails and lights do not change

**AC-6:**
- **Given:** The C3 is powered from the laptop's USB cable with a serial monitor open on it, and the device runs on the network
- **When:** The operator opens the network log with the log command and short-presses the button
- **Then:** Within 2 minutes the network log shows a message that the mill turned on or off, and the serial monitor shows no firmware log lines

**AC-7:**
- **Given:** A meter is in series with the pixel 5 V feed, all four lights are on at 100% white with no effect, the sails are stopped, and the AT-11 second reading is recorded
- **When:** The operator short-presses the button twice (off, then mill on) and reads the meter 5 s after the second press
- **Then:** The reading is at or below the second reading

**AC-8:**
- **Given:** HA is connected to the device
- **When:** The operator opens the device page in HA
- **Then:** HA shows the four lights, "Sails Turning", "Sail Speed" and "Mill Button", and no entity for the whole pixel strip

**Notes:**

AC-1 and AC-2 are agent-checkable; AC-3 to AC-8 need the operator after the button is wired from GPIO5 to ground. For AC-6 on laptop power, the operator uses a USB port that supplies 900 mA or more, or a powered hub, so the C3 does not brown out when the mill turns on. The mill-on look is a Phase 4 starting value (RGB about 100/47/16 in percent); the lights fade without an effect first, because ESPHome skips the default transition when a call sets an effect, and Lamplight starts after 3 s only on interior lights that are still on. The scripts never touch the reverse control, which Task 9 checks from the operator side. This task contributes AT-12, AT-13, AT-16, AT-21 (completed here), the third reading of AT-11 and the entity list of AT-19 (FR-02, FR-13, FR-15, FR-16, FR-17, FR-18, FR-19, FR-27, FR-40, FR-41, NFR-01).

### Task 8: Mill keeps its state through network loss

- **Status:** Backlog
- **Blocked by:** Task 7

**What to build:**

The node no longer restarts itself when WiFi or HA is gone. The sails and lights keep their state for as long as the loss lasts, and the button keeps working. WiFi and the HA connection come back on their own when the network or HA returns, and HA then shows the real state of the mill. A network update still restarts the device, and that restart leaves the mill dark and still.

**Acceptance criteria:**

**AC-1:**
- **Given:** The example secrets in place
- **When:** The reviewer runs the check script and reads the WiFi and API settings
- **Then:** All runs pass, both the WiFi and the API reboot timeout are 0 s, and the fallback access point is still configured with the default access point timeout

**AC-2:**
- **Given:** The mill is on after a button press and HA shows the device connected
- **When:** The operator turns off the WiFi access point for 20 minutes, watches the mill the whole time, scans for WiFi with a phone during the outage, short-presses the button at about minute 17 and again at about minute 18, then turns the access point back on
- **Then:** A phone scan during the outage shows "Windmill Fallback"; until the first press the sails turn and the lights stay on; the first press turns the mill off and the second turns it on (sails within 1 s of release, pixels finished within 4 s); the pixels never go dark and the sails never stop otherwise; within 5 minutes of the access point returning HA shows the device connected and its entities match the mill

**AC-3:**
- **Given:** The mill is on after a button press, HA shows the device connected, and the WiFi access point stays on
- **When:** The operator stops HA for 20 minutes, watches the mill the whole time, short-presses the button at about minute 17 and again at about minute 18, then starts HA again
- **Then:** The results of AC-2 other than the phone scan hold, and within 5 minutes of HA starting HA shows the device connected and its entities match the mill

**AC-4:**
- **Given:** The mill is on after a button press, HA shows the firmware build date, and the device is powered from its USB charger with no laptop connected
- **When:** The operator sends a newly compiled build over the network with the correct update password
- **Then:** The update installs, the device restarts with all pixels dark and the sails still, and HA shows the new build date with "Sails Turning" and all four lights off

**Notes:**

AC-1 is agent-checkable; AC-2 to AC-4 need the operator, who must be able to switch off the WiFi access point and stop HA for 20 minutes. Network resilience is its own task, not part of Task 1, because its proof needs the button and a running mill: with the default 15-minute reboot timeout, AC-2 would fail at about minute 15 when the device restarts dark. ESPHome keeps retrying the configured network while the fallback access point is up, which AC-2 confirms. This task contributes AT-14, AT-15 and AT-20 (completed here) (FR-17, FR-20, FR-21, FR-22, FR-23).

### Task 9: Bench option with reverse rotation

- **Status:** Backlog
- **Blocked by:** Task 8

**What to build:**

The node file gains one line that includes the bench package, and the bench package gives HA a "Reverse Rotation" switch that boots off. While it is on, the sails turn in reverse whenever they turn. Turning it on or off while the sails turn changes direction at once through the re-arm, with no stop and no change to "Sails Turning". The button and the mill scripts never change the reverse control. The check script now checks both configurations: it writes a derived copy of the node file without the bench line, runs lint, validation and compile on that copy first and on the full config last, deletes the copy on exit, and fails if more than one line names the bench package. The derived copy is git-ignored, and the README warns that an upload sends the last build, so the operator compiles the wanted config before an upload.

**Acceptance criteria:**

**AC-1:**
- **Given:** A fresh clone at this task's commit, a venv built from the pinned requirements, and the example secrets copied to the secrets file, with the bench line in the node file
- **When:** The reviewer runs the check script and reads the bench and sails packages
- **Then:** Six runs pass (lint, validation and compile, without and then with the bench option), the script exits 0, and no derived copy remains afterwards; "Reverse Rotation" has restore mode ALWAYS_OFF, and the direction setting does not restore (NFR-02)

**AC-2:**
- **Given:** A second line in the node file names the bench package
- **When:** The reviewer runs the check script
- **Then:** The script exits non-zero with a message about the bench line and runs no ESPHome command

**AC-3:**
- **Given:** The repository
- **When:** The reviewer asks git whether the derived copy is ignored, and searches every package other than the bench package for the bench ids
- **Then:** Git reports the derived copy as ignored, and no other package names a bench id

**AC-4:**
- **Given:** The motor is wired, Sail Speed is 170, the sails are stopped and "Reverse Rotation" is off
- **When:** The operator turns on "Reverse Rotation", turns on "Sails Turning", watches for one minute and times three turns; then while turning turns "Reverse Rotation" off and watches 30 s, turns it on and watches 30 s, and turns off "Sails Turning"
- **Then:** With reverse on the mark turns steadily clockwise, three turns take 34.3 s to 38.0 s, and HA shows "Sails Turning" on; reverse off changes the mark to anticlockwise without a stop and "Sails Turning" stays on; reverse on changes it back to clockwise; the mark stops within 1 s of "Sails Turning" going off

**AC-5:**
- **Given:** The sails are stopped and "Reverse Rotation" is on
- **When:** The operator turns on "Sails Turning" and then short-presses the button
- **Then:** The mark stops within 1 s of release, HA shows "Sails Turning" off within 5 s, and "Reverse Rotation" stays on

**AC-6:**
- **Given:** "Reverse Rotation" is off and the sails are stopped
- **When:** The operator turns on "Sails Turning" and watches for one minute
- **Then:** The mark turns anticlockwise, as in Task 2

**Notes:**

AC-1 to AC-3 are agent-checkable; AC-4 to AC-6 need the operator with the motor and button wired. The direction global lives in the sails package with no restore, and the re-arm multiplies the forward-direction substitution by its sign, so a reverse is the same re-arm with the opposite sign. The derived copy sits at the repository root so relative includes still resolve, and it keeps the node name, so both builds share one build directory that ends with the bench build. Both configurations share one node block, so HA sees one device and updates work in both directions. This task contributes AT-02, AT-24 (completed here) and an AT-01 repeat (FR-03, FR-15, FR-30, FR-31, FR-32, FR-33, FR-34, NFR-02, NFR-03).

### Task 10: Pixel addressing test and the production build

- **Status:** Backlog
- **Blocked by:** Task 9

**What to build:**

The bench package gains a "Pixel Addressing Test" button in HA. A press turns off any of the four lights that are on, with no fade, then lights each pixel alone in the order 0, 1, 2, 3, showing red, green and blue for 500 ms each, and leaves all four dark after one pass. The test drives the four capped partition lights, never the strip, so the 60% cap holds and HA shows each light's real state. A press during a run does nothing. The README gains a section on removing the bench option, and with that one line deleted HA shows neither test control.

**Acceptance criteria:**

**AC-1:**
- **Given:** The example secrets in place
- **When:** The reviewer runs the check script and reads the bench package
- **Then:** All six runs pass; the test script runs in single mode and targets only the four partition lights; nothing in the bench package names the strip or uses `addressable_set`

**AC-2:**
- **Given:** Four pixels are wired, each labelled with its intended position, and at least one mill light is on
- **When:** The operator starts the pixel addressing test from HA and watches the pixels and HA
- **Then:** HA shows every light that was on as off; the pixels light one at a time in the order 0, 1, 2, 3, each alone for at least 1 s; the pixel that lights at each step is the one with that label; red, green and blue show as those colours; after one pass all four are dark

**AC-3:**
- **Given:** A test run is in progress
- **When:** The operator presses "Pixel Addressing Test" again
- **Then:** The run continues once through and no second pass starts

**AC-4:**
- **Given:** The bench option is included and its two controls show in HA
- **When:** The reviewer deletes the bench line as the README says, the operator updates the device over the network and opens the device page in HA
- **Then:** HA shows no pixel addressing test, no reverse control and no light for the whole strip; the four lights, "Sails Turning", "Sail Speed" and "Mill Button" are still there (entities that HA marks "no longer provided" do not count); after the check the reviewer restores the bench line, the operator updates the device again, and the commit keeps the bench line

**Notes:**

AC-1 is agent-checkable; AC-2 to AC-4 need the operator with the pixels wired, and AC-4 also needs the reviewer to make the one-line change. During the test, HA shows each light on for its 1.5 s step, which is its real state; the "shows as off" check applies to the first step. The bench line goes back after AC-4, so the device and the repository keep the bench build for Task 12 and later bench work. The colour check is the spec's nice-to-have confirmation of pixel colour order. This task contributes AT-22 and AT-23 (FR-29, FR-33, NFR-01, NFR-03).

### Task 11: spec.md and the handbook match the firmware

- **Status:** Backlog
- **Blocked by:** Task 8, Task 10

**What to build:**

The "ESPHome configuration" section of the build spec loses its configuration listing and points to the node file and the mill packages by path. Its notes describe the re-arm model, the four lights plus the HA groups, the cap on every light, and logs over the network. The Option B step says the hub includes the sails, lights and controls packages and changes only the substitutions. The handbook's repository layout lists the bench package, the check script, the lint config and the README; its pre-commit section runs the check script; and its assumptions record ESPHome 2026.9.1 and the existing git repository (the Commits line that says to run git init goes), and close the RMT channel and brightness cap items. The README is read end to end against the final firmware. Over the finished tree, the reviewer repeats the package boundary check on all four packages and the check that the secrets file was never committed.

**Acceptance criteria:**

**AC-1:**
- **Given:** The repository after this task
- **When:** The reviewer opens the "ESPHome configuration" section of `spec.md`, reads its notes and reads the Option B step about moving configuration blocks
- **Then:** The section holds no configuration listing and names the node file and the mill packages by path; the notes and the Option B step match the firmware files and refer to no part of the removed listing

**AC-2:**
- **Given:** `spec.md`
- **When:** The reviewer searches it for the old light ids `mill_interior` and `mill_lights` as light ids, the `sails_running` global, an `on_boot` block, `rmt_channel` and the 2,000,000,000 target
- **Then:** None of them appears

**AC-3:**
- **Given:** The handbook and the README
- **When:** The reviewer compares the handbook's layout and pre-commit sections, and the README's sections (Setup, Checks, First USB flash and OTA, Remove the bench option, Forward direction, HA light groups), with the repository
- **Then:** Every file they name exists, every command the agent can run (venv setup, the check script, the git checks) works, the pre-commit section runs the check script, and the assumptions list records ESPHome 2026.9.1

**AC-4:**
- **Given:** The example secrets in place
- **When:** The reviewer runs the check script and the YAML lint
- **Then:** All runs pass

**AC-5:**
- **Given:** The four packages (sails, lights, controls and bench)
- **When:** The reviewer searches every package for node-level keys (`esphome:`, `esp32:`, `wifi:`, `api:`, `ota:`, `logger:`) and for literal GPIO numbers outside comments
- **Then:** No package holds either

**AC-6:**
- **Given:** The repository with its full commit history after this task
- **When:** The reviewer asks git whether the secrets file is ignored, searches the history for it, and reads the example secrets file
- **Then:** Git reports the file as ignored, no commit contains it, and the example file holds only placeholder values

**Notes:**

All ACs are agent-checkable (the README's device commands were exercised in Task 1 AC-8 to AC-10, Task 7 AC-6 and Task 10 AC-4); no device behaviour changes. The handbook says to update `spec.md` in the same change when the user agrees a divergence, and the user agreed these divergences in the approved spec and design, so this task closes them. The `mill_lights` name stays valid as a package file name; only its use as a light id must go. Keep the `spec.md` heading so links to it still work. This task contributes AT-27 (FR-38), completes the README for FR-39, and repeats the package checks of Task 2 and AT-25 over the final tree (FR-35, FR-36, NFR-02).

### Task 12: One-hour run with the capacitors fitted

- **Status:** Backlog
- **Blocked by:** Task 6, Task 11

**What to build:**

The operator fits the 1000 µF capacitor at the pixel string entry and the 470 µF capacitor at the stepper driver, and then runs the finished mill for one hour. During the hour the sails turn at 170 steps/s and all four lights show steady colours with no effect. No pixel flickers or changes colour at any point, and the sails turn for the whole hour. This task adds no firmware unless the run shows flicker after the hardware fixes.

**Acceptance criteria:**

**AC-1:**
- **Given:** The 1000 µF capacitor at the pixel string entry and the 470 µF capacitor at the stepper driver are fitted, the sails turn at 170 steps/s, and all four lights show steady colours with no effect
- **When:** The operator runs the mill for 60 minutes and watches the pixels, or reviews one continuous recording of them
- **Then:** No pixel flickers or changes colour at any point, and the sails turn for the whole hour

**Notes:**

AC-1 needs the operator and the two capacitors, which the operator does not own yet, so this is the only operator-gated task left open until the parts arrive; no other task depends on it. Steady lights with no effect send no frames, so the design expects any flicker in this run to come from the hardware: the capacitors, the 330R resistor and the routing. If AC-1 shows flicker after the hardware fixes (separation, 330R, capacitors), the first firmware fix is to raise the RMT symbol count on the strip, which on the C3 defaults to exactly one 4-pixel frame; the check script must pass again, and the build spec notes and handbook must be re-checked, before the operator repeats the run. The hour also covers at least five re-arms in a row at 170 steps/s, which completes the sails half of AT-29 from Task 4. This task contributes AT-29 (FR-01, FR-14).
