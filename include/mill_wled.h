// WLED maths: decodes the packets a WLED device sends and picks each pixel's
// colour. No ESPHome types, no clock reads and no static state, so
// tests/mill_wled_test.cpp runs it on the laptop.
// Used by: mill_wled_esphome.h (the glue that the UDP listeners and the light
// effects call).
//
// Two kinds of packet set the colours:
// - Sync (UDP port 21324, protocol byte 0): the WLED notifier, sent when the
//   WLED's colour or brightness changes. All four pixels take its primary
//   colour times its brightness, and keep it until the next sync.
// - Live: the WLED realtime protocols on port 21324 (WARLS, DRGB, DRGBW and
//   DNRGB) and DDP on port 4048 (a WLED "DDP" network output). Pixel n takes
//   LED n of the frame. A live frame wins over the sync colour until it times
//   out, then the pixels go back to the sync colour.
// Before any packet, every pixel is dark.
//
// Times are uint32_t milliseconds from millis(). Each difference is taken as
// int32_t(a - b), so it stays correct when the counter wraps.
#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace mill_wled {

// A realtime frame with timeout byte 255 never times out.
constexpr uint8_t kForever = 255;
// The gamma a WLED device puts on the colours of its DDP output (WLED's
// "Color gamma", 2.8 by default). The partition lights apply their own gamma
// 2.8 on top, so DDP values are taken back to linear first.
constexpr float kWledGamma = 2.8f;
// DDP carries no timeout. WLED uses 2.5 s for its own network inputs.
constexpr uint32_t kDdpTimeoutMs = 2500;
// The shortest notifier packet that holds the white byte (byte 10).
constexpr size_t kSyncMinSize = 11;
// The DDP header, and the header with a timecode.
constexpr size_t kDdpHeader = 10;
constexpr size_t kDdpTimecodeHeader = 14;

struct Rgbw {
  uint8_t r, g, b, w;
};

struct Wled {
  std::array<Rgbw, 4> sync{};  // the latest sync colour, the same on every pixel
  std::array<Rgbw, 4> live{};  // the latest live frame
  bool live_on = false;        // a live frame came and has not ended
  bool live_forever = false;   // the live frame never times out
  uint32_t live_until_ms = 0;  // the live frame ends at this time
};

inline int32_t since(uint32_t a, uint32_t b) { return int32_t(a - b); }

inline uint8_t scale(uint8_t channel, uint8_t by) { return uint8_t((channel * by + 127) / 255); }

// The linear value of a channel that WLED gamma-corrected with kWledGamma, so
// the light's own gamma gives the LED the level WLED meant.
inline uint8_t linear(uint8_t channel) {
  return uint8_t(std::lround(255.0f * std::pow(channel / 255.0f, 1.0f / kWledGamma)));
}

inline Rgbw linear(Rgbw c) { return {linear(c.r), linear(c.g), linear(c.b), linear(c.w)}; }

inline bool live_at(const Wled &w, uint32_t now) {
  return w.live_on && (w.live_forever || since(now, w.live_until_ms) < 0);
}

// The colour of pixel `slot` (0-3) at `now`.
inline Rgbw pixel(const Wled &w, int slot, uint32_t now) {
  return live_at(w, now) ? w.live[slot] : w.sync[slot];
}

// A live frame starts or goes on at `now`, and ends `timeout_ms` later. A
// frame after the last one ended starts from dark, so LEDs it leaves out stay
// dark.
inline Wled go_live(Wled w, uint32_t now, uint32_t timeout_ms, bool forever) {
  if (!live_at(w, now)) w.live = {};
  w.live_on = true;
  w.live_forever = forever;
  w.live_until_ms = now + timeout_ms;
  return w;
}

// Sets LED `led` of the live frame, if it is one of the four pixels.
inline void set_live(Wled &w, uint32_t led, Rgbw c) {
  if (led < w.live.size()) w.live[led] = c;
}

// One packet from UDP port 21324, received at `now`. A packet that is not a
// valid sync or realtime packet leaves the state as it was.
inline Wled receive_sync(Wled w, const uint8_t *data, size_t size, uint32_t now) {
  if (size < 2) return w;
  uint8_t protocol = data[0];
  if (protocol == 0) {
    if (size < kSyncMinSize) return w;
    uint8_t bri = data[2];
    Rgbw c{scale(data[3], bri), scale(data[4], bri), scale(data[5], bri), scale(data[10], bri)};
    w.sync.fill(c);
    return w;
  }
  // Realtime: byte 1 is the timeout in seconds. 0 ends the live frame now.
  uint8_t timeout = data[1];
  const uint8_t *p = data + 2;
  size_t n = size - 2;
  size_t step = protocol == 1 || protocol == 3 ? 4 : 3;
  size_t skip = protocol == 4 ? 2 : 0;
  if (protocol > 4 || n < skip || (n - skip) % step != 0) return w;
  if (timeout == 0) {
    w.live_on = false;
    return w;
  }
  w = go_live(w, now, timeout * 1000u, timeout == kForever);
  uint32_t first = protocol == 4 ? uint32_t(p[0]) << 8 | p[1] : 0;
  for (size_t i = skip, k = 0; i < n; i += step, k++) {
    const uint8_t *v = p + i;
    if (protocol == 1) set_live(w, v[0], {v[1], v[2], v[3], 0});         // WARLS: index, r, g, b
    else if (protocol == 3) set_live(w, uint32_t(k), {v[0], v[1], v[2], v[3]});  // DRGBW
    else set_live(w, first + uint32_t(k), {v[0], v[1], v[2], 0});        // DRGB, DNRGB
  }
  return w;
}

// One DDP packet from UDP port 4048, received at `now`. Only version 1 data
// packets count. Data type 0x1B is RGBW, any other type is RGB. The offset is
// in bytes from the first LED. WLED's DDP output carries its gamma-corrected
// colours, so each channel is taken back to linear with linear().
inline Wled receive_ddp(Wled w, const uint8_t *data, size_t size, uint32_t now) {
  if (size < kDdpHeader) return w;
  uint8_t flags = data[0];
  if ((flags & 0xC0) != 0x40 || (flags & 0x0C) != 0) return w;  // version 1, no query or reply
  size_t header = flags & 0x10 ? kDdpTimecodeHeader : kDdpHeader;
  size_t channels = data[2] == 0x1B ? 4 : 3;
  uint32_t offset = uint32_t(data[4]) << 24 | uint32_t(data[5]) << 16 | uint32_t(data[6]) << 8 | data[7];
  size_t length = size_t(data[8]) << 8 | data[9];
  if (size < header + length || offset % channels != 0 || length % channels != 0) return w;
  w = go_live(w, now, kDdpTimeoutMs, false);
  const uint8_t *p = data + header;
  for (size_t i = 0; i < length; i += channels) {
    uint32_t led = (offset + uint32_t(i)) / channels;
    set_live(w, led, linear({p[i], p[i + 1], p[i + 2], uint8_t(channels == 4 ? p[i + 3] : 0)}));
  }
  return w;
}

}  // namespace mill_wled
