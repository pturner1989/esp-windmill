// Sails ramp glue: holds the one shared ramp and connects mill_ramp.h to the
// clock. The sails tick in packages/mill_sails.yaml calls tick_now() every
// 20 ms, then drives the stepper from shared(). The ramp starts stopped at
// every boot and is never saved.
#pragma once

#include "esphome/core/hal.h"
#include "mill_ramp.h"

namespace mill_ramp {

// Every reader and writer runs in the main loop, so the state needs no lock.
inline Ramp &shared() {
  static Ramp ramp;
  return ramp;
}

// One ramp tick now toward `goal`.
inline void tick_now(float goal) { shared() = step(shared(), goal, esphome::millis()); }

}  // namespace mill_ramp
