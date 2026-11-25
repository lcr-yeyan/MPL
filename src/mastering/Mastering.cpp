#include "mpl/mastering/Mastering.hpp"

#include "mpl/audio/Processors.hpp"
#include "mpl/util/JsonWriter.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace mpl {
namespace {

bool hasCode(const std::vector<Diagnostic>& diagnostics, const std::string& code) {
    for (const Diagnostic& diagnostic : diagnostics) {
        if (diagnostic.code == code) {
            return true;
        }
    }
    return false;
}

double parameterOr(const ProcessorSpec& processor, const std::string& name, double fallback) {
    const auto iterator = processor.parameters.find(name);
    return iterator != processor.parameters.end() ? iterator->second : fallback;
}

}  // namespace

MasteringResult runMasteringFeedback(const ProductionIR& production,
                                     const TemporalIR& temporal,
                                     const AssetResolver& resolver,
                                     const AudioRenderConfig& config,
                                     const MasteringTarget& target,
                                     std::vector<Diagnostic>& diagnostics) {
    MasteringResult result;
    result.target = target;
    result.target.maxIterations = std::clamp(target.maxIterations, 0, 16);
    result.target.loudnessToleranceLu = std::max(0.01, target.loudnessToleranceLu);

    const ReferenceAudioBackend backend(config);
    result.raw = backend.render(production, temporal, resolver, diagnostics);
    result.before = analyzeAudio(result.raw, diagnostics);

    ProductionIR working = production;
    if (target.addLimiter) {
        for (const std::string& insert : working.mix.master.inserts) {
            const ProcessorSpec* processor = working.mix.findProcessor(insert);
            if (processor != nullptr && processor->kind == ProcessorKind::Limiter) {
                result.limiterName = insert;
                break;
            }
        }
        if (result.limiterName.empty()) {
            ProcessorSpec limiter;
            limiter.name = "MasterLimiter";
            limiter.kind = ProcessorKind::Limiter;
            limiter.parameters["attack"] = target.limiterAttackSeconds;
            limiter.parameters["release"] = target.limiterReleaseSeconds;
            applyProcessorDefaults(limiter);
            result.limiterName = limiter.name;
            working.mix.processors.push_back(std::move(limiter));
            working.mix.master.inserts.push_back(result.limiterName);
        }
        const ProcessorSpec* limiter = working.mix.findProcessor(result.limiterName);
        if (limiter != nullptr) {
            result.limiterAttackSeconds = parameterOr(*limiter, "attack", 0.005);
            result.limiterReleaseSeconds = parameterOr(*limiter, "release", 0.1);
        }
    }

    double gain = working.mix.master.gainDb;
    double limiterMarginDb = 1.0;
    const auto updateLimiterCeiling = [&]() {
        if (result.limiterName.empty()) {
            return;
        }
        ProcessorSpec* limiter = working.mix.findProcessor(result.limiterName);
        if (limiter == nullptr) {
            return;
        }
        // The limiter sits before the master fader. Compensating its ceiling by
        // the fader gain makes the final output respect the requested ceiling;
        // the extra margin leaves room for inter-sample peaks.
        const double ceiling =
            std::clamp(target.peakCeilingDb - limiterMarginDb - gain, -24.0, 0.0);
        limiter->parameters["ceiling"] = ceiling;
        result.limiterCeilingDb = ceiling;
    };
    updateLimiterCeiling();

    std::vector<Diagnostic> currentDiagnostics;
    const auto renderCurrent = [&]() {
        currentDiagnostics.clear();
        AudioBuffer rendered = backend.render(working, temporal, resolver, currentDiagnostics);
        AudioAnalysis measured = analyzeAudio(rendered, currentDiagnostics);
        return std::make_pair(std::move(rendered), std::move(measured));
    };

    std::pair<AudioBuffer, AudioAnalysis> current = renderCurrent();
    AudioBuffer buffer = std::move(current.first);
    AudioAnalysis analysis = current.second;
    result.trace.push_back(MasteringIteration{0, gain, result.limiterCeilingDb, analysis});

    const auto loudnessOk = [&](const AudioAnalysis& measured) {
        return std::isfinite(measured.integratedLoudness) &&
               std::fabs(measured.integratedLoudness - target.targetLufs) <=
                   result.target.loudnessToleranceLu;
    };
    const auto peakOk = [&](const AudioAnalysis& measured) {
        return measured.truePeakDb <= target.peakCeilingDb + result.target.peakToleranceDb;
    };

    bool unsatisfiable = false;
    for (int iteration = 1; iteration <= result.target.maxIterations; ++iteration) {
        if (loudnessOk(analysis) && peakOk(analysis)) {
            result.satisfied = true;
            break;
        }
        if (!std::isfinite(analysis.integratedLoudness)) {
            break;
        }

        if (!peakOk(analysis) && !result.limiterName.empty()) {
            // A sample-peak limiter can still overshoot the true-peak ceiling.
            // Increase the limiter margin instead of lowering the fader, which
            // would only reduce loudness without changing the pinned peak.
            limiterMarginDb = std::clamp(
                limiterMarginDb + (analysis.truePeakDb - target.peakCeilingDb) + 0.05,
                0.5, 6.0);
            if (limiterMarginDb >= 6.0 && !peakOk(analysis)) {
                unsatisfiable = true;
                break;
            }
            updateLimiterCeiling();
        } else {
            double adjustment = target.targetLufs - analysis.integratedLoudness;
            if (!peakOk(analysis)) {
                adjustment =
                    std::min(adjustment, target.peakCeilingDb - analysis.truePeakDb);
            } else if (result.limiterName.empty() && adjustment > 0.0 &&
                       target.peakCeilingDb - analysis.truePeakDb < 0.05) {
                unsatisfiable = true;
                break;
            }
            adjustment = std::clamp(adjustment, -12.0, 12.0);
            if (std::fabs(adjustment) < 0.02) {
                unsatisfiable = true;
                break;
            }
            const double newGain = std::clamp(gain + adjustment, -60.0, 24.0);
            if (std::fabs(newGain - gain) < 0.01) {
                unsatisfiable = true;
                break;
            }
            working.mix.master.gainDb = newGain;
            gain = newGain;
            updateLimiterCeiling();
        }

        std::pair<AudioBuffer, AudioAnalysis> next = renderCurrent();
        buffer = std::move(next.first);
        analysis = next.second;
        result.trace.push_back(
            MasteringIteration{iteration, gain, result.limiterCeilingDb, analysis});
    }

    diagnostics.insert(diagnostics.end(), currentDiagnostics.begin(),
                       currentDiagnostics.end());

    result.mastered = buffer;
    result.after = analysis;
    result.finalMasterGainDb = gain;
    result.finalPostFaderSampleCeilingDb =
        result.limiterCeilingDb + result.finalMasterGainDb;

    const bool loudnessWithin = loudnessOk(analysis);
    const bool peakWithin = peakOk(analysis);
    const bool peakStrict = analysis.truePeakDb <= target.peakCeilingDb;
    if (!std::isfinite(analysis.integratedLoudness)) {
        result.status = MasteringStatus::NumericalFailure;
    } else if (loudnessWithin && peakStrict) {
        result.status = MasteringStatus::StrictlySatisfied;
    } else if (loudnessWithin && peakWithin) {
        result.status = MasteringStatus::SatisfiedWithinTolerance;
    } else if (unsatisfiable) {
        result.status = MasteringStatus::ProvablyUnsatisfiable;
    } else if (result.target.maxIterations > 0 &&
               static_cast<int>(result.trace.size()) >= result.target.maxIterations + 1) {
        result.status = MasteringStatus::IterationExhausted;
    } else {
        result.status = MasteringStatus::Unsatisfied;
    }
    result.satisfied = result.status == MasteringStatus::StrictlySatisfied ||
                       result.status == MasteringStatus::SatisfiedWithinTolerance;

    if (!peakStrict && peakWithin) {
        diagnostics.push_back(Diagnostic::warning(
            "mastering-peak-tolerance",
            "mastered true peak is above the strict ceiling but inside the peak tolerance"));
    }

    if (!result.satisfied) {
        if (result.status == MasteringStatus::NumericalFailure) {
            if (!hasCode(diagnostics, "mastering-silent")) {
                diagnostics.push_back(Diagnostic::error(
                    "mastering-silent",
                    "mastering target cannot be evaluated because the render is silent or "
                    "contains non-finite measurements"));
            }
        } else {
            if (!peakWithin) {
                diagnostics.push_back(Diagnostic::error(
                    "mastering-peak-exceeded",
                    "mastered true peak exceeds the target ceiling"));
            }
            if (result.status == MasteringStatus::ProvablyUnsatisfiable ||
                !loudnessWithin) {
                diagnostics.push_back(Diagnostic::error(
                    "mastering-unsatisfiable",
                    "mastering target cannot be satisfied with the current ceiling and "
                    "iteration budget"));
            }
            if (result.status == MasteringStatus::IterationExhausted) {
                diagnostics.push_back(Diagnostic::warning(
                    "mastering-max-iterations",
                    "mastering stopped after the maximum number of iterations"));
            }
        }
    }

    return result;
}

