// Disco glue: holds the one shared disco state and connects mill_disco.h to
// the clock and the pixels. The "Disco" effect of each partition light in
// packages/mill_lights.yaml calls render() with its pixel number as the slot.
// It writes only it[0], the light's own pixel, so the light's 60% cap and
// brightness apply, and it publishes no state. The disco package's check
// every 250 ms reads the four lights through disco_mask(), any_disco() and
// any_left(). "Disco BPM" and "Disco Rate" set the clock through
// set_bpm_now() and set_rate_now().
#pragma once

#include <array>

#include "esphome/components/light/addressable_light.h"
#include "esphome/components/light/light_state.h"
#include "esphome/core/hal.h"
#include "esphome/core/string_ref.h"
#include "mill_disco.h"

namespace mill_disco {

// Every reader and writer runs in the main loop, so the state needs no lock.
inline Disco &shared() {
  static Disco disco;
  return disco;
}

inline void render(esphome::light::AddressableLight &it, int slot) {
  Frame f = frame(shared(), slot, esphome::millis());
  shared().slots[slot] = f.slot;
  it[0] = esphome::Color(f.out.r, f.out.g, f.out.b, f.out.w);
}

// Disco starts now: beat 0 at this moment. Called by the disco start script.
inline void start_now() { shared() = start(shared(), esphome::millis()); }

// A tempo change now. Returns true if the rate guard dropped 2x to 1x.
inline bool set_bpm_now(float bpm) {
  BpmSet set = set_bpm(shared(), bpm, esphome::millis());
  shared() = set.disco;
  return set.rate_dropped;
}

// A rate change to a "Disco Rate" option (½×, 1× or 2×). Returns the option of
// the accepted rate.
inline const char *set_rate_now(esphome::StringRef option) {
  shared() = set_rate(shared(), option == "½×" ? 0.5f : option == "2×" ? 2.0f : 1.0f);
  return shared().clock.rate == 0.5f ? "½×" : shared().clock.rate == 2.0f ? "2×" : "1×";
}

// The four lights in pixel order, so bit n of a mask is pixel n.
using Lights = std::array<esphome::light::LightState *, 4>;

// The lights that are on with "Disco". It reads the target state that HA
// shows, not the fading values, so a light turned off during its fade counts
// as off.
inline int disco_mask(const Lights &lights) {
  int mask = 0;
  for (int n = 0; n < 4; n++)
    if (lights[n]->remote_values.is_on() && lights[n]->get_effect_name() == "Disco") mask |= 1 << n;
  return mask;
}
inline bool any_disco(const Lights &lights) { return disco_mask(lights) != 0; }
inline bool any_left(const Lights &lights) { return disco_mask(lights) != 15; }

}  // namespace mill_disco
