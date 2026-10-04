// Host tests for include/mill_disco.h. scripts/check.sh builds and runs them.
// The program prints each failed check and exits 1 if any check fails.
#include "mill_disco.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <set>
#include <vector>

using namespace mill_disco;

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

constexpr uint32_t kFrameMs = 16;

struct Start {
  int slot;
  uint32_t at;     // nominal start, as frame() reports it
  uint32_t frame;  // time of the frame that started the flash
};

// A state with beat 0 at `anchor`, as the node has it at boot when `anchor` is 0.
Disco anchored_at(uint32_t anchor) {
  Disco d;
  d.clock.anchor_ms = anchor;
  for (Slot &s : d.slots) s.flash_ms = anchor - 10000;
  return d;
}

// Runs frames 16 ms apart on `d` from `from` for `duration` ms, all four slots
// in each frame, and adds each flash start to `starts`. Every frame at or
// after `from + late_at` comes 200 ms late.
void run(Disco &d, std::vector<Start> &starts, uint32_t from, uint32_t duration, uint32_t late_at = UINT32_MAX) {
  for (uint32_t k = 0; k * kFrameMs <= duration; k++) {
    uint32_t offset = k * kFrameMs;
    uint32_t now = from + offset + (offset >= late_at ? 200 : 0);
    for (int slot = 0; slot < 4; slot++) {
      Frame f = frame(d, slot, now);
      if (f.slot.flash_ms != d.slots[slot].flash_ms) starts.push_back({slot, f.slot.flash_ms, now});
      d.slots[slot] = f.slot;
    }
  }
}

std::vector<Start> simulate(Disco d, uint32_t from, uint32_t duration, uint32_t late_at = UINT32_MAX) {
  std::vector<Start> starts;
  run(d, starts, from, duration, late_at);
  return starts;
}

int count_in(const std::vector<Start> &starts, int slot, uint32_t from, uint32_t to) {
  return int(std::count_if(starts.begin(), starts.end(), [&](const Start &s) {
    return (slot < 0 || s.slot == slot) && s.at >= from && s.at < to;
  }));
}

double hue_of(Rgb c) {
  double r = c.r / 255.0, g = c.g / 255.0, b = c.b / 255.0;
  double hi = std::max({r, g, b}), lo = std::min({r, g, b}), span = hi - lo;
  double h = hi == r ? std::fmod((g - b) / span, 6.0) : hi == g ? (b - r) / span + 2 : (r - g) / span + 4;
  return std::fmod(h * 60 + 360, 360);
}

void test_chase_at_120_bpm() {
  std::vector<Start> starts = simulate(anchored_at(0), 0, 60000);
  // At 120 BPM and 1x a step is 500 ms and a quarter step 125 ms.
  for (uint32_t step = 0; step < 120; step++) {
    for (int slot = 0; slot < 4; slot++) CHECK(count_in(starts, slot, step * 500, step * 500 + 500) == 1);
  }
  for (uint32_t quarter = 0; quarter < 480; quarter++) {
    CHECK(count_in(starts, -1, quarter * 125, quarter * 125 + 125) == 1);
  }
  for (size_t i = 1; i < starts.size(); i++) CHECK(starts[i].frame != starts[i - 1].frame);
  for (int slot = 0; slot < 4; slot++) {
    const Start *previous = nullptr;
    for (const Start &s : starts) {
      if (s.slot != slot) continue;
      if (previous != nullptr) CHECK(int32_t(s.at - previous->at) >= 333);
      previous = &s;
    }
  }
  // A bar is 2000 ms. The four starts of its first step show its firing order.
  std::vector<Order> orders;
  for (uint32_t bar = 0; bar < 30; bar++) {
    Order o{};
    int n = 0;
    for (const Start &s : starts) {
      if (s.at >= bar * 2000 && s.at < bar * 2000 + 500 && n < 4) o[n++] = uint8_t(s.slot);
    }
    orders.push_back(o);
  }
  for (size_t first = 0; first + 8 <= orders.size(); first++) {
    CHECK(std::set<Order>(orders.begin() + first, orders.begin() + first + 8).size() == 8);
  }
}

