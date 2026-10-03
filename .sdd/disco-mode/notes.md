# Disco mode: feature request notes

**Date:** 2026-10-03
**Status:** Not started. Start with `/sddv2:research` after windmill-bench-firmware is implemented.

## Request

The user wants a disco mode for the real mill: when music is playing, switch the mill to disco mode and the four pixels flash different colours in time with a BPM the operator sets.

## Decided with the user

- Goes into the real firmware **and** the dusk visualisation. The visualisation already has it (MM02 Mill at Dusk artifact, version 2).
- It is a **separate SDD feature** after windmill-bench-firmware. It does not reopen the approved bench-firmware spec.
- **BPM only.** No tap-tempo button. The mill has no microphone, so it follows a set tempo and the phase can drift from the song.

## Proposed by Claude, to confirm in research

- **BPM range 60–180.** 180 BPM is 3 flashes a second, the common safe limit for flashing light (photosensitive epilepsy guidance).
- **HA entities:** a "Disco Mode" switch and a "Disco BPM" number.
- **Look (user feedback 2026-10-03: the lights must not all flash at the same time):** a staggered chase. Each pixel flashes once per beat, a quarter beat after the one before, in an order that reshuffles every bar of four beats. Each flash takes a new saturated colour and decays towards dim. This is how the visualisation does it (version 3).
- **Flash rate:** each single light flashes at most 3 times a second at 180 BPM. The mill as a whole changes 4 times per beat, up to 12 times a second; research should confirm this is acceptable for small, separate lights.

## Constraints from the handbook and the bench-firmware spec

- **Brightness cap:** the 60% cap still applies. Every light that writes pixels carries `color_correct` (handbook invariant 2).
- **Boot state:** the mill boots dark and stopped, so disco mode must not survive a restart (`ALWAYS_OFF`, handbook invariant 1).
- **Button and light state:** decide how the Mill Button toggle and the four light entities behave while disco mode is on. HA must still show the real state (FR-18).
- **Loop timing:** the stepper takes at most one step per loop pass. A per-beat effect must be rate-limited, like Lamplight's `addressable_flicker` (design v1.1, Quality Attributes).
- **Sails:** decide whether the sails keep turning, stop, or follow the beat during disco mode.
