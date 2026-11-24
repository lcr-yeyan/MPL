#pragma once

#include "mpl/audio/AudioBuffer.hpp"
#include "mpl/audio/AudioExecution.hpp"
#include "mpl/audio/BackendContract.hpp"
#include "mpl/core/Diagnostic.hpp"
#include "mpl/ir/ProductionIR.hpp"
#include "mpl/temporal/TemporalIR.hpp"

#include <filesystem>
#include <vector>

namespace mpl {

/// Deterministic, correctness-oriented offline audio backend. It is not a
/// competitor to SoundBoxer's DSP stack; it is the executable specification and
/// test oracle for MPL audio semantics.
class ReferenceAudioBackend {
public:
    explicit ReferenceAudioBackend(AudioRenderConfig config = {});

    const AudioRenderConfig& config() const noexcept { return config_; }

    BackendCapabilities capabilities() const;

    AudioBuffer renderPlan(const AudioExecutionPlan& plan, const TemporalIR& temporal,
                           std::vector<Diagnostic>& diagnostics) const;

    AudioBuffer render(const ProductionIR& production, const TemporalIR& temporal,
                       const AssetResolver& resolver, std::vector<Diagnostic>& diagnostics) const;

    void writeWav(const AudioBuffer& buffer, const std::filesystem::path& path,
                  std::vector<Diagnostic>& diagnostics) const;

private:
    AudioRenderConfig config_;
};

}  // namespace mpl
