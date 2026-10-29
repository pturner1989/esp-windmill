// Host tests for include/mill_wled.h. scripts/check.sh builds and runs them.
// The program prints each failed check and exits 1 if any check fails.
#include "mill_wled.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace mill_wled;

static int failures = 0;

#define CHECK(cond)                                              \
  do {                                                           \
    if (!(cond)) {                                               \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      failures++;                                                \
    }                                                            \
  } while (0)

using Bytes = std::vector<uint8_t>;

bool same(Rgbw a, Rgbw b) { return a.r == b.r && a.g == b.g && a.b == b.b && a.w == b.w; }

bool same(const Wled &a, const Wled &b) {
  for (int n = 0; n < 4; n++)
    if (!same(a.sync[n], b.sync[n]) || !same(a.live[n], b.live[n])) return false;
  return a.live_on == b.live_on && a.live_forever == b.live_forever && a.live_until_ms == b.live_until_ms;
}

Wled sync(const Wled &w, const Bytes &b, uint32_t now) { return receive_sync(w, b.data(), b.size(), now); }
Wled ddp(const Wled &w, const Bytes &b, uint32_t now) { return receive_ddp(w, b.data(), b.size(), now); }

// A WLED notifier packet (41 bytes) with brightness `bri` and primary colour
// r, g, b, w.
Bytes notifier(uint8_t bri, uint8_t r, uint8_t g, uint8_t b, uint8_t w) {
  Bytes p(41, 0);
  p[1] = 1;  // call mode: direct change
  p[2] = bri;
  p[3] = r;
  p[4] = g;
  p[5] = b;
  p[10] = w;
  p[11] = 12;  // version
  p[36] = 1;   // sync group 1
  return p;
}

// A DDP data packet with the push flag, `type`, byte offset `offset` and
// `payload`.
Bytes ddp_packet(uint8_t type, uint32_t offset, const Bytes &payload, bool timecode = false) {
  Bytes p = {uint8_t(0x41 | (timecode ? 0x10 : 0)), 1, type, 1, uint8_t(offset >> 24), uint8_t(offset >> 16),
             uint8_t(offset >> 8), uint8_t(offset), uint8_t(payload.size() >> 8), uint8_t(payload.size())};
  if (timecode) p.insert(p.end(), {0, 0, 0, 0});
  p.insert(p.end(), payload.begin(), payload.end());
  return p;
}

void test_dark_before_any_packet() {
  Wled w;
  for (int n = 0; n < 4; n++) CHECK(same(pixel(w, n, 0), {0, 0, 0, 0}));
}

void test_sync_sets_all_four_pixels() {
  Wled w = sync(Wled{}, notifier(255, 255, 128, 0, 40), 100);
  for (int n = 0; n < 4; n++) CHECK(same(pixel(w, n, 100), {255, 128, 0, 40}));
  // The colour stays however long no packet comes.
  CHECK(same(pixel(w, 2, 100 + 3600000), {255, 128, 0, 40}));
}

void test_sync_scales_by_brightness() {
  Wled w = sync(Wled{}, notifier(128, 255, 100, 0, 255), 0);
  CHECK(same(pixel(w, 0, 0), {128, 50, 0, 128}));
  // Brightness 0 is the WLED turned off: dark.
  w = sync(w, notifier(0, 255, 255, 255, 255), 0);
  CHECK(same(pixel(w, 3, 0), {0, 0, 0, 0}));
}

void test_sync_accepts_old_short_packets() {
  Bytes p = notifier(255, 10, 20, 30, 40);
  p.resize(kSyncMinSize);
  CHECK(same(pixel(sync(Wled{}, p, 0), 1, 0), {10, 20, 30, 40}));
  p.resize(kSyncMinSize - 1);
  CHECK(same(sync(Wled{}, p, 0), Wled{}));
}

