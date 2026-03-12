#include "tools/dom_asset_studio/file_watch.h"

#include <system_error>

namespace dom::tools {

void FileWatch::clear() {
  entries_.clear();
}

std::filesystem::path FileWatch::normalize(const std::filesystem::path& path) {
  std::error_code ec;
  const auto absolute = std::filesystem::absolute(path, ec);
  if (ec) return path.lexically_normal();
  return absolute.lexically_normal();
}

void FileWatch::snapshot(Entry& entry) {
  std::error_code ec;
  entry.exists = std::filesystem::exists(entry.path, ec);
  if (ec || !entry.exists) {
    entry.exists = false;
    entry.writeTime.reset();
    entry.fileSize = 0;
    return;
  }

  entry.writeTime = std::filesystem::last_write_time(entry.path, ec);
  if (ec) entry.writeTime.reset();

  entry.fileSize = std::filesystem::is_regular_file(entry.path, ec) ? std::filesystem::file_size(entry.path, ec) : 0;
  if (ec) entry.fileSize = 0;
}

void FileWatch::watch(const std::filesystem::path& path, std::string tag) {
  Entry entry{};
  entry.path = normalize(path);
  entry.tag = std::move(tag);
  snapshot(entry);
  entries_[entry.path.string()] = std::move(entry);
}

void FileWatch::unwatch(const std::filesystem::path& path) {
  entries_.erase(normalize(path).string());
}

bool FileWatch::is_watched(const std::filesystem::path& path) const {
  return entries_.contains(normalize(path).string());
}

std::vector<FileWatch::Change> FileWatch::poll_changes() {
  std::vector<Change> changes;
  for (auto& [_, entry] : entries_) {
    Entry current = entry;
    snapshot(current);

    const bool wasExisting = entry.exists;
    const bool isExisting = current.exists;
    const bool changed = (entry.writeTime != current.writeTime) || (entry.fileSize != current.fileSize);
    if (!wasExisting && isExisting) {
      changes.push_back({entry.path, entry.tag, ChangeType::Created});
    } else if (wasExisting && !isExisting) {
      changes.push_back({entry.path, entry.tag, ChangeType::Removed});
    } else if (wasExisting && isExisting && changed) {
      changes.push_back({entry.path, entry.tag, ChangeType::Modified});
    }

    entry.exists = current.exists;
    entry.writeTime = current.writeTime;
    entry.fileSize = current.fileSize;
  }

  return changes;
}

} // namespace dom::tools
