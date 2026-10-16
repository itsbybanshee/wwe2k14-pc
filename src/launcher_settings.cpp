#include "launcher_settings.h"

#include <chrono>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <map>
#include <regex>
#include <sstream>
#include <string>
#include <system_error>

#include <rex/cvar.h>
#include <rex/filesystem.h>
#include <rex/logging.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace wwe2k14 {
namespace {

namespace fs = std::filesystem;

// Same rule as the launcher (LauncherPaths): inside the WWE2K14PC folder (development build output), the folder
// with wwe2k14_config.toml; otherwise the exe's own folder.
fs::path FindRoot() {
  const fs::path exe_dir = rex::filesystem::GetExecutableFolder();
  for (fs::path dir = exe_dir; !dir.empty(); dir = dir.parent_path()) {
    std::error_code ec;
    if (fs::exists(dir / "wwe2k14_config.toml", ec) ||
        fs::exists(dir / "WWE 2K14 PC.exe", ec) ||
        fs::exists(dir / "WWE2K14 Launcher.exe", ec)) {
      return dir;
    }
    if (dir == dir.parent_path()) {
      break;
    }
  }
  return exe_dir;
}

// launcher.json is flat: "Name": "text" | true | false | number.
std::map<std::string, std::string> ReadSettings(const fs::path& file) {
  std::map<std::string, std::string> out;
  std::ifstream in(file, std::ios::binary);
  if (!in) {
    return out;
  }
  std::stringstream ss;
  ss << in.rdbuf();
  const std::string text = ss.str();
  static const std::regex entry(R"re("(\w+)"\s*:\s*(?:"((?:[^"\\]|\\.)*)"|(true|false|-?\d+)))re");
  for (std::sregex_iterator it(text.begin(), text.end(), entry), end; it != end; ++it) {
    const auto& m = *it;
    std::string value;
    if (m[2].matched) {
      const std::string raw = m[2].str();
      for (size_t i = 0; i < raw.size(); ++i) {
        if (raw[i] == '\\' && i + 1 < raw.size()) {
          ++i;
        }
        value += raw[i];
      }
    } else {
      value = m[3].str();
    }
    out[m[1].str()] = value;
  }
  return out;
}

void SetIfDefault(const char* name, const std::string& value) {
  if (!rex::cvar::HasNonDefaultValue(name)) {
    rex::cvar::SetFlagByName(name, value);
  }
}

void SetEnvIfUnset(const char* name, bool on) {
  if (!std::getenv(name)) {
#if defined(_WIN32)
    _putenv_s(name, on ? "1" : "0");
#else
    setenv(name, on ? "1" : "0", 0);
#endif
  }
}

void SetEnvStrIfUnset(const char* name, const std::string& value) {
  if (!std::getenv(name)) {
#if defined(_WIN32)
    _putenv_s(name, value.c_str());
#else
    setenv(name, value.c_str(), 0);
#endif
  }
}

#if defined(_WIN32)
// Starts the other version with this process's arguments. True when it started.
bool StartOtherVersion(const fs::path& exe) {
  std::wstring args = GetCommandLineW();
  // Skip argv[0] (quoted or not).
  size_t pos = 0;
  if (!args.empty() && args[0] == L'"') {
    pos = args.find(L'"', 1);
    pos = pos == std::wstring::npos ? args.size() : pos + 1;
  } else {
    pos = args.find(L' ');
    pos = pos == std::wstring::npos ? args.size() : pos;
  }
  std::wstring command = L"\"" + exe.wstring() + L"\"" + args.substr(pos);
  STARTUPINFOW si{sizeof(si)};
  PROCESS_INFORMATION pi{};
  if (!CreateProcessW(exe.c_str(), command.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
    return false;
  }
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  return true;
}
#endif

}  // namespace

void ApplyLauncherSettings(rex::PathConfig& paths, bool started_by_launcher) {
  if (started_by_launcher) {
    return;  // The launcher passes everything on the command line.
  }
  const fs::path root = FindRoot();
  const fs::path user_data = root / "userdata";
  std::error_code ec;
  if (!rex::cvar::HasNonDefaultValue("user_data_root") && fs::is_directory(user_data, ec)) {
    paths.user_data_root = user_data;
  }
  const auto s = ReadSettings(user_data / "launcher.json");
  if (s.empty()) {
    return;
  }
  auto flag = [&](const char* name, bool fallback) {
    auto it = s.find(name);
    return it == s.end() ? fallback : it->second == "true";
  };

  // Version: the launcher's Vanilla / Title update choice, when that version is built and the update installed.
  const fs::path update_folder = user_data / "title_update";
  const bool want_tu = flag("UseTitleUpdate", false) && fs::exists(update_folder / "default.xexp", ec);
#ifdef WWE2K14_TU
  constexpr bool is_tu = true;
#else
  constexpr bool is_tu = false;
#endif
#if defined(_WIN32)
  if (want_tu != is_tu) {
    const fs::path other =
        rex::filesystem::GetExecutableFolder() / (want_tu ? "wwe2k14_tu.exe" : "wwe2k14.exe");
    if (fs::exists(other, ec) && StartOtherVersion(other)) {
      TerminateProcess(GetCurrentProcess(), 0);
    }
  }
#endif
  if (is_tu && !rex::cvar::HasNonDefaultValue("update_data_root") && fs::is_directory(update_folder, ec)) {
    paths.update_data_root = update_folder;
  }

  // Game folder.
  if (auto it = s.find("GameFolder"); it != s.end() && !it->second.empty()) {
    const fs::path game = rex::to_path(it->second);
    if (fs::exists(game / "default.xex", ec)) {
      paths.game_data_root = game;
    }
  }

  // Display: same mapping as the launcher (LauncherSettings.Display): render scale on top of 1280x720.
  const std::string resolution = s.count("Resolution") ? s.at("Resolution") : "720";
  int scale = 1, width = 1280, height = 720;
  if (resolution == "1080") {
    scale = 2, width = 1920, height = 1080;
  } else if (resolution == "1440") {
    scale = 2, width = 2560, height = 1440;
  } else if (resolution == "2160") {
    scale = 3, width = 3840, height = 2160;  // native 4K (3x 720p)
  }

  const std::string upscaler = s.count("Upscaler") ? s.at("Upscaler") : "off";
  std::string presentEffect = "bilinear";
  if (upscaler == "fsr" || upscaler == "fsr1") {
    presentEffect = "fsr";
    if (resolution == "1080" || resolution == "1440") {
      scale = 1;
    } else if (resolution == "2160") {
      scale = 2;
    }
  }

  std::string window_mode = "borderless";
  if (s.count("WindowMode") && !s.at("WindowMode").empty()) {
    window_mode = s.at("WindowMode");
  } else if (flag("Fullscreen", false)) {
    window_mode = "fullscreen";
  } else {
    window_mode = "borderless";
  }
  const bool is_fullscreen = (window_mode == "fullscreen");
  const bool is_borderless = (window_mode == "borderless");
  const bool is_windowed = (window_mode == "windowed");

#if defined(_WIN32)
  if (is_windowed) {
    RECT area{};
    if (SystemParametersInfoW(SPI_GETWORKAREA, 0, &area, 0)) {
      const int aw = area.right - area.left, ah = area.bottom - area.top;
      if (width > aw || height > ah) {
        const double f = std::min(aw * 0.95 / width, ah * 0.95 / height);
        width = int(width * f);
        height = int(height * f);
      }
    }
  } else if (is_borderless) {
    HMONITOR mon = MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    if (GetMonitorInfoW(mon, &mi)) {
      width = mi.rcMonitor.right - mi.rcMonitor.left;
      height = mi.rcMonitor.bottom - mi.rcMonitor.top;
    }
  }
#endif
  SetIfDefault("resolution_scale", std::to_string(scale));
  SetIfDefault("window_width", std::to_string(width));
  SetIfDefault("window_height", std::to_string(height));
  SetIfDefault("fullscreen", is_fullscreen ? "true" : "false");
  SetIfDefault("present_effect", presentEffect);
  SetEnvStrIfUnset("WWE2K14_WINDOW_MODE", window_mode);
  SetEnvStrIfUnset("WWE2K14_RESOLUTION", resolution);
  SetEnvStrIfUnset("WWE2K14_UPSCALE", upscaler);

  // Frame pacing switches (read from the environment by wwe2k14_app.h / frame_rate.cpp).
  SetEnvIfUnset("WWE2K14_VSYNC", flag("Vsync", true));
  SetEnvIfUnset("WWE2K14_KEEP_60", flag("Keep60", true));
  SetEnvIfUnset("WWE2K14_LOW_LATENCY", flag("LowLatency", true));
  SetEnvIfUnset("WWE2K14_SHADOWS", flag("Shadows", true));
  SetEnvIfUnset("WWE2K14_FPS_COUNTER", flag("FpsCounter", false));

  const std::string aniso = s.count("Anisotropic") ? s.at("Anisotropic") : "16x";
  int anisoVal = 5;
  if (aniso == "off" || aniso == "0") anisoVal = 0;
  else if (aniso == "4x" || aniso == "4") anisoVal = 3;
  SetIfDefault("anisotropic_override", std::to_string(anisoVal));
  SetEnvStrIfUnset("WWE2K14_ANISO", std::to_string(anisoVal));

  const std::string aa = s.count("AntiAliasing") ? s.at("AntiAliasing") : "fxaa";
  std::string aaVal = "fxaa";
  if (aa == "off" || aa == "none") aaVal = "none";
  else if (aa == "extreme" || aa == "fxaa_extreme" || aa == "max") aaVal = "fxaa_extreme";
  SetIfDefault("swap_post_effect", aaVal);
  SetEnvStrIfUnset("WWE2K14_AA", aaVal);

  // Custom soundtrack and a log file, like a launcher start.
  const fs::path music = user_data / "music";
  fs::create_directories(music, ec);
  SetIfDefault("xmp_music_dir", rex::path_to_utf8(music));
  if (!rex::cvar::HasNonDefaultValue("log_file")) {
    const fs::path logs = root / "logs";
    fs::create_directories(logs, ec);
    const std::time_t now = std::time(nullptr);
    std::tm local{};
#if defined(_WIN32)
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    char name[64];
    std::strftime(name, sizeof(name), "run-%Y%m%d-%H%M%S.log", &local);
    rex::cvar::SetFlagByName("log_file", rex::path_to_utf8(logs / name));
  }
}

}  // namespace wwe2k14
