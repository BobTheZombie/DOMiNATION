#pragma once

#include <cstdint>

#ifdef DOM_HAS_IMGUI
#include <imgui.h>
#endif

namespace dom::sim {
struct World;
}

namespace dom::ui::theme {

#ifdef DOM_HAS_IMGUI
struct ScopedHudTheme {
  ScopedHudTheme(float uiScale, const ImVec4& civAccent);
  ~ScopedHudTheme();
  ScopedHudTheme(const ScopedHudTheme&) = delete;
  ScopedHudTheme& operator=(const ScopedHudTheme&) = delete;

private:
  float baseFrameRounding_{};
  float baseWindowRounding_{};
  ImVec2 baseFramePadding_{};
  ImVec2 baseItemSpacing_{};
  float baseWindowBorder_{};
};

ImVec4 civ_accent(const dom::sim::World& world, uint16_t player);
ImVec4 state_color_warning();
ImVec4 state_color_success();
ImVec4 state_color_failure();
ImVec4 state_color_info();

void section_header(const char* label);
void state_text(const char* prefix, const char* value, const ImVec4& color);
#endif

} // namespace dom::ui::theme