// At 120 BPM a phrase is 8000 ms. Each slot keeps one hue for a whole phrase
// and changes it at the first frame of the next phrase.
void test_colour_changes_only_at_phrase_start() {
  Disco d = anchored_at(0);
  double hue[4] = {};
  for (uint32_t now = 0; now <= 60000; now += kFrameMs) {
    bool phrase_start = now % 8000 < kFrameMs;
    for (int slot = 0; slot < 4; slot++) {
      Frame f = frame(d, slot, now);
      d.slots[slot] = f.slot;
      double h = hue_of({f.out.r, f.out.g, f.out.b});
      double change = std::fabs(std::remainder(h - hue[slot], 360.0));
      if (now > 0) CHECK(phrase_start ? change > 45 : change < 5);
      hue[slot] = h;
    }
  }
}

void test_late_frame_skips_old_starts() {
  const uint32_t late_at = 10000;
  std::vector<Start> starts = simulate(anchored_at(0), 0, 20000, late_at);
  std::vector<Start> on_time = simulate(anchored_at(0), 0, 20000);
  CHECK(starts.size() < on_time.size());  // the late frame skipped at least one start
  for (const Start &s : starts) CHECK(int32_t(s.frame - s.at) <= 60);
  const uint32_t late_frame = late_at + 200;
  for (int slot = 0; slot < 4; slot++) {
    // The slot's first scheduled start after the late frame: 500 ms steps, 4 steps
    // a bar, and the slot at its quarter in the bar's firing order.
    uint32_t next = 0;
    for (uint32_t step = 0; next <= late_frame; step++) {
      Order o = bar_order(step / 4);
      next = step * 500 + 125 * uint32_t(std::find(o.begin(), o.end(), slot) - o.begin());
    }
    CHECK(count_in(starts, slot, next, next + 1) == 1);
  }
}

// True if `b` is `a` with two neighbouring positions swapped.
bool one_neighbour_swap(const Order &a, const Order &b) {
  for (int i = 0; i < 3; i++) {
    Order swapped = a;
    std::swap(swapped[i], swapped[i + 1]);
    if (swapped == b) return true;
  }
  return false;
}

void test_bar_orders() {
  for (int64_t bar = -30; bar <= 30; bar++) {
    Order o = bar_order(bar);
    Order sorted = o;
    std::sort(sorted.begin(), sorted.end());
    CHECK((sorted == Order{0, 1, 2, 3}));
    CHECK(one_neighbour_swap(bar_order(bar - 1), o));
    CHECK(bar_order(bar + 24) == o);  // floored modulo: bar -1 reads the last entry
  }
}

void test_envelope() {
  CHECK_NEAR(envelope(0), 1.0, 1e-6);
  CHECK(envelope(1) >= 0.10f && envelope(1) <= 0.20f);
}

void test_level_undoes_gamma() {
  for (int i = 0; i <= 100; i++) {
    float fraction = i / 100.0f;
    CHECK_NEAR(std::pow(level(fraction) / 255.0, 2.8), fraction, 0.01);
  }
}

bool same(Rgb a, Rgb b) { return a.r == b.r && a.g == b.g && a.b == b.b; }

void test_phrase_colours() {
  for (int64_t phrase = -2; phrase <= 10; phrase++) {
    for (int slot = 0; slot < 4; slot++) {
      Rgb c = phrase_colour(phrase, slot);
      CHECK(std::max({c.r, c.g, c.b}) == 255);
      double gap = std::fmod(hue_of(phrase_colour(phrase, (slot + 1) % 4)) - hue_of(c) + 360, 360);
      CHECK_NEAR(gap, 90, 1);
      CHECK(!same(c, phrase_colour(phrase - 1, slot)));
    }
  }
  CHECK(phrase_of(0) == 0);
  CHECK(phrase_of(15.99) == 0);
  CHECK(phrase_of(-0.01) == -1);
  CHECK(phrase_of(-16) == -1);
  CHECK(phrase_of(-16.01) == -2);
}

void test_clock_across_millis_wrap() {
  const uint32_t anchor = UINT32_MAX - 999;  // 1 s before the counter wraps
  std::vector<Start> wrapped = simulate(anchored_at(anchor), anchor, 3000);
  std::vector<Start> plain = simulate(anchored_at(0), 0, 3000);
  CHECK(wrapped.size() == plain.size());
  for (size_t i = 0; i < std::min(wrapped.size(), plain.size()); i++) {
    CHECK(wrapped[i].slot == plain[i].slot);
    CHECK(wrapped[i].at - anchor == plain[i].at);
    CHECK(wrapped[i].frame - anchor == plain[i].frame);
  }
}

