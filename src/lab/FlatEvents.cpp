#include "mpl/lab/FlatEvents.hpp"

#include "mpl/audio/AudioAnalysis.hpp"
#include "mpl/audio/WavFile.hpp"
#include "mpl/lab/RenderArtifacts.hpp"
#include "mpl/observe/Observations.hpp"
#include "mpl/util/JsonValue.hpp"
#include "mpl/util/JsonWriter.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace mpl {
namespace {

bool parseRationalText(const std::string& text, Rational& value) {
    try {
        const std::size_t slash = text.find('/');
        if (slash == std::string::npos) {
            value = Rational(std::stoll(text), 1);
            return true;
        }
        const std::int64_t numerator = std::stoll(text.substr(0, slash));
        const std::int64_t denominator = std::stoll(text.substr(slash + 1));
        if (denominator <= 0) {
            return false;
        }
        value = Rational(numerator, denominator);
        return true;
    } catch (...) {
        return false;
    }
}

bool readRational(const JsonValue* value, Rational& out, std::string& error,
                  const char* field) {
    if (value == nullptr || !value->isString()) {
        error = std::string(field) + " must be an exact rational string";
        return false;
    }
    if (!parseRationalText(value->text, out) || (!out.isZero() && !out.isPositive())) {
        error = std::string(field) + " is not a non-negative rational";
        return false;
    }
    return true;
}

ObservationValue valueFor(const ObservationSpec& spec, std::string json) {
    ObservationValue value;
    value.name = spec.name;
    value.status = spec.kind == ObservationKind::Exact ? "exact" : "tolerance";
    value.available = true;
    value.jsonValue = std::move(json);
    return value;
}

ObservationValue unavailable(const ObservationSpec& spec, std::string reason) {
    ObservationValue value;
    value.name = spec.name;
    value.status = "unverifiable";
    value.jsonValue = "null";
    value.reason = std::move(reason);
    return value;
}

const ObservationSpec& specOf(std::string_view name) {
    const ObservationSpec* spec = findObservationSpec(name);
    if (spec == nullptr) {
        static const ObservationSpec fallback;
        return fallback;
    }
    return *spec;
}

}  // namespace

FlatParseResult parseFlatEvents(std::string_view text) {
    FlatParseResult result;
    JsonValue root;
    std::string error;
    if (!parseJson(text, root, error)) {
        result.error = "invalid flat event JSON: " + error;
        return result;
    }
    if (!root.isObject()) {
        result.error = "flat event document must be an object";
        return result;
    }
    FlatEventDocument& document = result.document;
    if (!readRational(root.find("logicalExtent"), document.logicalExtent, result.error,
                      "logicalExtent")) {
        return result;
    }
    if (const JsonValue* tempo = root.find("tempoBpm");
        tempo != nullptr && tempo->isNumber() && tempo->number > 0.0) {
        document.tempoBpm = tempo->number;
    } else {
        result.error = "tempoBpm must be a positive number";
        return result;
    }
    if (const JsonValue* context = root.find("taskContext");
        context != nullptr && context->isString()) {
        document.taskContext = context->text;
    }
    if (const JsonValue* meters = root.find("meterMap");
        meters != nullptr && meters->isArray()) {
        for (const JsonValue& entry : meters->array) {
            if (!entry.isObject()) {
                result.error = "meterMap entries must be objects";
                return result;
            }
            FlatMeter meter;
            if (!readRational(entry.find("position"), meter.position, result.error,
                              "meterMap.position")) {
                return result;
            }
            const JsonValue* numerator = entry.find("numerator");
            const JsonValue* denominator = entry.find("denominator");
            if (numerator == nullptr || !numerator->isNumber() || numerator->number < 1 ||
                denominator == nullptr || !denominator->isNumber() ||
                denominator->number < 1) {
                result.error = "meterMap requires positive numerator/denominator";
                return result;
            }
            meter.numerator = static_cast<int>(numerator->number);
            meter.denominator = static_cast<int>(denominator->number);
            if (const JsonValue* grouping = entry.find("grouping");
                grouping != nullptr && grouping->isString()) {
                meter.grouping = grouping->text;
            }
            document.meters.push_back(std::move(meter));
        }
    }
    if (document.meters.empty()) {
        document.meters.push_back(FlatMeter{Rational{}, 4, 4, "2+2"});
    }
    std::sort(document.meters.begin(), document.meters.end(),
              [](const FlatMeter& lhs, const FlatMeter& rhs) {
                  return lhs.position < rhs.position;
              });
    if (!document.meters.front().position.isZero()) {
        result.error = "meterMap must start at 0";
        return result;
    }

    const JsonValue* tracks = root.find("tracks");
    if (tracks == nullptr || !tracks->isArray()) {
        result.error = "tracks must be an array";
        return result;
    }
    std::set<std::string> trackIds;
    for (const JsonValue& trackValue : tracks->array) {
        if (!trackValue.isObject()) {
            result.error = "track entries must be objects";
            return result;
        }
        const JsonValue* id = trackValue.find("id");
        if (id == nullptr || !id->isString() || id->text.empty()) {
            result.error = "track requires non-empty string id";
            return result;
        }
        if (!trackIds.insert(id->text).second) {
            result.error = "duplicate track id '" + id->text + "'";
            return result;
        }
        FlatTrack track;
        track.id = id->text;
        const JsonValue* events = trackValue.find("events");
        if (events == nullptr || !events->isArray()) {
            result.error = "track '" + track.id + "' requires events array";
            return result;
        }
        for (const JsonValue& eventValue : events->array) {
            if (!eventValue.isObject()) {
                result.error = "event entries must be objects";
                return result;
            }
            FlatEvent event;
            const JsonValue* pitch = eventValue.find("pitch");
            if (pitch != nullptr && pitch->isNumber()) {
                event.pitch = static_cast<int>(pitch->number);
            } else if (pitch != nullptr && pitch->isString()) {
                try {
                    event.pitch = Pitch::parse(pitch->text).midi();
                } catch (...) {
                    result.error = "invalid pitch string '" + pitch->text + "'";
                    return result;
                }
            } else {
                result.error = "event requires integer or name pitch";
                return result;
            }
            if (event.pitch < 0 || event.pitch > 127) {
                result.error = "pitch must be in MIDI range [0,127]";
                return result;
            }
            if (!readRational(eventValue.find("onset"), event.onset, result.error,
                              "event.onset") ||
                !readRational(eventValue.find("duration"), event.duration, result.error,
                              "event.duration") ||
                !event.duration.isPositive()) {
                result.error = "event duration must be positive";
                return result;
            }
            if (const JsonValue* velocity = eventValue.find("velocity");
                velocity != nullptr && velocity->isNumber()) {
                event.velocity = static_cast<int>(velocity->number);
            }
            if (event.velocity < 1 || event.velocity > 127) {
                result.error = "velocity must be in [1,127]";
                return result;
            }
            track.events.push_back(event);
        }
        document.tracks.push_back(std::move(track));
    }
    result.ok = true;
    return result;
}