std::string_view masteringStatusName(MasteringStatus status) {
    switch (status) {
        case MasteringStatus::StrictlySatisfied: return "strictly-satisfied";
        case MasteringStatus::SatisfiedWithinTolerance: return "satisfied-within-tolerance";
        case MasteringStatus::Unsatisfied: return "unsatisfied";
        case MasteringStatus::IterationExhausted: return "iteration-exhausted";
        case MasteringStatus::NumericalFailure: return "numerical-failure";
        case MasteringStatus::ProvablyUnsatisfiable: return "provably-unsatisfiable";
    }
    return "unsatisfied";
}

std::string masteringResultToJson(const MasteringResult& result,
                                  const AudioAnalysisConfig& analysisConfig) {
    JsonWriter json;
    json.beginObject();
    json.key("format").value("mpl.mastering-report");
    json.key("version").value(1);

    json.key("target").beginObject();
    json.key("targetLufs").value(result.target.targetLufs);
    json.key("toleranceLu").value(result.target.loudnessToleranceLu);
    json.key("peakCeilingDbtp").value(result.target.peakCeilingDb);
    json.key("peakToleranceDb").value(result.target.peakToleranceDb);
    json.key("maxIterations").value(result.target.maxIterations);
    json.endObject();

    json.key("limiter").beginObject();
    json.key("name").value(result.limiterName);
    json.key("ceilingDb").value(result.limiterCeilingDb);
    json.key("attackSeconds").value(result.limiterAttackSeconds);
    json.key("releaseSeconds").value(result.limiterReleaseSeconds);
    json.endObject();

    json.key("before");
    writeAudioAnalysisJson(json, result.before, analysisConfig);
    json.key("after");
    writeAudioAnalysisJson(json, result.after, analysisConfig);

    json.key("iterations").beginArray();
    for (const MasteringIteration& iteration : result.trace) {
        json.beginObject();
        json.key("iteration").value(iteration.iteration);
        json.key("masterGainDb").value(iteration.masterGainDb);
        json.key("limiterCeilingDb").value(iteration.limiterCeilingDb);
        json.key("analysis");
        writeAudioAnalysisJson(json, iteration.analysis, analysisConfig);
        json.endObject();
    }
    json.endArray();

    json.key("satisfied").value(result.satisfied);
    json.key("status").value(masteringStatusName(result.status));
    json.key("finalParameters").beginObject();
    json.key("masterGainDb").value(result.finalMasterGainDb);
    json.key("limiterCeilingDb").value(result.limiterCeilingDb);
    json.key("postFaderSampleCeilingDb").value(result.finalPostFaderSampleCeilingDb);
    json.key("limiterAttackSeconds").value(result.limiterAttackSeconds);
    json.key("limiterReleaseSeconds").value(result.limiterReleaseSeconds);
    json.endObject();
    json.key("finalMasterGainDb").value(result.finalMasterGainDb);
    json.endObject();
    return json.take();
}

}  // namespace mpl
