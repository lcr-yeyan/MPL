#include "mpl/observe/Observations.hpp"

#include "mpl/audio/AudioAnalysis.hpp"
#include "mpl/audio/WavFile.hpp"
#include "mpl/lab/RenderArtifacts.hpp"
#include "mpl/util/JsonWriter.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace mpl {
namespace {

std::string rationalText(const Rational& value) {
    return value.toString();
}

ObservationValue valueFor(const ObservationSpec& spec, std::string json,
                          std::string reason = {}) {
    ObservationValue value;
    value.name = spec.name;
    value.available = true;
    value.jsonValue = std::move(json);
    value.status = spec.kind == ObservationKind::Exact ? "exact" : "tolerance";
    value.reason = std::move(reason);
    return value;
}

ObservationValue unverifiable(const ObservationSpec& spec, std::string reason) {
    ObservationValue value;
    value.name = spec.name;
    value.status = "unverifiable";
    value.available = false;
    value.jsonValue = "null";
    value.reason = std::move(reason);
    return value;
}

const ObservationSpec& spec(std::string_view name) {
    const ObservationSpec* found = findObservationSpec(name);
    if (found == nullptr) {
        static const ObservationSpec fallback;
        return fallback;
    }
    return *found;
}

}  // namespace

std::string_view observationKindName(ObservationKind kind) {
    switch (kind) {
        case ObservationKind::Exact: return "exact";
        case ObservationKind::Tolerance: return "tolerance";
        case ObservationKind::Unverifiable: return "unverifiable";
    }
    return "exact";
}

const std::vector<ObservationSpec>& observationRegistry() {
    static const std::vector<ObservationSpec> registry = [] {
        std::vector<ObservationSpec> values = {
        {"logical_extent", "project", "project", "whole-notes", ObservationKind::Exact,
         "ProductionIR.logicalExtent; includes intentional silence.",
         "Always available after compilation."},
        {"content_extent", "project", "project", "whole-notes", ObservationKind::Exact,
         "Maximum absolute note end across all clips; excludes trailing logical padding.",
         "Always available after compilation."},
        {"event_count", "project", "project", "count", ObservationKind::Exact,
         "ProductionIR.noteCount().", "Always available."},
        {"onset_set", "track/clip", "project", "whole-notes", ObservationKind::Exact,
         "Sorted unique absolute note onsets from all clips.",
         "Always available; empty project gives []."},
        {"onset_density", "project", "project", "events/whole-note", ObservationKind::Tolerance,
         "event_count / logical_extent.", "Unverifiable when logical extent is 0."},
        {"pitch_sequence", "track", "track", "midi", ObservationKind::Exact,
         "Per track: pitches ordered by absolute onset, then MIDI.",
         "Always available; empty tracks give []."},
        {"interval_sequence", "track", "track", "semitones", ObservationKind::Exact,
         "Per track: consecutive MIDI deltas in pitch_sequence order.",
         "Requires at least two notes; otherwise []."},
        {"normalized_rhythm", "track", "track", "ratio", ObservationKind::Exact,
         "Per track: inter-onset intervals divided by logical extent, exact rationals.",
         "Unverifiable when logical extent is 0."},
        {"harmonic_timeline", "Harmony", "project", "events", ObservationKind::Exact,
         "All evaluated harmony-literal nodes: logical length and chord events.",
         "Empty when the program has no harmony value."},
        {"fragment_range", "clip", "clip", "semitones", ObservationKind::Exact,
         "Per clip: highest MIDI minus lowest MIDI.", "Empty clip range is 0."},
        {"voice_movement", "Voicing sequence", "voice", "semitones", ObservationKind::Unverifiable,
         "Total semitone movement between explicitly supplied consecutive voicings.",
         "Not derivable from a compiled project without an explicit voicing sequence."},
        {"module_dependencies", "module", "project", "hash", ObservationKind::Exact,
         "Module records: content hash, resolved (transitive) hash, imports, exports.",
         "Empty when no module records exist."},
        {"track_symbolic_events", "track", "track", "event", ObservationKind::Exact,
         "Per track: absolute onset, duration, MIDI, velocity for every note.",
         "Always available after compilation."},
        {"sample_peak", "audio", "audio", "dBFS", ObservationKind::Tolerance,
         "AudioAnalysis.samplePeakDb from the supplied WAV.",
         "Unverifiable without a rendered WAV."},
        {"true_peak", "audio", "audio", "dBTP", ObservationKind::Tolerance,
         "4x oversampled AudioAnalysis.truePeakDb from the supplied WAV.",
         "Unverifiable without a rendered WAV."},
        {"integrated_loudness", "audio", "audio", "LUFS", ObservationKind::Tolerance,
         "BS.1770-style gated integrated loudness from the supplied WAV.",
         "Unverifiable for silence or buffers without a gating block."},
        };
        for (ObservationSpec& spec : values) {
            spec.id = spec.name;
            spec.version = 1;
            spec.preconditions = spec.conditions;
            spec.comparisonPolicy = std::string(observationKindName(spec.kind));
            spec.verificationStatus = "verified";
            if (spec.name == "logical_extent" || spec.name == "content_extent") {
                spec.outputType = "rational";
            } else if (spec.name == "event_count") {
                spec.outputType = "integer";
            } else if (spec.name == "onset_density" || spec.name == "voice_movement" ||
                       spec.name == "sample_peak" || spec.name == "true_peak" ||
                       spec.name == "integrated_loudness") {
                spec.outputType = "number";
            } else if (spec.name == "harmonic_timeline") {
                spec.outputType = "array";
            } else {
                spec.outputType = "array";
            }
        }
        return values;
    }();
    return registry;
}

