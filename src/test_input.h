#pragma once

#include <cstdint>

namespace wwe2k14 {

// Scripted input for automated tests (test_input.cpp). All zero / false when nothing is scripted.
struct TestInput {
  uint16_t buttons = 0;  // controller buttons to add this poll
  bool cursor = false;   // true: use (x, y) instead of the real mouse cursor
  float x = 0, y = 0;    // cursor position in the game's 1280x720 UI space
  bool left = false;     // left mouse button held
};

// Called once per controller poll; advances the script.
TestInput PollTestInput();

}  // namespace wwe2k14
