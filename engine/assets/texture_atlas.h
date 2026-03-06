#pragma once

#include <array>
#include <string>
#include <unordered_map>
#include <vector>

namespace dom::assets {

struct SpriteEntry {
  std::string spriteId;
  std::string atlasId;
  std::array<int, 4> rect{0, 0, 0, 0};
  std::array<float, 2> pivot{0.5f, 0.5f};
  std::vector<std::string> tags;
};

class TextureAtlas {
public:
  bool add_sprite(const SpriteEntry& entry);
  const SpriteEntry* get_sprite(const std::string& spriteId) const;
  const std::unordered_map<std::string, SpriteEntry>& sprites() const { return spritesById_; }

private:
  std::unordered_map<std::string, SpriteEntry> spritesById_;
};

} // namespace dom::assets
