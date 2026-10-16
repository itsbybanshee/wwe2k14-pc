#include "display_mode.h"

#include <cstdlib>
#include <cwchar>

#include <rex/logging.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <rex/ui/window_win.h>
#endif

namespace wwe2k14 {

namespace {
bool Enabled() {
  const char* v = std::getenv("WWE2K14_FORCE_60HZ");
  return (v && v[0] == '1');
}

// 59 Hz is how Windows reports 59.94 Hz modes.
bool Is60Multiple(DWORD hz) { return hz && (hz % 60 == 0 || (hz + 1) % 60 == 0); }
#if defined(_WIN32)
// Per-monitor DPI awareness, set before any window exists: without it Windows renders the game at the scaled
// ("logical") size and stretches it, e.g. a 1920x1080 image on a 3840x2160 desktop at 200 % scaling.
struct DpiAwareness {
  DpiAwareness() { SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2); }
} dpi_awareness;
#endif
}  // namespace

void DisplayMode60::Attach(rex::ui::Window* window) {
  window_ = window;
  if (window_) {
    window_->AddListener(this);
    Update();
  }
}

void DisplayMode60::Detach() {
  if (window_) {
    window_->RemoveListener(this);
    window_ = nullptr;
  }
  Restore();
}

void DisplayMode60::Update() {
#if defined(_WIN32)
  if (!window_ || !Enabled()) return;
  const bool fullscreen = window_->IsFullscreen();
  if (!fullscreen) {
    Restore();
    return;
  }
  if (switched_) return;
  HWND hwnd = static_cast<rex::ui::Win32Window*>(window_)->hwnd();
  MONITORINFOEXW monitor{};
  monitor.cbSize = sizeof(monitor);
  if (!hwnd || !GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &monitor)) return;
  DEVMODEW current{};
  current.dmSize = sizeof(current);
  if (!EnumDisplaySettingsW(monitor.szDevice, ENUM_CURRENT_SETTINGS, &current)) return;
  if (Is60Multiple(current.dmDisplayFrequency)) {  // 60 / 120 / 240 Hz: already even
    REXLOG_INFO("[wwe2k14] fullscreen: display already at {} Hz", current.dmDisplayFrequency);
    switched_ = false;
    return;
  }

  // The same resolution at 60 Hz (or 120 / 59), if the monitor offers it.
  DEVMODEW best{};
  for (DWORD i = 0;; ++i) {
    DEVMODEW mode{};
    mode.dmSize = sizeof(mode);
    if (!EnumDisplaySettingsW(monitor.szDevice, i, &mode)) break;
    if (mode.dmPelsWidth != current.dmPelsWidth || mode.dmPelsHeight != current.dmPelsHeight ||
        mode.dmBitsPerPel != current.dmBitsPerPel || !Is60Multiple(mode.dmDisplayFrequency)) {
      continue;
    }
    // Prefer exactly 60, then higher multiples.
    auto rank = [](DWORD hz) { return hz == 60 ? 0 : hz % 60 == 0 ? int(hz) : 1000 + int(hz); };
    if (!best.dmSize || rank(mode.dmDisplayFrequency) < rank(best.dmDisplayFrequency)) best = mode;
  }
  if (!best.dmSize) {
    REXLOG_INFO("[wwe2k14] display {} Hz, no 60 Hz mode at {}x{}: frames will not be evenly paced",
                current.dmDisplayFrequency, current.dmPelsWidth, current.dmPelsHeight);
    return;
  }
  best.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL | DM_DISPLAYFREQUENCY;
  LONG result = ChangeDisplaySettingsExW(monitor.szDevice, &best, nullptr, CDS_FULLSCREEN, nullptr);
  if (result == DISP_CHANGE_SUCCESSFUL) {
    switched_ = true;
    wcsncpy_s(device_, monitor.szDevice, _TRUNCATE);
    REXLOG_INFO("[wwe2k14] fullscreen: display switched from {} Hz to {} Hz", current.dmDisplayFrequency,
                best.dmDisplayFrequency);
  } else {
    REXLOG_WARN("[wwe2k14] could not switch the display to {} Hz (error {})", best.dmDisplayFrequency, result);
  }
#endif
}

void DisplayMode60::Restore() {
#if defined(_WIN32)
  if (!switched_) return;
  switched_ = false;
  ChangeDisplaySettingsExW(device_, nullptr, nullptr, 0, nullptr);
  REXLOG_INFO("[wwe2k14] display refresh rate restored");
#endif
}

}  // namespace wwe2k14
