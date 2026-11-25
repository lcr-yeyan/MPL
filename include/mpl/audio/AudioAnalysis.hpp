#pragma once

#include "mpl/audio/AudioBuffer.hpp"
#include "mpl/core/Diagnostic.hpp"

#include <cstddef>
#include <limits>
#include <string>
#include <vector>

namespace mpl {

class JsonWriter;

/// Offline audio measurement. All linear quantities are in [-inf, +inf] sample
/// units; dB helpers use the usual 20*log10(linear) convention. Loudness values
/// are LUFS and are only defined for buffers long enough to contain a gating
/// block (400 ms integrated, 3 s short-term).
struct AudioAnalysisConfig {
    /// 4x oversampling is the BS.1770-style true-peak grid. Values other than 4
    /// are rejected and reported as `analysis-config`.
    int truePeakOversampling = 4;
    double integratedBlockSeconds = 0.4;
    double blockHopSeconds = 0.1;
    double shortTermSeconds = 3.0;

    /// Optional analysis interval, in frames. `endFrame` is exclusive and may
    /// exceed the buffer to mean "to the end".
    std::size_t startFrame = 0;
    std::size_t endFrame = std::numeric_limits<std::size_t>::max();
};

struct AudioAnalysis {
    int sampleRate = 0;
    int channels = 0;
    std::size_t frames = 0;  ///< analysed frames (after range clipping)
    std::size_t startFrame = 0;

    double samplePeak = 0.0;      ///< max |x| across channels
    double samplePeakDb = 0.0;    ///< dBFS
    double rms = 0.0;             ///< RMS across all channel samples
    double rmsDb = 0.0;           ///< dBFS
    double crestFactorDb = 0.0;   ///< samplePeak / rms, in dB
    double truePeak = 0.0;        ///< 4x oversampled, max(sample peak, FIR grid)
    double truePeakDb = 0.0;      ///< dBTP

    /// -infinity when the interval has no gating block above the absolute gate.
    double integratedLoudness = -std::numeric_limits<double>::infinity();
    /// -infinity when shorter than the short-term window or fully gated.
    double maxShortTermLoudness = -std::numeric_limits<double>::infinity();

    bool clipped = false;   ///< samplePeak > 1.0
    bool nonFinite = false; ///< input contained NaN/Inf; those samples count as 0
};

AudioAnalysis analyzeAudio(const AudioBuffer& buffer, const AudioAnalysisConfig& config,
                           std::vector<Diagnostic>& diagnostics);
AudioAnalysis analyzeAudio(const AudioBuffer& buffer, std::vector<Diagnostic>& diagnostics);

/// Deterministic JSON with explicit units and a null for undefined loudness.
std::string audioAnalysisToJson(const AudioAnalysis& analysis,
                                const AudioAnalysisConfig& config);

/// Writes the analysis object into an existing writer, for embedding in
/// mastering reports.
void writeAudioAnalysisJson(JsonWriter& json, const AudioAnalysis& analysis,
                            const AudioAnalysisConfig& config);

double linearToDb(double linear);
double dbToLinear(double db);
double energyToLoudness(double energy);

}  // namespace mpl
