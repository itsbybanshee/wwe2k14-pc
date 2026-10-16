#pragma once

#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

struct ImFont;

namespace rex::ui {
class ImGuiDrawer;
class WindowedAppContext;
}  // namespace rex::ui

namespace wwe2k14 {

// "Achievement unlocked" pop-ups at the bottom of the game window, one after another (5 s each): the achievement's
// icon on the left, "Achievement unlocked: <name>" and the description centred on the right. The ImGui dialog
// exists only while a pop-up is on screen and draws on the foreground layer without a window, so it never takes
// input during play.
class AchievementToasts {
 public:
  // UI thread. Loads the pop-up fonts (Segoe UI regular and bold) into the overlay's font atlas.
  void Attach(rex::ui::WindowedAppContext* app_context, rex::ui::ImGuiDrawer* drawer);
  void Detach();

  // Any thread (unlocks arrive on guest threads). icon_path: PNG of the achievement icon, may be empty.
  void Push(std::string name, std::string description, std::string icon_path);

  // In-memory WAV played as each pop-up appears (achievement_sound.h). Empty = silent.
  void SetSound(std::vector<uint8_t> wav) { sound_ = std::move(wav); }

 private:
  friend class AchievementToastDialog;

  struct Toast {
    std::string name;
    std::string description;
    std::string icon_path;
  };

  void ShowFromUIThread();

  rex::ui::WindowedAppContext* app_context_ = nullptr;
  rex::ui::ImGuiDrawer* drawer_ = nullptr;
  ImFont* font_regular_ = nullptr;
  ImFont* font_bold_ = nullptr;
  std::mutex mutex_;
  std::deque<Toast> queue_;
  std::vector<uint8_t> sound_;
  bool dialog_active_ = false;  // UI thread only
};

}  // namespace wwe2k14
