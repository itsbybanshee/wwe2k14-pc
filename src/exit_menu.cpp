// wwe2k14 - Main Menu EXIT option, menu layout reordering, system string branding, and quit confirmation prompt.
//
// Changes made:
//   1. Main Menu Layout (replace WWE Shop with Exit at bottom, suppress Xbox Live item):
//      - The original Xbox 360 Main Menu order:
//          PLAY
//          30 YEARS OF WRESTLEMANIA
//          WWE UNIVERSE
//          WWE CREATIONS
//          Xbox Live  <--- REMOVED COMPLETELY (suppressed via GetMenuItemDesc & CheckMenuItemVisible)
//          MY WWE
//          OPTIONS
//          WWE SHOP   <--- REPLACED WITH "EXIT" (string 41035 patched in-place to "EXIT")
//      - We hook `GetMenuItemDesc` (`sub_8283BC58` in TU2 / `sub_8283A000` in Vanilla):
//          * When queried for string `41006` ("Xbox Live" / middle Exit), it returns the menu object's invalid
//            sentinel descriptor (`menu_obj + 11416`), which causes the entire menu system (visible item collector,
//            slot layout, cursor navigation, and rendering) to treat the entry as completely non-existent.
//      - We also hook `CheckMenuItemVisible` (`sub_82840368` in TU2 / `sub_8283E710` in Vanilla):
//          * Returns -1 for string `41006` to ensure it is never collected into visible items.
//          * Forces string `41035` ("WWE SHOP" -> "EXIT") visible at the bottom of the Main Menu.
//      - Only string `41035` ("WWE SHOP") in `pac\string.pac` is patched in-place to "EXIT" across all languages.
//        String `41006` is left untouched so no duplicate "EXIT" string exists.
//      - In `SetupMenuSlot` (`sub_82841008` in TU2 / `sub_8283F3B0` in Vanilla), brightness is forced to 1.0f (white).
//
//   2. Native "Are you sure you want to quit the game?" confirmation prompt with Yes (A) and No (B):
//      - When the user selects "EXIT" at the bottom of the menu (`ConfirmCheckAndPopup`):
//        * Instantiates DialogManager singleton if needed (`EnsureDialogManager`).
//        * Allocates guest request memory and configures a type 2 (Yes/No) modal dialog with prompt text.
//        * Sets `player_idx` to `0xFFFFFFFF` (-1) so any controller can respond.
//        * Sets default button to 0 (Yes / A).
//        * Displays the dialog via `ShowDialog` (`sub_82922660` in TU2 / `sub_8291FCA0` in Vanilla).
//      - In `DialogUpdate` (`sub_82923808` in TU2 / `sub_82920E48` in Vanilla):
//        * Runs every frame while the modal dialog is up.
//        * Right after `IMP_DialogUpdate` processes controller inputs, we poll `PollDialogResult` (`sub_82923720` in TU2 /
//          `sub_82920D60` in Vanilla).
//        * Returns -2 while still waiting for user input.
//        * When the user presses **Yes (A)** (status == 1):
//            Immediately dismisses the dialog via `ResetDialog` and calls `RequestCleanClose()`, cleanly restoring monitor
//            refresh rate and terminating the process on the exact frame button A is pressed.
//        * When the user presses **No (B)** (status == 0):
//            Dismisses the dialog via `ResetDialog` and returns the player to the Main Menu on "EXIT".
//      - `ResetDialog` (`sub_82922008` in TU2 / `sub_8291F630` in Vanilla) is also hooked as a safety fallback.
//
//   3. System name string branding ("Xbox 360 console" -> "PC"):
//      - Decompressed string tables in `ZlibDecompress` (`sub_82C44F68` in TU2 / `sub_82C41738` in Vanilla) are scanned
//        and system console name strings in English, French, Spanish, German, Italian, and Japanese are replaced with "PC".

#include <atomic>
#include <cstdint>
#include <cstring>
#include <string_view>

#include "mouse_input.h"

#include <rex/logging.h>
#include <rex/memory.h>
#include <rex/ppc/context.h>
#include <rex/ppc/function.h>
#include <rex/ppc/memory.h>
#include <rex/runtime.h>
#include <rex/ui/window.h>
#include <rex/ui/windowed_app_context.h>