const ObservationSpec* findObservationSpec(std::string_view name) {
    for (const ObservationSpec& entry : observationRegistry()) {
        if (entry.name == name) {
            return &entry;
        }
    }
    return nullptr;
}

const ObservationValue* ObservationSet::find(std::string_view name) const {    for (const ObservationValue& value : values) {
        if (value.name == name) {
            return &value;
        }
    }
    return nullptr;
}

std::string ObservationSet::toJson() const {
    JsonWriter json;
    json.beginObject();
    json.key("format").value("mpl.observations");
    json.key("version").value(1);
    json.key("observations").beginArray();
    for (const ObservationValue& value : values) {
        json.beginObject();
        json.key("name").value(value.name);
        json.key("status").value(value.status);
        json.key("available").value(value.available);
        json.key("reason").value(value.reason);
        // jsonValue is already canonical JSON; embed it as a string when it is
        // not null so the registry has a stable, simple schema.
        json.key("value").value(value.jsonValue);
        json.endObject();
    }
    json.endArray();
    json.endObject();
    return json.take();
}

VoiceMovementReference verifyVoiceMovementReference(
    const std::vector<std::vector<Pitch>>& voicings) {
    VoiceMovementReference result;
    if (voicings.empty()) {
        result.error = "empty voicing sequence";
        return result;
    }
    const std::size_t voices = voicings.front().size();
    if (voices == 0) {
        result.error = "voicings have no voices";
        return result;
    }
    result.perVoice.assign(voices, 0);
    for (const std::vector<Pitch>& voicing : voicings) {
        if (voicing.size() != voices) {
            result.error = "voice count changes between voicings";
            return result;
        }
    }
    for (std::size_t index = 1; index < voicings.size(); ++index) {
        for (std::size_t voice = 0; voice < voices; ++voice) {
            const int movement =
                std::abs(voicings[index][voice].midi() - voicings[index - 1][voice].midi());
            result.perVoice[voice] += movement;
            result.totalMovement += movement;
        }
    }
    result.ok = true;
    return result;
}

ObservationValue observeVoiceMovement(
    const std::vector<std::vector<Pitch>>& voicings) {
    const ObservationSpec& specEntry = spec("voice_movement");
    const VoiceMovementReference reference = verifyVoiceMovementReference(voicings);
    ObservationValue value;
    value.name = specEntry.name;
    if (!reference.ok) {
        value.status = "unverifiable";
        value.available = false;
        value.jsonValue = "null";
        value.reason = reference.error;
        return value;
    }
    JsonWriter json;
    json.beginObject();
    json.key("totalMovement").value(reference.totalMovement);
    json.key("perVoice").beginArray();
    for (const int movement : reference.perVoice) {
        json.value(movement);
    }
    json.endArray();
    json.key("correspondence").value("index");
    json.endObject();
    value.status = "exact";
    value.available = true;
    value.jsonValue = json.take();
    return value;
}

