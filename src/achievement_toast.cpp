#include "achievement_toast.h"

#include <algorithm>
#include <cfloat>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include <imgui.h>
#include <rex/logging.h>
#include <rex/ui/imgui_dialog.h>
#include <rex/ui/imgui_drawer.h>
#include <rex/ui/immediate_drawer.h>
#include <rex/ui/windowed_app_context.h>

#ifdef _WIN32
#include <objbase.h>
#include <wincodec.h>
#endif

#include "achievement_sound.h"

namespace wwe2k14 {
namespace {

// Decodes a PNG to R8G8B8A8 (what ImmediateDrawer::CreateTexture takes).
bool DecodePng(const std::string& path, uint32_t& width, uint32_t& height, std::vector<uint8_t>& rgba) {
#ifdef _WIN32
  if (path.empty())
    return false;
  const HRESULT co = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  bool ok = false;
  IWICImagingFactory* factory = nullptr;
  IWICBitmapDecoder* decoder = nullptr;
  IWICBitmapFrameDecode* frame = nullptr;
  IWICFormatConverter* converter = nullptr;
  const std::wstring wide = std::filesystem::u8path(path).wstring();
  do {
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))))
      break;
    if (FAILED(factory->CreateDecoderFromFilename(wide.c_str(), nullptr, GENERIC_READ,
                                                  WICDecodeMetadataCacheOnDemand, &decoder)) ||
        FAILED(decoder->GetFrame(0, &frame)) || FAILED(factory->CreateFormatConverter(&converter)) ||
        FAILED(converter->Initialize(frame, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0,
                                     WICBitmapPaletteTypeCustom)))
      break;
    UINT w = 0, h = 0;
    if (FAILED(converter->GetSize(&w, &h)) || !w || !h || w > 1024 || h > 1024)
      break;
    rgba.resize(size_t(w) * h * 4);
    if (FAILED(converter->CopyPixels(nullptr, w * 4, UINT(rgba.size()), rgba.data())))
      break;
    width = w;
    height = h;
    ok = true;
  } while (false);
  if (converter) converter->Release();
  if (frame) frame->Release();
  if (decoder) decoder->Release();
  if (factory) factory->Release();
  if (SUCCEEDED(co)) CoUninitialize();
  return ok;
#else
  (void)path, (void)width, (void)height, (void)rgba;
  return false;
#endif
}

ImU32 Color(float r, float g, float b, float a) { return ImGui::GetColorU32(ImVec4(r, g, b, a)); }

// Splits text into lines that fit `wrap_width` at `size` (word wrap), so each line can be drawn centred.
std::vector<std::string_view> WrapLines(ImFont* font, float size, std::string_view text, float wrap_width) {
  std::vector<std::string_view> lines;
  const char* p = text.data();
  const char* end = text.data() + text.size();
  while (p < end) {
    const char* line_end = font->CalcWordWrapPosition(size, p, end, wrap_width);
    if (line_end == p)  // a single word wider than the line: take at least one character
      ++line_end;
    std::string_view line(p, size_t(line_end - p));
    while (!line.empty() && (line.back() == ' ' || line.back() == '\n'))
      line.remove_suffix(1);
    lines.push_back(line);
    p = line_end;
    while (p < end && (*p == ' ' || *p == '\n'))
      ++p;
  }
  return lines;
}

}  // namespace

class AchievementToastDialog final : public rex::ui::ImGuiDialog {
 public:
  AchievementToastDialog(rex::ui::ImGuiDrawer* drawer, AchievementToasts* owner)
      : ImGuiDialog(drawer), drawer_(drawer), owner_(owner) {}