#ifdef WWE2K14_TU
#define WWE2K14_FN_ZlibDecompress sub_82C44F68
#define WWE2K14_IMP_ZlibDecompress __imp__sub_82C44F68
#define WWE2K14_FN_GetMenuItemDesc sub_8283BC58
#define WWE2K14_IMP_GetMenuItemDesc __imp__sub_8283BC58
#define WWE2K14_FN_CheckMenuItemVisible sub_82840368
#define WWE2K14_IMP_CheckMenuItemVisible __imp__sub_82840368
#define WWE2K14_FN_SetupMenuSlot sub_82841008
#define WWE2K14_IMP_SetupMenuSlot __imp__sub_82841008
#define WWE2K14_FN_ConfirmCheckAndPopup sub_8283E948
#define WWE2K14_IMP_ConfirmCheckAndPopup __imp__sub_8283E948
#define WWE2K14_FN_InitDialogReq sub_82921FB0
#define WWE2K14_FN_ShowDialog sub_82922660
#define WWE2K14_FN_EnsureDialogManager sub_82924130
#define WWE2K14_FN_PollDialogResult sub_82923720
#define WWE2K14_FN_DialogUpdate sub_82923808
#define WWE2K14_IMP_DialogUpdate __imp__sub_82923808
#define WWE2K14_FN_GetDialogResult sub_82923220
#define WWE2K14_FN_ResetDialog sub_82922008
#define WWE2K14_IMP_ResetDialog __imp__sub_82922008
#define WWE2K14_ADDR_DialogMgr 0x8392950Cu
#else
#define WWE2K14_FN_ZlibDecompress sub_82C41738
#define WWE2K14_IMP_ZlibDecompress __imp__sub_82C41738
#define WWE2K14_FN_GetMenuItemDesc sub_8283A000
#define WWE2K14_IMP_GetMenuItemDesc __imp__sub_8283A000
#define WWE2K14_FN_CheckMenuItemVisible sub_8283E710
#define WWE2K14_IMP_CheckMenuItemVisible __imp__sub_8283E710
#define WWE2K14_FN_SetupMenuSlot sub_8283F3B0
#define WWE2K14_IMP_SetupMenuSlot __imp__sub_8283F3B0
#define WWE2K14_FN_ConfirmCheckAndPopup sub_8283CCF0
#define WWE2K14_IMP_ConfirmCheckAndPopup __imp__sub_8283CCF0
#define WWE2K14_FN_InitDialogReq sub_8291F5D8
#define WWE2K14_FN_ShowDialog sub_8291FCA0
#define WWE2K14_FN_EnsureDialogManager sub_82921770
#define WWE2K14_FN_PollDialogResult sub_82920D60
#define WWE2K14_FN_DialogUpdate sub_82920E48
#define WWE2K14_IMP_DialogUpdate __imp__sub_82920E48
#define WWE2K14_FN_GetDialogResult sub_82920860
#define WWE2K14_FN_ResetDialog sub_8291F630
#define WWE2K14_IMP_ResetDialog __imp__sub_8291F630
#define WWE2K14_ADDR_DialogMgr 0x839294BCu
#endif

extern "C" PPC_FUNC(WWE2K14_IMP_GetMenuItemDesc);
extern "C" PPC_FUNC(WWE2K14_FN_InitDialogReq);
extern "C" PPC_FUNC(WWE2K14_FN_ShowDialog);
extern "C" PPC_FUNC(WWE2K14_FN_EnsureDialogManager);
extern "C" PPC_FUNC(WWE2K14_FN_PollDialogResult);
extern "C" PPC_FUNC(WWE2K14_FN_GetDialogResult);
extern "C" PPC_FUNC(WWE2K14_IMP_ResetDialog);

