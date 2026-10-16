// wwe2k14 - 60 FPS entrances / victory motions / cinematics, and late-frame flips.
//
// Addresses found by instruction pattern in the generated code (tools/asmindex.py); the Xbox D3D device offsets
// used here are 14000 (present interval), 11028 (flip queue) and 16952..16960 (vblank bookkeeping):
//
//   0x8392DC38      engine timing block: +8 frame rate (60/30, PAL 50/25), +12 hold flag, +28..+64 time steps,
//                   +40 frame length in ms (16 / 33)
//   sub_82BB6D50    60 -> 30 (50 -> 25): loads the 30 FPS time steps. Called when an entrance, victory motion,
//                   replay or cinematic camera starts
//   sub_82BB6DF0    30 -> 60 (25 -> 50), skipped while +12 is set
//   sub_82CAA828    D3D present interval setter, `stw r4,14000(r3)`
//   sub_82CB2A28    D3D flip-target choice for a finished frame, r3 = frontbuffer | interval << 8 | threshold
//   sub_82CB5758    D3D GPU interrupt handler (source, device)
//
// Environment switches (default first):
//   WWE2K14_KEEP_60=1|0     keep 60 FPS (and the 60 FPS time step) in entrances etc. | console 30 FPS
//   WWE2K14_LATE_FLIP=1|0   a late frame flips at once | waits for the next vblank (console behaviour)

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <rex/logging.h>
#include <rex/ppc/context.h>
#include <rex/ppc/memory.h>

// The title update (WWE2K14_TU build, wwe2k14_tu.exe) relinked the code: the same functions sit at other addresses
// (tools/port_functions.py). Vanilla first, title update 2 second.
#ifdef WWE2K14_TU
#define WWE2K14_FN_ToThirty sub_82BBA580
#define WWE2K14_IMP_ToThirty __imp__sub_82BBA580
#define WWE2K14_FN_ToSixty sub_82BBA620
#define WWE2K14_IMP_ToSixty __imp__sub_82BBA620
#define WWE2K14_FN_SetPresentInterval sub_82CAE118
#define WWE2K14_IMP_SetPresentInterval __imp__sub_82CAE118
#define WWE2K14_FN_ChooseFlip sub_82CB6318
#define WWE2K14_IMP_ChooseFlip __imp__sub_82CB6318
#define WWE2K14_FN_GpuInterrupt sub_82CB9048
#define WWE2K14_IMP_GpuInterrupt __imp__sub_82CB9048
#define WWE2K14_TIMING_BLOCK 0x8392DC88
#else
#define WWE2K14_FN_ToThirty sub_82BB6D50
#define WWE2K14_IMP_ToThirty __imp__sub_82BB6D50
#define WWE2K14_FN_ToSixty sub_82BB6DF0
#define WWE2K14_IMP_ToSixty __imp__sub_82BB6DF0
#define WWE2K14_FN_SetPresentInterval sub_82CAA828
#define WWE2K14_IMP_SetPresentInterval __imp__sub_82CAA828
#define WWE2K14_FN_ChooseFlip sub_82CB2A28
#define WWE2K14_IMP_ChooseFlip __imp__sub_82CB2A28
#define WWE2K14_FN_GpuInterrupt sub_82CB5758
#define WWE2K14_IMP_GpuInterrupt __imp__sub_82CB5758
#define WWE2K14_TIMING_BLOCK 0x8392DC38
#endif

namespace {

constexpr uint32_t kTimingBlock = WWE2K14_TIMING_BLOCK;
constexpr uint32_t kFrameRate = kTimingBlock + 8;

bool EnvDefaultOn(const char* name) {
  const char* value = std::getenv(name);
  return !(value != nullptr && std::strcmp(value, "0") == 0);
}

bool Keep60() {
  static const bool enabled = EnvDefaultOn("WWE2K14_KEEP_60");
  return enabled;
}

bool LateFlipImmediate() {
  return EnvDefaultOn("WWE2K14_LATE_FLIP");
}

uint64_t SteadyNs() {
  return uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(
                      std::chrono::steady_clock::now().time_since_epoch())
                      .count());
}

// The D3D device, captured from the GPU interrupt handler's second argument.
uint32_t g_d3d_device = 0;

}  // namespace

// --- Engine frame rate --------------------------------------------------------------------------------------------

// The engine's own switch to 30 FPS. Its animation runs on these time steps (not on wall-clock time), so skipping the
// switch keeps both the display rate and the simulation step at 60: entrances play at normal speed, twice as smooth.
// Forcing only the D3D present interval instead would keep the 1/30 s step: entrances at double speed.
extern "C" PPC_FUNC(WWE2K14_IMP_ToThirty);
extern "C" PPC_FUNC(WWE2K14_FN_ToThirty) {
  const uint32_t rate = PPC_LOAD_U32(kFrameRate);
  REXLOG_INFO("[wwe2k14-rate] halve request at rate {} ({})", rate, Keep60() ? "suppressed, staying at 60" : "applied");
  if (Keep60()) {
    return;
  }
  WWE2K14_IMP_ToThirty(ctx, base);
}

// Logged only: marks the end of a 30 FPS phase in the console behaviour.
extern "C" PPC_FUNC(WWE2K14_IMP_ToSixty);
extern "C" PPC_FUNC(WWE2K14_FN_ToSixty) {
  REXLOG_INFO("[wwe2k14-rate] restore request at rate {}", PPC_LOAD_U32(kFrameRate));
  WWE2K14_IMP_ToSixty(ctx, base);
}

