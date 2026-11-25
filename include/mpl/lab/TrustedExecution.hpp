#pragma once

#include "mpl/lab/RenderArtifacts.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace mpl {

struct TrustedRenderResult {
    bool ok = false;
    RenderArtifactManifest manifest;
    std::filesystem::path wavPath;
    std::vector<Diagnostic> diagnostics;
    std::string error;
};

/// Compiles the source, renders it with the reference backend, writes the WAV,
/// and binds the actual audio bytes to a manifest. This is the only supported
/// way to mark an audio file as a trusted render of a candidate.
TrustedRenderResult renderTrusted(std::string_view source,
                                  const std::filesystem::path& sourcePath,
                                  const RenderConfiguration& configuration,
                                  const std::filesystem::path& wavPath,
                                  const std::filesystem::path& manifestPath = {});

}  // namespace mpl