namespace {

constexpr uint32_t kMainMenuXboxLiveStringId = 41006;  // 0xA02E ("Xbox Live" on the top-level Main Menu, now eliminated)
constexpr uint32_t kWweShopStringId = 41035;           // 0xA04B ("WWE SHOP" at the bottom of Main Menu, now "EXIT")
constexpr uint32_t kFloatOneBits = 0x3F800000u;        // 1.0f in IEEE-754 big-endian / host register form
constexpr char kExitConfirmationPrompt[] = "Are you sure you want to quit the game?";

std::atomic<bool> g_exit_dialog_pending{false};

struct SubstringReplacement {
  std::string_view from;
  std::string_view to;
};

// Every replacement is strictly shorter than `from`, so it can be applied in-place inside each string's buffer.
constexpr SubstringReplacement kSystemNameReplacements[] = {
    {"Xbox 360 console", "PC"},               // English ("Please don't turn off your Xbox 360 console.")
    {"Console Xbox 360", "PC"},               // French ("Veuillez ne pas éteindre votre Console Xbox 360.")
    {"la Consola Xbox 360", "el PC"},         // Spanish ("No apagues la Consola Xbox 360.")
    {"una Consola Xbox 360", "un PC"},        // Spanish ("El juego debe jugarse en una Consola Xbox 360.")
    {"Consola Xbox 360", "PC"},               // Spanish ("No apagues tu Consola Xbox 360.")
    {"deine Xbox 360 Konsole", "deinen PC"},  // German ("Bitte schalte deine Xbox 360 Konsole nicht aus.")
    {"der Xbox 360 Konsole", "dem PC"},       // German ("Spiel muss auf der Xbox 360 Konsole gespielt werden.")
    {"Xbox 360 Konsole", "PC"},               // German ("Xbox 360 Konsole bitte nicht ausschalten.")
    {"sulla console Xbox 360", "sul PC"},     // Italian ("Il gioco deve essere giocato sulla console Xbox 360.")
    {"la console Xbox 360", "il PC"},         // Italian ("Non spegnere la console Xbox 360.")
    {"Xbox 360 \xE6\x9C\xAC\xE4\xBD\x93", "PC"},  // Japanese ("Xbox 360 本体の電源を切らないでください。")
};

uint32_t LoadLe32(const uint8_t* p) {
  return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}

void StoreLe32(uint8_t* p, uint32_t v) {
  p[0] = uint8_t(v & 0xFF);
  p[1] = uint8_t((v >> 8) & 0xFF);
  p[2] = uint8_t((v >> 16) & 0xFF);
  p[3] = uint8_t((v >> 24) & 0xFF);
}

bool ReplaceSystemNamesInPlace(uint8_t* slot, uint32_t capacity, uint32_t& out_length) {
  size_t len = 0;
  while (len < capacity && slot[len] != 0) {
    ++len;
  }
  bool modified = false;
  for (const auto& [from, to] : kSystemNameReplacements) {
    size_t search_pos = 0;
    while (search_pos < len) {
      const std::string_view view(reinterpret_cast<const char*>(slot), len);
      const size_t pos = view.find(from, search_pos);
      if (pos == std::string_view::npos) {
        break;
      }
      const size_t tail_len = len - (pos + from.size());
      std::memcpy(slot + pos, to.data(), to.size());
      std::memmove(slot + pos + to.size(), slot + pos + from.size(), tail_len);
      len -= (from.size() - to.size());
      search_pos = pos + to.size();
      modified = true;
    }
  }
  if (modified) {
    std::memset(slot + len, 0, capacity - len);
    out_length = uint32_t(len + 1);
  }
  return modified;
}

// Queries `WWE2K14_IMP_GetMenuItemDesc(menu_obj, group_id, item_id)` without disturbing the caller's PPCContext
// and returns the item's raw String ID (+24), or 0 if the descriptor is the invalid sentinel.
uint32_t GetMenuItemStringId(PPCContext& ctx, uint8_t* base, uint32_t menu_obj, uint32_t group_id, uint32_t item_id) {
  const PPCContext saved = ctx;
  ctx.r3.u64 = menu_obj;
  ctx.r4.s64 = int32_t(group_id);
  ctx.r5.s64 = int32_t(item_id);
  WWE2K14_IMP_GetMenuItemDesc(ctx, base);
  const uint32_t desc = ctx.r3.u32;
  ctx = saved;
  if (!desc || PPC_LOAD_U32(desc) == 0xFFFFFFFFu) {
    return 0;
  }
  return PPC_LOAD_U32(desc + 24);
}

struct ExitDialogGuestStorage {
  uint32_t req_ea = 0;
  uint32_t text_ea = 0;
};

// Allocates permanent guest virtual memory in the system heap for the dialog request and prompt string.
ExitDialogGuestStorage GetExitDialogGuestStorage() {
  static ExitDialogGuestStorage s_storage{};
  if (!s_storage.req_ea) {
    auto* rt = rex::Runtime::instance();
    if (rt && rt->memory()) {
      s_storage.req_ea = rt->memory()->SystemHeapAlloc(64, 16);
      s_storage.text_ea = rt->memory()->SystemHeapAlloc(sizeof(kExitConfirmationPrompt), 16);
      if (s_storage.text_ea && rt->virtual_membase()) {
        std::memcpy(rt->virtual_membase() + s_storage.text_ea, kExitConfirmationPrompt,
                    sizeof(kExitConfirmationPrompt));
      }
      REXLOG_INFO("[wwe2k14-menu] Exit dialog guest storage allocated: req=0x{:08X}, text=0x{:08X}",
                  s_storage.req_ea, s_storage.text_ea);
    }
  }
  return s_storage;
}

void RequestCleanClose() {
  REXLOG_INFO("[wwe2k14-menu] EXIT confirmed from Main Menu; requesting window close");
  if (auto* rt = rex::Runtime::instance(); rt && rt->app_context()) {
    rt->app_context()->CallInUIThreadDeferred([] {
      if (auto* runtime = rex::Runtime::instance(); runtime && runtime->display_window()) {
        runtime->display_window()->RequestClose();
      }
    });
  }
}

}  // namespace

