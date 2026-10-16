// wwe2k14 - WWE 2K14 (Xbox 360) static recompilation with the ReXGlue SDK.
// Customise the app by overriding virtual hooks from rex::ReXApp.

#pragma once

#include <cstdlib>
#include <cstring>
#include <memory>
#include <string_view>

#include <rex/cvar.h>
#include <rex/filesystem.h>
#include <rex/graphics/vulkan/graphics_system.h>
#include <rex/logging.h>
#include <rex/runtime.h>
#include <rex/rex_app.h>
#include <rex/system/xam/achievements.h>
#include <rex/ui/keybinds.h>

#include "achievement_sound.h"
#include "achievement_toast.h"
#include "disc_image.h"
#include "display_mode.h"
#include "fps_overlay.h"
#include "launcher_settings.h"
#include "screenshot.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

class Wwe2k14App : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<Wwe2k14App>(new Wwe2k14App(ctx, "wwe2k14", PPCImageConfig));
  }

  std::string_view GetWindowTitle() const override { return "WWE 2K14"; }

  // Started without the launcher (no game folder argument, e.g. double-clicking the exe): use the settings last
  // saved in the launcher (launcher_settings.cpp).
  void OnConfigurePaths(rex::PathConfig& paths) override {
    wwe2k14::ApplyLauncherSettings(paths, GetArgument("game_directory").has_value());
    // The game can also be a disc image (.iso): mounted in place of the folder before default.xex loads.
    wwe2k14::UseDiscImage(paths.game_data_root, paths.user_data_root);
  }

  void OnLoadXexImage(std::string& xex_image) override {
    (void)xex_image;
    wwe2k14::MountDiscImage(runtime());
  }

  void OnPreSetup(rex::RuntimeConfig& config) override {
    // The SDK picks D3D12 whenever it is built in. With WWE 2K14, D3D12 hangs the GPU a few
    // seconds into the attract match (DEVICE_HUNG, first boot 2026-10-12); Vulkan runs full matches.
    // WWE2K14_GPU=d3d12 keeps the SDK default for comparison.
    if (!EnvIs("WWE2K14_GPU", "d3d12")) {
      config.graphics = REX_GRAPHICS_BACKEND(rex::graphics::vulkan::VulkanGraphicsSystem);
    }
    REXLOG_INFO("[wwe2k14] graphics backend: {}", EnvIs("WWE2K14_GPU", "d3d12") ? "D3D12" : "Vulkan");

    // The game reads/writes the Xbox utility cache partition (cache:\EditEx.cac, cache:\CAR.cac:
    // Create mode / arena data). Back it with a folder in the user data root.
    rex::cvar::SetFlagByName("mount_cache", "true");

    // Enable keyboard and mouse controller emulation.
    rex::cvar::SetFlagByName("mnk_mode", "true");

    // Frame pacing. The SDK's Vulkan presenter prefers IMMEDIATE (no vsync: tears every frame), then
    // MAILBOX (tear-free fast vsync / triple buffering: drops stale frames and never stalls presentation).
    // Strict FIFO waits for host display's vblank: on a 75/120/144/240 Hz monitor, strict FIFO
    // halves 60 FPS to 37.5 / 30 FPS. Mailbox delivers tear-free 60 FPS on any refresh rate.
    // WWE2K14_VSYNC=0 enables immediate tearing mode.
    // WWE2K14_MAILBOX=0 forces strict FIFO vsync for testing.
    const bool vsync = !EnvIs("WWE2K14_VSYNC", "0");
    const bool allow_mailbox = !EnvIs("WWE2K14_MAILBOX", "0");
    rex::cvar::SetFlagByName("vulkan_allow_present_mode_immediate", vsync ? "false" : "true");
    rex::cvar::SetFlagByName("vulkan_allow_present_mode_mailbox", allow_mailbox ? "true" : "false");
    rex::cvar::SetFlagByName("vulkan_allow_present_mode_fifo_relaxed", "true");
    rex::cvar::SetFlagByName("display_refresh_match", "false");
    // With 60 Hz display mode switch on launch, locking guest vblank to host display vblank
    // eliminates clock drift, frame cadence flutter, and micro-stutter during entrances and victory motions.
    // If running at an incompatible refresh rate, the engine automatically falls back to timer vblank.
    rex::cvar::SetFlagByName("vsync_host_display", EnvIs("WWE2K14_HOST_VSYNC", "0") ? "false" : "true");
    // Swapchain image count:
    // Requesting 2 images provides low-latency double buffering, eliminating frame queue delay
    // for snappy, responsive controller and keyboard menu navigation.
    const bool low_latency = !EnvIs("WWE2K14_LOW_LATENCY", "0");
    rex::cvar::SetFlagByName("vulkan_swapchain_image_count", low_latency ? "2" : "0");

    const char* aaEnv = std::getenv("WWE2K14_AA");
    if (aaEnv && *aaEnv) {
      rex::cvar::SetFlagByName("swap_post_effect", aaEnv);
    }
    const char* anisoEnv = std::getenv("WWE2K14_ANISO");
    if (anisoEnv && *anisoEnv) {
      rex::cvar::SetFlagByName("anisotropic_override", anisoEnv);
    }

    const char* upscaleEnv = std::getenv("WWE2K14_UPSCALE");
    if (upscaleEnv && *upscaleEnv) {
      std::string upscale = upscaleEnv;
      for (char& c : upscale) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
      if (upscale == "fsr" || upscale == "fsr1") {
        rex::cvar::SetFlagByName("present_effect", "fsr");
      } else if (upscale == "cas") {
        rex::cvar::SetFlagByName("present_effect", "cas");
      } else if (upscale == "bilinear" || upscale == "off" || upscale == "none") {
        rex::cvar::SetFlagByName("present_effect", "bilinear");
      } else {
        rex::cvar::SetFlagByName("present_effect", upscale);
      }
    }
    const char* fsrSharpness = std::getenv("WWE2K14_FSR_SHARPNESS");
    if (fsrSharpness && *fsrSharpness) {
      rex::cvar::SetFlagByName("present_fsr_sharpness_reduction", fsrSharpness);
    }
    const char* casSharpness = std::getenv("WWE2K14_CAS_SHARPNESS");
    if (casSharpness && *casSharpness) {
      rex::cvar::SetFlagByName("present_cas_additional_sharpness", casSharpness);
    }

    REXLOG_INFO("[wwe2k14] present: {}, guest vblank: {}, swapchain images: {}, shadows: {}, AA: {}, Aniso: {}, upscaler: {}",
                vsync ? (allow_mailbox ? "Mailbox (vsync)" : "FIFO (vsync)") : "immediate",
                rex::cvar::GetFlagByName("vsync_host_display") == "true" ? "host display" : "timer",
                rex::cvar::GetFlagByName("vulkan_swapchain_image_count") == "2" ? "2 (low latency)" : "3",
                !EnvIs("WWE2K14_SHADOWS", "0") ? "on" : "off",
                rex::cvar::GetFlagByName("swap_post_effect"),
                rex::cvar::GetFlagByName("anisotropic_override"),
                rex::cvar::GetFlagByName("present_effect"));

    // First-time shaders (a new camera shot, a finisher flash, the Breaking Point bar, a DLC model)
    // compile in the background. By default the SDK then throws the whole frame away, which shows as
    // a stutter; 22 of 25 measured hitches were exactly that. Presenting the frame with only the
    // not-yet-ready draw missing (for a frame or two, once per shader: they are cached on disk) is
    // far less visible. WWE2K14_SKIP_INCOMPLETE=1 restores frame dropping.
    rex::cvar::SetFlagByName("vulkan_async_skip_incomplete_frames",
                             EnvIs("WWE2K14_SKIP_INCOMPLETE", "1") ? "true" : "false");

    // Texture memory: the SDK's defaults (soft 384 MB, hard 768 MB, + 24 MB of render-to-texture per extra scale step)
    // date from 1 GB GPUs. At 3x (native 4K) post-processing, camera zooms, and major moves require multiple
    // 4K render targets simultaneously. Hard limit at 3x now 8192 + 256 * 8 = 10.2 GB.
    if (!rex::cvar::HasNonDefaultValue("texture_cache_memory_limit_soft")) {
      rex::cvar::SetFlagByName("texture_cache_memory_limit_soft", "4096");
    }
    if (!rex::cvar::HasNonDefaultValue("texture_cache_memory_limit_hard")) {
      rex::cvar::SetFlagByName("texture_cache_memory_limit_hard", "8192");
    }
    if (!rex::cvar::HasNonDefaultValue("texture_cache_memory_limit_render_to_texture")) {
      rex::cvar::SetFlagByName("texture_cache_memory_limit_render_to_texture", "256");
    }

    // WWE 2K14's depth buffers are float24, which Vulkan has no format for. Converting in the pixel
    // shader avoids z-fighting and shadow artefacts.
    if (!EnvIs("WWE2K14_DEPTH_F24_CONVERT", "0")) {
      rex::cvar::SetFlagByName("depth_float24_convert_in_pixel_shader", "true");
    }
    REXLOG_INFO("[wwe2k14] depth_float24_convert_in_pixel_shader={}",
                rex::cvar::GetFlagByName("depth_float24_convert_in_pixel_shader"));
  }

  void OnPostSetup() override {
    rex::ui::RegisterBind("bind_toggle_fullscreen", "F11", "Toggle fullscreen / windowed", [this] {
#if defined(_WIN32)
      ToggleWindowMode();
#else
      if (auto* w = window()) {
        w->SetFullscreen(!w->IsFullscreen());
      }
#endif
    });
    // Full internal-resolution screenshot (a 4K render saves at 3840x2160 even on a 1080p screen).
    rex::ui::RegisterBind("bind_screenshot", "F12",
                          "Save a full-resolution screenshot to Documents\\wwe2k14\\Screenshots",
                          [this] { wwe2k14::TakeScreenshot(runtime()); });
  }

  // Achievements: the SDK keeps unlocks in userdata\achievements\545408B2.txt (achievements.cpp);
  // each new one shows an "Achievement unlocked" pop-up here.
  void OnCreateDialogs(rex::ui::ImGuiDrawer* drawer) override {
#if defined(_WIN32)
    SetupWindowMode();
#endif
    // Fullscreen on a 75 / 144 / 165 Hz display: switch it to 60 Hz while fullscreen (display_mode.cpp). Here, not in
    // OnPostSetup: the window is created after OnPostSetup.
    display_mode_.Attach(window());
    fps_overlay_.Attach(drawer);
    achievement_toasts_.Attach(&app_context(), drawer);
    achievement_toasts_.SetSound(
        wwe2k14::LoadOggAsWav(rex::filesystem::GetExecutableFolder() / "achievement.ogg"));
    rex::system::xam::SetAchievementUnlockedHandler([this](const rex::system::xam::AchievementInfo& a) {
      achievement_toasts_.Push(a.name, a.description, a.icon_path);
    });
  }

  // The title's achievement table is readable once the module is set up for launch (not in
  // OnPreLaunchModule: the kernel has no executable module yet there). Writes the full list with
  // unlock state, plus the icons, for the launcher's Achievements panel.
  void OnPostLaunchModule(rex::system::XThread* thread) override {
    (void)thread;
    rex::system::xam::WriteAchievementSummary();
  }

  void OnShutdown() override {
    rex::ui::UnregisterBind("bind_toggle_fullscreen");
    rex::ui::UnregisterBind("bind_screenshot");
    display_mode_.Detach();
    fps_overlay_.Detach();
    rex::system::xam::SetAchievementUnlockedHandler(nullptr);
    achievement_toasts_.Detach();
  }

 private:
  wwe2k14::AchievementToasts achievement_toasts_;
  wwe2k14::DisplayMode60 display_mode_;
  wwe2k14::FpsOverlay fps_overlay_;

