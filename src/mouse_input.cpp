// wwe2k14 - Mouse in menus.
//
// Menu screens (main menu, mode lists, their sub-menus, options): pointing at an item selects it, clicking it chooses
// it. An item in the sub-menu column on the right is reached by entering that column first, like a controller would;
// the panel's arrows scroll (up / down), enter (right) and go back (left). Wrestler select: pointing at a portrait
// moves the cursor there, clicking it picks that wrestler (claiming the 1P side first if needed). Elsewhere a left
// click presses A. Right click presses B (back), the wheel steps the selection up / down, the middle button presses
// Start.
//
// The game reads its controller through `XInputGetState` (`sub_82C92F98` in TU2 / `sub_82C8F6A8` in Vanilla). The hook
// adds the buttons to user 0's state, so the mouse works alongside a controller or the keyboard and the game's own
// navigation (sounds, animations, scrolling) runs as usual: selecting an item is a series of D-pad presses.
//
// Menu layout (found in memory dumps): the menu object reported by the SetupMenuSlot hook points at three row lists
// (+11664, +11668, +11672): the focused column, the sub-menu of its selected item, and the parent column (faded out to
// the left, not hit-tested). A list is {+0 rows, +12..+24 arrow node ids (up, down, right, left), +52 id of the
// panel's visible area, +64 UI scene handle}; it lays out at most 10 rows of 5 node ids, element 1 being the row's
// highlight bar and element 2 a part only switched on for the selected row of the focused column. Rows outside the
// visible area are laid out but clipped. The scene handle +12 is the scene instance: instance +12 = node pointer
// array, instance +4 -> +12 = node count. A node is switched on when +0x2C0 == 1 and drawn when +0x2C4 == 1 as well (a
// hidden parent clears it); its world matrix is at +0x2D0 (translation at +0x300 = the node's centre in 1280x720 UI
// coordinates) and its size at +0x1C0.
//
// Clicks only count while the game window is in the foreground and the cursor is inside its client area. Wheel notches
// come from a low-level mouse hook (the wheel cannot be polled).

#include "mouse_input.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "test_input.h"

#include <rex/logging.h>
#include <rex/ppc/context.h>
#include <rex/ppc/function.h>
#include <rex/ppc/memory.h>

#if defined(_WIN32)
#include <windows.h>
#endif

#ifdef WWE2K14_TU
#define WWE2K14_FN_XInputGetState sub_82C92F98
#define WWE2K14_IMP_XInputGetState __imp__sub_82C92F98
#define WWE2K14_FN_SelectGrid sub_8282E218
#define WWE2K14_IMP_SelectGrid __imp__sub_8282E218
#define WWE2K14_FN_QueueQuad sub_82BC6CB8
#define WWE2K14_IMP_QueueQuad __imp__sub_82BC6CB8
#define WWE2K14_FN_SelectCursor sub_8285AC40
#define WWE2K14_IMP_SelectCursor __imp__sub_8285AC40
constexpr uint32_t kUiResourceTable = 0x83929500;
constexpr uint32_t kSelectResourceSlot = 0x83929528;
#else
#define WWE2K14_FN_XInputGetState sub_82C8F6A8
#define WWE2K14_IMP_XInputGetState __imp__sub_82C8F6A8
#define WWE2K14_FN_SelectGrid sub_8282C648
#define WWE2K14_IMP_SelectGrid __imp__sub_8282C648
#define WWE2K14_FN_QueueQuad sub_82BC3518
#define WWE2K14_IMP_QueueQuad __imp__sub_82BC3518
#define WWE2K14_FN_SelectCursor sub_828584F0
#define WWE2K14_IMP_SelectCursor __imp__sub_828584F0
constexpr uint32_t kUiResourceTable = 0x839294B0;
constexpr uint32_t kSelectResourceSlot = 0x839294D8;
#endif

