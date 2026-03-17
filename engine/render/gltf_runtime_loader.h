#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace dom::render {

struct RuntimeModelData {
  std::string sourcePath;
  bool valid{false};
  float footprint{0.8f};
  float height{0.8f};
};

class GltfRuntimeLoader {
 public:
  const RuntimeModelData& load(std::string_view path);

 private:
  RuntimeModelData build_from_glb(std::string_view path) const;
  RuntimeModelData build_invalid(std::string_view path) const;

  std::unordered_map<std::string, RuntimeModelData> cache_;
  std::unordered_set<std::string> warned_;
};

} // namespace dom::render