// --- 1. String table patch ("WWE SHOP" -> "EXIT", "Xbox 360 console" -> "PC") --------------------------------------

extern "C" PPC_FUNC(WWE2K14_IMP_ZlibDecompress);
extern "C" PPC_FUNC(WWE2K14_FN_ZlibDecompress) {
  const uint32_t dst = ctx.r3.u32;
  const uint32_t dst_cap = ctx.r4.u32;
  WWE2K14_IMP_ZlibDecompress(ctx, base);

  if (ctx.r3.u32 == 0 || dst == 0 || dst_cap < 20) {
    return;
  }

  uint8_t* const p = base + dst;
  if (LoadLe32(p) != 0) {
    return;
  }
  const uint32_t count = LoadLe32(p + 4);
  if (count < 1 || count > 50000) {
    return;
  }
  const uint64_t table_end = 8ull + uint64_t(count) * 12ull;
  if (table_end > dst_cap || LoadLe32(p + 8) != uint32_t(table_end)) {
    return;
  }

  uint32_t pc_patched_count = 0;
  for (uint32_t i = 0; i < count; ++i) {
    uint8_t* const entry = p + 8 + i * 12;
    const uint32_t offset = LoadLe32(entry + 0);
    const uint32_t length = LoadLe32(entry + 4);
    const uint32_t id = LoadLe32(entry + 8);
    if (offset < table_end || uint64_t(offset) + length > dst_cap || length == 0) {
      continue;
    }

    // Only patch WWE SHOP (41035) to "EXIT". String 41006 (Xbox Live) is not patched so no middle "EXIT" string exists.
    if (id == kWweShopStringId && length >= 5) {
      std::memcpy(p + offset, "EXIT", 5);
      if (length > 5) {
        std::memset(p + offset + 5, 0, length - 5);
      }
      StoreLe32(entry + 4, 5);
      REXLOG_INFO("[wwe2k14-menu] patched Main Menu string {} (\"WWE SHOP\" -> \"EXIT\")", id);
      continue;
    }

    uint32_t new_length = length;
    if (ReplaceSystemNamesInPlace(p + offset, length, new_length)) {
      StoreLe32(entry + 4, new_length);
      ++pc_patched_count;
    }
  }

  if (pc_patched_count > 0) {
    REXLOG_INFO("[wwe2k14-menu] patched {} \"Xbox 360\" system string(s) to \"PC\" (table count={})",
                pc_patched_count, count);
  }
}

// --- 2. Menu descriptor hook: completely suppress Xbox Live / middle entry at descriptor lookup ---------------------

extern "C" PPC_FUNC(WWE2K14_FN_GetMenuItemDesc) {
  const uint32_t menu_obj = ctx.r3.u32;
  WWE2K14_IMP_GetMenuItemDesc(ctx, base);
  const uint32_t desc = ctx.r3.u32;
  if (desc && PPC_LOAD_U32(desc) != 0xFFFFFFFFu) {
    const uint32_t str_id = PPC_LOAD_U32(desc + 24);
    if (str_id == kMainMenuXboxLiveStringId) {
      // Suppress the old Xbox Live item: return the menu object's invalid sentinel descriptor (+11416).
      // This causes the entire menu engine to omit this slot completely (no button, no cursor stop, no gap).
      ctx.r3.u64 = menu_obj + 11416u;
      return;
    }
  }
}

