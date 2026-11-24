#include "mpl/Compiler.hpp"

#include "mpl/core/Error.hpp"
#include "mpl/ir/Automation.hpp"
#include "mpl/util/JsonWriter.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mpl {
namespace {

bool isPowerOfTwo(int value) {
    return value > 0 && (value & (value - 1)) == 0;
}

class ValidateProductionPass final : public Pass {
public:
    std::string_view name() const override { return "validate-production"; }

    void run(ProductionIR& production, std::vector<Diagnostic>& diagnostics) const override {
        if (production.tracks.empty()) {
            diagnostics.push_back(Diagnostic::error("no-tracks", "project has no tracks"));
            return;
        }
        if (production.tempoMap.empty()) {
            diagnostics.push_back(Diagnostic::error("missing-tempo", "production has no tempo event"));
        }
        if (production.meterMap.empty()) {
            diagnostics.push_back(
                Diagnostic::error("missing-meter", "production has no meter event"));
        }

        for (const TempoEvent& tempo : production.tempoMap) {
            if (!(tempo.bpm > 0.0) || tempo.bpm > 1000.0) {
                diagnostics.push_back(Diagnostic::error(
                    "invalid-tempo", "tempo must be in (0, 1000] bpm, got " +
                                         std::to_string(tempo.bpm)));
            }
            if (tempo.position.inWholeNotes().isNegative()) {
                diagnostics.push_back(
                    Diagnostic::error("negative-position", "tempo event starts before zero"));
            }
        }

        for (const MeterEvent& meter : production.meterMap) {
            if (meter.meter.numerator < 1 || !isPowerOfTwo(meter.meter.denominator)) {
                diagnostics.push_back(Diagnostic::error(
                    "invalid-meter", "meter " + meter.meter.toString() +
                                         " must have a positive numerator and a power-of-two denominator"));
            }
            if (meter.position.inWholeNotes().isNegative()) {
                diagnostics.push_back(
                    Diagnostic::error("negative-position", "meter event starts before zero"));
            }
        }

        std::set<std::string> names;
        std::set<InstanceId> instanceIds;
        for (const TrackIR& track : production.tracks) {
            if (track.name.empty()) {
                diagnostics.push_back(Diagnostic::error("unnamed-track", "track has an empty name"));
            } else if (!names.insert(track.name).second) {
                diagnostics.push_back(Diagnostic::error(
                    "duplicate-track", "duplicate track name '" + track.name + "'"));
            }
            if (track.instrument.empty()) {
                diagnostics.push_back(Diagnostic::error(
                    "missing-instrument", "track '" + track.name + "' has no instrument"));
            }
            if (track.clips.empty()) {
                diagnostics.push_back(
                    Diagnostic::error("empty-track", "track '" + track.name + "' has no clips"));
            }

            for (const ClipIR& clip : track.clips) {
                if (clip.instanceId != kInvalidInstanceId &&
                    !instanceIds.insert(clip.instanceId).second) {
                    diagnostics.push_back(Diagnostic::error(
                        "duplicate-instance",
                        "duplicate clip instance id " + std::to_string(clip.instanceId)));
                }
                if (clip.start.inWholeNotes().isNegative()) {
                    diagnostics.push_back(Diagnostic::error(
                        "negative-position", "clip '" + clip.name + "' starts before zero"));
                }
                if (!clip.content.duration().inWholeNotes().isPositive()) {
                    diagnostics.push_back(Diagnostic::error(
                        "empty-clip", "clip '" + clip.name + "' has zero length"));
                } else if (clip.content.empty()) {
                    diagnostics.push_back(Diagnostic::warning(
                        "silent-clip", "clip '" + clip.name + "' contains no notes"));
                }
            }
        }
    }
};

class ValidateAssetsPass final : public Pass {
public:
    std::string_view name() const override { return "validate-assets"; }