// A start puts beat 0 at the start time and keeps the tempo. A pixel that
// flashed just before the start does not flash again within 333 ms, so a quick
// off and on shows no double flash. At 150 BPM and 1/2x a step is 800 ms, and
// slot 0 has a scheduled start at the start time itself.
void test_start_resets_the_beat_and_keeps_the_gap() {
  Disco d = anchored_at(0);
  d.clock.bpm = 150;
  d.clock.rate = 0.5f;
  const uint32_t now = 7777;
  for (Slot &s : d.slots) s.flash_ms = now - 100;
  Disco started = start(d, now);
  CHECK_NEAR(beats_at(started.clock, now), 0, 1e-9);
  CHECK(started.clock.bpm == 150.0f && started.clock.rate == 0.5f);
  std::vector<Start> starts = simulate(started, now, 2000);
  for (int slot = 0; slot < 4; slot++) {
    uint32_t previous = now - 100;
    int flashes = 0;
    for (const Start &s : starts) {
      if (s.slot != slot) continue;
      CHECK(int32_t(s.at - previous) >= 333);
      previous = s.at;
      flashes++;
    }
    CHECK(flashes >= 2);  // the chase goes on after the start
  }
}

// The beat count and the phrase at `now` are the same on both clocks.
void expect_same_beat(const Clock &before, const Clock &after, uint32_t now) {
  CHECK_NEAR(beats_at(after, now), beats_at(before, now), 0.001);
  CHECK(phrase_of(beats_at(after, now)) == phrase_of(beats_at(before, now)));
}

// Runs `d` for 8 s from `now` and moves `now` to the end. Checks that a flash
// starts on each multiple of `every` beats, except at most `may_skip` of them.
void expect_on_beat_every(Disco &d, uint32_t &now, double every, int may_skip) {
  std::vector<Start> starts;
  run(d, starts, now, 8000);
  double first = beats_at(d.clock, now), last = beats_at(d.clock, now + 8000);
  int grid = 0, on_beat = 0;
  for (double b = std::ceil(first / every) * every; b < last; b += every) grid++;
  for (const Start &s : starts) {
    double b = beats_at(d.clock, s.at) / every;
    if (int32_t(s.at - now) >= 0 && std::fabs(b - std::round(b)) < 0.01) on_beat++;
  }
  CHECK(on_beat <= grid && on_beat >= grid - may_skip);
  now += 8000;
}

// A tempo change keeps the beat count, and a rate change keeps the beat, so the
// steps stay on whole beats. 2x needs 90 BPM or less.
void test_tempo_and_rate_changes_keep_the_beat() {
  Disco d = anchored_at(0);
  std::vector<Start> starts;
  run(d, starts, 0, 9300);
  uint32_t now = 9300;
  Clock before = d.clock;
  d = set_bpm(d, 100, now).disco;
  CHECK(d.clock.bpm == 100.0f);
  expect_same_beat(before, d.clock, now);
  expect_on_beat_every(d, now, 1, 0);
  before = d.clock;
  d = set_rate(d, 0.5f);
  CHECK(d.clock.rate == 0.5f);
  expect_same_beat(before, d.clock, now);
  expect_on_beat_every(d, now, 2, 0);
  before = d.clock;
  d = set_bpm(d, 90, now).disco;
  d = set_rate(d, 2);
  CHECK(d.clock.rate == 2.0f);
  expect_same_beat(before, d.clock, now);
  expect_on_beat_every(d, now, 0.5, 3);  // at most one skip at each bar change
}

void test_rate_guard_and_tempo_bounds() {
  Disco d;
  CHECK(d.clock.bpm == 120.0f && d.clock.rate == 1.0f);
  d = set_bpm(d, 90, 0).disco;
  CHECK(set_rate(d, 2).clock.rate == 2.0f);
  CHECK(set_rate(set_bpm(d, 90.1f, 0).disco, 2).clock.rate == 1.0f);
  BpmSet raised = set_bpm(set_rate(d, 2), 120, 1000);
  CHECK(raised.rate_dropped && raised.disco.clock.rate == 1.0f);
  BpmSet lowered = set_bpm(raised.disco, 80, 2000);
  CHECK(!lowered.rate_dropped && lowered.disco.clock.rate == 1.0f);
  CHECK(set_bpm(d, 200, 0).disco.clock.bpm == 180.0f);
  CHECK(set_bpm(d, 40, 0).disco.clock.bpm == 60.0f);
}