// --- 3. Menu slot visibility hook: hide middle Xbox Live slot, force bottom EXIT visible ---------------------------

extern "C" PPC_FUNC(WWE2K14_IMP_CheckMenuItemVisible);
extern "C" PPC_FUNC(WWE2K14_FN_CheckMenuItemVisible) {
  const uint32_t menu_obj = ctx.r3.u32;
  const uint32_t group_id = ctx.r4.u32;
  const uint32_t item_id = ctx.r5.u32;

  const uint32_t str_id = GetMenuItemStringId(ctx, base, menu_obj, group_id, item_id);
  if (str_id == kMainMenuXboxLiveStringId) {
    // Hide the old middle Xbox Live slot completely from the Main Menu
    ctx.r3.s64 = -1;
    return;
  }
  if (str_id == kWweShopStringId) {
    // Force WWE Shop (patched to EXIT) visible at the bottom of the Main Menu
    ctx.r3.u64 = kWweShopStringId;
    return;
  }

  WWE2K14_IMP_CheckMenuItemVisible(ctx, base);
}

// --- 4. Menu slot brightness override (render bottom "EXIT" at full white brightness) -------------------------------

extern "C" PPC_FUNC(WWE2K14_IMP_SetupMenuSlot);
extern "C" PPC_FUNC(WWE2K14_FN_SetupMenuSlot) {
  const uint32_t menu_obj = ctx.r3.u32;
  const uint32_t col_idx = ctx.r4.u32;
  const uint32_t group_id = ctx.r6.u32;
  const uint32_t item_slot = ctx.r7.u32;

  WWE2K14_IMP_SetupMenuSlot(ctx, base);

  const uint32_t str_id = GetMenuItemStringId(ctx, base, menu_obj, group_id, item_slot);
  wwe2k14::NoteMenuObject(menu_obj);
  if (str_id == kWweShopStringId) {
    const uint32_t page = PPC_LOAD_U32(menu_obj + 268);
    const uint32_t col_base = menu_obj + (page * 9u + col_idx) * 588u + 800u;
    PPC_STORE_U32(col_base + (item_slot + 87u) * 4u, kFloatOneBits);
  }
}

// --- 5. Confirm press handler (prompt "Are you sure you want to quit the game?" with Yes/No [A/B]) -------------------

extern "C" PPC_FUNC(WWE2K14_IMP_ConfirmCheckAndPopup);
extern "C" PPC_FUNC(WWE2K14_FN_ConfirmCheckAndPopup) {
  const uint32_t menu_obj = ctx.r3.u32;
  const uint32_t group_id = ctx.r4.u32;
  const uint32_t item_id = ctx.r5.u32;

  const uint32_t str_id = GetMenuItemStringId(ctx, base, menu_obj, group_id, item_id);
  if (str_id == kWweShopStringId || str_id == kMainMenuXboxLiveStringId) {
    if (g_exit_dialog_pending.load()) {
      ctx.r3.u64 = 1;
      return;
    }

    const PPCContext saved = ctx;

    // 1. Ensure DialogManager singleton exists (plays chime 64 and constructs singleton if needed)
    ctx.r3.s64 = 0;
    ctx.r4.s64 = 64;
    WWE2K14_FN_EnsureDialogManager(ctx, base);

    const uint32_t dialog_mgr = PPC_LOAD_U32(WWE2K14_ADDR_DialogMgr);
    if (!dialog_mgr) {
      REXLOG_WARN("[wwe2k14-menu] DialogManager not initialized; falling back to direct close");
      RequestCleanClose();
      ctx = saved;
      ctx.r3.u64 = 1;
      return;
    }

    const auto storage = GetExitDialogGuestStorage();
    const uint32_t req_ea = storage.req_ea ? storage.req_ea : ((ctx.r1.u32 - 128) & ~15u);
    const uint32_t text_ea = storage.text_ea ? storage.text_ea : (req_ea + 64);
    if (storage.text_ea && base) {
      std::memcpy(base + storage.text_ea, kExitConfirmationPrompt, sizeof(kExitConfirmationPrompt));
    } else if (!storage.text_ea) {
      std::memcpy(base + text_ea, kExitConfirmationPrompt, sizeof(kExitConfirmationPrompt));
    }

    // 2. Initialize 56-byte dialog request struct
    ctx.r3.u64 = req_ea;
    WWE2K14_FN_InitDialogReq(ctx, base);

    // 3. Configure dialog request:
    PPC_STORE_U32(req_ea + 0, 2);                      // dialog_type: 2 = Yes / No confirmation dialog
    PPC_STORE_U32(req_ea + 4, 50398);                  // message_string_id: 50398 ("Are you sure you want to quit?")
    PPC_STORE_U32(req_ea + 8, text_ea);                // custom message string: "Are you sure you want to quit the game?"
    PPC_STORE_U32(req_ea + 16, 0);                     // sub-type
    PPC_STORE_U32(req_ea + 20, 0xFFFFFFFFu);           // player_idx: -1 allows any controller to interact
    PPC_STORE_U32(req_ea + 28, 0);                     // default button: 0 (Yes / A)

    // 4. Open native modal dialog
    ctx.r3.u64 = dialog_mgr;
    ctx.r4.u64 = req_ea;
    WWE2K14_FN_ShowDialog(ctx, base);

    ctx = saved;
    g_exit_dialog_pending.store(true);
    REXLOG_INFO("[wwe2k14-menu] Opened exit confirmation dialog (\"Are you sure you want to quit the game?\")");

    ctx.r3.u64 = 1;
    return;
  }

  WWE2K14_IMP_ConfirmCheckAndPopup(ctx, base);
}

