#include "engine/render/gltf_runtime_loader.h"

#include <array>
#include <cstdio>
#include <iostream>
#include <vector>

#include <nlohmann/json.hpp>

namespace dom::render {
namespace {
constexpr std::array<char, 4> kGlbMagic{'g', 'l', 'T', 'F'};
constexpr uint32_t kJsonChunkType = 0x4E4F534Au;
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

  uint32_t version = 0;
  uint32_t length = 0;
  if (std::fread(&version, sizeof(uint32_t), 1, file) != 1 || std::fread(&length, sizeof(uint32_t), 1, file) != 1) {
    std::fclose(file);
    return data;
  }
  if (length < 20u) {
    std::fclose(file);
    return data;
  }

  std::string jsonChunk;
  while (std::ftell(file) >= 0 && static_cast<uint32_t>(std::ftell(file)) + 8u <= length) {
    uint32_t chunkLength = 0;
    uint32_t chunkType = 0;
    if (std::fread(&chunkLength, sizeof(uint32_t), 1, file) != 1 ||
        std::fread(&chunkType, sizeof(uint32_t), 1, file) != 1) {
      break;
    }
    if (chunkLength == 0 || static_cast<uint32_t>(std::ftell(file)) + chunkLength > length) break;
    std::vector<char> chunk(chunkLength);
    if (std::fread(chunk.data(), 1, chunkLength, file) != chunkLength) break;
    if (chunkType == kJsonChunkType && jsonChunk.empty()) {
      jsonChunk.assign(chunk.data(), chunk.data() + chunkLength);
      break;
    }
  }
  std::fclose(file);

  const uint32_t contentHash = static_cast<uint32_t>(data.sourcePath.size() * 2654435761u) ^ length;
  data.valid = true;
  data.footprint = 0.62f + static_cast<float>(contentHash % 21u) * 0.018f;
  data.height = 0.65f + static_cast<float>((contentHash / 21u) % 34u) * 0.03f;

  if (!jsonChunk.empty()) {
    auto doc = nlohmann::json::parse(jsonChunk, nullptr, false);
    if (doc.is_object()) {
      if (auto it = doc.find("animations"); it != doc.end() && it->is_array()) {
        size_t idx = 0;
        for (const auto& clip : *it) {
          std::string clipName = "clip_" + std::to_string(idx++);
          if (clip.is_object()) {
            if (auto nm = clip.find("name"); nm != clip.end() && nm->is_string()) clipName = nm->get<std::string>();
          }
          if (!clipName.empty()) data.clipNames.push_back(std::move(clipName));
        }
      }
    }
  }

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
