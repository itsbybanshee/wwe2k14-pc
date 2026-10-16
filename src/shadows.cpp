// wwe2k14 - Toggleable ring mat plane shadow rendering.
// Controlled by WWE2K14_SHADOWS=0|1 (Launcher: Shadows toggle).
//
// When disabled (WWE2K14_SHADOWS=0):
//   - Skips the ring mat plane shadow composite pass (sub_82BF22C8 in Vanilla / sub_82BF5BF8 in TU2, 'yplaneshadowmap').
//   - The character depth shadow map passes are kept intact so that the shadow map buffer remains cleanly
//     initialized, preventing uninitialized tiled memory patterns (striped/hatched shadow acne) on wrestler models.

#include <atomic>
#include <cstdlib>
#include <cstring>

#include <rex/logging.h>
#include <rex/ppc/context.h>
#include <rex/ppc/memory.h>

#ifdef WWE2K14_TU
#define WWE2K14_FN_PlaneShadowPass sub_82BF5BF8
#define WWE2K14_IMP_PlaneShadowPass __imp__sub_82BF5BF8
#else
#define WWE2K14_FN_PlaneShadowPass sub_82BF22C8
#define WWE2K14_IMP_PlaneShadowPass __imp__sub_82BF22C8
#endif

namespace {

bool ShadowsEnabled() {
  static const bool enabled = [] {
    const char* v = std::getenv("WWE2K14_SHADOWS");
    return !(v != nullptr && std::strcmp(v, "0") == 0);
  }();
  return enabled;
}

void LogShadowsDisabledOnce() {
  static std::atomic_flag logged = ATOMIC_FLAG_INIT;
  if (!logged.test_and_set(std::memory_order_relaxed)) {
    REXLOG_INFO("[wwe2k14-shadows] shadows disabled (WWE2K14_SHADOWS=0): skipping ring plane shadow pass");
  }
}

}  // namespace

extern "C" PPC_FUNC(WWE2K14_IMP_PlaneShadowPass);
extern "C" PPC_FUNC(WWE2K14_FN_PlaneShadowPass) {
  if (!ShadowsEnabled()) {
    LogShadowsDisabledOnce();
    ctx.r3.s64 = 0;
    return;
  }
  WWE2K14_IMP_PlaneShadowPass(ctx, base);
}

