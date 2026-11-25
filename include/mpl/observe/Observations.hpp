#pragma once

#include "mpl/Compiler.hpp"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace mpl {

enum class ObservationKind {
    /// Canonical JSON is compared exactly.
    Exact,
    /// Numeric JSON with an explicit tolerance.
    Tolerance,
    /// Not derivable from the available artifacts; the verifier must report it
    /// as unverifiable rather than silently pass or fail.
    Unverifiable,
};

std::string_view observationKindName(ObservationKind kind);

struct ObservationSpec {
    std::string name;
    std::string inputType;   // Fragment, Rhythm, Harmony, track, project, audio...
    std::string scope;       // project, track, clip, module, audio
    std::string unit;        // whole-notes, midi, dBFS, dBTP, LUFS, count, ratio
    ObservationKind kind = ObservationKind::Exact;
    std::string semantics;   // computation rule
    std::string conditions;  // applicability / unverifiable conditions
    std::string id;          // stable observation id (defaults to name)
    int version = 1;
    std::string outputType;  // rational / integer / number / array / object
    std::string preconditions;
    std::string comparisonPolicy;
    std::string verificationStatus;
};

const std::vector<ObservationSpec>& observationRegistry();
const ObservationSpec* findObservationSpec(std::string_view name);

struct ObservationOptions {
    /// Optional rendered WAV used for sample_peak / true_peak / integrated_loudness.
    std::filesystem::path audioPath;
    /// When set, audio observations are only trusted if the WAV hash matches.
    std::string expectedAudioHash;
};

struct ObservationValue {
    std::string name;
    std::string status = "unverifiable";  // exact / tolerance / unverifiable
    bool available = false;
    std::string jsonValue = "null";
    std::string reason;
};

struct ObservationSet {
    std::vector<ObservationValue> values;

    const ObservationValue* find(std::string_view name) const;
    std::string toJson() const;
};

/// Computes the RC observation registry from a compiled project. Audio
/// observations remain unverifiable unless `options.audioPath` points to an
/// existing WAV file.
ObservationSet observeCompilation(const Compilation& compilation,
                                  const ObservationOptions& options = {});

/// Explicit voice-movement observation for a sequence of low-to-high voicings.
/// Voice correspondence is by index; every voicing must have the same count.
ObservationValue observeVoiceMovement(
    const std::vector<std::vector<Pitch>>& voicings);

struct VoiceMovementReference {
    bool ok = false;
    int totalMovement = 0;
    std::vector<int> perVoice;
    std::string error;
};

/// Independent reference verifier for small voice-leading checks. It does not
/// call the compiler's voicing search; it only sums index-wise movements.
VoiceMovementReference verifyVoiceMovementReference(
    const std::vector<std::vector<Pitch>>& voicings);

}  // namespace mpl
