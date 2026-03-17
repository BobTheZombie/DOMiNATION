#include "engine/render/gltf_runtime_loader.h"

#include <array>
#include <cstdio>
#include <iostream>

namespace dom::render {
namespace {
constexpr std::array<char, 4> kGlbMagic{'g', 'l', 'T', 'F'};
}

const RuntimeModelData& GltfRuntimeLoader::load(std::string_view path) {
  auto key = std::string(path);
  auto it = cache_.find(key);
  if (it != cache_.end()) return it->second;
  auto model = build_from_glb(path);
  if (!model.valid) model = build_invalid(path);
  auto [insertedIt, _] = cache_.emplace(key, std::move(model));
  return insertedIt->second;
}

RuntimeModelData GltfRuntimeLoader::build_from_glb(std::string_view path) const {
  RuntimeModelData data;
  data.sourcePath = std::string(path);
  FILE* file = std::fopen(data.sourcePath.c_str(), "rb");
  if (!file) return data;
  std::array<char, 4> magic{};
  if (std::fread(magic.data(), 1, magic.size(), file) != magic.size()) {
    std::fclose(file);
    return data;
  }
  if (magic != kGlbMagic) {
    std::fclose(file);
    return data;
  }
  std::array<uint32_t, 2> header{};
  if (std::fread(header.data(), sizeof(uint32_t), header.size(), file) != header.size()) {
    std::fclose(file);
    return data;
  }
  std::fclose(file);
  if (header[1] < 20u) return data;

  const uint32_t contentHash = static_cast<uint32_t>(data.sourcePath.size() * 2654435761u) ^ header[1];
  data.valid = true;
  data.footprint = 0.62f + static_cast<float>(contentHash % 21u) * 0.018f;
  data.height = 0.65f + static_cast<float>((contentHash / 21u) % 34u) * 0.03f;
  return data;
}

RuntimeModelData GltfRuntimeLoader::build_invalid(std::string_view path) const {
  RuntimeModelData data;
  data.sourcePath = std::string(path);
  data.valid = false;
  data.footprint = 0.55f;
  data.height = 0.55f;
  return data;
}

} // namespace dom::render