#if defined(_WIN32)
  bool is_borderless_ = false;
  RECT pre_borderless_rect_{};
  DWORD pre_borderless_style_ = 0;

  void SetupWindowMode() {
    HWND hwnd = static_cast<HWND>(window()->GetNativeWindowHandle());
    if (!hwnd) return;

    const char* modeEnv = std::getenv("WWE2K14_WINDOW_MODE");
    std::string mode = modeEnv ? modeEnv : "";
    if (mode.empty()) {
      mode = rex::cvar::GetFlagByName("fullscreen") == "true" ? "fullscreen" : "borderless";
    }

    if (mode == "borderless") {
      HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
      MONITORINFO mi{};
      mi.cbSize = sizeof(mi);
      if (GetMonitorInfoW(monitor, &mi)) {
        pre_borderless_style_ = GetWindowLongW(hwnd, GWL_STYLE);
        GetWindowRect(hwnd, &pre_borderless_rect_);

        DWORD style = pre_borderless_style_;
        style &= ~WS_OVERLAPPEDWINDOW;
        style |= WS_POPUP | WS_VISIBLE;
        SetWindowLongW(hwnd, GWL_STYLE, style);

        int x = mi.rcMonitor.left;
        int y = mi.rcMonitor.top;
        int w = mi.rcMonitor.right - mi.rcMonitor.left;
        int h = mi.rcMonitor.bottom - mi.rcMonitor.top;

        SetWindowPos(hwnd, HWND_TOP, x, y, w, h,
                     SWP_NOOWNERZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        is_borderless_ = true;
        REXLOG_INFO("[wwe2k14] Borderless window mode applied: {}x{} at ({}, {})", w, h, x, y);
      }
    }

    RECT client_rect{};
    GetClientRect(hwnd, &client_rect);
    int client_w = client_rect.right - client_rect.left;
    int client_h = client_rect.bottom - client_rect.top;
    if (client_w <= 0 || client_h <= 0) {
      RECT win_rect{};
      GetWindowRect(hwnd, &win_rect);
      client_w = win_rect.right - win_rect.left;
      client_h = win_rect.bottom - win_rect.top;
    }

    int scale = 1;
    auto scale_str = rex::cvar::GetFlagByName("draw_resolution_scale");
    if (!scale_str.empty()) {
      scale = std::max(1, std::atoi(scale_str.c_str()));
    }
    const int render_w = 1280 * scale;
    const int render_h = 720 * scale;
    const std::string effect = rex::cvar::GetFlagByName("present_effect");
    REXLOG_INFO("[wwe2k14] Display Pipeline: Internal Render {}x{} (scale {}) -> Output {}x{} ({}) | Upscaler: {}",
                render_w, render_h, scale, client_w, client_h, mode,
                effect.empty() ? "bilinear" : effect);
  }

  void ToggleWindowMode() {
    HWND hwnd = static_cast<HWND>(window()->GetNativeWindowHandle());
    if (!hwnd) return;

    if (is_borderless_) {
      SetWindowLongW(hwnd, GWL_STYLE, pre_borderless_style_ ? pre_borderless_style_ : (WS_OVERLAPPEDWINDOW | WS_VISIBLE));
      int w = pre_borderless_rect_.right - pre_borderless_rect_.left;
      int h = pre_borderless_rect_.bottom - pre_borderless_rect_.top;
      if (w <= 0 || h <= 0) { w = 1280; h = 720; }
      SetWindowPos(hwnd, nullptr, pre_borderless_rect_.left, pre_borderless_rect_.top, w, h,
                   SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
      is_borderless_ = false;
      REXLOG_INFO("[wwe2k14] Switched from borderless to windowed");
    } else if (window()->IsFullscreen()) {
      window()->SetFullscreen(false);
    } else {
      HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
      MONITORINFO mi{};
      mi.cbSize = sizeof(mi);
      if (GetMonitorInfoW(monitor, &mi)) {
        pre_borderless_style_ = GetWindowLongW(hwnd, GWL_STYLE);
        GetWindowRect(hwnd, &pre_borderless_rect_);
        DWORD style = pre_borderless_style_;
        style &= ~WS_OVERLAPPEDWINDOW;
        style |= WS_POPUP | WS_VISIBLE;
        SetWindowLongW(hwnd, GWL_STYLE, style);
        int x = mi.rcMonitor.left;
        int y = mi.rcMonitor.top;
        int w = mi.rcMonitor.right - mi.rcMonitor.left;
        int h = mi.rcMonitor.bottom - mi.rcMonitor.top;
        SetWindowPos(hwnd, HWND_TOP, x, y, w, h,
                     SWP_NOOWNERZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        is_borderless_ = true;
        REXLOG_INFO("[wwe2k14] Switched from windowed to borderless");
      }
    }
  }
#endif

  static bool EnvIs(const char* name, const char* value) {
    const char* v = std::getenv(name);
    return v != nullptr && std::strcmp(v, value) == 0;
  }
};