void test_bad_packets_change_nothing() {
  Wled w = sync(Wled{}, notifier(255, 1, 2, 3, 4), 0);
  CHECK(same(sync(w, {}, 5), w));
  CHECK(same(sync(w, {2}, 5), w));
  CHECK(same(sync(w, {9, 5, 1, 2, 3}, 5), w));        // unknown protocol
  CHECK(same(sync(w, {2, 5, 1, 2, 3, 4}, 5), w));     // DRGB, not a multiple of 3
  CHECK(same(sync(w, {1, 5, 0, 1, 2}, 5), w));        // WARLS, not a multiple of 4
  CHECK(same(sync(w, {3, 5, 1, 2, 3, 4, 5}, 5), w));  // DRGBW, not a multiple of 4
  CHECK(same(sync(w, {4, 5, 0}, 5), w));              // DNRGB, no start index
  CHECK(same(ddp(w, {0x41, 1, 0x0B}, 5), w));         // short header
  CHECK(same(ddp(w, ddp_packet(0x0B, 0, {1, 2, 3, 4}), 5), w));  // not a multiple of 3
  Bytes p = ddp_packet(0x0B, 0, {1, 2, 3});
  p[0] = 0x81;  // version 2
  CHECK(same(ddp(w, p, 5), w));
  p[0] = 0x49;  // query
  CHECK(same(ddp(w, p, 5), w));
  p = ddp_packet(0x0B, 0, {1, 2, 3});
  p.pop_back();  // length says more bytes than came
  CHECK(same(ddp(w, p, 5), w));
}

void test_drgb_sets_pixels_in_order() {
  Wled w = sync(Wled{}, {2, 2, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}, 1000);
  CHECK(same(pixel(w, 0, 1000), {1, 2, 3, 0}));
  CHECK(same(pixel(w, 1, 1000), {4, 5, 6, 0}));
  CHECK(same(pixel(w, 2, 1000), {7, 8, 9, 0}));
  CHECK(same(pixel(w, 3, 1000), {10, 11, 12, 0}));
}

void test_drgbw_warls_and_dnrgb() {
  Wled w = sync(Wled{}, {3, 2, 1, 2, 3, 4, 5, 6, 7, 8}, 0);
  CHECK(same(pixel(w, 0, 0), {1, 2, 3, 4}));
  CHECK(same(pixel(w, 1, 0), {5, 6, 7, 8}));
  w = sync(w, {1, 2, 3, 9, 9, 9, 200, 1, 1, 1}, 0);  // WARLS: LED 3, and LED 200 is past the mill
  CHECK(same(pixel(w, 3, 0), {9, 9, 9, 0}));
  CHECK(same(pixel(w, 0, 0), {1, 2, 3, 4}));
  w = sync(w, {4, 2, 0, 2, 20, 21, 22, 30, 31, 32, 40, 41, 42}, 0);  // DNRGB from LED 2
  CHECK(same(pixel(w, 1, 0), {5, 6, 7, 8}));
  CHECK(same(pixel(w, 2, 0), {20, 21, 22, 0}));
  CHECK(same(pixel(w, 3, 0), {30, 31, 32, 0}));
}

void test_live_times_out_to_the_sync_colour() {
  Wled w = sync(Wled{}, notifier(255, 50, 60, 70, 0), 0);
  w = sync(w, {2, 2, 1, 2, 3}, 1000);  // 2 s timeout
  CHECK(same(pixel(w, 0, 2999), {1, 2, 3, 0}));
  CHECK(same(pixel(w, 1, 2999), {0, 0, 0, 0}));  // left out of the frame: dark
  CHECK(same(pixel(w, 0, 3000), {50, 60, 70, 0}));
  // A sync during a live frame waits until the frame ends.
  w = sync(w, {2, 2, 1, 2, 3}, 4000);
  w = sync(w, notifier(255, 90, 90, 90, 90), 4500);
  CHECK(same(pixel(w, 0, 4500), {1, 2, 3, 0}));
  CHECK(same(pixel(w, 0, 6000), {90, 90, 90, 90}));
}

void test_live_frame_after_timeout_starts_dark() {
  Wled w = sync(Wled{}, {2, 1, 1, 1, 1, 2, 2, 2}, 0);
  w = sync(w, {2, 1, 7, 7, 7}, 5000);
  CHECK(same(pixel(w, 0, 5000), {7, 7, 7, 0}));
  CHECK(same(pixel(w, 1, 5000), {0, 0, 0, 0}));
  // Within the timeout, a short frame keeps the other LEDs.
  w = sync(w, {1, 1, 0, 8, 8, 8}, 5100);
  CHECK(same(pixel(w, 0, 5100), {8, 8, 8, 0}));
}

