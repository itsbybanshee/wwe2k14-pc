#pragma once

#include <cstdint>

namespace wwe2k14 {

// The menu screen's update (SetupMenuSlot hook, exit_menu.cpp) reports the menu object it is drawing, so the mouse can
// hit-test its rows (mouse_input.cpp).
void NoteMenuObject(uint32_t menu_obj);

}  // namespace wwe2k14