// Logged only: one line per change of the game's requested present interval (1 = 60 FPS, 2 = 30 FPS), to see whether
// any path still asks for 30 FPS with the rate held at 60.
extern "C" PPC_FUNC(WWE2K14_FN_SetPresentInterval) {
  static uint32_t last = UINT32_MAX;
  const uint32_t interval = ctx.r4.u32;
  if (interval != last) {
    last = interval;
    REXLOG_INFO("[wwe2k14-interval] game set present interval {} (frame rate {})", interval,
                PPC_LOAD_U32(kFrameRate));
  }
  PPC_STORE_U32(ctx.r3.u32 + 14000, interval);
}

// --- Late frames ----------------------------------------------------------------------------------------------------

// After each Present the GPU waits until the frame's flip is processed. When a frame finishes after the vblank it was
// due at, Xbox D3D flips it immediately only if less than `threshold` % of the current vblank period has passed;
// otherwise the flip waits for the next vblank and the GPU stalls with it: a 17 ms frame costs 33 ms, a one-frame drop
// to 30 FPS. Heavy frames (camera cuts, close-up zooms with depth of field) hit exactly this. The host presents whole
// frames with FIFO vsync, so a 100 % threshold cannot tear: the late frame is shown at the next host vblank and the
// game carries on instead of losing a frame. Frames on time are unaffected (the 60 FPS cap stays).
namespace {
struct FlipStats {
  uint64_t window_start_ns = 0;
  uint32_t flips = 0;
  uint32_t late = 0;
  uint32_t late_bucket[4] = {};  // late by < 25 %, < 50 %, < 100 % of a vblank period, >= 1 period
};
}  // namespace

extern "C" PPC_FUNC(WWE2K14_IMP_ChooseFlip);
extern "C" PPC_FUNC(WWE2K14_FN_ChooseFlip) {
  static uint32_t last_threshold = UINT32_MAX;
  static FlipStats stats;
  const uint32_t orig_threshold = ctx.r3.u32 & 0xFF;
  const uint32_t interval = (ctx.r3.u32 >> 8) & 0xF;

  // Only allow immediate late flip if interval <= 1 (60 FPS mode).
  // When interval >= 2 (30 FPS cinematics / entrances), overriding threshold to 100 causes frames
  // that finish after 1 vblank (16.7ms) to flip immediately instead of waiting for the full 2 vblanks (33.3ms),
  // which produces chaotic 38-44 FPS and destroys animation smoothness.
  const bool override_late_flip = (interval <= 1 && LateFlipImmediate());
  if (override_late_flip) {
    ctx.r3.u32 = (ctx.r3.u32 & ~0xFFu) | 100;
  }

  const uint32_t applied_threshold = ctx.r3.u32 & 0xFF;
  if (applied_threshold != last_threshold) {
    last_threshold = applied_threshold;
    REXLOG_INFO("[wwe2k14-flip] present-immediate threshold: game {}% -> applied {}% interval {} ({})",
                orig_threshold, applied_threshold, interval,
                override_late_flip ? "late frames flip immediately" : "kept");
  }
  // Per-second late-frame statistics, logged only for seconds that had late frames: a late frame is a visible hitch
  // even when the second still counts 60 flips. Device fields: +16952 vblank count, +16956 time of the last vblank
  // (mftb, 50 MHz), +16960 vblank of the last flip.
  const uint32_t device = g_d3d_device;
  if (device && interval) {
    const uint32_t vblank_count = PPC_LOAD_U32(device + 16952);
    const uint32_t due = PPC_LOAD_U32(device + 16960) + interval;
    ++stats.flips;
    if (int32_t(vblank_count - due) >= 0) {
      ++stats.late;
      const uint32_t periods_late = vblank_count - due;
      const uint32_t since_vblank_ticks = uint32_t(PPC_QUERY_TIMEBASE()) - PPC_LOAD_U32(device + 16956);
      const uint32_t percent = since_vblank_ticks / (50'000'000 / 60 / 100);
      ++stats.late_bucket[periods_late ? 3 : percent < 25 ? 0 : percent < 50 ? 1 : 2];
    }
    const uint64_t now = SteadyNs();
    if (!stats.window_start_ns) {
      stats.window_start_ns = now;
    } else if (now - stats.window_start_ns >= 1'000'000'000ull) {
      if (stats.late) {
        REXLOG_INFO("[wwe2k14-flip] flips={} late={} (<25%={} <50%={} <100%={} >=1 vblank={}) rate={}",
                    stats.flips, stats.late, stats.late_bucket[0], stats.late_bucket[1], stats.late_bucket[2],
                    stats.late_bucket[3], PPC_LOAD_U32(kFrameRate));
      }
      stats = FlipStats{};
      stats.window_start_ns = now;
    }
  }
  WWE2K14_IMP_ChooseFlip(ctx, base);
}

extern "C" PPC_FUNC(WWE2K14_IMP_GpuInterrupt);
extern "C" PPC_FUNC(WWE2K14_FN_GpuInterrupt) {
  g_d3d_device = ctx.r4.u32;
  WWE2K14_IMP_GpuInterrupt(ctx, base);
}
