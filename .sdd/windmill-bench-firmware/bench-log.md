# Bench log: windmill-bench-firmware

Operator results for the acceptance criteria that need the hardware. "Pass" means the operator reported the result. "Accepted" means the operator closed the feature on 2026-10-03 and accepted the check without reporting an individual result.

Hardware: ESP32-C3 SuperMini on laptop USB; ULN2003 and 28BYJ-48 on the breadboard (IN1–IN4 on GPIO0, 1, 3, 4). Device at 192.168.1.7.

## Task 1: minimal node

| AC | Check | Result | Date | Notes |
|---|---|---|---|---|
| AC-8 | First USB flash; HA rejects a wrong key, accepts the right one | Accepted | 2026-10-03 | USB flash done. The encrypted API handshake succeeded with the right key from the laptop. HA adoption with a wrong key first is not yet reported. |
| AC-9 | Network update: wrong password fails, right one installs | Accepted | 2026-10-03 | Right password: Tasks 2 and 3 installed over the network. Wrong-password attempt not yet done. |
| AC-10 | Logs over the network; no firmware log lines on USB serial after a reset | Accepted | 2026-10-03 | Network log connected and showed the boot lines. Serial check after a reset not yet done. |
| AC-11 | Fallback access point | Accepted | 2026-10-03 | |

## Task 2: sails turn and stop

| AC | Check | Result | Date | Notes |
|---|---|---|---|---|
| AC-3 | Anticlockwise from the sail side, three turns 34.3–38.0 s, stop within 1 s, HA follows | Accepted | 2026-10-03 | Turns anticlockwise, so `sails_forward_direction` stays "1". The operator reported the speed correct. Stop timing not yet reported. |
| AC-4 | Holds when stopped (coil LED lit, 50–300 mA) | Accepted | 2026-10-03 | |
| AC-5 | Still after a 10 s power cut, HA shows off | Accepted | 2026-10-03 | |

## Task 3: Sail Speed

| AC | Check | Result | Date | Notes |
|---|---|---|---|---|
| AC-3 | First value 170; 400 and 50 rejected; 245 → 250 | Accepted | 2026-10-03 | |
| AC-4 | Live change 170 → 240 with no pause, correct times | Pass | 2026-10-03 | Operator: "the new speed was correct, speed change works". |
| AC-5 | Speed sweep 60–320 | Accepted | 2026-10-03 | |
| AC-6 | 240 survives a power cut | Accepted | 2026-10-03 | |
| AC-7 | Change while stopped does not move the sails | Accepted | 2026-10-03 | |

## Task 5: four capped lights

| AC | Check | Result | Date | Notes |
|---|---|---|---|---|
| AC-3 | Each light lights only its own pixel, correct colours, fades | Fail, fixed | 2026-10-03 | Lights hit the wrong pixels with wrong colours (Stone Floor Window → pixel 0 white + pixel 1 green; Bin Floor Window → pixel 1 white + pixel 2 green; Door Lamp → pixel 2 pink; pixel 3 never lit). Diagnosis: the pixels are SK6812 RGBW (4 bytes each, GRBW order) and were driven as RGB. Fix: GRBW order and a four-channel 60% cap. |
| AC-3 (re-test) | Same check after the RGBW fix and the new pixel order (0 lamp, 1 glow, 2 stone, 3 bin) | Pass | 2026-10-03 | Operator: "All works perfectly". Each light lights its own pixel with correct red, green and blue; brightness and fades correct. |

## Task 6: Lamplight

| AC | Check | Result | Date | Notes |
|---|---|---|---|---|
| AC-2 | Three interior pixels flicker independently | Pass | 2026-10-03 | Reported with the re-test above. |
| AC-3 | Door lamp offers no effect and stays steady | Pass | 2026-10-03 | Reported with the re-test above. |

## Observations

- 2026-10-03: the pixels are SK6812 **RGBW**, which settles the spec's open question. The brightness-cap current limit in Task 5 AC-4 must be recomputed from the RGBW datasheet (the white LED adds current).
- 2026-10-03: this laptop sees no mDNS or zeroconf services on the LAN, so the device is reached by IP (`--device 192.168.1.7`). HA at 192.168.1.41 failed handshakes until the old web-installer device is removed and Windmill is re-added with the new key.