namespace {

using Clock = std::chrono::steady_clock;

constexpr uint16_t kDpadUp = 0x0001;
constexpr uint16_t kDpadDown = 0x0002;
constexpr uint16_t kDpadLeft = 0x0004;
constexpr uint16_t kDpadRight = 0x0008;
constexpr uint16_t kStart = 0x0010;
constexpr uint16_t kX = 0x4000;
constexpr uint16_t kY = 0x8000;
constexpr uint16_t kBack = 0x0020;
constexpr uint16_t kLB = 0x0100;
constexpr uint16_t kRB = 0x0200;
// The triggers are analog values, not buttons: these bits above the 16 button bits stand for a fully pressed trigger.
constexpr uint32_t kLT = 0x10000;
constexpr uint32_t kRT = 0x20000;
constexpr uint16_t kA = 0x1000;
constexpr uint16_t kB = 0x2000;

constexpr uint32_t kMenuLists[3] = {11664, 11668, 11672};
constexpr int kMaxRows = 10;  // the menu lays out at most 10 rows per column

std::atomic<uint32_t> g_menu{0};
std::atomic<int64_t> g_menu_seen{0};  // Clock ticks of the last SetupMenuSlot call
std::atomic<int> g_wheel{0};          // pending wheel notches, positive = up

int64_t Now() { return Clock::now().time_since_epoch().count(); }
int64_t Ms(int ms) { return std::chrono::duration_cast<Clock::duration>(std::chrono::milliseconds(ms)).count(); }

// ---- Reading the menu -------------------------------------------------------------------------------------------

struct Rect {
  bool valid = false;
  float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  bool Contains(float x, float y) const { return valid && x >= x0 && x <= x1 && y >= y0 && y <= y1; }
  float CenterY() const { return (y0 + y1) / 2; }
};

// Arrows drawn on a column's panel; ids at list +12 / +16 / +20 / +24.
enum Arrow { kArrowUp, kArrowDown, kArrowRight, kArrowLeft, kArrowCount };

struct List {
  int rows = 0;                         // rows laid out (the first `rows` items from the scroll position)
  std::array<Rect, kMaxRows> row{};     // highlight bar of each row; invalid when scrolled out of the panel
  std::array<Rect, kArrowCount> arrow{};
  int selected = -1;                    // selected row when this column has focus
};

bool GuestAddress(uint32_t a) { return (a >= 0x40000000u && a < 0x80000000u) || (a >= 0xA0000000u && a < 0xC0000000u); }

float LoadF32(uint8_t* base, uint32_t a) {
  const uint32_t v = PPC_LOAD_U32(a);
  float f;
  std::memcpy(&f, &v, 4);
  return f;
}

// +0x2C0: the node is switched on; +0x2C4: it is actually drawn (all its parents are too).
bool Drawn(uint8_t* base, uint32_t node) {
  return node && PPC_LOAD_U32(node + 0x2C0) == 1 && PPC_LOAD_U32(node + 0x2C4) == 1;
}

// The node's rectangle in 1280x720 UI coordinates: world translation (centre) and size times the world scale.
Rect NodeRect(uint8_t* base, uint32_t node) {
  Rect r;
  if (!Drawn(base, node)) return r;
  const float cx = LoadF32(base, node + 0x300), cy = LoadF32(base, node + 0x304);
  const float sx = std::fabs(LoadF32(base, node + 0x2D0)), sy = std::fabs(LoadF32(base, node + 0x2E4));
  const float w = LoadF32(base, node + 0x1C0) * sx, h = LoadF32(base, node + 0x1C4) * sy;
  if (!std::isfinite(cx) || !std::isfinite(cy) || !(w > 1 && w < 2000) || !(h > 1 && h < 1000)) return r;
  r = {true, cx - w / 2, cy - h / 2, cx + w / 2, cy + h / 2};
  return r;
}

bool ReadList(uint8_t* base, uint32_t list, List& out) {
  out = List{};
  if (!GuestAddress(list)) return false;
  const uint32_t rows = PPC_LOAD_U32(list + 0);
  const uint32_t handle = PPC_LOAD_U32(list + 64);
  if (!GuestAddress(rows) || !GuestAddress(handle)) return false;
  const uint32_t inst = PPC_LOAD_U32(handle + 12);
  if (!GuestAddress(inst)) return false;
  const uint32_t nodes = PPC_LOAD_U32(inst + 12);
  const uint32_t res = PPC_LOAD_U32(inst + 4);
  if (!GuestAddress(nodes) || !GuestAddress(res)) return false;
  const uint32_t node_count = PPC_LOAD_U32(res + 12);
  if (node_count == 0 || node_count > 4096) return false;
  auto node = [&](uint32_t id) -> uint32_t {
    if (id >= node_count) return 0;
    const uint32_t n = PPC_LOAD_U32(nodes + id * 4);
    return GuestAddress(n) ? n : 0;
  };
  for (int a = 0; a < kArrowCount; ++a) out.arrow[a] = NodeRect(base, node(PPC_LOAD_U32(list + 12 + a * 4)));
  // Rows below or above the panel are laid out too but clipped by its visible area (id at list +52).
  const Rect area = NodeRect(base, node(PPC_LOAD_U32(list + 52)));
  for (int i = 0; i < kMaxRows; ++i) {
    const uint32_t bar = node(PPC_LOAD_U32(rows + i * 20 + 4));
    if (!Drawn(base, bar)) break;  // laid-out rows come first
    Rect r = NodeRect(base, bar);
    if (area.valid && (r.CenterY() < area.y0 || r.CenterY() > area.y1)) r.valid = false;
    out.row[out.rows] = r;
    const uint32_t mark = node(PPC_LOAD_U32(rows + i * 20 + 8));
    if (mark && PPC_LOAD_U32(mark + 0x2C0) == 1) out.selected = out.rows;
    ++out.rows;
  }
  return out.rows > 0;
}

// ---- Cursor and buttons ---------------------------------------------------------------------------------------------

struct Cursor {
  bool valid = false;  // over the game window
  float x = 0, y = 0;  // 1280x720 UI coordinates
  bool left = false, right = false, middle = false;
};

#if defined(_WIN32)
bool GameWindow(HWND* out) {
  HWND w = GetForegroundWindow();
  DWORD pid = 0;
  if (!w || !GetWindowThreadProcessId(w, &pid) || pid != GetCurrentProcessId()) return false;
  *out = w;
  return true;
}

bool Down(int vk) { return (GetAsyncKeyState(vk) & 0x8000) != 0; }

Cursor RealCursor() {
  Cursor c;
  HWND w;
  POINT p;
  RECT r;
  if (!GameWindow(&w) || !GetCursorPos(&p) || !ScreenToClient(w, &p) || !GetClientRect(w, &r)) return c;
  if (p.x < r.left || p.x >= r.right || p.y < r.top || p.y >= r.bottom) return c;
  // The 1280x720 image is scaled to fit the window, centred.
  const float cw = float(r.right - r.left), ch = float(r.bottom - r.top);
  const float s = std::min(cw / 1280.0f, ch / 720.0f);
  c.valid = true;
  c.x = (p.x - (cw - 1280.0f * s) / 2) / s;
  c.y = (p.y - (ch - 720.0f * s) / 2) / s;
  // GetAsyncKeyState reports the physical buttons; follow the Windows "swap buttons" setting for left-handed use.
  const bool swapped = GetSystemMetrics(SM_SWAPBUTTON) != 0;
  c.left = Down(swapped ? VK_RBUTTON : VK_LBUTTON);
  c.right = Down(swapped ? VK_LBUTTON : VK_RBUTTON);
  c.middle = Down(VK_MBUTTON);
  return c;
}

LRESULT CALLBACK MouseHook(int code, WPARAM wparam, LPARAM lparam) {
  if (code == HC_ACTION && wparam == WM_MOUSEWHEEL && RealCursor().valid) {
    const auto* info = reinterpret_cast<const MSLLHOOKSTRUCT*>(lparam);
    const int delta = static_cast<short>(HIWORD(info->mouseData));
    // Smooth-scrolling wheels and touchpads report a notch (WHEEL_DELTA) in several smaller steps: add them up, one
    // menu step per notch. A few pending steps at most, so a fast spin does not keep scrolling afterwards; turning the
    // wheel the other way drops the steps still pending.
    static int partial = 0;
    if (!delta) return CallNextHookEx(nullptr, code, wparam, lparam);
    if ((delta > 0) != (partial > 0)) partial = 0;
    partial += delta;
    while (std::abs(partial) >= WHEEL_DELTA) {
      const int step = partial > 0 ? 1 : -1;
      partial -= step * WHEEL_DELTA;
      int w = g_wheel.load();
      if ((w > 0) != (step > 0)) w = 0;
      g_wheel.store(std::clamp(w + step, -3, 3));
    }
  }
  return CallNextHookEx(nullptr, code, wparam, lparam);
}

void StartWheelHook() {
  static std::once_flag once;
  std::call_once(once, [] {
    std::thread([] {
      if (!SetWindowsHookExW(WH_MOUSE_LL, MouseHook, GetModuleHandleW(nullptr), 0)) {
        REXLOG_WARN("[wwe2k14-mouse] wheel hook failed ({})", GetLastError());
        return;
      }
      MSG msg;
      while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
      }
    }).detach();
  });
}
#endif

// ---- Driving the menu -----------------------------------------------------------------------------------------------

