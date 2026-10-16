// wwe2k14 - Running the game from a disc image (.iso).
//
// The SDK mounts the game folder as \Device\Harddisk0\Partition1 (game:\ and d:\ link there) and registers a catch-all
// NullDevice for \Device\Harddisk0 after it. Devices resolve in registration order by mount-path prefix, so to put an
// image in place of the folder both are unregistered, the image is registered, and the NullDevice comes back after it.
// The image is read by the SDK's Xbox 360 disc reader (DiscImageDevice, XGD layouts). Enabled mods come from a folder
// the launcher stages and are laid over the image's files (see ApplyOverlay).

#include "disc_image.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <memory>
#include <string>
#include <system_error>

#include <rex/filesystem/devices/host_path_device.h>
#include <rex/filesystem/devices/null_device.h>
#include <rex/filesystem/vfs.h>
#include <rex/logging.h>
#include <rex/runtime.h>

#include "../sdk/rexglue/src/filesystem/devices/disc_image_device.h"

#if defined(_WIN32)
#include <windows.h>
#endif

namespace wwe2k14 {
namespace {

std::filesystem::path g_image;

// Mod files laid over the image. The launcher stages the enabled mods in a folder (WWE2K14_GAME_OVERLAY); its files
// are read through a host folder device and their entries are moved into the image's directory tree, replacing files
// of the same name and adding new ones. Files are opened as "parent directory entry -> child", so the tree itself has
// to hold them. The host device stays alive because its entries read through it.
std::unique_ptr<rex::filesystem::Device> g_overlay;

struct EntryAccess : rex::filesystem::Entry {
  static constexpr auto kChildren = &EntryAccess::children_;
};

bool SameName(const std::string& a, const std::string& b) {
  return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
           return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
         });
}

int Graft(rex::filesystem::Entry* into, rex::filesystem::Entry* from) {
  auto& target = into->*EntryAccess::kChildren;
  auto& source = from->*EntryAccess::kChildren;
  int files = 0;
  for (auto& child : source) {
    auto existing = std::find_if(target.begin(), target.end(),
                                 [&](const auto& e) { return SameName(e->name(), child->name()); });
    bool is_dir = child->attributes() & rex::filesystem::kFileAttributeDirectory;
    if (is_dir && existing != target.end() && ((*existing)->attributes() & rex::filesystem::kFileAttributeDirectory)) {
      files += Graft(existing->get(), child.get());
      continue;
    }
    REXLOG_INFO("[wwe2k14] mod file: {}", child->path());
    files++;
    if (existing != target.end()) {
      *existing = std::move(child);
    } else {
      target.push_back(std::move(child));
    }
  }
  source.erase(std::remove(source.begin(), source.end(), nullptr), source.end());
  return files;
}

void ApplyOverlay(rex::filesystem::Device* image) {
  const char* folder = std::getenv("WWE2K14_GAME_OVERLAY");
  std::error_code ec;
  if (!folder || !*folder || !std::filesystem::is_directory(folder, ec)) return;
  auto overlay = std::make_unique<rex::filesystem::HostPathDevice>("\\Device\\WWE2K14Mods",
                                                                    std::filesystem::path(folder), true);
  if (!overlay->Initialize()) return;
  auto* image_root = image->ResolvePath("");
  auto* overlay_root = overlay->ResolvePath("");
  if (!image_root || !overlay_root) return;
  int count = Graft(image_root, overlay_root);
  REXLOG_INFO("[wwe2k14] {} mod file(s) laid over the disc image from {}", count, folder);
  g_overlay = std::move(overlay);
}

bool IsDiscImage(const std::filesystem::path& path) {
  std::error_code ec;
  if (!std::filesystem::is_regular_file(path, ec)) return false;
  std::string ext = path.extension().string();
  for (char& c : ext) c = char(std::tolower(static_cast<unsigned char>(c)));
  return ext == ".iso";
}

void ReportFailure(const std::filesystem::path& image) {
  REXLOG_ERROR("[wwe2k14] could not read the disc image {}", rex::path_to_utf8(image));
#if defined(_WIN32)
  const std::wstring text = L"Could not read the game disc image:\n" + image.wstring() +
                            L"\n\nChoose an Xbox 360 WWE 2K14 .iso, or the extracted game folder, in the launcher.";
  MessageBoxW(nullptr, text.c_str(), L"WWE 2K14", MB_OK | MB_ICONERROR);
#endif
}

}  // namespace

bool UseDiscImage(std::filesystem::path& game_root, const std::filesystem::path& user_data_root) {
  if (!IsDiscImage(game_root)) return false;
  g_image = std::filesystem::absolute(game_root);
  std::filesystem::path placeholder = (user_data_root.empty() ? g_image.parent_path() : user_data_root) / "disc_mount";
  std::error_code ec;
  std::filesystem::create_directories(placeholder, ec);
  game_root = placeholder;
  REXLOG_INFO("[wwe2k14] game disc image: {}", rex::path_to_utf8(g_image));
  return true;
}

void MountDiscImage(rex::Runtime* runtime) {
  if (g_image.empty() || !runtime || !runtime->file_system()) return;
  auto* fs = runtime->file_system();
  constexpr const char* kGameMount = "\\Device\\Harddisk0\\Partition1";
  auto image = std::make_unique<rex::filesystem::DiscImageDevice>(kGameMount, g_image);
  if (!image->Initialize()) {
    ReportFailure(g_image);
    return;
  }
  fs->UnregisterDevice(kGameMount);
  fs->UnregisterDevice("\\Device\\Harddisk0");
  ApplyOverlay(image.get());
  fs->RegisterDevice(std::move(image));
  auto null_device = std::make_unique<rex::filesystem::NullDevice>(
      "\\Device\\Harddisk0", std::initializer_list<std::string>{"\\Partition0", "\\Cache0", "\\Cache1"});
  if (null_device->Initialize()) fs->RegisterDevice(std::move(null_device));
  REXLOG_INFO("[wwe2k14] mounted the disc image as the game drive");
}

}  // namespace wwe2k14
