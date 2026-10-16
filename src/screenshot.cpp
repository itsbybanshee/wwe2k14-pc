// wwe2k14 - full-resolution screenshots (F12).
//
// Windows' own screenshot keys capture the window, i.e. the frame after it has been scaled to the display (a 4K render
// shown on a 1080p monitor is saved at 1920x1080). This captures the game's output image before that scaling, at the
// internal render resolution (3840x2160 at the launcher's 4K setting), and saves it as PNG in
// Documents\wwe2k14\Screenshots. The GPU readback runs on the UI thread; PNG encoding (Windows Imaging Component) runs
// on a worker thread so the game does not hitch while a large image is written.

#include "screenshot.h"

#include <chrono>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>
#include <utility>

#include <rex/graphics/graphics_system.h>
#include <rex/logging.h>
#include <rex/runtime.h>
#include <rex/ui/presenter.h>

#ifdef _WIN32
#include <objbase.h>
#include <shlobj.h>
#include <wincodec.h>
#endif

namespace wwe2k14 {
namespace {

std::filesystem::path ScreenshotFolder() {
#ifdef _WIN32
  PWSTR documents = nullptr;
  if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &documents))) {
    std::filesystem::path folder = std::filesystem::path(documents) / L"wwe2k14" / L"Screenshots";
    CoTaskMemFree(documents);
    return folder;
  }
#endif
  return std::filesystem::current_path() / "Screenshots";
}

std::filesystem::path NextScreenshotPath(const std::filesystem::path& folder) {
  const std::time_t now = std::time(nullptr);
  std::tm local{};
#ifdef _WIN32
  localtime_s(&local, &now);
#else
  localtime_r(&now, &local);
#endif
  char stamp[32];
  std::strftime(stamp, sizeof(stamp), "%Y-%m-%d_%H-%M-%S", &local);
  std::filesystem::path path = folder / (std::string("WWE2K14_") + stamp + ".png");
  // Several shots within the same second get _2, _3...
  for (int n = 2; std::filesystem::exists(path); ++n) {
    path = folder / (std::string("WWE2K14_") + stamp + "_" + std::to_string(n) + ".png");
  }
  return path;
}

#ifdef _WIN32
// RawImage is R8 G8 B8 X8; WIC's 32bppRGB is B G R X, so swap R and B, then encode as 24-bit PNG.
// If target_width and target_height are specified and differ from image dimensions, downscale/scale with WIC.
bool WritePng(const std::filesystem::path& path, rex::ui::RawImage& image, uint32_t target_width = 0,
              uint32_t target_height = 0) {
  for (uint32_t y = 0; y < image.height; ++y) {
    uint8_t* row = image.data.data() + y * image.stride;
    for (uint32_t x = 0; x < image.width; ++x) {
      std::swap(row[x * 4 + 0], row[x * 4 + 2]);
      row[x * 4 + 3] = 0xFF;
    }
  }
  const HRESULT co = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  bool ok = false;
  IWICImagingFactory* factory = nullptr;
  IWICStream* stream = nullptr;
  IWICBitmapEncoder* encoder = nullptr;
  IWICBitmapFrameEncode* frame = nullptr;
  IWICBitmap* bitmap = nullptr;
  IWICBitmapScaler* scaler = nullptr;
  do {
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&factory))))
      break;
    if (FAILED(factory->CreateBitmapFromMemory(image.width, image.height, GUID_WICPixelFormat32bppBGR,
                                               static_cast<UINT>(image.stride),
                                               static_cast<UINT>(image.data.size()),
                                               image.data.data(), &bitmap)))
      break;
    if (FAILED(factory->CreateStream(&stream)) ||
        FAILED(stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE)))
      break;
    if (FAILED(factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder)) ||
        FAILED(encoder->Initialize(stream, WICBitmapEncoderNoCache)))
      break;
    if (FAILED(encoder->CreateNewFrame(&frame, nullptr)) || FAILED(frame->Initialize(nullptr)))
      break;

    IWICBitmapSource* source = bitmap;
    UINT width = image.width, height = image.height;
    if (target_width > 0 && target_height > 0 &&
        (target_width != image.width || target_height != image.height)) {
      if (SUCCEEDED(factory->CreateBitmapScaler(&scaler))) {
        if (SUCCEEDED(scaler->Initialize(bitmap, target_width, target_height,
                                         WICBitmapInterpolationModeFant))) {
          source = scaler;
          width = target_width;
          height = target_height;
        }
      }
    }

    WICPixelFormatGUID format = GUID_WICPixelFormat24bppBGR;
    if (FAILED(frame->SetSize(width, height)) || FAILED(frame->SetPixelFormat(&format)))
      break;
    // WriteSource converts 32bppBGR to the frame's 24bppBGR format.
    if (FAILED(frame->WriteSource(source, nullptr)) || FAILED(frame->Commit()) || FAILED(encoder->Commit()))
      break;
    ok = true;
  } while (false);
  if (scaler) scaler->Release();
  if (frame) frame->Release();
  if (encoder) encoder->Release();
  if (stream) stream->Release();
  if (bitmap) bitmap->Release();
  if (factory) factory->Release();
  if (SUCCEEDED(co)) CoUninitialize();
  return ok;
}
#endif

}  // namespace

