#pragma once

#include "mpl/Compiler.hpp"
#include "mpl/audio/AudioExecution.hpp"

#include <cstdint>
#include <filesystem>
#include <string>

namespace mpl {

struct RenderConfiguration {
    int sampleRate = 48000;
    int blockSize = 256;
    int channels = 2;
    double tailSeconds = 1.0;
};

/// Trusted binding between a candidate program and a rendered artifact. A WAV
/// with a matching hash can be used as that candidate's render; arbitrary
/// external WAVs are not accepted as the current candidate's output.
struct RenderArtifactManifest {
    std::string sourceHash;        // FNV fast identity
    std::string moduleHash;
    std::string productionHash;
    std::string assetManifestHash;
    std::string rendererVersion;
    RenderConfiguration configuration;
    std::string audioHash;
    // Cryptographic content identity (SHA-256, lowercase hex).
    std::string sourceSha256;
    std::string moduleSha256;
    std::string productionSha256;
    std::string assetSha256;
    std::string audioSha256;

    std::string toJson() const;
};

std::uint64_t hashBytes(std::string_view bytes);
std::string hashFileHex(const std::filesystem::path& path);
RenderArtifactManifest buildRenderManifest(std::string_view source,
                                           const Compilation& compilation,
                                           const RenderConfiguration& configuration,
                                           const std::filesystem::path& audioPath);

/// Recomputes every field from the source/compilation/config/audio and compares
/// it with `manifest`. Returns false and fills `error` on any mismatch.
bool verifyRenderManifest(const RenderArtifactManifest& manifest,
                          std::string_view source, const Compilation& compilation,
                          const RenderConfiguration& configuration,
                          const std::filesystem::path& audioPath, std::string& error);

bool parseRenderManifest(std::string_view text, RenderArtifactManifest& manifest,
                         std::string& error);

}  // namespace mpl