    void run(ProductionIR& production, std::vector<Diagnostic>& diagnostics) const override {
        for (const TrackIR& track : production.tracks) {
            std::set<std::string> slots;
            for (const AssetSlot& slot : track.assetSlots) {
                if (slot.name.empty()) {
                    diagnostics.push_back(Diagnostic::error(
                        "unnamed-slot", "track '" + track.name + "' has an unnamed asset slot"));
                } else if (!slots.insert(slot.name).second) {
                    diagnostics.push_back(Diagnostic::error(
                        "duplicate-slot",
                        "track '" + track.name + "' has duplicate asset slot '" + slot.name + "'"));
                }
            }
            std::set<std::string> boundSlots;
            for (const AssetBinding& binding : track.assetBindings) {
                if (slots.find(binding.slot) == slots.end()) {
                    diagnostics.push_back(Diagnostic::error(
                        "unknown-slot", "binding references unknown slot '" + binding.slot +
                                            "' on track '" + track.name + "'"));
                }
                if (!boundSlots.insert(binding.slot).second) {
                    diagnostics.push_back(Diagnostic::error(
                        "duplicate-binding",
                        "track '" + track.name + "' binds slot '" + binding.slot + "' twice"));
                }
            }
        }
    }
};

const TrackIR* findTrack(const ProductionIR& production, const std::string& name) {
    for (const TrackIR& track : production.tracks) {
        if (track.name == name) {
            return &track;
        }
    }
    return nullptr;
}

bool curveFitsRange(const Curve& curve, double minimum, double maximum) {
    for (const CurveSegment& segment : curve.segments) {
        if (segment.from < minimum || segment.from > maximum || segment.to < minimum ||
            segment.to > maximum) {
            return false;
        }
    }
    return true;
}

void checkExpressionLane(TemporalIR& temporal, const TrackIR& track, const ClipIR& clip,
                         std::string_view laneName, TemporalId id, double minimum,
                         double maximum, std::vector<Diagnostic>& diagnostics) {
    if (id == kInvalidTemporalId) {
        return;
    }
    const std::string where = "track '" + track.name + "' clip '" + clip.name + "' " +
                              std::string(laneName);
    if (!temporal.contains(id)) {
        diagnostics.push_back(Diagnostic::error(
            "unknown-curve", where + " references unknown curve #" + std::to_string(id)));
        return;
    }
    if (temporal.domain(id) != TimeDomain::Relative) {
        diagnostics.push_back(Diagnostic::error(
            "expression-domain", where + " requires a relative-domain curve"));
    }
    if (temporal.extent(id) != Rational(1, 1)) {
        diagnostics.push_back(Diagnostic::error(
            "expression-extent", where + " requires a normalized extent of 1"));
    }
    const Curve& curve = temporal.evaluate(id);
    if (!curveFitsRange(curve, minimum, maximum)) {
        diagnostics.push_back(Diagnostic::error(
            "expression-range", where + " curve values leave [" + std::to_string(minimum) +
                                    ", " + std::to_string(maximum) + "]"));
    }
}

void validateTemporal(const ProductionIR& production, TemporalIR& temporal,
                      std::vector<Diagnostic>& diagnostics) {
    for (const TrackIR& track : production.tracks) {
        for (const ClipIR& clip : track.clips) {
            checkExpressionLane(temporal, track, clip, "velocity", clip.expression.velocity, 0.05,
                                2.0, diagnostics);
            checkExpressionLane(temporal, track, clip, "timing", clip.expression.timing, -0.5,
                                0.5, diagnostics);
            checkExpressionLane(temporal, track, clip, "gate", clip.expression.gate, 0.1, 2.0,
                                diagnostics);
            checkExpressionLane(temporal, track, clip, "pan", clip.expression.pan, -1.0, 1.0,
                                diagnostics);
        }
    }

    std::map<std::string, int> replaceCounts;
    for (const AutomationBinding& binding : production.automation) {
        if (!binding.enabled) {
            continue;
        }
        const std::string parameter = binding.target.id.str();
        if (binding.target.id.parameter.empty()) {
            diagnostics.push_back(
                Diagnostic::error("empty-parameter", "automation binding has an empty parameter"));
        }
        if (!(binding.target.minimum <= binding.target.maximum)) {
            diagnostics.push_back(Diagnostic::error(
                "parameter-range", "parameter '" + parameter + "' has an inverted range"));
        }
        if (!temporal.contains(binding.curve)) {
            diagnostics.push_back(Diagnostic::error(
                "unknown-curve", "automation '" + parameter + "' references unknown curve #" +
                                     std::to_string(binding.curve)));
            continue;
        }
        if (temporal.domain(binding.curve) != binding.domain) {
            diagnostics.push_back(Diagnostic::error(
                "automation-domain", "automation '" + parameter +
                                         "' domain does not match its curve"));
        }

        if (binding.target.id.scope != ParameterScope::Master) {
            const TrackIR* track = findTrack(production, binding.target.id.track);
            if (track == nullptr) {
                diagnostics.push_back(Diagnostic::error(
                    "unknown-target", "automation '" + parameter + "' references unknown track '" +
                                          binding.target.id.track + "'"));
            } else if (binding.target.id.scope == ParameterScope::Clip) {
                const auto clipIt = std::find_if(
                    track->clips.begin(), track->clips.end(), [&](const ClipIR& clip) {
                        return clip.name == binding.target.id.clip;
                    });
                if (clipIt == track->clips.end()) {
                    diagnostics.push_back(Diagnostic::error(
                        "unknown-target", "automation '" + parameter +
                                              "' references unknown clip '" +
                                              binding.target.id.clip + "'"));
                }
            }
        }

        const Curve& curve = temporal.evaluate(binding.curve);
        if (!curveFitsRange(curve, binding.target.minimum, binding.target.maximum)) {
            diagnostics.push_back(Diagnostic::error(
                "automation-range", "automation '" + parameter + "' values leave [" +
                                        std::to_string(binding.target.minimum) + ", " +
                                        std::to_string(binding.target.maximum) + "]"));
        }

        if (binding.mode == AutomationMode::Replace) {
            const std::string key =
                parameter + "|" + std::string(timeDomainName(binding.domain));
            if (++replaceCounts[key] > 1) {
                diagnostics.push_back(Diagnostic::error(
                    "automation-conflict",
                    "multiple replace bindings target '" + parameter + "'"));
            }
        }
    }
}

void collectDerivation(const MusicIR& music, ObjectId id, std::vector<ObjectId>& order) {
    if (std::find(order.begin(), order.end(), id) != order.end()) {
        return;
    }
    order.push_back(id);
    for (const ObjectId input : music.node(id).inputs) {
        collectDerivation(music, input, order);
    }
}

bool findOriginPath(const MusicIR& music, ObjectId current, ObjectId target,
                    std::vector<ObjectId>& path) {
    if (!music.contains(current)) {
        return false;
    }
    path.push_back(current);
    if (current == target) {
        return true;
    }
    for (const ObjectId input : music.node(current).inputs) {
        if (findOriginPath(music, input, target, path)) {
            return true;
        }
    }
    path.pop_back();
    return false;
}

void writeCurveJson(JsonWriter& json, const Curve& curve) {
    json.beginObject();
    json.key("domain").value(timeDomainName(curve.domain));
    json.key("extent").value(curve.extent.toString());
    json.key("segments").beginArray();
    for (const CurveSegment& segment : curve.segments) {
        json.beginObject();
        json.key("start").value(segment.start.toString());
        json.key("end").value(segment.end.toString());
        json.key("from").value(segment.from);
        json.key("to").value(segment.to);
        json.key("interpolation").value(interpolationName(segment.interpolation));
        json.endObject();
    }
    json.endArray();
    json.endObject();
}

}  // namespace

