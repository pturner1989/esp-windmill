// Sails ramp maths: the "Sail Speed" conversion and the speed ramp. Pure
// functions with no ESPHome types, no clock reads and no static state, so
// tests/mill_ramp_test.cpp runs them on the laptop.
// Used by: packages/mill_sails.yaml (the number and the 20 ms sails tick) and
// mill_ramp_esphome.h (the glue that holds the ramp).
//
// A speed is in steps/s and signed: positive turns forward, negative turns in
// reverse. The ramp moves the speed toward the goal, so a stop, a start, a
// speed change and a reversal are all the same ramp. A reversal passes through
// 0, so the sails slow to a stop before they turn the other way.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace mill_ramp {

// The 28BYJ-48 in full-step mode: steps per output turn.
constexpr float kStepsPerTurn = 2048;
// The range, step and default of "Sail Speed".
constexpr float kMinRpm = 1;
constexpr float kMaxRpm = 9;
constexpr float kDefaultRpm = 5;
// A ramp from stopped to the set speed, or back, takes this long.
constexpr float kRampS = 3;
// A longer gap between two ticks counts as this long, so a late tick never
// makes a jump.
constexpr float kMaxTickS = 0.1f;
// Steps aimed ahead of the stepper at each tick. The tick re-bases the
// position to 0 each time, so the position never overflows. If the ticks
// ever stop, the sails stop within 30 s (1000 steps at 1 rpm).
constexpr int32_t kAimSteps = 1000;

// True for a "Sail Speed" in 1-9 rpm. False for NaN.
inline bool rpm_ok(float rpm) { return rpm >= kMinRpm && rpm <= kMaxRpm; }

// Rounds half up to a multiple of 0.5 rpm.
inline float round_rpm(float rpm) { return std::floor(rpm * 2 + 0.5f) / 2; }

// "Sail Speed" in steps/s. NaN or a value outside 1-9 rpm (for example a
// steps/s value saved by older firmware) gives 5 rpm.
inline float steps_per_s(float rpm) {
  return round_rpm(rpm_ok(rpm) ? rpm : kDefaultRpm) * kStepsPerTurn / 60;
}

// The speed the sails ramp to: 0 when "Sails Turning" is off.
inline float goal(bool turning, bool reverse, float rpm) {
  if (!turning) return 0;
  return reverse ? -steps_per_s(rpm) : steps_per_s(rpm);
}

struct Ramp {
  float speed = 0;      // steps/s now, signed
  float top = 0;        // the largest speed of this ramp; sets its rate
  uint32_t at_ms = 0;   // time of the last tick
};

// One tick at `now_ms`: moves the speed toward `goal` at top / 3 s per second.
// `top` is the larger of the speeds at the two ends of the ramp, so a start or
// a stop takes 3 s at any speed, and a speed change or a reversal moves at the
// same rate. A tick that would pass through 0 stops at 0.
inline Ramp step(Ramp r, float goal, uint32_t now_ms) {
  float dt = std::clamp(int32_t(now_ms - r.at_ms) / 1000.0f, 0.0f, kMaxTickS);
  r.at_ms = now_ms;
  r.top = std::max({r.top, std::fabs(r.speed), std::fabs(goal)});
  float delta = r.top / kRampS * dt;
  float next = std::fabs(goal - r.speed) <= delta ? goal : r.speed + std::copysign(delta, goal - r.speed);
  r.speed = next * r.speed < 0 ? 0 : next;
  // At the goal the ramp ends, so the next ramp takes its rate from here.
  if (r.speed == goal) r.top = std::fabs(goal);
  return r;
}

// The stepper's speed limit for the ramp: the speed without its sign, and
// never 0, which the stepper does not accept.
inline float max_speed(const Ramp &r) { return std::max(std::fabs(r.speed), 1.0f); }

// The target after the re-base to 0: ahead in the direction of the speed, or
// 0 to stop and hold.
inline int32_t aim(const Ramp &r) { return r.speed > 0 ? kAimSteps : r.speed < 0 ? -kAimSteps : 0; }

}  // namespace mill_ramp