// Each start sits on a quarter step of clock `c`, and no quarter step holds two.
void expect_one_start_per_quarter(const std::vector<Start> &starts, const Clock &c) {
  std::set<long> quarters;
  for (const Start &s : starts) {
    double q = beats_at(c, s.at) * c.rate * 4;
    CHECK_NEAR(q, std::round(q), 0.01);
    CHECK(quarters.insert(std::lround(q)).second);
  }
}

// Every gap between two starts on one slot is at least 333 ms, and no start
// shows more than 60 ms late.
void expect_flash_limits(const std::vector<Start> &starts) {
  for (int slot = 0; slot < 4; slot++) {
    const Start *previous = nullptr;
    for (const Start &s : starts) {
      if (s.slot != slot) continue;
      CHECK(int32_t(s.frame - s.at) >= 0 && int32_t(s.frame - s.at) <= 60);
      if (previous != nullptr) CHECK(int32_t(s.at - previous->at) >= 333);
      previous = &s;
    }
  }
}

void test_flash_limits_at_each_setting() {
  struct Setting {
    float bpm, rate;
    int fewest, most;  // flashes of slot 0 in 30 s
  };
  for (const Setting &s : {Setting{120, 1, 59, 61}, {120, 0.5f, 29, 31}, {90, 2, 84, 90}, {180, 1, 84, 90}}) {
    Disco d = set_rate(set_bpm(anchored_at(0), s.bpm, 0).disco, s.rate);
    CHECK(d.clock.bpm == s.bpm && d.clock.rate == s.rate);
    std::vector<Start> starts = simulate(d, 0, 30000);
    int door_lamp = count_in(starts, 0, 0, 30000);
    CHECK(door_lamp >= s.fewest && door_lamp <= s.most);
    expect_one_start_per_quarter(starts, d.clock);
    expect_flash_limits(starts);
    // Each slot has one scheduled start in each step.
    long steps = std::lround(30000 / step_ms(d.clock));
    std::printf("%g BPM %gx: skips per slot", s.bpm, s.rate);
    for (int slot = 0; slot < 4; slot++) std::printf(" %ld", steps - count_in(starts, slot, 0, 30000));
    std::printf("\n");
  }
}

// At 90 BPM and 1/2x a step is 1333 ms, so 14000 ms is half-way between two
// on-beat starts. The raise to 120 BPM drops 2x to 1x.
void test_flash_limits_across_rate_changes() {
  Disco d = set_rate(set_bpm(anchored_at(0), 90, 0).disco, 0.5f);
  std::vector<Start> all, part;
  run(d, part, 0, 14000);
  expect_one_start_per_quarter(part, d.clock);
  all.insert(all.end(), part.begin(), part.end());
  d = set_rate(d, 2);
  CHECK(d.clock.rate == 2.0f);
  part.clear();
  run(d, part, 14000, 10000);
  expect_one_start_per_quarter(part, d.clock);
  all.insert(all.end(), part.begin(), part.end());
  BpmSet raised = set_bpm(d, 120, 24000);
  CHECK(raised.rate_dropped);
  d = raised.disco;
  part.clear();
  run(d, part, 24000, 10000);
  expect_one_start_per_quarter(part, d.clock);
  all.insert(all.end(), part.begin(), part.end());
  expect_flash_limits(all);
}

int main() {
  test_chase_at_120_bpm();
  test_start_resets_the_beat_and_keeps_the_gap();
  test_colour_changes_only_at_phrase_start();
  test_late_frame_skips_old_starts();
  test_bar_orders();
  test_envelope();
  test_level_undoes_gamma();
  test_phrase_colours();
  test_clock_across_millis_wrap();
  test_tempo_and_rate_changes_keep_the_beat();
  test_rate_guard_and_tempo_bounds();
  test_flash_limits_at_each_setting();
  test_flash_limits_across_rate_changes();
  if (failures > 0) {
    std::printf("mill_disco_test: %d checks failed\n", failures);
    return 1;
  }
  std::printf("mill_disco_test: all checks passed\n");
  return 0;
}
