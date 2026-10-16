// Fullscreen at 60 Hz: the game runs at a fixed 60 FPS, and on a display refreshing at a rate that isn't a multiple
// of 60 (75, 144, 165 Hz ...) frames are shown for uneven times (judder). While the game is fullscreen, the monitor is
// switched to a 60 Hz mode of the same resolution, and back when leaving fullscreen or quitting.
// WWE2K14_FORCE_60HZ=0 keeps the desktop refresh rate.

#pragma once

#include <rex/ui/window.h>
#include <rex/ui/window_listener.h>

namespace wwe2k14 {

class DisplayMode60 : public rex::ui::WindowListener {
 public:
  void Attach(rex::ui::Window* window);
  void Detach();

  void OnOpened(rex::ui::UISetupEvent&) override { Update(); }
  void OnResize(rex::ui::UISetupEvent&) override { Update(); }
  void OnClosing(rex::ui::UIEvent&) override { Restore(); }

 private:
  void Update();
  void Restore();

  rex::ui::Window* window_ = nullptr;
  bool switched_ = false;
  wchar_t device_[32] = {};
};

}  // namespace wwe2k14