bool flatSupportsObservation(std::string_view name) {
    static const std::vector<std::string> supported = {
        "logical_extent", "content_extent", "event_count", "onset_set",
        "onset_density", "pitch_sequence", "interval_sequence",
        "normalized_rhythm", "track_symbolic_events", "sample_peak",
        "true_peak", "integrated_loudness",
    };
    return std::find(supported.begin(), supported.end(), name) != supported.end();
}

std::string flatUnsupportedReason(std::string_view name) {
    if (name == "harmonic_timeline") {
        return "flat events do not carry a harmonic timeline";
    }
    if (name == "module_dependencies") {
        return "flat events have no module provenance";
    }
    if (name == "voice_movement") {
        return "flat events have no explicit voice correspondence";
    }
    if (name == "fragment_range") {
        return "flat events have tracks, not MPL clip/source ranges";
    }
    return "observation is not representable by the flat event baseline";
}

ObservationSet observeFlatEvents(const FlatEventDocument& document,
                                 const ObservationOptions& options) {
    ObservationSet set;
    const Rational logical = document.logicalExtent;
    Rational contentEnd;
    std::set<Rational> onsets;
    for (const FlatTrack& track : document.tracks) {
        for (const FlatEvent& event : track.events) {
            onsets.insert(event.onset);
            contentEnd = std::max(contentEnd, event.onset + event.duration);
        }
    }

    {
        JsonWriter json;
        json.value(logical.toString());
        set.values.push_back(valueFor(specOf("logical_extent"), json.take()));
    }
    {
        JsonWriter json;
        json.value(contentEnd.toString());
        set.values.push_back(valueFor(specOf("content_extent"), json.take()));
    }
    {
        JsonWriter json;
        json.value(static_cast<std::int64_t>([&] {
            std::size_t count = 0;
            for (const FlatTrack& track : document.tracks) {
                count += track.events.size();
            }
            return count;
        }()));
        set.values.push_back(valueFor(specOf("event_count"), json.take()));
    }
    {
        JsonWriter json;
        json.beginArray();
        for (const Rational& onset : onsets) {
            json.value(onset.toString());
        }
        json.endArray();
        set.values.push_back(valueFor(specOf("onset_set"), json.take()));
    }
    if (logical.isPositive()) {
        std::size_t count = 0;
        for (const FlatTrack& track : document.tracks) {
            count += track.events.size();
        }
        JsonWriter json;
        json.value(static_cast<double>(count) / logical.toDouble());
        set.values.push_back(valueFor(specOf("onset_density"), json.take()));
    } else {
        set.values.push_back(unavailable(specOf("onset_density"), "logical extent is zero"));
    }

    std::vector<std::vector<std::pair<Rational, FlatEvent>>> ordered;
    for (const FlatTrack& track : document.tracks) {
        std::vector<std::pair<Rational, FlatEvent>> events;
        for (const FlatEvent& event : track.events) {
            events.emplace_back(event.onset, event);
        }
        std::stable_sort(events.begin(), events.end(),
                         [](const auto& lhs, const auto& rhs) {
                             if (lhs.first != rhs.first) {
                                 return lhs.first < rhs.first;
                             }
                             return lhs.second.pitch < rhs.second.pitch;
                         });
        ordered.push_back(std::move(events));
    }
    {
        JsonWriter json;
        json.beginArray();
        for (const auto& track : ordered) {
            json.beginArray();
            for (const auto& event : track) {
                json.value(event.second.pitch);
            }
            json.endArray();
        }
        json.endArray();
        set.values.push_back(valueFor(specOf("pitch_sequence"), json.take()));
    }
    {
        JsonWriter json;
        json.beginArray();
        for (const auto& track : ordered) {
            json.beginArray();
            for (std::size_t index = 1; index < track.size(); ++index) {
                json.value(track[index].second.pitch - track[index - 1].second.pitch);
            }
            json.endArray();
        }
        json.endArray();
        set.values.push_back(valueFor(specOf("interval_sequence"), json.take()));
    }
    {
        JsonWriter json;
        json.beginArray();
        for (const auto& track : ordered) {
            json.beginArray();
            for (std::size_t index = 1; index < track.size(); ++index) {
                if (logical.isPositive()) {
                    json.value(((track[index].first - track[index - 1].first) / logical)
                                   .toString());
                }
            }
            json.endArray();
        }
        json.endArray();
        if (logical.isPositive()) {
            set.values.push_back(valueFor(specOf("normalized_rhythm"), json.take()));
        } else {
            set.values.push_back(unavailable(specOf("normalized_rhythm"),
                                             "logical extent is zero"));
        }
    }
    if (flatSupportsObservation("fragment_range")) {
        JsonWriter json;
        json.beginArray();
        for (const FlatTrack& track : document.tracks) {
            int low = 127;
            int high = 0;
            for (const FlatEvent& event : track.events) {
                low = std::min(low, event.pitch);
                high = std::max(high, event.pitch);
            }
            json.beginObject();
            json.key("track").value(track.id);
            json.key("clip").value("flat");
            json.key("range").value(track.events.empty() ? 0 : high - low);
            json.endObject();
        }
        json.endArray();
        set.values.push_back(valueFor(specOf("fragment_range"), json.take()));
    } else {
        set.values.push_back(unavailable(specOf("fragment_range"),
                                         flatUnsupportedReason("fragment_range")));
    }
    set.values.push_back(unavailable(specOf("harmonic_timeline"),
                                     flatUnsupportedReason("harmonic_timeline")));
    set.values.push_back(unavailable(specOf("voice_movement"),
                                     flatUnsupportedReason("voice_movement")));
    set.values.push_back(unavailable(specOf("module_dependencies"),
                                     flatUnsupportedReason("module_dependencies")));
    {
        JsonWriter json;
        json.beginArray();
        for (std::size_t index = 0; index < ordered.size(); ++index) {
            json.beginObject();
            json.key("track").value(document.tracks[index].id);
            json.key("events").beginArray();
            for (const auto& event : ordered[index]) {
                json.beginObject();
                json.key("onset").value(event.first.toString());
                json.key("duration").value(event.second.duration.toString());
                json.key("midi").value(event.second.pitch);
                json.key("velocity").value(event.second.velocity);
                json.endObject();
            }
            json.endArray();
            json.endObject();
        }
        json.endArray();
        set.values.push_back(valueFor(specOf("track_symbolic_events"), json.take()));
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
        set.values.push_back(valueFor(specOf("sample_peak"), sample.take()));
        JsonWriter truePeak;
        truePeak.value(analysis.truePeakDb);
        set.values.push_back(valueFor(specOf("true_peak"), truePeak.take()));
        if (std::isfinite(analysis.integratedLoudness)) {
            JsonWriter loudness;
            loudness.value(analysis.integratedLoudness);
            set.values.push_back(valueFor(specOf("integrated_loudness"), loudness.take()));
        } else {
            set.values.push_back(unavailable(specOf("integrated_loudness"),
                                             "silence or no gating block"));
        }
    } else {
        const std::string reason = !audioFailure.empty()
                                       ? audioFailure
                                       : options.audioPath.empty()
                                             ? "no audio path supplied"
                                             : "WAV could not be read";
        set.values.push_back(unavailable(specOf("sample_peak"), reason));
        set.values.push_back(unavailable(specOf("true_peak"), reason));
        set.values.push_back(unavailable(specOf("integrated_loudness"), reason));
    }
    return set;
}

}  // namespace mpl