// One button press at a time: held for a few frames, then released, so the menu sees each press once.
struct Pulse {
  uint32_t button = 0;
  int64_t until = 0;
  bool releasing = false;
};

// The focused column is always list 0 and the sub-menu of its selected item list 1. List 2 holds the parent column,
// which is faded out to the left, so it is not hit-tested (right click goes back to it).
constexpr int kFocus = 0;
constexpr int kChild = 1;

struct Target {
  bool active = false;
  int list = -1, row = -1;
  bool choose = false;    // click: choose the item once it is selected
  int64_t started = 0;
  int child_rows = -1;    // after entering a sub-menu: its row count, to recognise it once it has moved into list 0
  int64_t entered = 0;    // when A was pressed to enter the sub-menu
};

struct State {
  Pulse pulse;
  Target target;
  bool was_left = false;
  int hover_list = -1, hover_row = -1;
};

State g;

bool StartPulse(uint32_t button, int64_t now) {
  if (g.pulse.button || g.pulse.releasing) return false;
  g.pulse = {button, now + Ms(40), false};
  return true;
}

uint32_t PulseButtons(int64_t now) {
  if (g.pulse.button && !g.pulse.releasing) {
    if (now < g.pulse.until) return g.pulse.button;
    g.pulse.releasing = true;
    g.pulse.until = now + Ms(45);
    return 0;
  }
  if (g.pulse.releasing && now >= g.pulse.until) g.pulse = {};
  return 0;
}

bool Busy() { return g.pulse.button || g.pulse.releasing; }

void HitTest(const std::array<List, 3>& lists, const Cursor& c, int* hit_list, int* hit_row, int* hit_arrow) {
  *hit_list = *hit_row = *hit_arrow = -1;
  if (!c.valid) return;
  for (int a = 0; a < kArrowCount; ++a)
    if (lists[kFocus].arrow[a].Contains(c.x, c.y)) {
      *hit_arrow = a;
      return;
    }
  float best = 1e9f;
  for (int l : {kFocus, kChild}) {
    for (int r = 0; r < lists[l].rows; ++r) {
      const Rect& row = lists[l].row[r];
      if (!row.Contains(c.x, c.y)) continue;
      const float d = std::fabs(c.y - row.CenterY());  // rows overlap slightly: take the nearest
      if (d < best) best = d, *hit_list = l, *hit_row = r;
    }
  }
}

// Returns true when a menu of this kind is on screen (it has rows to point at).
bool MenuMouse(uint8_t* base, const Cursor& c, bool click, int64_t now) {
  std::array<List, 3> lists;
  const uint32_t menu = g_menu.load();
  // The menu reports itself every frame while it is on screen (also when a screen on top of it is closing).
  const bool menu_live = menu && now - g_menu_seen.load() < Ms(250);
  if (menu_live)
    for (int l = 0; l < 3; ++l) ReadList(base, PPC_LOAD_U32(menu + kMenuLists[l]), lists[l]);
  const bool has_focus = lists[kFocus].selected >= 0;
  const int focused = has_focus ? kFocus : -1;
  int hit_list, hit_row, hit_arrow;
  HitTest(lists, c, &hit_list, &hit_row, &hit_arrow);
  const bool visible = lists[kFocus].rows > 0 || lists[kChild].rows > 0;

  if (click && hit_arrow >= 0 && has_focus) {
    // The panel's arrows: scroll, enter the sub-menu, go back.
    static constexpr uint16_t kArrowButton[kArrowCount] = {kDpadUp, kDpadDown, kA, kB};
    g.target = {};
    StartPulse(kArrowButton[hit_arrow], now);
  } else if (click) {
    if (hit_list >= 0 && has_focus) g.target = {true, hit_list, hit_row, true, now};
  } else if (!g.target.choose && (hit_list != g.hover_list || hit_row != g.hover_row)) {
    // Pointing at a row of the focused column selects it.
    g.target = {};
    if (hit_list == kFocus && has_focus) g.target = {true, hit_list, hit_row, false, now};
  }
  g.hover_list = hit_list, g.hover_row = hit_row;

  static const bool trace = std::getenv("WWE2K14_TEST_INPUT") != nullptr;
  if (trace) {
    static int last_key = -2;
    const int key = (menu_live ? 1 : 0) + 2 * (focused + 1) + 8 * (lists[0].selected + 1) + 256 * (lists[1].selected + 1) +
                    8192 * (hit_list + 1) + 32768 * (hit_row + 1) + 524288 * (hit_arrow + 1);
    if (key != last_key) {
      last_key = key;
      REXLOG_INFO(
          "[mouse] menu {:08X} live={} focused={} selected={},{},{} rows={},{},{} hover={}:{} arrow={} cursor=({:.0f},{:.0f})",
          menu, menu_live, focused, lists[0].selected, lists[1].selected, lists[2].selected, lists[0].rows, lists[1].rows,
          lists[2].rows, hit_list, hit_row, hit_arrow, c.x, c.y);
      for (int l = 0; l < 3; ++l)
        if (lists[l].rows)
          REXLOG_INFO("[mouse]   list {} row0 ({:.0f},{:.0f})-({:.0f},{:.0f}) last ({:.0f},{:.0f})", l, lists[l].row[0].x0,
                      lists[l].row[0].y0, lists[l].row[0].x1, lists[l].row[0].y1, lists[l].row[lists[l].rows - 1].x0,
                      lists[l].row[lists[l].rows - 1].y0);
    }
  }

  if (!g.target.active || Busy()) return visible;
  if (!menu_live || now - g.target.started > Ms(3000)) {
    g.target = {};
    return visible;
  }
  if (g.target.list == kChild) {
    // A sub-menu item: enter the sub-menu first (A on the focused item). Its rows then move into list 0.
    if (!has_focus || g.target.row >= lists[kChild].rows) {
      g.target = {};
      return visible;
    }
    g.target.child_rows = lists[kChild].rows;
    g.target.list = kFocus;
    g.target.entered = now;
    StartPulse(kA, now);
    return visible;
  }
  if (g.target.child_rows >= 0) {
    // Wait until the sub-menu has become the focused column.
    if (now - g.target.entered < Ms(250) || lists[kFocus].rows != g.target.child_rows || !has_focus) return visible;
    g.target.child_rows = -1;
  }
  const List& tl = lists[kFocus];
  if (!has_focus || g.target.row >= tl.rows) {
    g.target = {};
    return visible;
  }
  if (tl.selected != g.target.row) {
    StartPulse(tl.selected < g.target.row ? kDpadDown : kDpadUp, now);
  } else {
    if (g.target.choose) StartPulse(kA, now);
    g.target = {};
  }
  return visible;
}

