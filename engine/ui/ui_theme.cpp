#include "engine/ui/ui_theme.h"
#include "engine/sim/simulation.h"

#ifdef DOM_HAS_IMGUI
#include <algorithm>

namespace dom::ui::theme {

ScopedHudTheme::ScopedHudTheme(float uiScale, const ImVec4& civAccent) {
  ImGuiStyle& style = ImGui::GetStyle();
  const float scale = std::clamp(uiScale, 0.8f, 2.5f);
  baseFrameRounding_ = style.FrameRounding;
  baseWindowRounding_ = style.WindowRounding;
  baseFramePadding_ = style.FramePadding;
  baseItemSpacing_ = style.ItemSpacing;
  baseWindowBorder_ = style.WindowBorderSize;

  style.FrameRounding = 6.0f * scale;
  style.WindowRounding = 8.0f * scale;
  style.FramePadding = ImVec2(8.0f * scale, 5.0f * scale);
  style.ItemSpacing = ImVec2(9.0f * scale, 7.0f * scale);
  style.WindowBorderSize = 1.0f;

  ImVec4* colors = style.Colors;
  colors[ImGuiCol_WindowBg] = ImVec4(0.07f, 0.08f, 0.10f, 0.92f);
  colors[ImGuiCol_ChildBg] = ImVec4(0.09f, 0.10f, 0.12f, 0.75f);
  colors[ImGuiCol_Header] = ImVec4(civAccent.x * 0.75f, civAccent.y * 0.75f, civAccent.z * 0.75f, 0.72f);
  colors[ImGuiCol_HeaderHovered] = ImVec4(civAccent.x, civAccent.y, civAccent.z, 0.92f);
  colors[ImGuiCol_Button] = ImVec4(civAccent.x * 0.65f, civAccent.y * 0.65f, civAccent.z * 0.65f, 0.72f);
  colors[ImGuiCol_ButtonHovered] = ImVec4(civAccent.x, civAccent.y, civAccent.z, 0.90f);
  colors[ImGuiCol_Border] = ImVec4(civAccent.x * 0.6f, civAccent.y * 0.6f, civAccent.z * 0.6f, 0.55f);
}

ScopedHudTheme::~ScopedHudTheme() {
  ImGuiStyle& style = ImGui::GetStyle();
  style.FrameRounding = baseFrameRounding_;
  style.WindowRounding = baseWindowRounding_;
  style.FramePadding = baseFramePadding_;
  style.ItemSpacing = baseItemSpacing_;
  style.WindowBorderSize = baseWindowBorder_;
}

ImVec4 civ_accent(const dom::sim::World& world, uint16_t player) {
  if (player >= world.players.size()) return ImVec4(0.35f, 0.55f, 0.95f, 1.0f);
  const auto& civ = world.players[player].civilization;
  const float r = std::clamp(0.25f + civ.militaryBias * 0.45f, 0.2f, 0.95f);
  const float g = std::clamp(0.25f + civ.economyBias * 0.45f, 0.2f, 0.95f);
  const float b = std::clamp(0.25f + civ.scienceBias * 0.45f, 0.2f, 0.95f);
  return ImVec4(r, g, b, 1.0f);
}

ImVec4 state_color_warning() { return ImVec4(1.0f, 0.70f, 0.20f, 1.0f); }
ImVec4 state_color_success() { return ImVec4(0.35f, 0.85f, 0.45f, 1.0f); }
ImVec4 state_color_failure() { return ImVec4(0.95f, 0.35f, 0.30f, 1.0f); }
ImVec4 state_color_info() { return ImVec4(0.60f, 0.75f, 1.0f, 1.0f); }

void section_header(const char* label) {
  ImGui::Spacing();
  ImGui::SeparatorText(label);
}

void state_text(const char* prefix, const char* value, const ImVec4& color) {
  ImGui::TextUnformatted(prefix);
  ImGui::SameLine();
  ImGui::PushStyleColor(ImGuiCol_Text, color);
  ImGui::TextUnformatted(value);
  ImGui::PopStyleColor();
}

} // namespace dom::ui::theme
#endif