ObservationSet observeCompilation(const Compilation& compilation,
                                  const ObservationOptions& options) {
    ObservationSet set;
    const ProductionIR& production = compilation.production;
    const Rational logical = production.logicalExtent.inWholeNotes();

    Rational contentEnd;
    std::set<Rational> onsets;
    for (const TrackIR& track : production.tracks) {
        for (const ClipIR& clip : track.clips) {
            for (const Note& note : clip.content.notes()) {
                const Rational onset = clip.start.inWholeNotes() + note.offset.inWholeNotes();
                onsets.insert(onset);
                contentEnd = std::max(contentEnd, onset + note.duration.inWholeNotes());
            }
        }
    }

    {
        JsonWriter json;
        json.value(rationalText(logical));
        set.values.push_back(valueFor(spec("logical_extent"), json.take()));
    }
    {
        JsonWriter json;
        json.value(rationalText(contentEnd));
        set.values.push_back(valueFor(spec("content_extent"), json.take()));
    }
    {
        JsonWriter json;
        json.value(static_cast<std::int64_t>(production.noteCount()));
        set.values.push_back(valueFor(spec("event_count"), json.take()));
    }
    {
        JsonWriter json;
        json.beginArray();
        for (const Rational& onset : onsets) {
            json.value(rationalText(onset));
        }
        json.endArray();
        set.values.push_back(valueFor(spec("onset_set"), json.take()));
    }
    if (logical.isPositive()) {
        JsonWriter json;
        json.value(static_cast<double>(production.noteCount()) / logical.toDouble());
        set.values.push_back(valueFor(spec("onset_density"), json.take()));
    } else {
        set.values.push_back(unverifiable(spec("onset_density"),
                                          "logical extent is zero"));
    }

    std::vector<std::vector<std::pair<Rational, int>>> trackPitches;
    std::vector<std::vector<Rational>> trackOnsets;
    std::vector<std::vector<std::pair<Rational, Note>>> trackNotes;
    for (const TrackIR& track : production.tracks) {
        std::vector<std::pair<Rational, int>> pitches;
        std::vector<Rational> trackOnsetList;
        std::vector<std::pair<Rational, Note>> notes;
        for (const ClipIR& clip : track.clips) {
            for (const Note& note : clip.content.notes()) {
                const Rational onset = clip.start.inWholeNotes() + note.offset.inWholeNotes();
                pitches.emplace_back(onset, note.pitch.midi());
                trackOnsetList.push_back(onset);
                notes.emplace_back(onset, note);
            }
        }
        std::stable_sort(pitches.begin(), pitches.end(),
                         [](const auto& lhs, const auto& rhs) {
                             if (lhs.first != rhs.first) {
                                 return lhs.first < rhs.first;
                             }
                             return lhs.second < rhs.second;
                         });
        std::stable_sort(notes.begin(), notes.end(),
                         [](const auto& lhs, const auto& rhs) {
                             if (lhs.first != rhs.first) {
                                 return lhs.first < rhs.first;
                             }
                             return lhs.second.pitch.midi() < rhs.second.pitch.midi();
                         });
        trackPitches.push_back(std::move(pitches));
        trackOnsets.push_back(std::move(trackOnsetList));
        trackNotes.push_back(std::move(notes));
    }

    {
        JsonWriter json;
        json.beginArray();
        for (const auto& track : trackPitches) {
            json.beginArray();
            for (const auto& entry : track) {
                json.value(entry.second);
            }
            json.endArray();
        }
        json.endArray();
        set.values.push_back(valueFor(spec("pitch_sequence"), json.take()));
    }
    {
        JsonWriter json;
        json.beginArray();
        for (const auto& track : trackPitches) {
            json.beginArray();
            for (std::size_t index = 1; index < track.size(); ++index) {
                json.value(track[index].second - track[index - 1].second);
            }
            json.endArray();
        }
        json.endArray();
        set.values.push_back(valueFor(spec("interval_sequence"), json.take()));
    }
    {
        JsonWriter json;
        json.beginArray();
        bool ok = logical.isPositive();
        for (const auto& track : trackPitches) {
            json.beginArray();
            for (std::size_t index = 1; index < track.size(); ++index) {
                if (ok) {
                    json.value(rationalText((track[index].first - track[index - 1].first) /
                                            logical));
                }
            }
            json.endArray();
        }
        json.endArray();
        if (ok) {
            set.values.push_back(valueFor(spec("normalized_rhythm"), json.take()));
        } else {
            set.values.push_back(unverifiable(spec("normalized_rhythm"),
                                              "logical extent is zero"));
        }
    }
    {
        JsonWriter json;
        json.beginArray();
        for (const MusicNode& node : compilation.music.nodes()) {
            if (!node.harmonyValue.has_value()) {
                continue;
            }
            json.beginObject();
            json.key("label").value(node.label);
            json.key("logicalLength").value(node.harmonyValue->logicalLength().toString());
            json.key("events").beginArray();
            for (const HarmonyEvent& event : node.harmonyValue->events()) {
                json.beginObject();
                json.key("start").value(event.start.toString());
                json.key("duration").value(event.duration.toString());
                json.key("chord").value(event.chord.name());
                json.endObject();
            }
            json.endArray();
            json.endObject();
        }
        json.endArray();
        set.values.push_back(valueFor(spec("harmonic_timeline"), json.take()));
    }
    {
        JsonWriter json;
        json.beginArray();
        for (const TrackIR& track : production.tracks) {
            for (const ClipIR& clip : track.clips) {
                int low = 127;
                int high = 0;
                for (const Note& note : clip.content.notes()) {
                    low = std::min(low, note.pitch.midi());
                    high = std::max(high, note.pitch.midi());
                }
                json.beginObject();
                json.key("track").value(track.name);
                json.key("clip").value(clip.name);
                json.key("range").value(clip.content.notes().empty() ? 0 : high - low);
                json.endObject();
            }
        }
        json.endArray();
        set.values.push_back(valueFor(spec("fragment_range"), json.take()));
    }
    set.values.push_back(unverifiable(
        spec("voice_movement"),
        "requires an explicit Voicing sequence; not derivable from a compiled project"));
    {
        JsonWriter json;
        json.beginArray();
        for (const ModuleRecord& module : compilation.modules) {
            json.beginObject();
            json.key("name").value(module.name);
            json.key("contentHash").value(std::to_string(module.contentHash));
            json.key("resolvedHash").value(std::to_string(module.resolvedHash));
            json.key("imports").beginArray();
            for (const std::string& import : module.imports) {
                json.value(import);
            }
            json.endArray();
            json.key("exports").beginArray();
            for (const std::string& exportName : module.exports) {
                json.value(exportName);
            }
            json.endArray();
            json.endObject();
        }
        json.endArray();
        set.values.push_back(valueFor(spec("module_dependencies"), json.take()));
    }
    {
        JsonWriter json;
        json.beginArray();
        for (std::size_t trackIndex = 0; trackIndex < production.tracks.size();
             ++trackIndex) {
            const TrackIR& track = production.tracks[trackIndex];
            json.beginObject();
            json.key("track").value(track.name);
            json.key("events").beginArray();
            for (const auto& entry : trackNotes[trackIndex]) {
                const Rational onset = entry.first;
                const Note& note = entry.second;
                json.beginObject();
                json.key("onset").value(onset.toString());
                json.key("duration").value(note.duration.toString());
                json.key("midi").value(note.pitch.midi());
                json.key("velocity").value(note.velocity);
                json.endObject();
            }
            json.endArray();
            json.endObject();
        }
        json.endArray();
        set.values.push_back(valueFor(spec("track_symbolic_events"), json.take()));
    }

    AudioAnalysis analysis;
    bool haveAudio = false;
    std::string audioFailure;
    if (!options.audioPath.empty()) {
        if (!options.expectedAudioHash.empty() &&
            hashFileHex(options.audioPath) != options.expectedAudioHash) {
            audioFailure = "WAV hash does not match the trusted render manifest";
        } else {
            std::vector<Diagnostic> diagnostics;
            if (std::optional<AudioBuffer> buffer =
                    loadWavFile(options.audioPath, diagnostics)) {
                analysis = analyzeAudio(*buffer, diagnostics);
                haveAudio = true;
            } else {
                audioFailure = "WAV could not be read";
            }
        }
    }
    if (haveAudio) {
        JsonWriter sample;
        sample.value(analysis.samplePeakDb);
        set.values.push_back(valueFor(spec("sample_peak"), sample.take()));
        JsonWriter truePeak;
        truePeak.value(analysis.truePeakDb);
        set.values.push_back(valueFor(spec("true_peak"), truePeak.take()));
        if (std::isfinite(analysis.integratedLoudness)) {
            JsonWriter loudness;
            loudness.value(analysis.integratedLoudness);
            set.values.push_back(valueFor(spec("integrated_loudness"), loudness.take()));
        } else {
            set.values.push_back(
                unverifiable(spec("integrated_loudness"), "silence or no gating block"));
        }
    } else {
        const std::string reason = !audioFailure.empty()
                                       ? audioFailure
                                       : options.audioPath.empty()
                                             ? "no audio path supplied"
                                             : "WAV could not be read";
        set.values.push_back(unverifiable(spec("sample_peak"), reason));
        set.values.push_back(unverifiable(spec("true_peak"), reason));
        set.values.push_back(unverifiable(spec("integrated_loudness"), reason));
    }

    return set;
}

}  // namespace mpl