void TakeScreenshot(rex::Runtime* runtime) {
  auto* graphics = runtime ? static_cast<rex::graphics::GraphicsSystem*>(runtime->graphics_system()) : nullptr;
  rex::ui::Presenter* presenter = graphics ? graphics->presenter() : nullptr;
  if (!presenter) {
    REXLOG_WARN("[wwe2k14-screenshot] no presenter yet");
    return;
  }
  auto image = std::make_shared<rex::ui::RawImage>();
  if (!presenter->CaptureGuestOutput(*image) || !image->width || !image->height) {
    REXLOG_WARN("[wwe2k14-screenshot] nothing to capture yet (no game frame presented)");
    return;
  }

  uint32_t target_width = image->width;
  uint32_t target_height = image->height;
  const char* res_env = std::getenv("WWE2K14_RESOLUTION");
  if (res_env) {
    if (std::strcmp(res_env, "1080") == 0) {
      target_width = 1920;
      target_height = 1080;
    } else if (std::strcmp(res_env, "720") == 0) {
      target_width = 1280;
      target_height = 720;
    } else if (std::strcmp(res_env, "1440") == 0) {
      target_width = 2560;
      target_height = 1440;
    } else if (std::strcmp(res_env, "2160") == 0) {
      target_width = 3840;
      target_height = 2160;
    }
  } else {
    const char* win_w = std::getenv("WWE2K14_WINDOW_WIDTH");
    const char* win_h = std::getenv("WWE2K14_WINDOW_HEIGHT");
    if (image->width == 2560 && image->height == 1440) {
      if ((win_w && std::strcmp(win_w, "1920") == 0) || (win_h && std::strcmp(win_h, "1080") == 0)) {
        target_width = 1920;
        target_height = 1080;
      }
    }
  }

#ifdef _WIN32
  std::thread([image, target_width, target_height] {
    const std::filesystem::path folder = ScreenshotFolder();
    std::error_code ec;
    std::filesystem::create_directories(folder, ec);
    const std::filesystem::path path = NextScreenshotPath(folder);
    if (WritePng(path, *image, target_width, target_height)) {
      REXLOG_INFO("[wwe2k14-screenshot] saved {}x{} to {}", target_width, target_height,
                  rex::path_to_utf8(path));
    } else {
      REXLOG_ERROR("[wwe2k14-screenshot] could not write {}", rex::path_to_utf8(path));
    }
  }).detach();
#else
  REXLOG_WARN("[wwe2k14-screenshot] PNG saving is only implemented on Windows");
#endif
}

}  // namespace wwe2k14
