// Host tests for include/mill_ramp.h. scripts/check.sh builds and runs them.
// The program prints each failed check and exits 1 if any check fails.
#include "mill_ramp.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>

using namespace mill_ramp;

static int failures = 0;

#define CHECK(cond)                                              \
  do {                                                           \
    if (!(cond)) {                                               \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      failures++;                                                \
    }                                                            \
  } while (0)

#define CHECK_NEAR(actual, expected, tol)                                                      \
  do {                                                                                         \
    double a_ = (actual), e_ = (expected);                                                     \
    if (std::fabs(a_ - e_) > (tol)) {                                                          \
      std::printf("FAIL %s:%d: %s = %f, expected %f +- %f\n", __FILE__, __LINE__, #actual, a_, \
                  e_, double(tol));                                                            \
      failures++;                                                                              \
    }                                                                                          \
  } while (0)

constexpr uint32_t kTickMs = 20;
constexpr float kNan = std::numeric_limits<float>::quiet_NaN();

// The result of ticks every kTickMs toward one goal.
struct Run {
  Ramp ramp;
  uint32_t ms;            // time until the speed first equals the goal, or the run length
  bool crossed;           // a tick passed through 0 without a tick at 0
  bool overshot;          // the speed went past the goal
  float largest_change;   // the largest change of speed in one tick
};

// Ticks from `r` toward `goal` until the speed equals it, for at most `limit_ms`.
Run run(Ramp r, float goal, uint32_t limit_ms = 20000) {
  Run out{r, 0, false, false, 0};
  float from = r.speed;
  while (out.ms < limit_ms && out.ramp.speed != goal) {
    Ramp next = step(out.ramp, goal, out.ramp.at_ms + kTickMs);
    if (next.speed * out.ramp.speed < 0) out.crossed = true;
    if ((goal - from) * (next.speed - goal) > 0) out.overshot = true;
    out.largest_change = std::max(out.largest_change, std::fabs(next.speed - out.ramp.speed));
    out.ramp = next;
    out.ms += kTickMs;
  }
  return out;
}

// A ramp that has stood still at `speed` for a while.
Ramp steady(float speed) {
  Ramp r;
  r.speed = speed;
  r.top = std::fabs(speed);
  r.at_ms = 1000;
  return r;
}

void test_speed_conversion() {
  CHECK_NEAR(steps_per_s(1), 34.133, 0.01);
  CHECK_NEAR(steps_per_s(5), 170.667, 0.01);
  CHECK_NEAR(steps_per_s(9), 307.2, 0.01);
  CHECK_NEAR(steps_per_s(6.5), 6.5 * 2048 / 60, 0.01);
  // Off-step values round half up to 0.5 rpm.
  CHECK(round_rpm(5.2f) == 5.0f);
  CHECK(round_rpm(5.25f) == 5.5f);
  CHECK(round_rpm(5.7f) == 5.5f);
  CHECK(round_rpm(8.8f) == 9.0f);
  CHECK(round_rpm(4.5f) == 4.5f);
  // NaN and values outside 1-9 rpm, such as an old steps/s value, give 5 rpm.
  CHECK(!rpm_ok(kNan));
  CHECK(!rpm_ok(170));
  CHECK(!rpm_ok(0.9f));
  CHECK(!rpm_ok(9.1f));
  CHECK(rpm_ok(1) && rpm_ok(9));
  for (float bad : {kNan, 0.0f, -3.0f, 0.99f, 9.01f, 60.0f, 170.0f, 320.0f, 1e9f}) {
    CHECK_NEAR(steps_per_s(bad), steps_per_s(5), 1e-3);
  }
}

void test_goal() {
  CHECK(goal(false, false, 5) == 0);
  CHECK(goal(false, true, 5) == 0);
  CHECK_NEAR(goal(true, false, 5), steps_per_s(5), 1e-3);
  CHECK_NEAR(goal(true, true, 5), -steps_per_s(5), 1e-3);
  CHECK_NEAR(goal(true, false, 170), steps_per_s(5), 1e-3);
  CHECK_NEAR(goal(true, true, kNan), -steps_per_s(5), 1e-3);
  // The goal stays in 1-9 rpm for any input.
  for (float rpm : {kNan, -1.0f, 0.0f, 1.0f, 4.75f, 9.0f, 9.5f, 400.0f}) {
    float g = goal(true, false, rpm);
    CHECK(g >= steps_per_s(1) - 1e-3 && g <= steps_per_s(9) + 1e-3);
  }
}

// From stopped, the sails reach the set speed in about 3 s at any speed.
void test_start_takes_3s() {
  for (float rpm : {1.0f, 5.0f, 9.0f}) {
    Run r = run(steady(0), steps_per_s(rpm));
    CHECK_NEAR(r.ms, 3000, 60);
    CHECK(!r.overshot);
  }
}

// From the set speed, the sails stop in about 3 s at any speed, in either direction.
void test_stop_takes_3s() {
  for (float rpm : {1.0f, 5.0f, 9.0f}) {
    for (float sign : {1.0f, -1.0f}) {
      Run r = run(steady(sign * steps_per_s(rpm)), 0);
      CHECK_NEAR(r.ms, 3000, 60);
      CHECK(!r.overshot);
      CHECK(r.ramp.speed == 0);
      CHECK(aim(r.ramp) == 0);
    }
  }
}

