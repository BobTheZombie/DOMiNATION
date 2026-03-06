#include "engine/tools/asset_browser.h"

#ifdef DOM_HAS_IMGUI
#include <imgui.h>
#endif

#include <algorithm>
#include <string>

namespace dom::tools {

void AssetBrowser::draw(dom::assets::AssetManager& assets) {
#ifdef DOM_HAS_IMGUI
  if (!visible_) {
    return;
  }

  if (!ImGui::Begin("Asset Browser", &visible_)) {
    ImGui::End();
    return;
  }

  const auto& status = assets.status();
  ImGui::Text("Manifest status: %s", status.valid ? "OK" : "INVALID");
  if (!status.errors.empty()) {
    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Errors: %d", static_cast<int>(status.errors.size()));
  }
  if (!status.warnings.empty()) {
    ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.4f, 1.0f), "Warnings: %d", static_cast<int>(status.warnings.size()));
  }

  ImGui::InputText("Search asset id", search_, sizeof(search_));
  ImGui::InputText("Category/type", category_, sizeof(category_));
  ImGui::InputText("Civilization", civilization_, sizeof(civilization_));
  ImGui::InputText("Biome tag", biome_, sizeof(biome_));

  ImGui::SeparatorText("Assets");
  int shown = 0;
  int index = 0;
  for (const auto& [assetId, asset] : assets.registry().assets()) {
    const auto idMatch = std::string(search_).empty() || assetId.find(search_) != std::string::npos;
    const auto categoryMatch = std::string(category_).empty() || asset.type.find(category_) != std::string::npos;
    const auto civMatch = std::string(civilization_).empty() || asset.civilizationTheme.find(civilization_) != std::string::npos;
    const bool biomeMatch = std::string(biome_).empty() || std::find(asset.biomeTags.begin(), asset.biomeTags.end(), std::string(biome_)) != asset.biomeTags.end();
    if (!(idMatch && categoryMatch && civMatch && biomeMatch)) {
      ++index;
      continue;
    }
    if (ImGui::Selectable(assetId.c_str(), selectedIndex_ == index)) {
      selectedIndex_ = index;
    }
    ++shown;
    ++index;
  }
  ImGui::Text("Shown: %d", shown);

  ImGui::SeparatorText("Preview");
  if (selectedIndex_ >= 0) {
    int i = 0;
    for (const auto& [assetId, asset] : assets.registry().assets()) {
      if (i != selectedIndex_) {
        ++i;
        continue;
      }
      ImGui::Text("Asset: %s", assetId.c_str());
      ImGui::Text("Type: %s", asset.type.c_str());
      ImGui::Text("Civilization: %s", asset.civilizationTheme.c_str());
      ImGui::Text("Mesh: %s", asset.meshPath.c_str());
      ImGui::Text("Icon: %s", asset.iconPath.c_str());
      if (!asset.lodIds.empty()) {
        ImGui::Text("LOD entries:");
        for (const auto& lod : asset.lodIds) {
          const auto* mesh = assets.get_mesh(lod);
          ImGui::BulletText("%s -> %s", lod.c_str(), mesh ? mesh->sourcePath.c_str() : "<fallback>");
        }
      }
      const auto* sprite = assets.get_sprite(assetId);
      if (sprite) {
        ImGui::Text("Atlas: %s", sprite->atlasId.c_str());
        ImGui::Text("Rect: [%d,%d,%d,%d]", sprite->rect[0], sprite->rect[1], sprite->rect[2], sprite->rect[3]);
      } else {
        ImGui::TextColored(ImVec4(1.f, 0.6f, 0.3f, 1.f), "Using fallback preview (missing sprite)");
      }
      break;
    }
  } else {
    ImGui::Text("Select an asset to preview metadata.");
  }

  ImGui::End();
#else
  (void)assets;
#endif
}

} // namespace dom::tools