bool Compilation::hasErrors() const {
    return std::any_of(diagnostics.begin(), diagnostics.end(), [](const Diagnostic& diagnostic) {
        return diagnostic.severity == Severity::Error;
    });
}

std::size_t Compilation::warningCount() const {
    return static_cast<std::size_t>(
        std::count_if(diagnostics.begin(), diagnostics.end(), [](const Diagnostic& diagnostic) {
            return diagnostic.severity == Severity::Warning;
        }));
}

std::string Compilation::toTraceJson(bool includePassTimings) const {
    JsonWriter json;
    json.beginObject();
    json.key("format").value("mpl.trace");
    json.key("version").value(3);

    json.key("context").beginObject();
    json.key("key").value(music.key().name());
    json.key("meter").value(music.meter().toString());
    json.endObject();

    json.key("music").beginObject();
    json.key("objects").value(static_cast<std::int64_t>(music.size()));
    json.key("evaluated").value(static_cast<std::int64_t>(music.evaluatedCount()));
    json.key("evaluations").value(static_cast<std::int64_t>(music.evaluationCount()));
    json.key("cacheHits").value(static_cast<std::int64_t>(music.cacheHitCount()));
    json.endObject();

    json.key("temporal").beginObject();
    json.key("objects").value(static_cast<std::int64_t>(temporal.size()));
    json.key("evaluated").value(static_cast<std::int64_t>(temporal.evaluatedCount()));
    json.key("evaluations").value(static_cast<std::int64_t>(temporal.evaluationCount()));
    json.key("cacheHits").value(static_cast<std::int64_t>(temporal.cacheHitCount()));
    json.key("curves").beginArray();
    for (const TemporalNode& node : temporal.nodes()) {
        json.beginObject();
        json.key("id").value(static_cast<std::int64_t>(node.id));
        json.key("kind").value(temporalKindName(temporalKind(node.payload)));
        json.key("label").value(node.label);
        json.key("inputs").beginArray();
        for (const TemporalId input : node.inputs) {
            json.value(static_cast<std::int64_t>(input));
        }
        json.endArray();
        if (node.value.has_value()) {
            json.key("value");
            writeCurveJson(json, *node.value);
        }
        json.endObject();
    }
    json.endArray();
    json.endObject();

    json.key("production").beginObject();
    json.key("tracks").value(static_cast<std::int64_t>(production.tracks.size()));
    json.key("clips").value(static_cast<std::int64_t>(production.clipCount()));
    json.key("notes").value(static_cast<std::int64_t>(production.noteCount()));
    json.key("durationWholeNotes").value(production.totalDuration().toString());
    json.endObject();

    json.key("objects").beginArray();
    for (const MusicNode& node : music.nodes()) {
        const NodeKind kind = nodeKind(node.payload);
        json.beginObject();
        json.key("id").value(static_cast<std::int64_t>(node.id));
        json.key("kind").value(nodeKindName(kind));
        json.key("label").value(node.label);
        json.key("inputs").beginArray();
        for (const ObjectId input : node.inputs) {
            json.value(static_cast<std::int64_t>(input));
        }
        json.endArray();
        json.key("contextDependencies").beginArray();
        for (const ContextAspect aspect : contextDependencies(kind)) {
            json.value(contextAspectName(aspect));
        }
        json.endArray();
        if (node.value.has_value()) {
            json.key("noteCount").value(static_cast<std::int64_t>(node.value->noteCount()));
        }
        json.endObject();
    }
    json.endArray();

    json.key("automation").beginArray();
    for (const AutomationBinding& binding : production.automation) {
        json.beginObject();
        json.key("label").value(binding.label);
        json.key("parameter").value(binding.target.id.str());
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

    json.key("expressions").beginArray();
    for (const TrackIR& track : production.tracks) {
        for (const ClipIR& clip : track.clips) {
            json.beginObject();
            json.key("track").value(track.name);
            json.key("clip").value(clip.name);
            json.key("instanceId").value(clip.instanceId == kInvalidInstanceId
                                              ? std::int64_t{-1}
                                              : static_cast<std::int64_t>(clip.instanceId));
            const auto lane = [](TemporalId id) {
                return id == kInvalidTemporalId ? std::int64_t{-1}
                                                : static_cast<std::int64_t>(id);
            };
            json.key("velocity").value(lane(clip.expression.velocity));
            json.key("timing").value(lane(clip.expression.timing));
            json.key("gate").value(lane(clip.expression.gate));
            json.key("pan").value(lane(clip.expression.pan));
            json.endObject();
        }
    }
    json.endArray();

    json.key("provenance").beginArray();
    for (const TrackIR& track : production.tracks) {
        for (const ClipIR& clip : track.clips) {
            json.beginObject();
            json.key("track").value(track.name);
            json.key("clip").value(clip.name);
            json.key("instanceId").value(clip.instanceId == kInvalidInstanceId
                                              ? std::int64_t{-1}
                                              : static_cast<std::int64_t>(clip.instanceId));
            json.key("sourceObject").value(static_cast<std::int64_t>(clip.sourceObject));
            json.key("sourceLabel").value(clip.sourceLabel);
            json.key("startWholeNotes").value(clip.start.toString());

            std::vector<ObjectId> derivation;
            if (clip.sourceObject != kInvalidObjectId && music.contains(clip.sourceObject)) {
                collectDerivation(music, clip.sourceObject, derivation);
            }
            json.key("derivation").beginArray();
            for (const ObjectId object : derivation) {
                json.value(static_cast<std::int64_t>(object));
            }
            json.endArray();

            json.key("notes").beginArray();
            for (std::size_t index = 0; index < clip.content.notes().size(); ++index) {
                const Note& note = clip.content.notes()[index];
                json.beginObject();
                json.key("index").value(static_cast<std::int64_t>(index));
                json.key("pitch").value(note.pitch.name());
                json.key("atWholeNotes")
                    .value(Position(clip.start.inWholeNotes() + note.offset.inWholeNotes())
                               .toString());
                json.key("durationWholeNotes").value(note.duration.toString());
                json.key("origin").value(note.origin == kInvalidObjectId
                                             ? std::int64_t{-1}
                                             : static_cast<std::int64_t>(note.origin));
                json.key("originLabel")
                    .value(note.origin != kInvalidObjectId && music.contains(note.origin)
                               ? music.node(note.origin).label
                               : std::string{});
                json.key("noteInstance")
                    .value(NoteInstanceId{clip.instanceId, static_cast<std::uint32_t>(index)}
                               .str());

                std::vector<ObjectId> originChain;
                if (note.origin != kInvalidObjectId && clip.sourceObject != kInvalidObjectId &&
                    music.contains(clip.sourceObject) && music.contains(note.origin)) {
                    (void)findOriginPath(music, clip.sourceObject, note.origin, originChain);
                }
                json.key("originChain").beginArray();
                for (const ObjectId object : originChain) {
                    json.value(static_cast<std::int64_t>(object));
                }
                json.endArray();
                json.endObject();
            }
            json.endArray();
            json.endObject();
        }
    }
    json.endArray();

    json.key("passes").beginArray();
    for (const PassStats& pass : passes) {
        json.beginObject();
        json.key("name").value(pass.name);
        if (includePassTimings) {
            json.key("durationMs").value(std::round(pass.milliseconds * 1000.0) / 1000.0);
        }
        json.endObject();
    }
    json.endArray();

    json.key("diagnostics").beginArray();
    for (const Diagnostic& diagnostic : diagnostics) {
        json.beginObject();
        json.key("severity").value(diagnostic.severityName());
        json.key("code").value(diagnostic.code);
        json.key("message").value(diagnostic.message);
        json.endObject();
    }
    json.endArray();

    json.endObject();
    return json.take();
}

Compilation Compiler::compile(Project project) const {
    Compilation result;

    // Read scalar production settings before moving the graphs out of the
    // project, so lowering never observes a moved-from object.
    const double tempo = project.tempo();
    const Meter meter = project.meter();

    result.music = std::move(project.music());
    result.temporal = std::move(project.temporal());
    result.music.evaluateAll();
    result.temporal.evaluateAll();

    ProductionIR& production = result.production;
    production.automation = project.automation();
    production.mix = project.mix();
    if (project.logicalExtent().has_value()) {
        production.logicalExtent = *project.logicalExtent();
    }
    production.tempoMap.push_back(TempoEvent{Position{}, tempo});
    production.meterMap.push_back(MeterEvent{Position{}, meter});

    InstanceId nextInstance = 0;
    std::set<InstanceId> explicitInstances;
    for (const TrackSpec& spec : project.tracks()) {
        for (const ClipSpec& clipSpec : spec.clips) {
            if (clipSpec.instanceId != kInvalidInstanceId) {
                explicitInstances.insert(clipSpec.instanceId);
            }
        }
    }

    for (const TrackSpec& spec : project.tracks()) {
        TrackIR track;
        track.name = spec.name;
        track.instrument = spec.instrument;
        track.role = spec.role;
        track.assetSlots.push_back(AssetSlot{"main", AssetKind::Instrument, spec.instrument});
        track.assetBindings.push_back(AssetBinding{"main", spec.instrument, "symbolic"});

        for (const ClipSpec& clipSpec : spec.clips) {
            if (!result.music.contains(clipSpec.part)) {
                throw Error("track '" + spec.name + "' references unknown music object #" +
                                std::to_string(clipSpec.part),
                            "lowering");
            }

            const MusicNode& node = result.music.node(clipSpec.part);
            const Fragment& part = result.music.evaluate(clipSpec.part);

            ClipIR clip;
            if (!clipSpec.name.empty()) {
                clip.name = clipSpec.name;
            } else if (!node.label.empty()) {
                clip.name = node.label;
            } else {
                clip.name = spec.name + " Part";
            }
            clip.start = clipSpec.start;
            clip.content = part;
            clip.sourceObject = clipSpec.part;
            clip.sourceLabel = node.label;
            if (clipSpec.instanceId != kInvalidInstanceId) {
                clip.instanceId = clipSpec.instanceId;
            } else {
                while (explicitInstances.count(nextInstance) != 0) {
                    ++nextInstance;
                }
                clip.instanceId = nextInstance++;
            }
            clip.expression = clipSpec.expression;
            track.clips.push_back(std::move(clip));
        }

        production.tracks.push_back(std::move(track));
    }

    PassManager manager;
    manager.add(std::make_unique<ValidateProductionPass>());
    manager.add(std::make_unique<ValidateAssetsPass>());
    result.passes = manager.run(production, result.diagnostics);

    validateTemporal(production, result.temporal, result.diagnostics);
    return result;
}

}  // namespace mpl
