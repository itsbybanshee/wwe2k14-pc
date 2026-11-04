// wwe2k14 - Create-An-Entrance preview: Exit always returns to the menu.
//
// The menu's state machine (sub_833552A8, state at +116) starts a preview in state 8, shows the EXIT prompt and reads
// Start in state 9, and brings the menu back in state 10. States 8 and 10 only move on in a frame where the scene is
// up, the music is not in state 3 (sub_8334BF78) and no fade is running (sub_823122A8). A fade starts about 30 ms after
// the scene comes up and lasts until the entrance ends, but the music leaves state 3 about 200 ms after the scene, so
// the frame the states wait for usually never comes:
//   - stuck in state 8: no EXIT prompt, and Start does nothing
//   - stuck in state 10: Exit restarts the entrance, with no prompt and no menu
// These two checks now ignore the fade. The scene and music checks stay, and every other caller still sees the fade.

#include <cstdint>

#include <rex/ppc/context.h>
#include <rex/ppc/memory.h>

// Vanilla (shared by the Divas Evolution build) first, title update 2 second (tools/port_functions.py).
#ifdef WWE2K14_TU
#define WWE2K14_FN_IsFading sub_82313540
#define WWE2K14_IMP_IsFading __imp__sub_82313540
#define WWE2K14_CAE_STATES_START 0x83358BE0
#define WWE2K14_CAE_STATES_END 0x83358FF0
#else
#define WWE2K14_FN_IsFading sub_823122A8
#define WWE2K14_IMP_IsFading __imp__sub_823122A8
#define WWE2K14_CAE_STATES_START 0x833552A8
#define WWE2K14_CAE_STATES_END 0x833556B8
#endif

extern "C" PPC_FUNC(WWE2K14_IMP_IsFading);
extern "C" PPC_FUNC(WWE2K14_FN_IsFading) {
  const uint32_t caller = uint32_t(ctx.lr);
  if (caller >= WWE2K14_CAE_STATES_START && caller < WWE2K14_CAE_STATES_END) {
    ctx.r3.u64 = 0;
    return;
  }
  WWE2K14_IMP_IsFading(ctx, base);
}