// ---- Wrestler select grid ----------------------------------------------------------------------------------------
//
// The wrestler select screen keeps one record per player at screen +0x1B140 + 32 * player: +4 state (3 = picking a
// wrestler), +8 side (-1 = no side claimed yet), +12 column, +16 row. Its grid routine (`sub_8282E218` in TU2 /
// `sub_8282C648` in Vanilla, screen in r3) runs when the cursor moves or a player claims a side; the hook keeps the
// screen. The screen is open while the game's UI resource table holds its layout, "/MENU/SSFD". Portraits sit on a
// 44 x 50 lattice: column c, row r is centred at (153 + 44c, 411 + 50r) in 1280x720 UI coordinates.

std::atomic<uint32_t> g_select{0};
// Each player's cursor object on the select screen (its event handler `sub_8285AC40` in TU2, object in r3): +264
// screen, +276 player, and its own copy of the cell, +80 column, +64 row, which a move starts from.
std::atomic<uint32_t> g_select_cursor[6];

constexpr float kGridX0 = 153, kGridY0 = 411, kGridDX = 44, kGridDY = 50;
constexpr int kGridCols = 23, kGridRows = 4;

bool SelectOpen(uint8_t* base) {
  const uint32_t res = PPC_LOAD_U32(kSelectResourceSlot);
  if (!GuestAddress(res)) return false;
  static const char kName[] = "/MENU/SSFD";
  for (uint32_t i = 0; i < sizeof(kName); ++i)
    if (PPC_LOAD_U8(res + 0x34 + i) != uint8_t(kName[i])) return false;
  return true;
}

// Which wrestler sits in a cell (the game's own lookup, `sub_8293CD90` in TU2): the screen's grid object (screen +16 ->
// +56792) holds per page {+52 rows, +56 row pointers} at +12 * page and the column count at +48 + 12 * page; a cell
// holds -1 when empty. Row 4 is the tab row below the portraits: Custom Superstars, Downloaded Superstars,
// Divas / Superstars and Managers sit at columns 0, 3, 17 and 20.
bool CellUsed(uint8_t* base, uint32_t screen, int col, int row) {
  if (row == kGridRows) return col == 0 || col == 3 || col == 17 || col == 20;
  if (!screen || col < 0 || row < 0) return false;
  const uint32_t holder = PPC_LOAD_U32(screen + 16);
  if (!GuestAddress(holder)) return false;
  const uint32_t grid = PPC_LOAD_U32(holder + 56792);
  if (!GuestAddress(grid) || PPC_LOAD_U32(grid + 76) != 2) return false;
  const uint32_t page = PPC_LOAD_U32(grid + 88);
  if (page > 16) return false;
  const uint32_t rows = PPC_LOAD_U32(grid + page * 12 + 56);
  if (!GuestAddress(rows) || uint32_t(row) >= PPC_LOAD_U32(grid + page * 12 + 52)) return false;
  if (uint32_t(col) >= PPC_LOAD_U32(grid + page * 12 + 48)) return false;
  const uint32_t line = PPC_LOAD_U32(rows + row * 4);
  return GuestAddress(line) && PPC_LOAD_U32(line + col * 4) != 0xFFFFFFFFu;
}

// The cell under the cursor: portraits on the lattice, the tabs (wider than a cell) in row 4.
bool CellAt(const Cursor& c, int* col, int* row) {
  if (!c.valid) return false;
  const int r = int(std::lround((c.y - kGridY0) / kGridDY));
  if (r < 0 || r > kGridRows || std::fabs(c.y - (kGridY0 + r * kGridDY)) > 25) return false;
  if (r == kGridRows) {
    static constexpr float kTabX[4][2] = {{133, 262}, {266, 397}, {883, 1012}, {1016, 1145}};
    static constexpr int kTabCols[4] = {0, 3, 17, 20};
    for (int k = 0; k < 4; ++k)
      if (c.x >= kTabX[k][0] && c.x <= kTabX[k][1]) {
        *col = kTabCols[k], *row = r;
        return true;
      }
    return false;
  }
  const int cc = int(std::lround((c.x - kGridX0) / kGridDX));
  if (cc < 0 || cc >= kGridCols || std::fabs(c.x - (kGridX0 + cc * kGridDX)) > 22) return false;
  *col = cc, *row = r;
  return true;
}

int g_grid_pending_col = -1, g_grid_pending_row = -1;  // pointed-at cell the cursor is still moving to

struct GridClick {
  bool active = false;
  int col = 0, row = 0;
  int64_t started = 0;
  bool claim_pressed = false;  // A was pressed to claim a side first
  int64_t moved = 0;           // when the cursor was put on the cell
};
GridClick gg;
int g_grid_hover_col = -1, g_grid_hover_row = -1;

// Moves the cursor onto (col, row) in one step: the cursor is put on a neighbouring cell and the D-pad moves it from
// there, so the game's own move runs (it updates the wrestler preview, which writing the cell alone does not).
// Puts the cursor on (col, row): the screen's record and the cursor object's own copy (the one a move starts from).
// Returns false when the cursor object is not known yet: then only a D-pad step can move it.
bool SetCell(uint8_t* base, uint32_t screen, uint32_t rec, int col, int row) {
  const uint32_t player = (rec - screen - 0x1B140) / 32;
  const uint32_t cursor = player < 6 ? g_select_cursor[player].load() : 0;
  // The object of this player, holding the same cell as the record.
  if (!cursor || !GuestAddress(cursor) || PPC_LOAD_U32(cursor + 276) != player ||
      PPC_LOAD_U32(cursor + 80) != PPC_LOAD_U32(rec + 12) || PPC_LOAD_U32(cursor + 64) != PPC_LOAD_U32(rec + 16))
    return false;
  PPC_STORE_U32(cursor + 80, uint32_t(col));
  PPC_STORE_U32(cursor + 64, uint32_t(row));
  PPC_STORE_U32(rec + 12, uint32_t(col));
  PPC_STORE_U32(rec + 16, uint32_t(row));
  return true;
}

void StepInto(uint8_t* base, uint32_t screen, uint32_t rec, int col, int row, int64_t now) {
  // Tabs: no portrait next to them to step from; placing the cursor on the tab highlights it.
  if (row == kGridRows && SetCell(base, screen, rec, col, row)) return;
  static constexpr struct {
    int dc, dr;
    uint16_t button;
  } kFrom[] = {{-1, 0, kDpadRight}, {1, 0, kDpadLeft}, {0, -1, kDpadDown}, {0, 1, kDpadUp}};
  const int cur_col = int(PPC_LOAD_U32(rec + 12)), cur_row = int(PPC_LOAD_U32(rec + 16));
  for (const auto& f : kFrom) {
    const int nc = col + f.dc, nr = row + f.dr;
    if (nr == kGridRows || !CellUsed(base, screen, nc, nr)) continue;
    if (!SetCell(base, screen, rec, nc, nr)) {
      // Cursor object not known yet: one ordinary step towards the cell (the step makes the game report it).
      if (cur_row != row)
        StartPulse(cur_row < row ? kDpadDown : kDpadUp, now);
      else
        StartPulse(cur_col < col ? kDpadRight : kDpadLeft, now);
      return;
    }
    StartPulse(f.button, now);
    return;
  }
}

