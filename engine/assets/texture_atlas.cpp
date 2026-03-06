#include "engine/assets/texture_atlas.h"

namespace dom::assets {

bool TextureAtlas::add_sprite(const SpriteEntry& entry) {
  return spritesById_.emplace(entry.spriteId, entry).second;
}

const SpriteEntry* TextureAtlas::get_sprite(const std::string& spriteId) const {
  auto it = spritesById_.find(spriteId);
  if (it == spritesById_.end()) {
    return nullptr;
  }
  return &it->second;
}

} // namespace dom::assets
