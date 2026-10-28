// WLED glue: holds the one shared WLED state and connects mill_wled.h to the
// clock, the UDP listeners and the pixels. The two listeners in
// packages/mill_wled.yaml pass each packet to receive_sync_now() (port 21324)
// or receive_ddp_now() (port 4048). The "WLED" effect of each partition light
// in packages/mill_lights.yaml calls render() with its pixel number as the
// slot. It writes only it[0], the light's own pixel, so the light's 60% cap,
// gamma and brightness apply, and it publishes no state.
#pragma once

#include <cstdint>
#include <vector>

#include "esphome/components/light/addressable_light.h"
#include "esphome/core/hal.h"
#include "mill_wled.h"

namespace mill_wled {

// Every reader and writer runs in the main loop, so the state needs no lock.
inline Wled &shared() {
  static Wled wled;
  return wled;
}

inline void receive_sync_now(const std::vector<uint8_t> &data) {
  shared() = receive_sync(shared(), data.data(), data.size(), esphome::millis());
}

inline void receive_ddp_now(const std::vector<uint8_t> &data) {
  shared() = receive_ddp(shared(), data.data(), data.size(), esphome::millis());
}

inline void render(esphome::light::AddressableLight &it, int slot) {
  Rgbw c = pixel(shared(), slot, esphome::millis());
  it[0] = esphome::Color(c.r, c.g, c.b, c.w);
}

}  // namespace mill_wled