// --- 6. Dialog tick hook (polls user response every frame; instantly exits on Yes [A] without waiting for B) --------

extern "C" PPC_FUNC(WWE2K14_IMP_DialogUpdate);
extern "C" PPC_FUNC(WWE2K14_FN_DialogUpdate) {
  WWE2K14_IMP_DialogUpdate(ctx, base);

  if (g_exit_dialog_pending.load()) {
    PPCContext poll_ctx = ctx;
    WWE2K14_FN_PollDialogResult(poll_ctx, base);
    const int32_t status = poll_ctx.r3.s32;

    if (status != -2) {
      g_exit_dialog_pending.store(false);
      REXLOG_INFO("[wwe2k14-menu] Exit dialog status polled in DialogUpdate: {}", status);

      const uint32_t dialog_mgr = PPC_LOAD_U32(WWE2K14_ADDR_DialogMgr);
      if (dialog_mgr) {
        PPCContext reset_ctx = ctx;
        reset_ctx.r3.u64 = dialog_mgr;
        WWE2K14_IMP_ResetDialog(reset_ctx, base);
      }

      if (status == 1) {
        REXLOG_INFO("[wwe2k14-menu] Exit confirmed with Yes (A); closing game immediately");
        RequestCleanClose();
      } else {
        REXLOG_INFO("[wwe2k14-menu] Exit cancelled with No (B); dismissed dialog and returned to Main Menu");
      }
    }
  }
}

// --- 7. Dialog dismissal fallback hook (catches cancel / back button [B] dismissals) --------------------------------

extern "C" PPC_FUNC(WWE2K14_IMP_ResetDialog);
extern "C" PPC_FUNC(WWE2K14_FN_ResetDialog) {
  const uint32_t dialog_mgr = ctx.r3.u32;

  if (g_exit_dialog_pending.exchange(false)) {
    PPCContext check_ctx = ctx;
    check_ctx.r3.u64 = dialog_mgr;
    WWE2K14_FN_GetDialogResult(check_ctx, base);
    const uint32_t result = check_ctx.r3.u32;
    const uint32_t raw_sel = dialog_mgr ? PPC_LOAD_U32(dialog_mgr + 64) : 0xFFFFFFFFu;

    REXLOG_INFO("[wwe2k14-menu] Exit confirmation dialog dismissed via ResetDialog: result={}, raw_sel=0x{:X}",
                result, raw_sel);

    if (result == 1 || raw_sel == 0) {
      REXLOG_INFO("[wwe2k14-menu] Exit confirmed with Yes (A); closing game");
      RequestCleanClose();
    } else {
      REXLOG_INFO("[wwe2k14-menu] Exit cancelled with No (B); remaining in Main Menu");
    }
  }

  WWE2K14_IMP_ResetDialog(ctx, base);
}
