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

// Runs frames 16 ms apart from `from` for `duration` ms, all four slots in
// each frame, and returns each flash start. Every frame at or after
// `from + late_at` comes 200 ms late.
std::vector<Start> simulate(Disco d, uint32_t from, uint32_t duration, uint32_t late_at = UINT32_MAX) {
  std::vector<Start> starts;
  for (uint32_t k = 0; k * kFrameMs <= duration; k++) {
    uint32_t offset = k * kFrameMs;
    uint32_t now = from + offset + (offset >= late_at ? 200 : 0);
    for (int slot = 0; slot < 4; slot++) {
      Frame f = frame(d, slot, now);
      if (f.slot.flash_ms != d.slots[slot].flash_ms) starts.push_back({slot, f.slot.flash_ms, now});
      d.slots[slot] = f.slot;
    }
  }
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
  if (failures > 0) {
    std::printf("mill_disco_test: %d checks failed\n", failures);
    return 1;
  }
  std::printf("mill_disco_test: all checks passed\n");
  return 0;
}
