#include "mpl/analysis/SemanticDelta.hpp"

#include "mpl/observe/Observations.hpp"
#include "mpl/util/JsonWriter.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace mpl {
namespace {

std::string roleName(InstrumentRole role) {
    switch (role) {
        case InstrumentRole::Bass: return "bass";
        case InstrumentRole::Harmony: return "harmony";
        case InstrumentRole::Percussion: return "percussion";
        case InstrumentRole::Texture: return "texture";
        case InstrumentRole::Melodic: return "melodic";
    }
    return "melodic";
}

std::string notesJson(const Fragment& fragment) {
    std::vector<std::string> notes;
    for (const Note& note : fragment.notes()) {
        JsonWriter json;
        json.beginObject();
        json.key("onset").value(note.offset.inWholeNotes().toString());
        json.key("duration").value(note.duration.inWholeNotes().toString());
        json.key("midi").value(note.pitch.midi());
        json.key("velocity").value(note.velocity);
        json.endObject();
        notes.push_back(json.take());
    }
    std::sort(notes.begin(), notes.end());
    JsonWriter array;
    array.beginArray();
    for (const std::string& note : notes) {
        array.value(note);
    }
    array.endArray();
    return array.take();
}

std::string tempoJson(const ProductionIR& production) {
    JsonWriter json;
    json.beginArray();
    for (const TempoEvent& event : production.tempoMap) {
        json.beginObject();
        json.key("position").value(event.position.inWholeNotes().toString());
        json.key("bpm").value(event.bpm);
        json.endObject();
    }
    json.endArray();
    return json.take();
}

std::string meterJson(const ProductionIR& production) {
    JsonWriter json;
    json.beginArray();
    for (const MeterEvent& event : production.meterMap) {
        json.beginObject();
        json.key("position").value(event.position.inWholeNotes().toString());
        json.key("meter").value(event.meter.toString());
        json.key("grouping").value(event.grouping.toString());
        json.endObject();
    }
    json.endArray();
    return json.take();
}

std::string harmonyJson(const HarmonyTimeline& timeline) {
    JsonWriter json;
    json.beginObject();
    json.key("logicalLength").value(timeline.logicalLength().toString());
    json.key("events").beginArray();
    for (const HarmonyEvent& event : timeline.events()) {
        json.beginObject();
        json.key("start").value(event.start.toString());
        json.key("duration").value(event.duration.toString());
        json.key("chord").value(event.chord.name());
        json.endObject();
    }
    json.endArray();
    json.endObject();
    return json.take();
}

std::string channelJson(const ChannelStripSpec& strip) {
    JsonWriter json;
    json.beginObject();
    json.key("gainDb").value(strip.gainDb);
    json.key("pan").value(strip.pan);
    json.key("inserts").beginArray();
    for (const std::string& insert : strip.inserts) {
        json.value(insert);
    }
    json.endArray();
    json.endObject();
    return json.take();
}

std::string mixJson(const MixSpec& mix) {
    JsonWriter json;
    json.beginObject();
    json.key("tracks").beginObject();
    for (const ChannelStripSpec& track : mix.tracks) {
        json.key(track.name).value(channelJson(track));
    }
    json.endObject();
    json.key("master").value(channelJson(mix.master));
    json.key("processors").beginArray();
    for (const ProcessorSpec& processor : mix.processors) {
        json.beginObject();
        json.key("name").value(processor.name);
        json.key("kind").value(std::string(processorKindName(processor.kind)));
        json.key("parameters").beginObject();
        for (const auto& [key, value] : processor.parameters) {
            json.key(key).value(value);
        }
        json.endObject();
        json.endObject();
    }
    json.endArray();
    json.key("processorAutomation").beginArray();
    for (const ProcessorAutomationSpec& automation : mix.processorAutomation) {
        json.beginObject();
        json.key("target").value(automation.target);
        json.key("processor").value(automation.processor);
        json.key("parameter").value(automation.parameter);
        json.endObject();
    }
    json.endArray();
    json.endObject();
    return json.take();
}

std::string assetsJson(const TrackIR& track) {
    JsonWriter json;
    json.beginArray();
    for (const AssetBinding& binding : track.assetBindings) {
        json.beginObject();
        json.key("slot").value(binding.slot);
        json.key("assetId").value(binding.assetId);
        json.key("provider").value(binding.provider);
        json.endObject();
    }
    json.endArray();
    return json.take();
}

std::string automationJson(const ProductionIR& production) {
    JsonWriter json;
    json.beginArray();
    for (const AutomationBinding& binding : production.automation) {
        json.beginObject();
        json.key("label").value(binding.label);
        json.key("scope").value(std::string(parameterScopeName(binding.target.id.scope)));
        json.key("track").value(binding.target.id.track);
        json.key("clip").value(binding.target.id.clip);
        json.key("parameter").value(binding.target.id.parameter);
        json.key("mode").value(binding.mode == AutomationMode::Replace
                                   ? "replace"
                                   : binding.mode == AutomationMode::Add ? "add"
                                                                         : "multiply");
        json.endObject();
    }
    json.endArray();
    return json.take();
}

DeltaCause classifyCause(const Compilation& before, const Compilation& after) {
    if (before.modules.empty() || after.modules.empty()) {
        return DeltaCause::Unknown;
    }
    const ModuleRecord& beforeRoot = before.modules.front();
    const ModuleRecord& afterRoot = after.modules.front();
    if (beforeRoot.contentHash != afterRoot.contentHash) {
        return DeltaCause::Direct;
    }
    std::map<std::string, std::uint64_t> beforeDeps;
    std::map<std::string, std::uint64_t> afterDeps;
    for (const ModuleRecord& module : before.modules) {
        if (&module != &beforeRoot) {
            beforeDeps[module.name] = module.resolvedHash;
        }
    }
    for (const ModuleRecord& module : after.modules) {
        if (&module != &afterRoot) {
            afterDeps[module.name] = module.resolvedHash;
        }
    }
    if (beforeDeps != afterDeps) {
        return DeltaCause::Dependency;
    }
    return DeltaCause::Unknown;
}

void addChange(SemanticDelta& delta, std::string change, std::string objectType,
               std::string key, std::string property, std::string before,
               std::string after, DeltaIdentity identity, DeltaCause cause) {
    SemanticChange entry;
    entry.change = std::move(change);
    entry.objectType = std::move(objectType);
    entry.key = std::move(key);
    entry.property = std::move(property);
    entry.before = std::move(before);
    entry.after = std::move(after);
    entry.identity = identity;
    entry.cause = cause;
    entry.source = entry.objectType + ":" + entry.key;
    delta.changes.push_back(std::move(entry));
}

std::string keyWithIndex(const std::string& name, int index) {
    return name + "#" + std::to_string(index);
}

}  // namespace