// The stop never jumps: no tick changes the speed by more than 1/100 of 9 rpm.
void test_no_jumps() {
  float most = steps_per_s(9) / kRampS * kTickMs / 1000 + 1e-3f;
  CHECK(run(steady(steps_per_s(9)), 0).largest_change <= most);
  CHECK(run(steady(0), steps_per_s(9)).largest_change <= most);
  CHECK(run(steady(steps_per_s(9)), -steps_per_s(9)).largest_change <= most);
  CHECK(run(steady(steps_per_s(9)), steps_per_s(1)).largest_change <= most);
  // A late tick counts as 0.1 s, not as the whole gap.
  Ramp r = steady(0);
  r = step(r, steps_per_s(9), r.at_ms + 60000);
  CHECK_NEAR(r.speed, steps_per_s(9) / kRampS * kMaxTickS, 1e-3);
}

// A speed change while turning ramps at the rate of the faster speed, up or down.
void test_speed_change() {
  float rate9 = steps_per_s(9) / kRampS;  // steps/s per second
  Run up = run(steady(steps_per_s(5)), steps_per_s(9));
  CHECK_NEAR(up.ms, (steps_per_s(9) - steps_per_s(5)) / rate9 * 1000, 60);
  CHECK(!up.overshot);
  Run down = run(steady(steps_per_s(9)), steps_per_s(1));
  CHECK_NEAR(down.ms, (steps_per_s(9) - steps_per_s(1)) / rate9 * 1000, 60);
  CHECK(!down.overshot);
  // Then a stop from 1 rpm takes 3 s again.
  CHECK_NEAR(run(down.ramp, 0).ms, 3000, 60);
}

// A reversal slows to a stop, then speeds up the other way: 3 s each.
void test_reverse() {
  for (float sign : {1.0f, -1.0f}) {
    float s = sign * steps_per_s(5);
    Run to_stop = run(steady(s), 0);
    Ramp r = steady(s);
    uint32_t ms = 0;
    uint32_t zero_at = 0;
    bool crossed = false;
    while (r.speed != -s && ms < 20000) {
      Ramp next = step(r, -s, r.at_ms + kTickMs);
      if (next.speed * r.speed < 0) crossed = true;
      if (next.speed == 0 && zero_at == 0) zero_at = ms + kTickMs;
      // Until the stop, the speed keeps the old sign and the aim the old direction.
      if (zero_at == 0) CHECK(aim(next) == (sign > 0 ? kAimSteps : -kAimSteps));
      r = next;
      ms += kTickMs;
    }
    CHECK(!crossed);
    CHECK_NEAR(zero_at, to_stop.ms, kTickMs);
    CHECK_NEAR(zero_at, 3000, 60);
    CHECK_NEAR(ms, 6000, 120);
    CHECK(aim(r) == (sign > 0 ? -kAimSteps : kAimSteps));
  }
}

// Races: a new goal part way through a ramp continues from the speed now, at
// the same rate, with no jump.
void test_races() {
  float s = steps_per_s(5);
  float most = s / kRampS * kTickMs / 1000 + 1e-3f;
  // A stop during the start: half way up, the stop takes 1.5 s.
  Run half_up = run(steady(0), s, 1500);
  CHECK_NEAR(half_up.ramp.speed, s / 2, s / 50);
  Run stop = run(half_up.ramp, 0);
  CHECK_NEAR(stop.ms, 1500, 60);
  CHECK(stop.largest_change <= most);
  // A start during the stop: half way down, back to speed in 1.5 s.
  Run half_down = run(steady(s), 0, 1500);
  Run start = run(half_down.ramp, s);
  CHECK_NEAR(start.ms, 1500, 60);
  CHECK(start.largest_change <= most);
  // A stop during a reversal, after the sails turned the other way: they stop.
  Run part = run(steady(s), -s, 4500);
  CHECK(part.ramp.speed < 0);
  Run stop2 = run(part.ramp, 0);
  CHECK(stop2.ramp.speed == 0);
  CHECK(!stop2.crossed);
  CHECK_NEAR(stop2.ms, 1500, 60);
  // A reversal back during a reversal, before the stop: back to forward with no crossing.
  Run early = run(steady(s), -s, 1000);
  CHECK(early.ramp.speed > 0);
  Run back = run(early.ramp, s);
  CHECK(!back.crossed);
  CHECK_NEAR(back.ms, 1000, 60);
  // A speed change during the start: the ramp goes on to the new speed.
  Run part_up = run(steady(0), s, 1000);
  Run faster = run(part_up.ramp, steps_per_s(9));
  CHECK(!faster.overshot);
  CHECK(faster.largest_change <= steps_per_s(9) / kRampS * kTickMs / 1000 + 1e-3f);
}

// The stepper settings for a speed.
void test_stepper_settings() {
  Ramp r;
  CHECK(aim(r) == 0);
  CHECK(max_speed(r) == 1.0f);
  r.speed = 170;
  CHECK(aim(r) == kAimSteps);
  CHECK(max_speed(r) == 170.0f);
  r.speed = -0.5f;
  CHECK(aim(r) == -kAimSteps);
  CHECK(max_speed(r) == 1.0f);
}

// A clock that wraps between two ticks still gives a 20 ms tick.
void test_clock_wrap() {
  Ramp r;
  r.at_ms = UINT32_MAX - 9;
  r = step(r, steps_per_s(9), 10);
  CHECK_NEAR(r.speed, steps_per_s(9) / kRampS * 0.02f, 1e-3);
}

int main() {
  test_speed_conversion();
  test_goal();
  test_start_takes_3s();
  test_stop_takes_3s();
  test_no_jumps();
  test_speed_change();
  test_reverse();
  test_races();
  test_stepper_settings();
  test_clock_wrap();
  if (failures) {
    std::printf("%d ramp checks failed\n", failures);
    return 1;
  }
  std::printf("ramp header tests passed\n");
  return 0;
}