// Returns true while the select screen handles the mouse (the menu code is skipped). Pointing at a portrait moves the
// cursor there in one step; a click then presses A.
bool GridMouse(uint8_t* base, const Cursor& c, bool click, int64_t now) {
  if (!SelectOpen(base)) {
    g_select.store(0);
    gg = {};
    return false;
  }
  const uint32_t screen = g_select.load();
  // The record of whoever is choosing now: 1P picks its own wrestler, then the opponent's (another record).
  uint32_t rec = 0;
  for (uint32_t p = 0; screen && p < 6 && !rec; ++p) {
    const uint32_t r = screen + 0x1B140 + 32 * p;
    if (PPC_LOAD_U32(r + 4) == 3 && PPC_LOAD_U32(r + 8) != 0xFFFFFFFFu) rec = r;
  }
  const bool picking = rec != 0;
  const int cur_col = picking ? int(PPC_LOAD_U32(rec + 12)) : -1, cur_row = picking ? int(PPC_LOAD_U32(rec + 16)) : -1;

  int hc = -1, hr = -1;
  if (CellAt(c, &hc, &hr) && screen && !CellUsed(base, screen, hc, hr)) hc = hr = -1;
  if (click && hc >= 0) gg = {true, hc, hr, now};
  // Downloaded / Custom Superstars are a list scrolled next to the 1P name: its small up / down arrows.
  if (click && hc < 0 && !picking && c.valid && c.x > 590 && c.x < 618) {
    if (c.y > 292 && c.y < 317) StartPulse(kDpadUp, now);
    if (c.y > 317 && c.y < 344) StartPulse(kDpadDown, now);
  }
  // No side claimed yet: the controller icon on the 1P bar (or the free one under the grid) claims it, like A.
  // (The screen is only known once a cursor has been claimed or moved: unknown means unclaimed.)
  if (click && hc < 0 && (!screen || PPC_LOAD_U32(screen + 0x1B140 + 8) == 0xFFFFFFFFu) && c.valid &&
      ((c.x > 140 && c.x < 232 && c.y > 298 && c.y < 356) || (c.x > 600 && c.x < 680 && c.y > 588 && c.y < 632)))
    StartPulse(kA, now);
  // Pointing at a portrait or tab moves the cursor there.
  // Pointing at another portrait or tab moves the cursor there. Only a move of the mouse does: the wheel or a
  // controller can take the cursor elsewhere while the mouse rests.
  if (hc != g_grid_hover_col || hr != g_grid_hover_row) g_grid_pending_col = hc, g_grid_pending_row = hr;
  if (g_grid_pending_col >= 0 && picking && cur_col == g_grid_pending_col && cur_row == g_grid_pending_row)
    g_grid_pending_col = g_grid_pending_row = -1;
  if (picking && !gg.active && g_grid_pending_col >= 0 && !Busy())
    StepInto(base, screen, rec, g_grid_pending_col, g_grid_pending_row, now);
  g_grid_hover_col = hc, g_grid_hover_row = hr;

  static const bool trace = std::getenv("WWE2K14_TEST_INPUT") != nullptr;
  if (trace) {
    static int last_key = -1;
    const int key = (cur_col + 1) + 64 * (cur_row + 1) + 4096 * (hc + 1) + 262144 * (hr + 1) + (picking ? 1 << 24 : 0);
    if (key != last_key) {
      last_key = key;
      REXLOG_INFO("[mouse] select screen {:08X} picking={} cursor={},{} hover={},{} click={}:{},{}", screen, picking,
                  cur_col, cur_row, hc, hr, gg.active, gg.col, gg.row);
    }
  }

  if (!gg.active || Busy()) return true;
  if (now - gg.started > Ms(3000)) {
    gg = {};
    return true;
  }
  if (!picking) {
    // No side claimed yet: A claims one, then the click carries on.
    if (!gg.claim_pressed) {
      StartPulse(kA, now);
      gg.claim_pressed = true;
    }
    return true;
  }
  if (!CellUsed(base, screen, gg.col, gg.row)) {
    gg = {};
    return true;
  }
  if (cur_col != gg.col || cur_row != gg.row) {
    StepInto(base, screen, rec, gg.col, gg.row, now);
    gg.moved = now;
    return true;
  }
  if (gg.moved && now - gg.moved < Ms(50)) return true;  // let the game take the new cell in before choosing it
  StartPulse(kA, now);
  gg = {};
  return true;
}

// ---- Button prompts -------------------------------------------------------------------------------------------------
//
// Prompts like "(A) SELECT  (B) BACK" are text with controller icons in it. The 2D renderer queues every glyph as a quad
// of 4 vertices in 1280x720 UI coordinates. Icons come from the first row of the font's icon sheet: 0.082 x 0.078 of the
// texture, one every 0.125 in u (Y at 0.125, B at 0.25, A at 0.375). Each icon and the label after it (glyphs on the
// same line up to the next icon) make one clickable prompt; a click there presses the button.
// (The quad is queued by `sub_82BC6CB8` in TU2 / `sub_82BC3518` in Vanilla: renderer in r3, vertices in r4, vertex
// count in r5.)

struct Prompt {
  uint32_t button;
  float x0, y0, x1, y1;
  int64_t seen;
};

// Text drawn where the title screen says "PRESS START BUTTON" (glyphs on its line, 1280x720 UI coordinates); the
// title screen has no other way to tell it apart from a screen where a click should do nothing.
std::atomic<int64_t> g_title_text_seen{0};
std::atomic<int64_t> g_prompt_seen{0};  // last time a button prompt icon was drawn (the title screen has none)
std::atomic<int> g_title_glyphs{0};

std::mutex g_prompt_mutex;
std::vector<Prompt> g_prompts;  // recently drawn prompts
int g_prompt_open = -1;         // prompt whose label is being drawn