SemanticDelta computeSemanticDelta(const Compilation& before, const Compilation& after) {
    SemanticDelta delta;
    const DeltaCause cause = classifyCause(before, after);

    std::map<std::string, const TrackIR*> beforeTracks;
    std::map<std::string, const TrackIR*> afterTracks;
    for (const TrackIR& track : before.production.tracks) {
        beforeTracks[track.name] = &track;
    }
    for (const TrackIR& track : after.production.tracks) {
        afterTracks[track.name] = &track;
    }
    for (const auto& [name, track] : beforeTracks) {
        if (afterTracks.find(name) == afterTracks.end()) {
            addChange(delta, "removed", "track", name, "track", roleName(track->role), "",
                      DeltaIdentity::Value, cause);
        }
    }
    for (const auto& [name, track] : afterTracks) {
        if (beforeTracks.find(name) == beforeTracks.end()) {
            addChange(delta, "added", "track", name, "track", "", roleName(track->role),
                      DeltaIdentity::Value, cause);
        }
    }

    for (const auto& [name, track] : beforeTracks) {
        const auto found = afterTracks.find(name);
        if (found == afterTracks.end()) {
            continue;
        }
        const TrackIR& next = *found->second;
        if (track->instrument != next.instrument) {
            addChange(delta, "modified", "track", name, "instrument", track->instrument,
                      next.instrument, DeltaIdentity::Value, cause);
        }
        if (track->role != next.role) {
            addChange(delta, "modified", "track", name, "role", roleName(track->role),
                      roleName(next.role), DeltaIdentity::Value, cause);
        }
        if (assetsJson(*track) != assetsJson(next)) {
            addChange(delta, "modified", "track", name, "assetBindings",
                      assetsJson(*track), assetsJson(next), DeltaIdentity::Value, cause);
        }
        std::map<std::string, const ClipIR*> beforeClips;
        std::map<std::string, const ClipIR*> afterClips;
        for (std::size_t index = 0; index < track->clips.size(); ++index) {
            beforeClips[keyWithIndex(track->clips[index].name,
                                     static_cast<int>(index))] = &track->clips[index];
        }
        for (std::size_t index = 0; index < next.clips.size(); ++index) {
            afterClips[keyWithIndex(next.clips[index].name, static_cast<int>(index))] =
                &next.clips[index];
        }
        for (const auto& [clipKey, clip] : beforeClips) {
            if (afterClips.find(clipKey) == afterClips.end()) {
                addChange(delta, "removed", "clip", name + "/" + clipKey, "clip",
                          clip->sourceLabel, "", DeltaIdentity::Value, cause);
            }
        }
        for (const auto& [clipKey, clip] : afterClips) {
            if (beforeClips.find(clipKey) == beforeClips.end()) {
                addChange(delta, "added", "clip", name + "/" + clipKey, "clip", "",
                          clip->sourceLabel, DeltaIdentity::Value, cause);
                continue;
            }
            const ClipIR& oldClip = *beforeClips.at(clipKey);
            const std::string key = name + "/" + clipKey;
            if (oldClip.start.inWholeNotes() != clip->start.inWholeNotes()) {
                addChange(delta, "modified", "clip", key, "start",
                          oldClip.start.inWholeNotes().toString(),
                          clip->start.inWholeNotes().toString(), DeltaIdentity::Value, cause);
            }
            if (oldClip.content.duration().inWholeNotes() !=
                clip->content.duration().inWholeNotes()) {
                addChange(delta, "modified", "clip", key, "duration",
                          oldClip.content.duration().inWholeNotes().toString(),
                          clip->content.duration().inWholeNotes().toString(),
                          DeltaIdentity::Value, cause);
            }
            const std::string oldNotes = notesJson(oldClip.content);
            const std::string newNotes = notesJson(clip->content);
            if (oldNotes != newNotes) {
                addChange(delta, "modified", "clip", key, "symbolicEvents", oldNotes,
                          newNotes, DeltaIdentity::Value, cause);
            }
            if (oldClip.instanceId != clip->instanceId) {
                addChange(delta, "modified", "clip", key, "instanceId",
                          std::to_string(oldClip.instanceId),
                          std::to_string(clip->instanceId), DeltaIdentity::Instance, cause);
            }
            if (oldClip.sourceObject != clip->sourceObject) {
                addChange(delta, "modified", "clip", key, "sourceObject",
                          std::to_string(oldClip.sourceObject),
                          std::to_string(clip->sourceObject), DeltaIdentity::Generation,
                          cause);
            }
        }
    }

    std::map<std::string, SectionRecord> beforeSections;
    std::map<std::string, SectionRecord> afterSections;
    for (const SectionRecord& section : before.sections) {
        beforeSections[keyWithIndex(section.name, section.occurrence)] = section;
    }
    for (const SectionRecord& section : after.sections) {
        afterSections[keyWithIndex(section.name, section.occurrence)] = section;
    }
    for (const auto& [key, section] : beforeSections) {
        if (afterSections.find(key) == afterSections.end()) {
            addChange(delta, "removed", "section", key, "section",
                      section.start.inWholeNotes().toString(), "", DeltaIdentity::Value,
                      cause);
        }
    }
    for (const auto& [key, section] : afterSections) {
        const auto found = beforeSections.find(key);
        if (found == beforeSections.end()) {
            addChange(delta, "added", "section", key, "section", "",
                      section.start.inWholeNotes().toString(), DeltaIdentity::Value, cause);
            continue;
        }
        if (found->second.start.inWholeNotes() != section.start.inWholeNotes()) {
            addChange(delta, "modified", "section", key, "start",
                      found->second.start.inWholeNotes().toString(),
                      section.start.inWholeNotes().toString(), DeltaIdentity::Value, cause);
        }
        if (found->second.duration.inWholeNotes() != section.duration.inWholeNotes()) {
            addChange(delta, "modified", "section", key, "duration",
                      found->second.duration.inWholeNotes().toString(),
                      section.duration.inWholeNotes().toString(), DeltaIdentity::Value,
                      cause);
        }
    }

    std::map<std::string, std::string> beforeHarmony;
    std::map<std::string, std::string> afterHarmony;
    for (const MusicNode& node : before.music.nodes()) {
        if (node.harmonyValue.has_value()) {
            beforeHarmony[keyWithIndex(node.label,
                                       static_cast<int>(beforeHarmony.size()))] =
                harmonyJson(*node.harmonyValue);
        }
    }
    for (const MusicNode& node : after.music.nodes()) {
        if (node.harmonyValue.has_value()) {
            afterHarmony[keyWithIndex(node.label,
                                      static_cast<int>(afterHarmony.size()))] =
                harmonyJson(*node.harmonyValue);
        }
    }
    for (const auto& [key, value] : beforeHarmony) {
        if (afterHarmony.find(key) == afterHarmony.end()) {
            addChange(delta, "removed", "harmony", key, "timeline", value, "",
                      DeltaIdentity::Value, cause);
        }
    }
    for (const auto& [key, value] : afterHarmony) {
        const auto found = beforeHarmony.find(key);
        if (found == beforeHarmony.end()) {
            addChange(delta, "added", "harmony", key, "timeline", "", value,
                      DeltaIdentity::Value, cause);
        } else if (found->second != value) {
            addChange(delta, "modified", "harmony", key, "timeline", found->second, value,
                      DeltaIdentity::Value, cause);
        }
    }

    if (tempoJson(before.production) != tempoJson(after.production)) {
        addChange(delta, "modified", "production", "project", "tempo",
                  tempoJson(before.production), tempoJson(after.production),
                  DeltaIdentity::Value, cause);
    }
    if (meterJson(before.production) != meterJson(after.production)) {
        addChange(delta, "modified", "production", "project", "meter",
                  meterJson(before.production), meterJson(after.production),
                  DeltaIdentity::Value, cause);
    }
    if (before.production.logicalExtent.inWholeNotes() !=
        after.production.logicalExtent.inWholeNotes()) {
        addChange(delta, "modified", "production", "project", "logicalExtent",
                  before.production.logicalExtent.inWholeNotes().toString(),
                  after.production.logicalExtent.inWholeNotes().toString(),
                  DeltaIdentity::Value, cause);
    }
    if (mixJson(before.production.mix) != mixJson(after.production.mix)) {
        addChange(delta, "modified", "production", "project", "mix",
                  mixJson(before.production.mix), mixJson(after.production.mix),
                  DeltaIdentity::Value, cause);
    }
    if (automationJson(before.production) != automationJson(after.production)) {
        addChange(delta, "modified", "production", "project", "automation",
                  automationJson(before.production), automationJson(after.production),
                  DeltaIdentity::Value, cause);
    }
    if (before.temporal.toJson() != after.temporal.toJson()) {
        addChange(delta, "modified", "production", "project", "temporalCurves",
                  before.temporal.toJson(), after.temporal.toJson(),
                  DeltaIdentity::Value, cause);
    }

    const ObservationSet beforeObservations = observeCompilation(before);
    const ObservationSet afterObservations = observeCompilation(after);
    for (const ObservationSpec& spec : observationRegistry()) {
        const ObservationValue* oldValue = beforeObservations.find(spec.name);
        const ObservationValue* newValue = afterObservations.find(spec.name);
        if (oldValue == nullptr || newValue == nullptr) {
            continue;
        }
        ChangedObservation changed;
        changed.name = spec.name;
        changed.before = oldValue->jsonValue;
        changed.after = newValue->jsonValue;
        if (oldValue->status == "unverifiable" || newValue->status == "unverifiable") {
            if (oldValue->status != newValue->status) {
                changed.status = "unverifiable";
                changed.reason = "observation availability changed";
                delta.changedObservations.push_back(std::move(changed));
            }
            continue;
        }
        if (oldValue->jsonValue != newValue->jsonValue) {
            changed.status = "changed";
            delta.changedObservations.push_back(std::move(changed));
        }
    }

    delta.unobserved = {"objectIds"};
    if (cause == DeltaCause::Direct) {
        delta.causeSummary = "direct";
    } else if (cause == DeltaCause::Dependency) {
        delta.causeSummary = "dependency";
    } else {
        delta.causeSummary = "unknown";
    }
    return delta;
}

