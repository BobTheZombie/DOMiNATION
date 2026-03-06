#pragma once

#include "engine/assets/asset_manager.h"

namespace dom::tools {

class AssetBrowser {
public:
  void toggle() { visible_ = !visible_; }
  bool visible() const { return visible_; }
  void draw(dom::assets::AssetManager& assets);

private:
  bool visible_{false};
  int selectedIndex_{-1};
  char search_[128]{};
  char category_[64]{};
  char civilization_[64]{};
  char biome_[64]{};
};

} // namespace dom::tools
