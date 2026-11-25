#include "mpl/ir/ProductionIR.hpp"

#include "mpl/core/Error.hpp"
#include "mpl/util/JsonWriter.hpp"

#include <algorithm>

namespace mpl {
namespace {

MeterMap meterMapOf(const ProductionIR& production) {
    if (production.meterMap.empty()) {
        return MeterMap{};
    }
    MeterMap map(production.meterMap.front().meter,
                 effectiveGrouping(production.meterMap.front()));
    for (std::size_t index = 1; index < production.meterMap.size(); ++index) {
        const MeterEvent& event = production.meterMap[index];
        std::string error;
        if (!map.tryAdd(event.position, event.meter, effectiveGrouping(event), error)) {
            return map;
        }
    }
    return map;
}

std::string barBeatAt(const ProductionIR& production, const Position& position) {
    if (position.inWholeNotes().isNegative()) {
        return position.toString();
    }
    try {
        const MeterMap map = meterMapOf(production);
        const MetricPosition metric = map.toMetric(position);
        return std::to_string(metric.bar + 1) + ":" + (metric.beat + Rational(1, 1)).toString();
    } catch (const Error&) {
        return position.toString();
    }
}

}  // namespace

Position TrackIR::endPosition() const {
    Position end;
    for (const ClipIR& clip : clips) {
        const Position clipEnd = clip.endPosition();
        if (end < clipEnd) {
            end = clipEnd;
        }
    }
    return end;
}

std::size_t TrackIR::noteCount() const {
    std::size_t count = 0;
    for (const ClipIR& clip : clips) {
        count += clip.content.noteCount();
    }
    return count;
}

const AssetBinding* TrackIR::findBinding(std::string_view slot) const {
    for (const AssetBinding& binding : assetBindings) {
        if (binding.slot == slot) {
            return &binding;
        }
    }
    return nullptr;
}

Position ProductionIR::endPosition() const {
    Position end;
    for (const TrackIR& track : tracks) {
        const Position trackEnd = track.endPosition();
        if (end < trackEnd) {
            end = trackEnd;
        }
    }
    return end;
}

Duration ProductionIR::totalDuration() const {
    return Duration(endPosition().inWholeNotes());
}

std::size_t ProductionIR::noteCount() const {
    std::size_t count = 0;
    for (const TrackIR& track : tracks) {
        count += track.noteCount();
    }
    return count;
}

std::size_t ProductionIR::clipCount() const {
    std::size_t count = 0;
    for (const TrackIR& track : tracks) {
        count += track.clips.size();
    }
    return count;
}

std::string ProductionIR::toJson() const {
    JsonWriter json;
    json.beginObject();
    json.key("format").value("mpl.production-ir");
    json.key("version").value(5);
    json.key("timeUnit").value("whole-note");

    json.key("tempoMap").beginArray();
    for (const TempoEvent& tempo : tempoMap) {
        json.beginObject();
        json.key("atWholeNotes").value(tempo.position.toString());
        json.key("barBeat").value(barBeatAt(*this, tempo.position));
        json.key("bpm").value(tempo.bpm);
        json.endObject();
    }
    json.endArray();

    json.key("meterMap").beginArray();
    for (const MeterEvent& meter : meterMap) {
        json.beginObject();
        json.key("atWholeNotes").value(meter.position.toString());
        json.key("barIndex").value(meter.barIndex);
        json.key("barBeat").value(barBeatAt(*this, meter.position));
        json.key("meter").value(meter.meter.toString());
        const MetricGrouping grouping = effectiveGrouping(meter);
        json.key("grouping").value(grouping.toString());
        json.key("explicitGrouping").value(grouping.explicitGrouping);
        json.endObject();
    }
    json.endArray();

    json.key("tracks").beginArray();
    for (const TrackIR& track : tracks) {
        json.beginObject();
        json.key("name").value(track.name);
        json.key("instrument").value(track.instrument);
        json.key("role").value(instrumentRoleName(track.role));
        json.key("assetSlots").beginArray();
        for (const AssetSlot& slot : track.assetSlots) {
            json.beginObject();
            json.key("name").value(slot.name);
            json.key("kind").value(assetKindName(slot.kind));
            json.key("preferredAsset").value(slot.preferredAsset);
            json.endObject();
        }
        json.endArray();
        json.key("assetBindings").beginArray();
        for (const AssetBinding& binding : track.assetBindings) {
            json.beginObject();
            json.key("slot").value(binding.slot);
            json.key("assetId").value(binding.assetId);
            json.key("provider").value(binding.provider);
            json.key("resolved").value(binding.isResolved());
            json.endObject();
        }
        json.endArray();
        json.key("clips").beginArray();
        for (const ClipIR& clip : track.clips) {
            json.beginObject();
            json.key("name").value(clip.name);
            json.key("sourceObject").value(static_cast<std::int64_t>(clip.sourceObject));
            json.key("sourceLabel").value(clip.sourceLabel);
            json.key("instanceId").value(clip.instanceId == kInvalidInstanceId
                                              ? std::int64_t{-1}
                                              : static_cast<std::int64_t>(clip.instanceId));
            json.key("startWholeNotes").value(clip.start.toString());
            json.key("startBarBeat").value(barBeatAt(*this, clip.start));
            json.key("durationWholeNotes").value(clip.content.duration().toString());
            json.key("expression").beginObject();
            const auto laneId = [](TemporalId id) {
                return id == kInvalidTemporalId ? std::int64_t{-1}
                                                : static_cast<std::int64_t>(id);
            };
            json.key("velocity").value(laneId(clip.expression.velocity));
            json.key("timing").value(laneId(clip.expression.timing));
            json.key("gate").value(laneId(clip.expression.gate));
            json.key("pan").value(laneId(clip.expression.pan));
            json.endObject();
            json.key("notes").beginArray();
            for (const Note& note : clip.content.notes()) {
                json.beginObject();
                json.key("pitch").value(note.pitch.name());
                json.key("midi").value(note.pitch.midi());
                json.key("offsetWholeNotes").value(note.offset.toString());
                json.key("durationWholeNotes").value(note.duration.toString());
                json.key("velocity").value(note.velocity);
                if (note.rhythmOrigin != kInvalidObjectId) {
                    json.key("rhythmOrigin")
                        .value(static_cast<std::int64_t>(note.rhythmOrigin));
                }
                if (note.rhythmEventIndex != kInvalidRhythmEvent) {
                    json.key("rhythmEvent")
                        .value(static_cast<std::int64_t>(note.rhythmEventIndex));
                }
                json.endObject();
            }
            json.endArray();
            json.endObject();
        }
        json.endArray();
        json.endObject();
    }
    json.endArray();

    json.key("automation").beginArray();
    for (const AutomationBinding& binding : automation) {
        json.beginObject();
        json.key("label").value(binding.label);
        json.key("parameter").value(binding.target.id.str());
        json.key("scope").value(parameterScopeName(binding.target.id.scope));
        json.key("unit").value(parameterUnitName(binding.target.unit));
        json.key("minimum").value(binding.target.minimum);
        json.key("maximum").value(binding.target.maximum);
        json.key("curve").value(binding.curve == kInvalidTemporalId
                                    ? std::int64_t{-1}
                                    : static_cast<std::int64_t>(binding.curve));
        json.key("domain").value(timeDomainName(binding.domain));
        json.key("mode").value(automationModeName(binding.mode));
        json.key("enabled").value(binding.enabled);
        json.endObject();
    }
    json.endArray();

    json.key("logicalExtentWholeNotes").value(logicalExtent.toString());

    const auto writeStrip = [&json](const ChannelStripSpec& strip) {
        json.key("name").value(strip.name);
        json.key("gainDb").value(strip.gainDb);
        json.key("pan").value(strip.pan);
        json.key("inserts").beginArray();
        for (const std::string& insert : strip.inserts) {
            json.value(insert);
        }
        json.endArray();
        json.key("sends").beginArray();
        for (const SendSpec& send : strip.sends) {
            json.beginObject();
            json.key("bus").value(send.bus);
            json.key("gain").value(send.gain);
            json.endObject();
        }
        json.endArray();
        json.key("sidechains").beginArray();
        for (const SidechainSpec& sidechain : strip.sidechains) {
            json.beginObject();
            json.key("processor").value(sidechain.processor);
            json.key("source").value(sidechain.source);
            json.endObject();
        }
        json.endArray();
    };

    json.key("mix").beginObject();
    json.key("processors").beginArray();
    for (const ProcessorSpec& processor : mix.processors) {
        json.beginObject();
        json.key("name").value(processor.name);
        json.key("kind").value(processorKindName(processor.kind));
        json.key("mode").value(processor.mode);
        json.key("parameters").beginObject();
        for (const auto& parameter : processor.parameters) {
            json.key(parameter.first).value(parameter.second);
        }
        json.endObject();
        json.endObject();
    }
    json.endArray();
    json.key("tracks").beginArray();
    for (const ChannelStripSpec& strip : mix.tracks) {
        json.beginObject();
        writeStrip(strip);
        json.endObject();
    }
    json.endArray();
    json.key("buses").beginArray();
    for (const BusSpec& bus : mix.buses) {
        json.beginObject();
        writeStrip(bus.strip);
        json.endObject();
    }
    json.endArray();
    json.key("master").beginObject();
    writeStrip(mix.master);
    json.endObject();
    json.key("processorAutomation").beginArray();
    for (const ProcessorAutomationSpec& processorAutomation : mix.processorAutomation) {
        json.beginObject();
        json.key("target").value(processorAutomation.target);
        json.key("processor").value(processorAutomation.processor);
        json.key("parameter").value(processorAutomation.parameter);
        json.key("curve").value(processorAutomation.curve == kInvalidTemporalId
                                    ? std::int64_t{-1}
                                    : static_cast<std::int64_t>(processorAutomation.curve));
        json.key("mode").value(automationModeName(processorAutomation.mode));
        json.key("minimum").value(processorAutomation.minimum);
        json.key("maximum").value(processorAutomation.maximum);
        json.endObject();
    }
    json.endArray();
    json.endObject();

    json.key("totalDurationWholeNotes").value(totalDuration().toString());
    json.endObject();
    return json.take();
}

}  // namespace mpl