// The controller icon at (u0, v0) of the font's icon sheet: face buttons in the first row (one every 0.125), the
// bumpers at v 0.344 (left at u 0, right at u 0.148, Back at u 0.297) and the triggers at v 0.430.
uint32_t IconButton(float u0, float v0) {
  if (std::fabs(v0) < 0.004f) {
    static constexpr uint32_t kRow0[4] = {kX, kY, kB, kA};
    const int slot = int(std::lround(u0 / 0.125f));
    return slot >= 0 && slot < 4 && std::fabs(u0 - slot * 0.125f) < 0.004f ? kRow0[slot] : 0;
  }
  const bool left = std::fabs(u0) < 0.004f, right = std::fabs(u0 - 0.148f) < 0.006f;
  if (std::fabs(v0 - 0.344f) < 0.006f) {
    if (std::fabs(u0 - 0.297f) < 0.006f) return kBack;  // "HELP MENU"
    return left ? kLB : right ? kRB : 0;
  }
  if (std::fabs(v0 - 0.430f) < 0.006f) return left ? kLT : right ? kRT : 0;
  return 0;
}

// One queued quad: its bounding box on screen and in the texture.
void NoteQuad(float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, int64_t now) {
  if (!(x1 > x0) || !(y1 > y0)) return;
  static const bool trace = std::getenv("WWE2K14_TEST_INPUT") != nullptr;
  const float cx = (x0 + x1) / 2, cy = (y0 + y1) / 2;
  // The title screen's "PRESS START BUTTON" (one image) and the "Press START button" line of the attract demo.
  if (cy > 380 && cy < 420 && x0 > 280 && x0 < 320 && x1 > 480 && x1 < 520 && y1 - y0 < 50) g_title_text_seen.store(now);
  if (cy > 52 && cy < 86 && x0 > 320 && x1 < 560 && x1 - x0 < 24 && y1 - y0 > 10 && y1 - y0 < 30) {
    // 15+ such glyphs within 50 ms: the line is on screen.
    static int64_t window = 0;
    static int count = 0;
    if (now - window > Ms(50)) window = now, count = 0;
    if (++count >= 15) g_title_text_seen.store(now);
    g_title_glyphs.store(count);
  }
  std::lock_guard<std::mutex> lock(g_prompt_mutex);
  const bool icon = std::fabs(v1 - v0 - 0.078f) < 0.006f && u1 - u0 > 0.04f && u1 - u0 < 0.12f && x1 - x0 > 12 &&
                    x1 - x0 < 40 && y1 - y0 > 14 && y1 - y0 < 30;
  if (icon) {
    g_prompt_open = -1;
    const uint32_t button = IconButton(u0, v0);
    static const bool trace = std::getenv("WWE2K14_TEST_INPUT") != nullptr;
    if (trace) {
      static float last_u = -1, last_v = -1;
      if (u0 != last_u || v0 != last_v) {
        last_u = u0, last_v = v0;
        REXLOG_INFO("[mouse] icon u={:.3f} v={:.3f} at ({:.0f},{:.0f}) button {:05X}", u0, v0, x0, y0, button);
      }
    }
    if (!button) return;
    g_prompt_seen.store(now);
    // Bumper and trigger icons sit at either end of a panel title ("LB ATTIRE RB"): the icon alone is the prompt.
    const bool labelled = button <= 0xFFFF && button != kLB && button != kRB;
    for (size_t i = 0; i < g_prompts.size(); ++i) {
      Prompt& p = g_prompts[i];
      if (p.button == button && std::fabs(p.x0 - x0) < 1 && std::fabs(p.y0 - y0) < 1) {
        p.x1 = x1, p.seen = now;  // drawn again: the label is measured again below
        g_prompt_open = labelled ? int(i) : -1;
        return;
      }
    }
    if (g_prompts.size() > 64) g_prompts.erase(g_prompts.begin());
    g_prompts.push_back({button, x0, y0, x1, y1, now});
    g_prompt_open = labelled ? int(g_prompts.size() - 1) : -1;
    return;
  }
  if (g_prompt_open < 0) return;
  Prompt& p = g_prompts[g_prompt_open];
  // The label: glyphs on the icon's line that follow it closely.
  if (std::fabs(y0 - p.y0) < 4 && x0 >= p.x1 - 2 && x0 <= p.x1 + 16 && x1 - x0 < 40)
    p.x1 = x1;
  else
    g_prompt_open = -1;
}

// The button of the prompt under the cursor, or 0.
uint32_t PromptAt(const Cursor& c, int64_t now) {
  if (!c.valid) return 0;
  std::lock_guard<std::mutex> lock(g_prompt_mutex);
  for (const Prompt& p : g_prompts)
    if (now - p.seen < Ms(150) && c.x >= p.x0 - 2 && c.x <= p.x1 + 4 && c.y >= p.y0 - 4 && c.y <= p.y1 + 4)
      return p.button;
  return 0;
}

// ---- Text lists: option screens and Yes / No popups -------------------------------------------------------------
//
// Screens outside the menu system (option lists such as Options > Gameplay, Match Rules, the quit popup's YES / NO)
// show the selected entry in red. Each text item is one line: its glyphs give its rectangle and the item's colour
// (bytes at +0x30 / +0x34 / +0x38 of the text item passed to the quad emitter) whether it is selected. Lines aligned on
// the left with a red line form a vertical list (option rows: pointing at a row selects it with up / down; a click on
// the selected row left / right of its value presses left / right). White lines on the red line's row are side-by-side
// choices (YES / NO: pointing switches with left / right, a click chooses with A, the wheel switches).

struct TextLine {
  uint64_t item;  // text object and the string's position: one object draws many strings in turn
  float x0, y0, x1, y1;
  bool red;
  int64_t seen;
  float cy() const { return (y0 + y1) / 2; }
};

std::mutex g_text_mutex;
std::vector<TextLine> g_text;

void NoteText(uint64_t item, float x0, float y0, float x1, float y1, bool red, int64_t now) {
  std::lock_guard<std::mutex> lock(g_text_mutex);
  for (TextLine& l : g_text) {
    if (l.item != item) continue;
    if (now - l.seen > Ms(8)) {
      l = {item, x0, y0, x1, y1, red, now};  // drawn again in a new frame
    } else {
      l.x0 = std::min(l.x0, x0), l.y0 = std::min(l.y0, y0), l.x1 = std::max(l.x1, x1), l.y1 = std::max(l.y1, y1);
      l.red = l.red || red, l.seen = now;
    }
    return;
  }
  if (g_text.size() >= 256) {
    g_text.erase(std::remove_if(g_text.begin(), g_text.end(), [&](const TextLine& l) { return now - l.seen > Ms(200); }),
                 g_text.end());
    if (g_text.size() >= 256) g_text.erase(g_text.begin());
  }
  g_text.push_back({item, x0, y0, x1, y1, red, now});
}

