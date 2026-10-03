// Disco glue: holds the one shared disco state and connects mill_disco.h to
// the clock and the pixels. The "Disco" effect of each partition light in
// packages/mill_lights.yaml calls render() with its pixel number as the slot.
// It writes only it[0], the light's own pixel, so the light's 60% cap and
// brightness apply, and it publishes no state.
#pragma once

#include "esphome/components/light/addressable_light.h"
#include "esphome/core/hal.h"
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

}  // namespace mill_disco
