#pragma once

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace dom::tools {

class FileWatch {
public:
  enum class ChangeType {
    Created,
    Modified,
    Removed,
  };

  struct Change {
    std::filesystem::path path;
    std::string tag;
    ChangeType type{ChangeType::Modified};
  };

  void clear();
  void watch(const std::filesystem::path& path, std::string tag);
  void unwatch(const std::filesystem::path& path);
  bool is_watched(const std::filesystem::path& path) const;
  std::vector<Change> poll_changes();

private:
  struct Entry {
    std::filesystem::path path;
    std::string tag;
    bool exists{false};
    std::optional<std::filesystem::file_time_type> writeTime;
    uintmax_t fileSize{0};
  };

  static std::filesystem::path normalize(const std::filesystem::path& path);
  static void snapshot(Entry& entry);

  std::unordered_map<std::string, Entry> entries_;
};

} // namespace dom::tools