struct TextTarget {
  bool active = false;
  bool horizontal = false;
  uint64_t item = 0;
  bool choose = false;  // press A once the choice is selected (popups)
  int64_t started = 0;
};
TextTarget gt;
uint64_t g_text_hover = 0;
bool g_text_horizontal = false;  // side-by-side choices on screen: the wheel steps left / right

// Returns true while a text list is on screen and handled the mouse.
bool TextMouse(const Cursor& c, bool click, int64_t now) {
  // Only a move of the mouse points at something new: a list that appears under a resting cursor (e.g. after a popup
  // closes) keeps its selection.
  static float last_x = -1, last_y = -1;
  const bool moved = c.valid && (std::fabs(c.x - last_x) > 1 || std::fabs(c.y - last_y) > 1);
  last_x = c.x, last_y = c.y;
  std::vector<TextLine> lines;
  {
    std::lock_guard<std::mutex> lock(g_text_mutex);
    for (const TextLine& l : g_text)
      if (now - l.seen < Ms(80)) lines.push_back(l);
  }
  // The list: a red line with others aligned below / above it (vertical) or white ones beside it (horizontal).
  // An option row has two red lines (label and value): the longest aligned column wins, the leftmost on a tie.
  std::vector<TextLine> list, side;
  float list_x = 1e9f;
  for (const TextLine& r : lines) {
    if (!r.red) continue;
    const float h = r.y1 - r.y0;
    std::vector<TextLine> v, s;
    for (const TextLine& l : lines) {
      if (std::fabs((l.y1 - l.y0) - h) > 8) continue;
      if (std::fabs(l.x0 - r.x0) < 6) v.push_back(l);
      if (std::fabs(l.cy() - r.cy()) < 6 && (!l.red || l.item == r.item)) s.push_back(l);
    }
    if (v.size() >= 3 && (v.size() > list.size() || (v.size() == list.size() && r.x0 < list_x))) list = v, list_x = r.x0;
    // Choices are short words (YES / NO); a selected option label beside MIN / MAX is not one of them.
    const bool short_words = std::all_of(s.begin(), s.end(), [](const TextLine& l) { return l.x1 - l.x0 <= 120; });
    if (s.size() >= 2 && short_words && v.size() < 3 && side.empty()) side = s;
  }
  // A popup's choices are drawn over the screen below it, whose list is still drawn too: the choices win.
  const bool horizontal = !side.empty();
  if (horizontal) list = side;
  g_text_horizontal = horizontal && !list.empty();
  if (list.empty()) {
    gt = {};
    g_text_hover = 0;
    return false;
  }
  if (horizontal)
    std::sort(list.begin(), list.end(), [](const TextLine& a, const TextLine& b) { return a.x0 < b.x0; });
  else
    std::sort(list.begin(), list.end(), [](const TextLine& a, const TextLine& b) { return a.cy() < b.cy(); });
  int sel_index = -1;
  for (size_t i = 0; i < list.size(); ++i)
    if (list[i].red) sel_index = int(i);

  // The line under the cursor: a whole row of a vertical list, a choice (with some margin) of a horizontal one.
  int hit = -1;
  if (c.valid) {
    for (size_t i = 0; i < list.size(); ++i) {
      const TextLine& l = list[i];
      const bool in = horizontal ? (c.x > l.x0 - 40 && c.x < l.x1 + 40 && c.y > l.y0 - 14 && c.y < l.y1 + 14)
                                 : (c.y > l.y0 && c.y < l.y1 && c.x > l.x0 - 20 && c.x < l.x0 + 960);
      if (in) hit = int(i);
    }
  }
  const uint64_t hit_item = hit >= 0 ? list[hit].item : 0;
  static const bool trace = std::getenv("WWE2K14_TEST_INPUT") != nullptr;
  if (trace) {
    static int last_key = -1;
    const int key = int(list.size()) + 64 * (horizontal ? 1 : 0) + 128 * (sel_index + 1) + 8192 * (hit + 1);
    if (key != last_key) {
      last_key = key;
      REXLOG_INFO("[mouse] text list {} entries {} selected {} hover {} first ({:.0f},{:.0f}) cursor ({:.0f},{:.0f})",
                  list.size(), horizontal ? "side by side" : "rows", sel_index, hit, list[0].x0, list[0].y0, c.x, c.y);
    }
  }

  if (click && hit >= 0) {
    if (horizontal) {
      gt = {true, true, hit_item, true, now};
    } else if (hit == sel_index && c.x > list[hit].x1 + 30) {
      // The selected option row: left / right of its value (the other text on the row, e.g. ON or MIN ... MAX).
      float vx0 = 1e9f, vx1 = -1e9f;
      for (const TextLine& l : lines)
        if (std::fabs(l.cy() - list[hit].cy()) < 6 && l.item != list[hit].item) vx0 = std::min(vx0, l.x0), vx1 = std::max(vx1, l.x1);
      const float centre = vx1 > vx0 ? (vx0 + vx1) / 2 : list[hit].x1 + 500;
      StartPulse(c.x < centre ? kDpadLeft : kDpadRight, now);
      gt = {};
    } else {
      gt = {true, false, hit_item, false, now};
    }
  } else if (!gt.choose && hit_item && hit_item != g_text_hover && hit != sel_index && moved) {
    gt = {true, horizontal, hit_item, false, now};  // pointing at another entry selects it
  }
  g_text_hover = hit_item;

  if (!gt.active || Busy()) return true;
  if (now - gt.started > Ms(2000)) {
    gt = {};
    return true;
  }
  int target = -1;
  for (size_t i = 0; i < list.size(); ++i)
    if (list[i].item == gt.item) target = int(i);
  if (target < 0 || sel_index < 0) {
    gt = {};
    return true;
  }
  if (target != sel_index) {
    if (horizontal)
      StartPulse(target < sel_index ? kDpadLeft : kDpadRight, now);
    else
      StartPulse(target < sel_index ? kDpadUp : kDpadDown, now);
  } else {
    if (gt.choose) StartPulse(kA, now);
    gt = {};
  }
  return true;
}

}  // namespace

