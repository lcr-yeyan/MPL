#pragma once

#include "mpl/audio/AudioAnalysis.hpp"
#include "mpl/audio/AudioExecution.hpp"
#include "mpl/audio/ReferenceAudioBackend.hpp"
#include "mpl/ir/ProductionIR.hpp"
#include "mpl/temporal/TemporalIR.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace mpl {

/// Offline production target. This is not a signal-graph cycle: the compiler
/// renders, analyses, adjusts a master parameter, renders again, and reports
/// the trace. Loudness is in LUFS, peak ceiling in dBTP.
struct MasteringTarget {
    double targetLufs = -14.0;
    double loudnessToleranceLu = 0.5;
    double peakCeilingDb = -0.5;
    /// Independent measurement tolerance for the true-peak constraint. A
    /// result inside this tolerance is "satisfied within tolerance", not strict.
    double peakToleranceDb = 0.1;
    int maxIterations = 4;

    /// Insert a sample-peak limiter on the master chain when none is present.
    bool addLimiter = true;
    double limiterAttackSeconds = 0.005;
    double limiterReleaseSeconds = 0.1;
};

struct MasteringIteration {
    int iteration = 0;
    double masterGainDb = 0.0;
    double limiterCeilingDb = 0.0;
    AudioAnalysis analysis;
};

enum class MasteringStatus {
    StrictlySatisfied,
    SatisfiedWithinTolerance,
    Unsatisfied,
    IterationExhausted,
    NumericalFailure,
    ProvablyUnsatisfiable,
};

std::string_view masteringStatusName(MasteringStatus status);

struct MasteringResult {
    MasteringTarget target;
    AudioBuffer raw;
    AudioBuffer mastered;
    AudioAnalysis before;
    AudioAnalysis after;
    std::vector<MasteringIteration> trace;
    std::string limiterName;
    double limiterCeilingDb = 0.0;
    double limiterAttackSeconds = 0.0;
    double limiterReleaseSeconds = 0.0;
    double finalMasterGainDb = 0.0;
    double finalPostFaderSampleCeilingDb = 0.0;
    MasteringStatus status = MasteringStatus::Unsatisfied;
    bool satisfied = false;
};

MasteringResult runMasteringFeedback(const ProductionIR& production,
                                     const TemporalIR& temporal,
                                     const AssetResolver& resolver,
                                     const AudioRenderConfig& config,
                                     const MasteringTarget& target,
                                     std::vector<Diagnostic>& diagnostics);

std::string masteringResultToJson(const MasteringResult& result,
                                  const AudioAnalysisConfig& analysisConfig);

}  // namespace mpl
