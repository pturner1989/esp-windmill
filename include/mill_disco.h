// Disco maths: pure functions of time and state. No ESPHome types, no clock
// reads and no static state, so tests/mill_disco_test.cpp runs it on the laptop.
// Used by: mill_disco_esphome.h (the glue that the light effects call).
//
// Times are uint32_t milliseconds from millis(). Each difference is taken as
// int32_t(a - b), so it stays correct when the counter wraps.
#pragma once

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

inline int32_t since(uint32_t a, uint32_t b) { return int32_t(a - b); }

inline double beats_at(const Clock &c, uint32_t t) {
  return c.anchor_beat + since(t, c.anchor_ms) * double(c.bpm) / 60000.0;
}

inline double step_ms(const Clock &c) { return 60000.0 / (double(c.bpm) * c.rate); }

// The nominal time of the latest scheduled start of `slot` at or before `now`.
// The firing order is fixed: slot s starts a quarter step after slot s - 1.
inline uint32_t latest_start(const Clock &c, int slot, uint32_t now) {
  double steps = beats_at(c, now) * c.rate;
  double start = std::floor(steps) + slot / 4.0;
  if (start > steps) start -= 1;
  double offset_ms = (start / c.rate - c.anchor_beat) * 60000.0 / c.bpm;
  return c.anchor_ms + uint32_t(int32_t(std::lround(offset_ms)));
}

// Light output after a flash: 1 at its start, then a fade towards a 15% glow.
// `phase` is the time since the flash start in steps.
inline float envelope(float phase) { return 0.15f + 0.85f * std::exp(-5.0f * phase); }

// The channel scale (0-255) that gives `fraction` of full light output after
// the partition applies gamma.
inline uint8_t level(float fraction) { return uint8_t(std::lround(255.0f * std::pow(fraction, 1.0f / kGamma))); }

// Hue 20 + 90 * slot degrees, HSL saturation 100% and lightness 55%, so the
// brightest channel is 255.
inline Rgb slot_colour(int slot) {
  float h = (20 + 90 * slot) % 360 / 60.0f;
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
  Rgb c = slot_colour(slot);
  return {s, {scale(c.r, by), scale(c.g, by), scale(c.b, by), 0}};
}

}  // namespace mill_disco
