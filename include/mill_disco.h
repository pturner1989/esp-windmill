// Disco maths: pure functions of time and state. No ESPHome types, no clock
// reads and no static state, so tests/mill_disco_test.cpp runs it on the laptop.
// Used by: mill_disco_esphome.h (the glue that the light effects call).
//
// Times are uint32_t milliseconds from millis(). Each difference is taken as
// int32_t(a - b), so it stays correct when the counter wraps.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace mill_disco {

// Shortest time between two chase flashes on one pixel.
constexpr int32_t kGapMs = 333;
// A scheduled start older than this gets no flash, so a late frame never
// shows a catch-up flash.
constexpr int32_t kLateMs = 60;
// Must match gamma_correct on the partition lights (ESPHome default 2.8).
constexpr float kGamma = 2.8f;

struct Rgb {
  uint8_t r, g, b;
};

struct Rgbw {
  uint8_t r, g, b, w;
};

struct Clock {
  uint32_t anchor_ms = 0;  // the time at which the beat count is anchor_beat
  double anchor_beat = 0;
  float bpm = 120;
  float rate = 1;  // steps per beat
};

struct Slot {
  uint32_t flash_ms = uint32_t(-10000);  // nominal start of the latest chase flash
};

struct Disco {
  Clock clock;
  std::array<Slot, 4> slots;
};

struct Frame {
  Slot slot;
  Rgbw out;
};

// A firing order: the slot that starts at each quarter of a step.
using Order = std::array<uint8_t, 4>;

// The firing order of `bar` (4 beats). The 24 orders are in Steinhaus-Johnson-
// Trotter order, so each bar differs from the bar before by one swap of two
// neighbours, also from the last entry back to the first.
inline Order bar_order(int64_t bar) {
  static constexpr Order kOrders[24] = {
      {0, 1, 2, 3}, {0, 1, 3, 2}, {0, 3, 1, 2}, {3, 0, 1, 2}, {3, 0, 2, 1}, {0, 3, 2, 1},
      {0, 2, 3, 1}, {0, 2, 1, 3}, {2, 0, 1, 3}, {2, 0, 3, 1}, {2, 3, 0, 1}, {3, 2, 0, 1},
      {3, 2, 1, 0}, {2, 3, 1, 0}, {2, 1, 3, 0}, {2, 1, 0, 3}, {1, 2, 0, 3}, {1, 2, 3, 0},
      {1, 3, 2, 0}, {3, 1, 2, 0}, {3, 1, 0, 2}, {1, 3, 0, 2}, {1, 0, 3, 2}, {1, 0, 2, 3}};
  return kOrders[(bar % 24 + 24) % 24];
}

inline int32_t since(uint32_t a, uint32_t b) { return int32_t(a - b); }

inline double beats_at(const Clock &c, uint32_t t) {
  return c.anchor_beat + since(t, c.anchor_ms) * double(c.bpm) / 60000.0;
}

inline double step_ms(const Clock &c) { return 60000.0 / (double(c.bpm) * c.rate); }

// The scheduled start of `slot` in step `step`, in steps: the step plus the
// slot's quarter in the firing order of the bar that holds the step.
inline double start_in(const Clock &c, double step, int slot) {
  Order o = bar_order(int64_t(std::floor(step / (4 * c.rate))));
  return step + (std::find(o.begin(), o.end(), slot) - o.begin()) / 4.0;
}

// The nominal time of the latest scheduled start of `slot` at or before `now`.
inline uint32_t latest_start(const Clock &c, int slot, uint32_t now) {
  double steps = beats_at(c, now) * c.rate;
  double start = start_in(c, std::floor(steps), slot);
  if (start > steps) start = start_in(c, std::floor(steps) - 1, slot);
  double offset_ms = (start / c.rate - c.anchor_beat) * 60000.0 / c.bpm;
  return c.anchor_ms + uint32_t(int32_t(std::lround(offset_ms)));
}

// Light output after a flash: 1 at its start, then a fade towards a 15% glow.
// `phase` is the time since the flash start in steps.
inline float envelope(float phase) { return 0.15f + 0.85f * std::exp(-5.0f * phase); }

// The channel scale (0-255) that gives `fraction` of full light output after
// the partition applies gamma.
inline uint8_t level(float fraction) { return uint8_t(std::lround(255.0f * std::pow(fraction, 1.0f / kGamma))); }

// The phrase (16 beats) that contains `beats`, floored for negative beats.
inline int64_t phrase_of(double beats) { return int64_t(std::floor(beats / 16)); }

// Hue (phrase * 137 + 20 + slot * 90) mod 360 degrees, HSL saturation 100% and
// lightness 55%, so the four slots are 90 degrees apart and the brightest
// channel is 255.
inline Rgb phrase_colour(int64_t phrase, int slot) {
  float h = float(((phrase * 137 + 20 + slot * 90) % 360 + 360) % 360) / 60.0f;
  float x = 0.9f * (1 - std::fabs(std::fmod(h, 2.0f) - 1));
  float rgb[6][3] = {{0.9f, x, 0}, {x, 0.9f, 0}, {0, 0.9f, x}, {0, x, 0.9f}, {x, 0, 0.9f}, {0.9f, 0, x}};
  const float *v = rgb[int(h)];
  auto byte = [](float c) { return uint8_t(std::lround((c + 0.1f) * 255)); };
  return {byte(v[0]), byte(v[1]), byte(v[2])};
}

inline uint8_t scale(uint8_t channel, uint8_t by) { return uint8_t((channel * by + 127) / 255); }

// The next state and output of `slot` at `now`. A new scheduled start becomes
// a flash only if it is at least kGapMs after the slot's last flash and at
// most kLateMs in the past. Otherwise the start passes with no flash.
inline Frame frame(const Disco &d, int slot, uint32_t now) {
  Slot s = d.slots[slot];
  uint32_t start = latest_start(d.clock, slot, now);
  if (since(start, s.flash_ms) >= kGapMs && since(now, start) <= kLateMs) s.flash_ms = start;
  float phase = float(std::fmax(0.0, since(now, s.flash_ms) / step_ms(d.clock)));
  uint8_t by = level(envelope(phase));
  Rgb c = phrase_colour(phrase_of(beats_at(d.clock, now)), slot);
  return {s, {scale(c.r, by), scale(c.g, by), scale(c.b, by), 0}};
}

}  // namespace mill_disco
