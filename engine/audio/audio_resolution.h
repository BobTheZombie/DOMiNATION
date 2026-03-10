#pragma once

#include "engine/audio/audio_events.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace dom::audio {

struct AudioResolveResult {
  std::string soundId;
  AudioCategory category{AudioCategory::UI};
  bool fallback{false};
  bool silent{false};
};

struct AudioResolutionCounters {
  uint64_t audioResolveCount{0};
  uint64_t audioFallbackCount{0};
};

class AudioResolver {
public:
  bool load_manifest(const std::string& path);
  AudioResolveResult resolve(AudioEventKey key, const std::string& civilization, const std::string& theme);
  const AudioResolutionCounters& counters() const { return counters_; }
  std::vector<std::string> consume_missing_reports();

private:
  struct Entry { std::string soundId; AudioCategory category{AudioCategory::UI}; };
  std::unordered_map<std::string, Entry> exact_;
  std::unordered_map<std::string, Entry> civ_;
  std::unordered_map<std::string, Entry> category_;
  std::unordered_map<std::string, Entry> defaults_;
  std::vector<std::string> missingReports_;
  AudioResolutionCounters counters_{};
};

AudioCategory category_for_event(AudioEventKey key);

} // namespace dom::audio