void test_timeout_255_and_0() {
  Wled w = sync(Wled{}, {2, kForever, 1, 2, 3}, 0);
  CHECK(same(pixel(w, 0, 3000000000u), {1, 2, 3, 0}));
  w = sync(w, {2, 0, 9, 9, 9}, 10);  // timeout 0: back to the sync colour now
  CHECK(same(pixel(w, 0, 10), {0, 0, 0, 0}));
}

void test_live_timeout_survives_the_clock_wrap() {
  uint32_t now = 0xFFFFFC00u;  // 1024 ms before the wrap
  Wled w = sync(Wled{}, {2, 2, 4, 5, 6}, now);
  CHECK(same(pixel(w, 0, now + 1500), {4, 5, 6, 0}));
  CHECK(same(pixel(w, 0, now + 2000), {0, 0, 0, 0}));
}

void test_ddp_rgb_and_rgbw() {
  Wled w = ddp(Wled{}, ddp_packet(0x0B, 0, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}), 0);
  CHECK(same(pixel(w, 0, 0), linear(Rgbw{1, 2, 3, 0})));
  CHECK(same(pixel(w, 3, 0), linear(Rgbw{10, 11, 12, 0})));
  w = ddp(w, ddp_packet(0x1B, 8, {1, 2, 3, 4, 5, 6, 7, 8}), 10);  // RGBW from LED 2
  CHECK(same(pixel(w, 1, 10), linear(Rgbw{4, 5, 6, 0})));
  CHECK(same(pixel(w, 2, 10), linear(Rgbw{1, 2, 3, 4})));
  CHECK(same(pixel(w, 3, 10), linear(Rgbw{5, 6, 7, 8})));
  w = ddp(w, ddp_packet(0x0B, 3, {20, 21, 22}, true), 20);  // timecode header, LED 1
  CHECK(same(pixel(w, 1, 20), linear(Rgbw{20, 21, 22, 0})));
}

// WLED sends 255/150/60 as 255/58/4 after its gamma 2.8. The mill takes that
// back to linear, so the light's own gamma lights the LED as WLED meant.
void test_ddp_undoes_wled_gamma() {
  CHECK(linear(uint8_t(0)) == 0);
  CHECK(linear(uint8_t(255)) == 255);
  CHECK(linear(uint8_t(58)) == 150);
  for (int v = 1; v < 256; v++) CHECK(linear(uint8_t(v)) >= linear(uint8_t(v - 1)));
  for (int v = 64; v < 256; v++) {
    uint8_t sent = uint8_t(std::lround(255.0 * std::pow(v / 255.0, 2.8)));
    CHECK(std::abs(linear(sent) - v) <= 2);
  }
  Wled w = ddp(Wled{}, ddp_packet(0x0B, 0, {255, 58, 4}), 0);
  CHECK(same(pixel(w, 0, 0), {255, 150, linear(uint8_t(4)), 0}));
}

void test_ddp_times_out() {
  Wled w = sync(Wled{}, notifier(255, 5, 5, 5, 5), 0);
  w = ddp(w, ddp_packet(0x0B, 0, {1, 2, 3}), 100);
  CHECK(same(pixel(w, 0, 100 + kDdpTimeoutMs - 1), linear(Rgbw{1, 2, 3, 0})));
  CHECK(same(pixel(w, 0, 100 + kDdpTimeoutMs), {5, 5, 5, 5}));
}

int main() {
  test_dark_before_any_packet();
  test_sync_sets_all_four_pixels();
  test_sync_scales_by_brightness();
  test_sync_accepts_old_short_packets();
  test_bad_packets_change_nothing();
  test_drgb_sets_pixels_in_order();
  test_drgbw_warls_and_dnrgb();
  test_live_times_out_to_the_sync_colour();
  test_live_frame_after_timeout_starts_dark();
  test_timeout_255_and_0();
  test_live_timeout_survives_the_clock_wrap();
  test_ddp_rgb_and_rgbw();
  test_ddp_undoes_wled_gamma();
  test_ddp_times_out();
  if (failures > 0) {
    std::printf("mill_wled_test: %d checks failed\n", failures);
    return 1;
  }
  std::printf("mill_wled_test: all checks passed\n");
  return 0;
}
