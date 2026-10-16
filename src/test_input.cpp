// wwe2k14 - Scripted input for automated tests (WWE2K14_TEST_INPUT=1).
//
// Writing logs\test_input.txt queues its lines; the file is deleted once read. Each line is one command:
//   A B X Y START BACK UP DOWN LEFT RIGHT LB RB   press the controller button for 100 ms, then release it for 150 ms
//   WAIT <ms>                                     do nothing for that long
//   MOUSE <x> <y>                                 put a virtual mouse cursor at (x, y) in 1280x720 UI coordinates
//   CLICK <x> <y>                                 move the virtual cursor there and click the left button
//   MOUSE OFF                                     back to the real mouse cursor
// The input reaches the game through the XInputGetState hook (mouse_input.cpp), so it works whether or not the game
// window has focus. Off unless the variable is set.

#include "test_input.h"

#include <chrono>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <system_error>

namespace wwe2k14 {
namespace {

struct Step {
  uint16_t buttons = 0;
  bool left = false;
  int press_ms = 0;
  int release_ms = 0;
  enum class Cursor { kKeep, kSet, kOff } cursor = Cursor::kKeep;
  float x = 0, y = 0;
};

std::mutex g_mutex;
std::deque<Step> g_steps;

uint16_t ButtonFor(const std::string& name) {
  static const struct {
    const char* name;
    uint16_t bit;
  } kButtons[] = {{"UP", 0x0001},    {"DOWN", 0x0002}, {"LEFT", 0x0004}, {"RIGHT", 0x0008},
                  {"START", 0x0010}, {"BACK", 0x0020}, {"LB", 0x0100},   {"RB", 0x0200},
                  {"A", 0x1000},     {"B", 0x2000},    {"X", 0x4000},    {"Y", 0x8000}};
  for (const auto& b : kButtons)
    if (name == b.name) return b.bit;
  return 0;
}

void ReadScript() {
  const std::filesystem::path path = "logs/test_input.txt";
  std::error_code ec;
  if (!std::filesystem::exists(path, ec)) return;
  std::ifstream in(path);
  std::string line;
  std::deque<Step> steps;
  while (std::getline(in, line)) {
    std::istringstream words(line);
    std::string cmd;
    if (!(words >> cmd)) continue;
    Step s;
    if (cmd == "WAIT") {
      words >> s.release_ms;
    } else if (cmd == "MOUSE" || cmd == "CLICK") {
      std::string first;
      words >> first;
      if (first == "OFF") {
        s.cursor = Step::Cursor::kOff;
      } else {
        s.cursor = Step::Cursor::kSet;
        s.x = std::strtof(first.c_str(), nullptr);
        words >> s.y;
        if (cmd == "CLICK") s.left = true, s.press_ms = 100;
        s.release_ms = 150;
      }
    } else if (uint16_t b = ButtonFor(cmd)) {
      s.buttons = b, s.press_ms = 100, s.release_ms = 150;
    } else {
      continue;
    }
    steps.push_back(s);
  }
  in.close();
  std::filesystem::remove(path, ec);
  std::lock_guard<std::mutex> lock(g_mutex);
  g_steps.insert(g_steps.end(), steps.begin(), steps.end());
}

}  // namespace

TestInput PollTestInput() {
  static const bool enabled = [] {
    const char* v = std::getenv("WWE2K14_TEST_INPUT");
    return v && *v == '1';
  }();
  TestInput out;
  if (!enabled) return out;
  using clock = std::chrono::steady_clock;
  static clock::time_point next_check{};
  static clock::time_point step_start{};
  static bool running = false;
  static bool cursor = false;
  static float cx = 0, cy = 0;
  const auto now = clock::now();
  if (now >= next_check) {
    next_check = now + std::chrono::milliseconds(200);
    ReadScript();
  }
  std::lock_guard<std::mutex> lock(g_mutex);
  if (!g_steps.empty()) {
    const Step& s = g_steps.front();
    if (!running) {
      running = true;
      step_start = now;
      if (s.cursor == Step::Cursor::kSet) cursor = true, cx = s.x, cy = s.y;
      if (s.cursor == Step::Cursor::kOff) cursor = false;
    }
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - step_start).count();
    if (elapsed < s.press_ms) {
      out.buttons = s.buttons;
      out.left = s.left;
    } else if (elapsed >= s.press_ms + s.release_ms) {
      g_steps.pop_front();
      running = false;
    }
  }
  out.cursor = cursor;
  out.x = cx;
  out.y = cy;
  return out;
}

}  // namespace wwe2k14
