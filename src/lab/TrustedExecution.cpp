#include "mpl/lab/TrustedExecution.hpp"

#include "mpl/Compiler.hpp"
#include "mpl/audio/AudioExecution.hpp"
#include "mpl/audio/ReferenceAudioBackend.hpp"
#include "mpl/frontend/Frontend.hpp"

#include <fstream>
#include <string>
#include <utility>

namespace mpl {

TrustedRenderResult renderTrusted(std::string_view source,
                                  const std::filesystem::path& sourcePath,
                                  const RenderConfiguration& configuration,
                                  const std::filesystem::path& wavPath,
                                  const std::filesystem::path& manifestPath) {
    TrustedRenderResult result;
    result.wavPath = wavPath;
    const FrontendResult frontend =
        compileSource(source, sourcePath.generic_string());
    if (!frontend.ok()) {
        result.error = "frontend compilation failed";
        for (const FrontendDiagnostic& diagnostic : frontend.diagnostics) {
            result.diagnostics.push_back(
                Diagnostic{Severity::Error, diagnostic.code, diagnostic.message});
        }
        return result;
    }
    const Compiler compiler;
    const Compilation compilation =
        compiler.compile(std::move(frontend.program->project));
    if (compilation.hasErrors()) {
        result.error = "compilation produced errors";
        result.diagnostics = compilation.diagnostics;
        return result;
    }

    AudioRenderConfig audioConfig;
    audioConfig.sampleRate = configuration.sampleRate;
    audioConfig.blockSize = configuration.blockSize;
    audioConfig.channels = configuration.channels;
    audioConfig.tailSeconds = configuration.tailSeconds;
    ReferenceAssetLibrary resolver(sourcePath.parent_path());
    ReferenceAudioBackend backend(audioConfig);
    std::vector<Diagnostic> renderDiagnostics;
    const AudioBuffer buffer =
        backend.render(compilation.production, compilation.temporal, resolver,
                       renderDiagnostics);
    backend.writeWav(buffer, wavPath, renderDiagnostics);
    result.diagnostics.insert(result.diagnostics.end(), renderDiagnostics.begin(),
                              renderDiagnostics.end());
    for (const Diagnostic& diagnostic : renderDiagnostics) {
        if (diagnostic.severity == Severity::Error) {
            result.error = "render failed";
            return result;
        }
    }
    result.manifest =
        buildRenderManifest(source, compilation, configuration, wavPath);
    if (!manifestPath.empty()) {
        std::ofstream stream(manifestPath, std::ios::binary | std::ios::trunc);
        if (!stream) {
            result.error = "cannot write render manifest";
            return result;
        }
        stream << result.manifest.toJson();
        if (!stream) {
            result.error = "cannot write render manifest";
            return result;
        }
    }
    result.ok = true;
    return result;
}

}  // namespace mpl