std::string SemanticDelta::toJson() const {
    JsonWriter json;
    json.beginObject();
    json.key("format").value("mpl.semantic-delta");
    json.key("version").value(1);
    json.key("causeSummary").value(causeSummary);
    json.key("changes").beginArray();
    for (const SemanticChange& change : changes) {
        json.beginObject();
        json.key("change").value(change.change);
        json.key("objectType").value(change.objectType);
        json.key("key").value(change.key);
        json.key("property").value(change.property);
        json.key("before").value(change.before);
        json.key("after").value(change.after);
        json.key("identity").value(change.identity == DeltaIdentity::Value
                                        ? "value"
                                        : change.identity == DeltaIdentity::Generation
                                              ? "generation"
                                              : "instance");
        json.key("cause").value(change.cause == DeltaCause::Direct
                                    ? "direct"
                                    : change.cause == DeltaCause::Dependency
                                          ? "dependency"
                                          : "unknown");
        json.key("source").value(change.source);
        json.endObject();
    }
    json.endArray();
    json.key("changedObservations").beginArray();
    for (const ChangedObservation& changed : changedObservations) {
        json.beginObject();
        json.key("name").value(changed.name);
        json.key("status").value(changed.status);
        json.key("before").value(changed.before);
        json.key("after").value(changed.after);
        json.key("reason").value(changed.reason);
        json.endObject();
    }
    json.endArray();
    json.key("unobserved").beginArray();
    for (const std::string& item : unobserved) {
        json.value(item);
    }
    json.endArray();
    json.endObject();
    return json.take();
}

}  // namespace mpl