extern "C" PPC_FUNC(WWE2K14_IMP_QueueQuad);
extern "C" PPC_FUNC(WWE2K14_FN_QueueQuad) {
  const uint32_t verts = ctx.r4.u32, count = ctx.r5.u32;
  if (count == 4 && GuestAddress(verts)) {
    // Vertices of 12 floats: position x, y, z, w; colour r, g, b, a; texture u, v; (u, v again).
    float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f, u0 = 1e9f, v0 = 1e9f, u1 = -1e9f, v1 = -1e9f;
    for (uint32_t k = 0; k < 4; ++k) {
      const uint32_t v = verts + k * 48;
      const float x = LoadF32(base, v), y = LoadF32(base, v + 4), u = LoadF32(base, v + 32), w = LoadF32(base, v + 36);
      x0 = std::min(x0, x), x1 = std::max(x1, x), y0 = std::min(y0, y), y1 = std::max(y1, y);
      u0 = std::min(u0, u), u1 = std::max(u1, u), v0 = std::min(v0, w), v1 = std::max(v1, w);
    }
    if (std::isfinite(x0) && std::isfinite(y0) && std::isfinite(x1) && std::isfinite(y1)) {
      const int64_t now = Now();
      NoteQuad(x0, y0, x1, y1, u0, v0, u1, v1, now);
      // The text object's colour: bytes R, G, B at +0x30 / +0x34 / +0x38.
      const uint32_t item = ctx.r3.u32;
      if (GuestAddress(item) && y1 - y0 >= 12 && y1 - y0 <= 40 && x1 - x0 <= 60) {
        const uint32_t r = PPC_LOAD_U32(item + 0x30), g = PPC_LOAD_U32(item + 0x34), b = PPC_LOAD_U32(item + 0x38);
        // The string's position (+0x90 / +0x94) tells the strings this object draws apart.
        const uint64_t key = (uint64_t(item) << 32) ^ (uint64_t(PPC_LOAD_U32(item + 0x90)) * 0x9E3779B1u) ^ PPC_LOAD_U32(item + 0x94);
        // Selected text is red and pulses between bright and dark red: a red hue at any brightness.
        if (r < 256 && g < 256 && b < 256) NoteText(key, x0, y0, x1, y1, r > 40 && r > 2 * g + 20 && r > 2 * b + 20, now);
      }
    }
  }
  WWE2K14_IMP_QueueQuad(ctx, base);
}

extern "C" PPC_FUNC(WWE2K14_IMP_SelectCursor);
extern "C" PPC_FUNC(WWE2K14_FN_SelectCursor) {
  const uint32_t cursor = ctx.r3.u32;
  const uint32_t player = PPC_LOAD_U32(cursor + 276);
  if (player < 6) g_select_cursor[player].store(cursor);
  WWE2K14_IMP_SelectCursor(ctx, base);
}

extern "C" PPC_FUNC(WWE2K14_IMP_SelectGrid);
extern "C" PPC_FUNC(WWE2K14_FN_SelectGrid) {
  g_select.store(ctx.r3.u32);
  WWE2K14_IMP_SelectGrid(ctx, base);
}

namespace wwe2k14 {

void NoteMenuObject(uint32_t menu_obj) {
  g_menu.store(menu_obj);
  g_menu_seen.store(Now());
}

}  // namespace wwe2k14

extern "C" PPC_FUNC(WWE2K14_IMP_XInputGetState);

extern "C" PPC_FUNC(WWE2K14_FN_XInputGetState) {
  const uint32_t user = ctx.r3.u32;
  const uint32_t state = ctx.r4.u32;
  WWE2K14_IMP_XInputGetState(ctx, base);
#if defined(_WIN32)
  if (user != 0 || !state) return;
  StartWheelHook();
  const int64_t now = Now();
  const wwe2k14::TestInput test = wwe2k14::PollTestInput();
  Cursor c = RealCursor();
  if (test.cursor) {
    c = Cursor{};
    c.valid = true, c.x = test.x, c.y = test.y, c.left = test.left;
  }
  const bool click = c.left && !g.was_left;
  g.was_left = c.left;

  static const bool trace = std::getenv("WWE2K14_TEST_INPUT") != nullptr;
  if (trace) {
    static uint32_t last[24] = {};
    bool changed = false;
    for (int i = 0; i < 24; ++i) {
      const uint32_t v = PPC_LOAD_U32(kUiResourceTable + i * 4);
      if (v != last[i]) changed = true, last[i] = v;
    }
    if (changed) {
      std::string s;
      for (int i = 0; i < 24; ++i) {
        if (!GuestAddress(last[i])) continue;
        std::string name;
        for (uint32_t o = 0x10; o < 0x60 && name.size() < 24; ++o) {
          const uint8_t ch = PPC_LOAD_U8(last[i] + o);
          if (ch == '/' || (name.size() && ch >= 32 && ch < 127)) name += char(ch);
          else if (name.size() >= 4) break;
          else name.clear();
        }
        s += fmt::format(" [{}]{}", i, name);
      }
      REXLOG_INFO("[mouse] ui resources:{}", s);
    }
  }
  uint32_t buttons = test.buttons;
  // A click on a button prompt presses that button; otherwise the screen's own pointing and clicking.
  const uint32_t prompt = click ? PromptAt(c, now) : 0;
  if (prompt) StartPulse(prompt, now);
  // Title screen and attract demo: a click presses Start.
  const bool menu_on_screen = g_menu.load() && now - g_menu_seen.load() < Ms(250);
  if (click && !prompt && !menu_on_screen && !SelectOpen(base) && now - g_title_text_seen.load() < Ms(300) &&
      now - g_prompt_seen.load() > Ms(500))
    StartPulse(kStart, now);
  const bool grid = GridMouse(base, c, click && !prompt, now);
  const bool menu = !grid && MenuMouse(base, c, click && !prompt, now);
  if (!grid && !menu)
    TextMouse(c, click && !prompt, now);
  else
    g_text_horizontal = false;
  buttons |= PulseButtons(now);
  if (c.middle) buttons |= kStart;

  // Wheel notches step the selection, one press each.
  if (!Busy() && !g.target.active && !gg.active && !gt.active) {
    const int w = g_wheel.load();
    if (w != 0) {
      g_wheel += w > 0 ? -1 : 1;
      // Side-by-side choices (YES / NO) step left / right, everything else up / down.
      if (g_text_horizontal)
        StartPulse(w > 0 ? kDpadLeft : kDpadRight, now);
      else
        StartPulse(w > 0 ? kDpadUp : kDpadDown, now);
      buttons |= PulseButtons(now);
    }
  }
  if (!buttons) return;
  if (ctx.r3.u32 != 0) {
    // No controller or keyboard state this poll: report a neutral pad carrying just these buttons.
    for (uint32_t i = 0; i < 16; i += 4) PPC_STORE_U32(state + i, 0);
    ctx.r3.u64 = 0;
  }
  PPC_STORE_U16(state + 4, PPC_LOAD_U16(state + 4) | uint16_t(buttons));
  if (buttons & kLT) PPC_STORE_U8(state + 6, 255);
  if (buttons & kRT) PPC_STORE_U8(state + 7, 255);
  // A new packet number tells the game the state changed.
  PPC_STORE_U32(state + 0, PPC_LOAD_U32(state + 0) + 1);
#endif
}
