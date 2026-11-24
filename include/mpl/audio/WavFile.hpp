#pragma once

#include "mpl/audio/AudioBuffer.hpp"
#include "mpl/core/Diagnostic.hpp"

#include <filesystem>
#include <optional>
#include <vector>

namespace mpl {

/// Loads PCM16 or IEEE-float WAV files. Unsupported encodings produce a
/// diagnostic instead of silently decoding garbage.
std::optional<AudioBuffer> loadWavFile(const std::filesystem::path& path,
                                       std::vector<Diagnostic>& diagnostics);

/// Writes 32-bit IEEE float WAV. This keeps reference renders bit-stable and
/// avoids clipping decisions in the offline renderer.
void writeWavFile(const std::filesystem::path& path, const AudioBuffer& buffer,
                  std::vector<Diagnostic>& diagnostics);

}  // namespace mpl
