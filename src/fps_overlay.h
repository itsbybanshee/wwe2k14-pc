#pragma once

namespace rex::ui {
class ImGuiDrawer;
}

namespace wwe2k14 {

class FpsOverlay {
 public:
  void Attach(rex::ui::ImGuiDrawer* drawer);
  void Detach();

 private:
  rex::ui::ImGuiDrawer* drawer_ = nullptr;
  bool active_ = false;
};

}  // namespace wwe2k14
