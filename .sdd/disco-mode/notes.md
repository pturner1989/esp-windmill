# Disco mode: feature request notes

**Date:** 2026-10-03
**Status:** Implemented (spec v1.6, design v1.2, six tasks). Closed by the user 2026-10-04. Taps were removed from scope.

## Request

The user wants a disco mode for the real mill: when music is playing, switch the mill to disco mode and the four pixels flash different colours in time with a BPM the operator sets.

## Decided with the user

- Goes into the real firmware **and** the dusk visualisation. The visualisation already has it (MM02 Mill at Dusk artifact, version 2).
- It is a **separate SDD feature** after windmill-bench-firmware. It does not reopen the approved bench-firmware spec.
- **Tap tempo: yes** (2026-10-03; this reverses the earlier "BPM only" choice). A set BPM drifts against the song, 0.5 BPM out is half a beat wrong after a minute, and the mill has no microphone. Venue lighting desks use tap tempo or a clock sync for the same reason.
- **Disco Rate: yes.** The chase runs at ½×, 1× or 2× the tempo, as venue desks do with speed multipliers. ½× suits fast tracks.
- **Colours change per phrase: yes.** The colour scheme holds for a 16-beat phrase, then changes. The chase moves on every step, but the look changes only on the phrase.

## Proposed by Claude, to confirm in research

- **BPM range 60–180.** 180 BPM is 3 flashes a second, the common safe limit for flashing light (photosensitive epilepsy guidance).
- **HA entities:** a "Disco Mode" switch, a "Disco BPM" number (60–180), a "Disco Rate" select (½×, 1×, 2×) and a "Disco Tap" button.
- **Rate guard:** 2× is only allowed while BPM × 2 ÷ 60 ≤ 3, so each light stays at 3 flashes a second or fewer. Raising the BPM past 90 while on 2× drops the rate to 1×.
- **Tap behaviour:** every tap snaps the phase so that moment is a beat; 4 to 8 taps within 2.5 s also set the BPM from their average spacing. A gap of over 1.2 s starts a new tap run. A tap while disco is off turns it on, starting on that beat. Taps align the beat, not the bar or phrase.
- **Look (user feedback 2026-10-03: the lights must not all flash at the same time):** a staggered chase. Each pixel flashes once per step (a beat × the rate), a quarter step after the one before, in an order that reshuffles every bar. Each pixel holds its colour for the phrase (four colours a quarter of the colour wheel apart), and each flash decays towards dim. This is how the visualisation does it (version 4).
- **Flash rate:** each single light flashes at most 3 times a second at 180 BPM. The mill as a whole changes 4 times per beat, up to 12 times a second; research should confirm this is acceptable for small, separate lights.

## Constraints from the handbook and the bench-firmware spec

- **Brightness cap:** the 60% cap still applies. Every light that writes pixels carries `color_correct` (handbook invariant 2).
- **Boot state:** the mill boots dark and stopped, so disco mode must not survive a restart (`ALWAYS_OFF`, handbook invariant 1).
- **Button and light state:** decide how the Mill Button toggle and the four light entities behave while disco mode is on. HA must still show the real state (FR-18).
- **Loop timing:** the stepper takes at most one step per loop pass. A per-beat effect must be rate-limited, like Lamplight's `addressable_flicker` (design v1.1, Quality Attributes).
- **Sails:** decide whether the sails keep turning, stop, or follow the beat during disco mode.
- **Tap latency:** a tap on an HA dashboard reaches the device through HA and the network, with a delay that varies by tens to a few hundred milliseconds. That limits how well a tap lines up the phase. Research should measure it and consider timestamping taps in HA, or using the physical doorbell for taps while disco is on (which conflicts with its toggle role).