 protected:
  void OnDraw(ImGuiIO& io) override {
    using clock = std::chrono::steady_clock;
    constexpr float kShowSeconds = 5.0f;
    constexpr float kFadeSeconds = 0.3f;

    if (!current_) {
      {
        std::lock_guard lock(owner_->mutex_);
        if (owner_->queue_.empty()) {
          owner_->dialog_active_ = false;
          Close();
          return;
        }
        current_ = std::move(owner_->queue_.front());
        owner_->queue_.pop_front();
      }
      icon_.reset();
      uint32_t w = 0, h = 0;
      std::vector<uint8_t> rgba;
      if (drawer_->immediate_drawer() && DecodePng(current_->icon_path, w, h, rgba)) {
        icon_ = drawer_->immediate_drawer()->CreateTexture(w, h, rex::ui::ImmediateTextureFilter::kLinear, false,
                                                           rgba.data());
      }
      shown_at_ = clock::now();
      PlayWavAsync(owner_->sound_);  // one chime per pop-up, as it appears
    }
    const float t = std::chrono::duration<float>(clock::now() - shown_at_).count();
    if (t >= kShowSeconds) {
      current_.reset();  // next one (or close) on the following frame
      icon_.reset();
      return;
    }
    const float alpha = std::clamp(std::min(t, kShowSeconds - t) / kFadeSeconds, 0.0f, 1.0f);
    ImFont* regular = owner_->font_regular_ ? owner_->font_regular_ : ImGui::GetFont();
    ImFont* bold = owner_->font_bold_ ? owner_->font_bold_ : regular;

    // Proportions of the design at 1080p (about 470 x 76 px); everything scales with the window height.
    const float s = std::max(0.5f, io.DisplaySize.y / 1080.0f);
    const float pad = 7.0f * s, icon = 62.0f * s, gap = 12.0f * s, rounding = 10.0f * s;
    const float title_size = 19.0f * s, desc_size = 15.0f * s, desc_line = 17.0f * s, min_gap = 6.0f * s;
    const char* kPrefix = "Achievement unlocked: ";

    const float prefix_w = regular->CalcTextSizeA(title_size, FLT_MAX, 0.0f, kPrefix).x;
    const float name_w = bold->CalcTextSizeA(title_size, FLT_MAX, 0.0f, current_->name.c_str()).x;
    const float text_w = std::clamp(prefix_w + name_w + 16.0f * s, 370.0f * s, 640.0f * s);
    const auto lines = WrapLines(regular, desc_size, current_->description, text_w - 8.0f * s);

    // The title starts at the icon's top edge and the description ends at its bottom edge; the space
    // between them is the separation. A description too long for that grows the panel instead.
    const float desc_h = lines.size() * desc_line;
    const float content_h = std::max(icon, title_size + (lines.empty() ? 0.0f : min_gap + desc_h));
    const float box_w = pad + icon + gap + text_w + gap;
    const float box_h = content_h + 2.0f * pad;
    const ImVec2 p0(std::floor((io.DisplaySize.x - box_w) * 0.5f), std::floor(io.DisplaySize.y - box_h - 56.0f * s));
    const ImVec2 p1(p0.x + box_w, p0.y + box_h);

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    // Panel: flat dark, with a thin light border.
    dl->AddRectFilled(p0, p1, Color(0.075f, 0.08f, 0.09f, 0.96f * alpha), rounding);
    dl->AddRect(p0, p1, Color(0.82f, 0.84f, 0.88f, 0.55f * alpha), rounding, 0, std::max(1.0f, 1.2f * s));

    // Icon, vertically centred on the left.
    const float content_top = p0.y + pad;
    const ImVec2 i0(p0.x + pad, content_top + (content_h - icon) * 0.5f);
    const ImVec2 i1(i0.x + icon, i0.y + icon);
    if (icon_) {
      dl->AddImageRounded(reinterpret_cast<ImTextureID>(icon_.get()), i0, i1, ImVec2(0, 0), ImVec2(1, 1),
                          Color(1, 1, 1, alpha), 6.0f * s);
    } else {
      dl->AddRectFilled(i0, i1, Color(0.2f, 0.21f, 0.24f, alpha), 6.0f * s);
    }
    dl->AddRect(i0, i1, Color(1, 1, 1, 0.18f * alpha), 6.0f * s);

    // Text, centred horizontally in the area right of the icon: title at the top, description at the bottom.
    const float area_x = i1.x + gap;
    const float centre_x = area_x + text_w * 0.5f;
    float y = content_top;
    const float title_x = std::floor(centre_x - (prefix_w + name_w) * 0.5f);
    dl->AddText(regular, title_size, ImVec2(title_x, y), Color(1, 1, 1, alpha), kPrefix);
    dl->AddText(bold, title_size, ImVec2(title_x + prefix_w, y), Color(1.0f, 0.85f, 0.1f, alpha),
                current_->name.c_str());
    y = content_top + content_h - desc_h;
    for (const auto& line : lines) {
      const float w = regular->CalcTextSizeA(desc_size, FLT_MAX, 0.0f, line.data(), line.data() + line.size()).x;
      dl->AddText(regular, desc_size, ImVec2(std::floor(centre_x - w * 0.5f), y), Color(1, 1, 1, 0.92f * alpha),
                  line.data(), line.data() + line.size());
      y += desc_line;
    }
  }

  void OnClose() override { icon_.reset(); }

 private:
  rex::ui::ImGuiDrawer* drawer_;
  AchievementToasts* owner_;
  std::optional<AchievementToasts::Toast> current_;
  std::unique_ptr<rex::ui::ImmediateTexture> icon_;
  std::chrono::steady_clock::time_point shown_at_;
};

void AchievementToasts::Attach(rex::ui::WindowedAppContext* app_context, rex::ui::ImGuiDrawer* drawer) {
  app_context_ = app_context;
  drawer_ = drawer;
  if (!drawer_)
    return;
  // Segoe UI, baked large so it stays smooth when drawn smaller; the default ImGui font stays first.
  ImFontAtlas* fonts = drawer_->GetIO().Fonts;
  if (fonts->Fonts.empty())
    fonts->AddFontDefault();
  const std::string windows_fonts = "C:\\Windows\\Fonts\\";
  if (std::filesystem::exists(windows_fonts + "segoeui.ttf") && std::filesystem::exists(windows_fonts + "segoeuib.ttf")) {
    font_regular_ = fonts->AddFontFromFileTTF((windows_fonts + "segoeui.ttf").c_str(), 40.0f);
    font_bold_ = fonts->AddFontFromFileTTF((windows_fonts + "segoeuib.ttf").c_str(), 40.0f);
    drawer_->RebuildFontTexture();
  } else {
    REXLOG_WARN("[wwe2k14] Segoe UI not found; achievement pop-ups use the default font");
  }
}

void AchievementToasts::Detach() {
  std::lock_guard lock(mutex_);
  queue_.clear();
  app_context_ = nullptr;
  drawer_ = nullptr;
}

void AchievementToasts::Push(std::string name, std::string description, std::string icon_path) {
  rex::ui::WindowedAppContext* context;
  {
    std::lock_guard lock(mutex_);
    if (!app_context_)
      return;
    queue_.push_back({std::move(name), std::move(description), std::move(icon_path)});
    context = app_context_;
  }
  context->CallInUIThread([this] { ShowFromUIThread(); });
}

void AchievementToasts::ShowFromUIThread() {
  if (dialog_active_ || !drawer_)
    return;
  dialog_active_ = true;
  new AchievementToastDialog(drawer_, this);  // registers itself; deletes itself when closed
}

}  // namespace wwe2k14
