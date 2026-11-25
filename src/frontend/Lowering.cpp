#include "mpl/frontend/Lowering.hpp"

#include "mpl/core/Error.hpp"
#include "mpl/frontend/ParameterRegistry.hpp"
#include "mpl/music/Analysis.hpp"
#include "mpl/music/Pitch.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace mpl {
namespace {

constexpr int kMaxCallDepth = 32;
constexpr int kMaxFunctionCalls = 4096;
constexpr int kMaxLoopIterations = 8192;
constexpr int kMaxCollectionSize = 1024;

struct Value {
    TypeKind kind = TypeKind::Error;
    ObjectId fragment = kInvalidObjectId;
    ObjectId rhythm = kInvalidObjectId;
    ObjectId harmony = kInvalidObjectId;
    ObjectId chordNode = kInvalidObjectId;
    TemporalId curve = kInvalidTemporalId;
    Rational rational{0, 1};
    double number = 0.0;
    std::int64_t integer = 0;
    bool boolean = false;
    std::shared_ptr<ChordSymbol> chordStorage;
    std::shared_ptr<Voicing> voicingStorage;
    std::string text;
    std::vector<Value> items;

    const ChordSymbol& chordRef() const {
        static const ChordSymbol kNone;
        return chordStorage ? *chordStorage : kNone;
    }
    const Voicing& voicingRef() const {
        static const Voicing kNone;
        return voicingStorage ? *voicingStorage : kNone;
    }
};

bool isRhythmOperatorName(std::string_view name) {
    return name == "overlay" || name == "subdivide" || name == "accent" ||
           name == "realign" || name == "period" || name == "commonPeriod" ||
           name == "quantize" || name == "metricAccent" || name == "metricWeight" ||
           name == "zip" || name == "cycle" || name == "intersect" || name == "union" ||
           name == "difference" || name == "complement" || name == "accentFollow" ||
           name == "onsetSet" || name == "retrigger";
}

bool isOverloadedRhythmName(std::string_view name) {
    return name == "sequence" || name == "repeat" || name == "rotate" ||
           name == "stretch" || name == "shift";
}

bool isHarmonyOperatorName(std::string_view name) {
    return name == "chord" || name == "chordAt" || name == "chordAtBar" ||
           name == "harmonyLength" || name == "harmonyOverlaps" || name == "chordName" ||
           name == "chordPitchClasses" || name == "chordContains" || name == "chordTone" ||
           name == "voicing" || name == "voiceLead" || name == "voicingNotes" ||
           name == "voicingStatus" || name == "voicingMovement" ||
           name == "voicingCrossings" || name == "voicingComplete" ||
           name == "voicingSearchComplete" || name == "voiceDistance" ||
           name == "commonTones" || name == "interval" ||
           name == "pitchClass" || name == "pitchRange" || name == "noteOverlap" ||
           name == "fragmentRange" || name == "parallelPerfects";
}

InstrumentRole roleFromText(std::string_view text) {
    if (text == "bass") return InstrumentRole::Bass;
    if (text == "harmony") return InstrumentRole::Harmony;
    if (text == "percussion") return InstrumentRole::Percussion;
    if (text == "texture") return InstrumentRole::Texture;
    return InstrumentRole::Melodic;
}

Interpolation interpolationFromText(std::string_view text) {
    if (text == "step") return Interpolation::Step;
    if (text == "smoothstep") return Interpolation::Smoothstep;
    return Interpolation::Linear;
}

}  // namespace

InstanceId stableInstanceId(std::string_view key) {
    std::uint32_t hash = 2166136261u;
    for (const char character : key) {
        hash ^= static_cast<std::uint8_t>(character);
        hash *= 16777619u;
    }
    if (hash == kInvalidInstanceId || hash == 0) {
        hash = 1;
    }
    return hash;
}

namespace {

struct LoweredClip {
    std::size_t trackIndex = 0;
    std::string name;
    ObjectId part = kInvalidObjectId;
    PerformanceExpression expression;
    Rational offset{0, 1};
    SourceSpan span;
};

struct LoweredSection {
    std::string name;
    Rational duration{0, 1};
    MeterMap meterMap;
    std::vector<LoweredClip> clips;
    SourceSpan span;
};

/// Metric context used to resolve bars/beats and to key the function cache.
/// A section has its own map and a local anchor; global declarations use the
/// project meter map with anchor zero.
struct MetricScope {
    const MeterMap* map = nullptr;
    Position anchor;
    std::string id;
};

class Lowering {
public:
    Lowering(const Module& module, const HirModule& hir, DiagnosticSink& diagnostics)
        : module_(module), hir_(hir), diagnostics_(diagnostics) {}

    LoweringResult run() {
        project_.setTempo(hir_.tempo);
        project_.setMeter(hir_.meter.numerator, hir_.meter.denominator);
        project_.setKey(hir_.key);
        baseMeterMap_ = project_.meterMap();
        metricScopes_.push_back(MetricScope{&baseMeterMap_, Position{}, "base"});
        project_.modules() = hir_.modules;

        symbolValues_.assign(hir_.symbols.size(), Value{});
        loweredSections_.resize(module_.sections.size());

        for (const FunctionInfo& function : hir_.functions) {
            FunctionDefinitionRecord record;
            record.name = function.name;
            record.returnType = typeName(function.returnType);
            for (const FunctionParamInfo& parameter : function.params) {
                record.parameters.emplace_back(parameter.name, typeName(parameter.type));
            }
            record.contextKey = function.contextKey;
            record.contextMeter = function.contextMeter;
            record.version = function.version;
            project_.functionDefinitions().push_back(std::move(record));
        }

        addTracks();
        buildMix();
        lowerDeclarations();
        lowerArrangement();
        metricScopes_.clear();

        LoweringResult result;
        result.project = std::move(project_);
        return result;
    }

private:
    void error(const SourceSpan& span, std::string code, std::string message,
               std::string related = {}) {
        diagnostics_.error(std::move(code), span, std::move(message), {}, {}, std::move(related));
    }

    const MetricScope& metricContext() const {
        if (metricScopes_.empty()) {
            static const MetricScope kFallback{nullptr, Position{}, "base"};
            return kFallback;
        }
        return metricScopes_.back();
    }

    std::string metricContextKey() const {
        const MetricScope& scope = metricContext();
        return scope.id + "@" + scope.anchor.toString();
    }

    std::string metricContextDescription() const {
        const MetricScope& scope = metricContext();
        if (scope.map == nullptr) {
            return metricContextKey();
        }
        return metricContextKey() + " " + scope.map->meterAt(scope.anchor).toString();
    }

    Rational resolveMetricConstant(const Constant& constant, const SourceSpan& span) {
        if (constant.kind != Constant::Kind::Metric) {
            return constant.rational;
        }
        const MetricScope& scope = metricContext();
        if (scope.map == nullptr) {
            return constant.rational;
        }
        MetricDuration duration;
        duration.amount = constant.metricAmount;
        switch (constant.metricUnit) {
            case Constant::MetricUnit::Bars: duration.unit = MetricUnit::Bars; break;
            case Constant::MetricUnit::Beats: duration.unit = MetricUnit::Beats; break;
            case Constant::MetricUnit::None: duration.unit = MetricUnit::Whole; break;
        }
        std::string message;
        const std::optional<Rational> resolved =
            scope.map->resolveDuration(scope.anchor, duration, message);
        if (!resolved.has_value()) {
            error(span, "meter-metric-duration", message);
            return Rational(0, 1);
        }
        return *resolved;
    }

    Rational resolvedRational(ExprId id) {
        const Constant constant = hir_.constantOf(id);
        return resolveMetricConstant(constant, module_.expr(id).span);
    }

    const Symbol* symbolFor(const std::string& name) const {
        const std::optional<SymbolId> id = hir_.symbols.find(name);
        if (!id.has_value()) {
            return nullptr;
        }
        lastSymbolId_ = *id;
        return &hir_.symbols.get(*id);
    }

    void ensureGlobal(SymbolId id) {
        if (id >= symbolValues_.size() || globalResolved_.count(id) != 0) {
            return;
        }
        if (symbolValues_[id].kind != TypeKind::Error) {
            globalResolved_.insert(id);
            return;
        }
        if (!globalResolving_.insert(id).second) {
            error(module_.span, "sema-global-cycle",
                  "cyclic global binding detected during lowering");
            return;
        }
        const Symbol& symbol = hir_.symbols.get(id);
        if (symbol.kind == SymbolKind::Let &&
            symbol.declarationIndex < module_.lets.size()) {
            lowerLet(module_.lets[symbol.declarationIndex]);
        } else if (symbol.kind == SymbolKind::Curve &&
                   symbol.declarationIndex < module_.curves.size()) {
            lowerCurve(module_.curves[symbol.declarationIndex]);
        }
        globalResolving_.erase(id);
        globalResolved_.insert(id);
    }

    void addTracks() {
        trackIndices_.resize(module_.tracks.size());
        for (std::size_t index = 0; index < module_.tracks.size(); ++index) {
            const TrackDecl& decl = module_.tracks[index];
            const Symbol* symbol = symbolFor(decl.name);
            if (symbol == nullptr) {
                continue;
            }
            std::string provider = decl.asset;
            if (const std::optional<SymbolId> asset = hir_.symbols.find(decl.asset);
                asset.has_value()) {
                const Symbol& assetSymbol = hir_.symbols.get(*asset);
                if (assetSymbol.kind == SymbolKind::Asset &&
                    assetSymbol.declarationIndex < module_.assets.size()) {
                    provider = module_.assets[assetSymbol.declarationIndex].provider;
                }
            }
            TrackSpec& track =
                project_.addTrack(decl.name, provider, roleFromText(decl.role));
            (void)track;
            trackIndices_[index] = index;
        }
    }

    void buildMix() {
        MixSpec& mix = project_.mix();
        for (const ProcessorDecl& decl : module_.processors) {
            ProcessorKind kind = ProcessorKind::Gain;
            if (!processorKindFromName(decl.kind, kind)) {
                continue;
            }
            ProcessorSpec processor;
            processor.name = decl.name;
            processor.kind = kind;
            processor.mode = decl.mode;
            for (const ProcessorField& field : decl.fields) {
                processor.parameters[field.name] = numberOf(field.value);
            }
            applyProcessorDefaults(processor);
            mix.processors.push_back(std::move(processor));
        }

        for (const BusDecl& decl : module_.buses) {
            ChannelStripSpec strip;
            strip.name = decl.name;
            strip.gainDb = decl.gain == kInvalidExprId ? 0.0 : numberOf(decl.gain);
            strip.pan = decl.pan == kInvalidExprId ? 0.0 : numberOf(decl.pan);
            strip.inserts = decl.inserts;
            mix.buses.push_back(BusSpec{std::move(strip)});
        }

        for (const TrackDecl& decl : module_.tracks) {
            ChannelStripSpec strip;
            strip.name = decl.name;
            strip.gainDb = decl.gain == kInvalidExprId ? 0.0 : numberOf(decl.gain);
            strip.pan = decl.pan == kInvalidExprId ? 0.0 : numberOf(decl.pan);
            strip.inserts = decl.inserts;
            for (const SendDecl& send : decl.sends) {
                SendSpec spec;
                spec.bus = send.bus;
                spec.gain = send.level == kInvalidExprId ? 0.0 : numberOf(send.level);
                strip.sends.push_back(std::move(spec));
            }
            for (const SidechainDecl& sidechain : decl.sidechains) {
                strip.sidechains.push_back(
                    SidechainSpec{sidechain.processor, sidechain.source});
            }
            mix.tracks.push_back(std::move(strip));
        }

        if (module_.master.has_value()) {
            ChannelStripSpec& master = mix.master;
            master.name = "master";
            master.gainDb = module_.master->gain == kInvalidExprId
                                ? 0.0
                                : numberOf(module_.master->gain);
            master.pan = module_.master->pan == kInvalidExprId
                             ? 0.0
                             : numberOf(module_.master->pan);
            master.inserts = module_.master->inserts;
        }
    }

    void lowerDeclarations() {
        for (const DeclRef& reference : module_.declarationOrder) {
            switch (reference.kind) {
                case DeclKind::Let: {
                    const LetDecl& decl = module_.lets[reference.index];
                    if (const std::optional<SymbolId> symbol = hir_.symbols.find(decl.name);
                        symbol.has_value()) {
                        ensureGlobal(*symbol);
                    } else {
                        lowerLet(decl);
                    }
                    break;
                }
                case DeclKind::Curve: {
                    const CurveDecl& decl = module_.curves[reference.index];
                    if (const std::optional<SymbolId> symbol = hir_.symbols.find(decl.name);
                        symbol.has_value()) {
                        ensureGlobal(*symbol);
                    } else {
                        lowerCurve(decl);
                    }
                    break;
                }
                case DeclKind::Section:
                    lowerSection(module_.sections[reference.index], reference.index);
                    break;
                case DeclKind::Automation:
                    lowerAutomation(module_.automations[reference.index]);
                    break;
                default:
                    break;
            }
        }
    }

    void lowerLet(const LetDecl& decl) {
        const Symbol* symbol = symbolFor(decl.name);
        if (symbol == nullptr) {
            return;
        }
        const SymbolId id = lastSymbolId_;
        Value value = lowerExpr(decl.value, decl.name);
        symbolValues_[id] = std::move(value);
    }

    void lowerCurve(const CurveDecl& decl) {
        const Symbol* symbol = symbolFor(decl.name);
        if (symbol == nullptr) {
            return;
        }
        const SymbolId id = lastSymbolId_;
        const Type type = hir_.symbolTypes[id];
        const Rational extent = hir_.symbolCurveExtents[id];
        Value value = lowerCurveValue(decl.value, type.domain, extent, decl.name);
        symbolValues_[id] = std::move(value);
    }

    Value lowerCurveValue(ExprId id, TimeDomain domain, const Rational& extent,
                          const std::string& label) {
        const Expr& expr = module_.expr(id);
        Value value;
        value.kind = TypeKind::Curve;
        if (expr.kind == ExprKind::Keyframes) {
            std::vector<CurvePoint> points;
            for (const KeyframeEntry& entry : expr.keyframes) {
                CurvePoint point;
                point.time = rationalOf(entry.time);
                point.value = entry.value;
                point.interpolation = interpolationFromText(entry.interpolation);
                points.push_back(point);
            }
            value.curve = project_.temporal().keyframes(std::move(points), domain, extent, label);
            return value;
        }
        if (expr.kind != ExprKind::Call) {
            error(expr.span, "type-curve-context", "expected a curve literal");
            return Value{};
        }
        if (expr.text == "constant") {
            value.curve = project_.temporal().constant(numberOf(expr.args[0]), domain, extent,
                                                       label);
        } else if (expr.text == "linear") {
            value.curve = project_.temporal().linear(numberOf(expr.args[0]),
                                                     numberOf(expr.args[1]), domain, extent,
                                                     label);
        } else if (expr.text == "step") {
            value.curve = project_.temporal().step(numberOf(expr.args[0]),
                                                   numberOf(expr.args[1]),
                                                   rationalOf(expr.args[2]), domain, extent, label);
        } else if (expr.text == "smoothstep") {
            value.curve = project_.temporal().smoothstep(numberOf(expr.args[0]),
                                                         numberOf(expr.args[1]), domain, extent,
                                                         label);
        } else {
            error(expr.span, "type-curve-context", "unknown curve constructor");
            return Value{};
        }
        return value;
    }

    MeterSpec specOf(const SectionDecl& decl) const {
        if (decl.meter.has_value()) {
            return *decl.meter;
        }
        MeterSpec spec;
        spec.span = decl.span;
        spec.numerator = hir_.meter.numerator;
        spec.denominator = hir_.meter.denominator;
        return spec;
    }

    static MetricGrouping groupingOf(const MeterSpec& spec) {
        MetricGrouping grouping;
        grouping.groups = spec.grouping;
        grouping.explicitGrouping = spec.explicitGrouping;
        return grouping;
    }

    void lowerSection(const SectionDecl& decl, std::size_t sectionIndex) {
        LoweredSection section;
        section.name = decl.name;
        section.span = decl.span;

        const MeterSpec spec = specOf(decl);
        const Meter initialMeter{spec.numerator, spec.denominator};
        section.meterMap = MeterMap(initialMeter, groupingOf(spec));

        const std::string scopeId = "section:" + std::to_string(sectionIndex);
        metricScopes_.push_back(
            MetricScope{&section.meterMap, Position{}, scopeId});

        for (const MeterChange& change : decl.meterChanges) {
            Value position = lowerExpr(change.at, {});
            if (position.kind != TypeKind::Duration) {
                continue;
            }
            if (position.rational.isNegative()) {
                error(change.span, "meter-change-invalid",
                      "meter change position must not be negative");
                continue;
            }
            const Meter meter{change.meter.numerator, change.meter.denominator};
            std::string message;
            if (!section.meterMap.tryAdd(Position(position.rational), meter,
                                         groupingOf(change.meter), message)) {
                error(change.span, "meter-change-invalid", message);
            }
        }

        Value duration = lowerExpr(decl.duration, {});
        if (duration.kind != TypeKind::Duration) {
            duration.rational = Rational(0, 1);
        }
        section.duration = duration.rational;
        if (!section.duration.isPositive()) {
            error(decl.span, "type-duration",
                  "section '" + decl.name + "' has a non-positive duration");
        } else if (section.meterMap.isInRange(Position(section.duration)) &&
                   !section.meterMap.isBarStart(Position(section.duration))) {
            error(decl.span, "section-not-bar-aligned",
                  "section '" + decl.name + "' lasts " + section.duration.toString() +
                      " whole notes, which is not an integer number of bars in " +
                      section.meterMap.describe() +
                      "; the next section would start mid-bar");
        }

        std::size_t clipTrackCursor = 0;
        for (std::size_t index = 0; index < sectionIndex; ++index) {
            clipTrackCursor += module_.sections[index].clips.size();
        }

        for (std::size_t clipIndex = 0; clipIndex < decl.clips.size(); ++clipIndex) {
            const ClipDecl& clip = decl.clips[clipIndex];
            const SymbolId trackSymbol = hir_.clipTrackSymbols[clipTrackCursor + clipIndex];
            if (trackSymbol == kInvalidSymbolId) {
                continue;
            }
            const Symbol& track = hir_.symbols.get(trackSymbol);
            if (track.declarationIndex >= trackIndices_.size()) {
                continue;
            }
            const std::string clipName = clip.name.empty() ? decl.name : clip.name;
            Rational offset(0, 1);
            if (clip.at != kInvalidExprId) {
                Value offsetValue = lowerExpr(clip.at, {});
                if (offsetValue.kind == TypeKind::Duration) {
                    offset = offsetValue.rational;
                }
            }
            if (offset.isNegative()) {
                error(clip.span, "type-duration", "clip offset must not be negative");
                offset = Rational(0, 1);
            }
            // Clip content is evaluated with its own metric anchor so functions
            // using bars/beats resolve relative to the clip's placement inside
            // the section.
            metricScopes_.push_back(
                MetricScope{&section.meterMap, Position(offset), scopeId});
            Value part = lowerExpr(clip.play, clipName);
            metricScopes_.pop_back();
            if (part.fragment == kInvalidObjectId) {
                continue;
            }
            LoweredClip lowered;
            lowered.trackIndex = trackIndices_[track.declarationIndex];
            lowered.name = clipName;
            lowered.part = part.fragment;
            lowered.span = clip.span;
            lowered.offset = offset;
            lowered.expression.velocity = lowerLane(clip.velocity);
            lowered.expression.timing = lowerLane(clip.timing);
            lowered.expression.gate = lowerLane(clip.gate);
            lowered.expression.pan = lowerLane(clip.pan);

            const Duration contentDuration = [&]() {
                try {
                    return project_.music().evaluate(part.fragment).duration();
                } catch (const Error& evaluation) {
                    diagnostics_.error(
                        evaluation.code().empty() ? "music-evaluation" : evaluation.code(),
                        clip.span, evaluation.what());
                    return Duration{};
                }
            }();
            if (offset + contentDuration.inWholeNotes() > section.duration) {
                diagnostics_.warning(
                    "sema-section-overflow", clip.span,
                    "clip content (" + contentDuration.toString() +
                        " at offset " + offset.toString() +
                        ") is longer than section '" + decl.name + "' (" +
                        section.duration.toString() + "); the clip will overlap later sections");
            }
            section.clips.push_back(std::move(lowered));
        }
        metricScopes_.pop_back();
        loweredSections_[sectionIndex] = std::move(section);
    }

    TemporalId lowerLane(ExprId id) {
        if (id == kInvalidExprId) {
            return kInvalidTemporalId;
        }
        Value value = lowerExpr(id, {});
        return value.curve;
    }

    void lowerAutomation(const AutomationDecl& decl) {
        if (!decl.processor.empty()) {
            const std::optional<SymbolId> processorSymbol = hir_.symbols.find(decl.processor);
            if (!processorSymbol.has_value()) {
                return;
            }
            const Symbol& symbol = hir_.symbols.get(*processorSymbol);
            if (symbol.kind != SymbolKind::Processor) {
                return;
            }
            const ProcessorDecl& processor =
                module_.processors[symbol.declarationIndex];
            ProcessorKind kind = ProcessorKind::Gain;
            if (!processorKindFromName(processor.kind, kind)) {
                return;
            }
            const ProcessorParameterDescriptor* descriptor = nullptr;
            for (const ProcessorParameterDescriptor& candidate : processorParameters(kind)) {
                if (candidate.name == decl.parameter) {
                    descriptor = &candidate;
                    break;
                }
            }
            if (descriptor == nullptr) {
                return;
            }
            Value curve = lowerExpr(decl.curve, {});
            if (curve.curve == kInvalidTemporalId) {
                return;
            }
            ProcessorAutomationSpec automation;
            automation.target = decl.scope == "master" ? "master" : decl.track;
            automation.processor = decl.processor;
            automation.parameter = decl.parameter;
            automation.curve = curve.curve;
            if (decl.mode == "add") {
                automation.mode = AutomationMode::Add;
            } else if (decl.mode == "multiply") {
                automation.mode = AutomationMode::Multiply;
            }
            automation.minimum = descriptor->minimum;
            automation.maximum = descriptor->maximum;
            project_.mix().processorAutomation.push_back(std::move(automation));
            return;
        }

        const ParameterInfo* parameter = findParameter(decl.parameter);
        if (parameter == nullptr) {
            return;
        }
        Value curve = lowerExpr(decl.curve, {});
        if (curve.curve == kInvalidTemporalId) {
            return;
        }
        ParameterTarget target = makeParameterTarget(
            decl.scope == "master" ? ParameterScope::Master : ParameterScope::Track, decl.track,
            "", decl.parameter, parameter->unit);
        target.minimum = parameter->minimum;
        target.maximum = parameter->maximum;
        AutomationMode mode = AutomationMode::Replace;
        if (decl.mode == "add") {
            mode = AutomationMode::Add;
        } else if (decl.mode == "multiply") {
            mode = AutomationMode::Multiply;
        }
        AutomationBinding binding;
        binding.label = decl.scope == "master" ? "master." + decl.parameter
                                               : decl.track + "." + decl.parameter;
        binding.target = target;
        binding.curve = curve.curve;
        binding.domain = TimeDomain::Musical;
        binding.mode = mode;
        project_.automation().push_back(std::move(binding));
    }

    void lowerArrangement() {
        if (!module_.arrangement.has_value()) {
            return;
        }
        std::optional<MeterMap> globalMeterMap;
        std::map<std::string, int> occurrences;
        Rational cursor(0, 1);
        for (std::size_t index = 0; index < module_.arrangement->plays.size(); ++index) {
            const SymbolId sectionSymbol = hir_.arrangementSymbols[index];
            if (sectionSymbol == kInvalidSymbolId) {
                continue;
            }
            const Symbol& symbol = hir_.symbols.get(sectionSymbol);
            if (symbol.declarationIndex >= loweredSections_.size()) {
                continue;
            }
            const LoweredSection& section = loweredSections_[symbol.declarationIndex];

            if (!globalMeterMap.has_value()) {
                const MeterMapEvent& first = section.meterMap.initial();
                globalMeterMap = MeterMap(first.meter, first.grouping);
            } else if (!globalMeterMap->isBarStart(Position(cursor))) {
                error(section.span, "section-not-bar-aligned",
                      "section '" + section.name + "' starts at " + cursor.toString() +
                          ", which is not a bar boundary in " +
                          globalMeterMap->describe());
            } else {
                const MeterMapEvent& first = section.meterMap.initial();
                const MeterMapEvent& current = globalMeterMap->eventAt(Position(cursor));
                const bool sameMeter =
                    current.meter.numerator == first.meter.numerator &&
                    current.meter.denominator == first.meter.denominator &&
                    current.grouping == first.grouping;
                if (!sameMeter) {
                    std::string message;
                    if (!globalMeterMap->tryAdd(Position(cursor), first.meter, first.grouping,
                                                message)) {
                        error(section.span, "meter-change-invalid", message);
                    }
                }
            }

            if (globalMeterMap.has_value()) {
                for (std::size_t eventIndex = 1; eventIndex < section.meterMap.size();
                     ++eventIndex) {
                    const MeterMapEvent& event = section.meterMap.events()[eventIndex];
                    const Position absolute(cursor + event.position.inWholeNotes());
                    std::string message;
                    if (!globalMeterMap->tryAdd(absolute, event.meter, event.grouping,
                                                message)) {
                        error(section.span, "meter-change-invalid", message);
                    }
                }
            }

            const int occurrence = occurrences[section.name]++;
            SectionRecord sectionRecord;
            sectionRecord.name = section.name;
            sectionRecord.occurrence = occurrence;
            sectionRecord.start = Position(cursor);
            sectionRecord.duration = Duration(section.duration);
            project_.sections().push_back(std::move(sectionRecord));
            for (const LoweredClip& clip : section.clips) {
                const std::string trackName = module_.tracks[clip.trackIndex].name;
                const std::string key = "track:" + trackName + "|section:" + section.name +
                                        "|play:" + std::to_string(occurrence) +
                                        "|clip:" + clip.name;
                const InstanceId instanceId = stableInstanceId(key);
                if (!instanceIds_.insert(instanceId).second) {
                    error(clip.span, "sema-instance-id-collision",
                          "stable instance id collision for '" + key + "'", key);
                }
                const Position start(cursor + clip.offset);
                if (clip.trackIndex >= project_.tracks().size()) {
                    continue;
                }
                TrackSpec& track =
                    project_.addClip(clip.trackIndex, clip.name, clip.part, start);
                track.clips.back().instanceId = instanceId;
                track.clips.back().expression = clip.expression;
            }
            cursor += section.duration;
        }

        project_.setLogicalExtent(Duration(cursor));
        if (globalMeterMap.has_value()) {
            project_.setMeterMap(std::move(*globalMeterMap));
        } else {
            project_.setMeterMap(baseMeterMap_);
        }

        for (std::size_t trackIndex = 0; trackIndex < project_.tracks().size(); ++trackIndex) {
            if (project_.tracks()[trackIndex].clips.empty()) {
                const std::string& name = project_.tracks()[trackIndex].name;
                error(module_.tracks[trackIndex].span, "sema-empty-track",
                      "track '" + name + "' has no clips in the arrangement", name);
            }
        }
    }

    std::optional<Value> lookupLocalValue(const std::string& name) const {
        for (auto scope = localScopes_.rbegin(); scope != localScopes_.rend(); ++scope) {
            const auto iterator = scope->find(name);
            if (iterator != scope->end()) {
                return iterator->second;
            }
        }
        return std::nullopt;
    }

    static Rational rationalOfValue(const Value& value) {
        if (value.kind == TypeKind::Integer) {
            return Rational(value.integer, 1);
        }
        if (value.rational.isZero() && value.integer != 0) {
            return Rational(value.integer, 1);
        }
        return value.rational;
    }

    static std::int64_t integerOfValue(const Value& value) {
        if (value.kind == TypeKind::Integer) {
            return value.integer;
        }
        if (value.kind == TypeKind::Number || value.kind == TypeKind::Scalar) {
            return static_cast<std::int64_t>(std::llround(value.number));
        }
        if (value.kind == TypeKind::Duration || value.kind == TypeKind::Position) {
            const Rational rational = value.rational;
            if (rational.denominator() == 1) {
                return rational.numerator();
            }
            return static_cast<std::int64_t>(std::llround(rational.toDouble()));
        }
        return value.integer;
    }

    static std::string valueKey(const Value& value) {
        switch (value.kind) {
            case TypeKind::Fragment: return "f" + std::to_string(value.fragment);
            case TypeKind::Rhythm: return "y" + std::to_string(value.rhythm);
            case TypeKind::Harmony: return "h" + std::to_string(value.harmony);
            case TypeKind::Chord: return "c" + value.chordRef().name();
            case TypeKind::Voicing: {
                std::string key = std::string("v") + std::string(voicingStatusName(value.voicingRef().status));
                for (const Pitch& pitch : value.voicingRef().notes) {
                    key += ":" + std::to_string(pitch.midi());
                }
                return key;
            }
            case TypeKind::Curve: return "c" + std::to_string(value.curve);
            case TypeKind::Boolean: return value.boolean ? "b1" : "b0";
            case TypeKind::Pitch:
            case TypeKind::String: return "s" + value.text;
            case TypeKind::Integer: return "i" + std::to_string(value.integer);
            case TypeKind::Duration:
            case TypeKind::Position: return "r" + value.rational.toString();
            case TypeKind::List: {
                std::string key = "[";
                for (const Value& item : value.items) {
                    key += valueKey(item);
                    key += ",";
                }
                key += "]";
                return key;
            }
            case TypeKind::Number:
            case TypeKind::Scalar:
            case TypeKind::Gain:
            case TypeKind::Frequency:
            case TypeKind::PhysicalTime:
                return "n" + std::to_string(value.number);
            case TypeKind::Error: return "error";
            default: return "v";
        }
    }

    static std::string describeArguments(const std::vector<Value>& arguments) {
        std::string text;
        for (std::size_t index = 0; index < arguments.size(); ++index) {
            if (index != 0) {
                text += ", ";
            }
            text += valueKey(arguments[index]);
        }
        return text;
    }

    Value evaluateUserFunction(const FunctionInfo& function, const Expr& call) {
        std::vector<Value> arguments;
        for (const ExprId argument : call.args) {
            arguments.push_back(lowerExpr(argument, {}));
        }

        FunctionCallRecord record;
        record.function = function.name;
        record.callFile = call.span.file;
        record.callLine = call.span.line;
        record.callColumn = call.span.column;
        record.instance = callInstanceCounter_++;
        record.depth = callDepth_;
        record.arguments = describeArguments(arguments);
        record.contextKey = hir_.key.name();
        const MetricScope& scope = metricContext();
        record.contextMeter = scope.map == nullptr
                                  ? hir_.meter.toString()
                                  : scope.map->meterAt(scope.anchor).toString();
        record.metricContext = metricContextDescription();

        std::string key = "fn:" + function.name + ":v" +
                          std::to_string(function.version) + ":k" + hir_.key.name() +
                          ":mc" + metricContextKey();
        for (const Value& argument : arguments) {
            key += "|" + valueKey(argument);
        }
        const auto cached = functionCache_.find(key);
        if (cached != functionCache_.end()) {
            record.cacheHit = true;
            if (cached->second.fragment != kInvalidObjectId) {
                record.objects.push_back(cached->second.fragment);
            }
            if (cached->second.curve != kInvalidTemporalId) {
                record.objects.push_back(static_cast<ObjectId>(cached->second.curve));
            }
            if (cached->second.rhythm != kInvalidObjectId) {
                record.objects.push_back(cached->second.rhythm);
            }
            project_.functionCalls().push_back(std::move(record));
            return cached->second;
        }

        if (callDepth_ >= kMaxCallDepth) {
            error(call.span, "limit-call-depth",
                  "function call depth exceeds " + std::to_string(kMaxCallDepth));
            return Value{};
        }
        if (callCount_ >= kMaxFunctionCalls) {
            error(call.span, "limit-call-count",
                  "function call count exceeds " + std::to_string(kMaxFunctionCalls));
            return Value{};
        }
        ++callCount_;
        if (function.declarationIndex >= module_.functions.size()) {
            return Value{};
        }
        const FunctionDecl& decl = module_.functions[function.declarationIndex];
        if (arguments.size() != decl.params.size()) {
            return Value{};
        }

        const std::size_t firstObject = project_.music().size();
        const std::size_t recordIndex = project_.functionCalls().size();
        project_.functionCalls().push_back(record);
        localScopes_.emplace_back();
        for (std::size_t index = 0; index < decl.params.size(); ++index) {
            Value argument = arguments[index];
            const Type& expected = function.params[index].type;
            if (expected.kind == TypeKind::Integer &&
                (argument.kind == TypeKind::Number || argument.kind == TypeKind::Scalar)) {
                argument.kind = TypeKind::Integer;
                argument.integer =
                    static_cast<std::int64_t>(std::llround(argument.number));
                argument.number = static_cast<double>(argument.integer);
                argument.rational = Rational(argument.integer, 1);
            }
            localScopes_.back()[decl.params[index].name] = argument;
        }
        ++callDepth_;
        Value result;
        for (const Stmt& statement : decl.body) {
            if (statement.kind == StmtKind::Let) {
                const Value value = lowerExpr(statement.value, function.name);
                localScopes_.back()[statement.name] = value;
            } else {
                result = lowerExpr(statement.value, function.name);
            }
        }
        --callDepth_;
        localScopes_.pop_back();

        for (std::size_t object = firstObject; object < project_.music().size(); ++object) {
            const MusicNode& node =
                project_.music().node(static_cast<ObjectId>(object));
            record.objects.push_back(static_cast<ObjectId>(object));
            std::string operation(nodeKindName(nodeKind(node.payload)));
            if (!node.label.empty()) {
                operation += ":" + node.label;
            }
            record.operators.push_back(std::move(operation));
        }
        if (result.fragment != kInvalidObjectId &&
            std::find(record.objects.begin(), record.objects.end(), result.fragment) ==
                record.objects.end()) {
            record.objects.push_back(result.fragment);
        }
        if (result.rhythm != kInvalidObjectId &&
            std::find(record.objects.begin(), record.objects.end(), result.rhythm) ==
                record.objects.end()) {
            record.objects.push_back(result.rhythm);
        }
        project_.functionCalls()[recordIndex] = std::move(record);
        functionCache_[key] = result;
        return result;
    }

    Value lowerBinary(const Expr& expr, ExprId id, const std::string& label) {
        const Value left = lowerExpr(expr.args[0], label);
        const Value right = lowerExpr(expr.args[1], label);
        const TypeKind resultKind = hir_.typeOf(id).kind;
        if (expr.text == "&&") {
            Value value;
            value.kind = TypeKind::Boolean;
            value.boolean = left.boolean && right.boolean;
            return value;
        }
        if (expr.text == "||") {
            Value value;
            value.kind = TypeKind::Boolean;
            value.boolean = left.boolean || right.boolean;
            return value;
        }
        if (expr.text == "==" || expr.text == "!=" || expr.text == "<" ||
            expr.text == "<=" || expr.text == ">" || expr.text == ">=") {
            bool result = false;
            if (left.kind == TypeKind::String) {
                result = expr.text == "==" ? left.text == right.text
                                            : expr.text == "!=" ? left.text != right.text
                                                                : false;
            } else if (left.kind == TypeKind::Boolean) {
                result = expr.text == "==" ? left.boolean == right.boolean
                                            : expr.text == "!=" ? left.boolean != right.boolean
                                                                : false;
            } else if (left.kind == TypeKind::Duration || left.kind == TypeKind::Position) {
                const Rational a = rationalOfValue(left);
                const Rational b = rationalOfValue(right);
                if (expr.text == "==") result = a == b;
                else if (expr.text == "!=") result = !(a == b);
                else if (expr.text == "<") result = a < b;
                else if (expr.text == "<=") result = a < b || a == b;
                else if (expr.text == ">") result = b < a;
                else result = b < a || a == b;
            } else {
                const double a = left.kind == TypeKind::Integer
                                     ? static_cast<double>(left.integer)
                                     : left.number;
                const double b = right.kind == TypeKind::Integer
                                     ? static_cast<double>(right.integer)
                                     : right.number;
                if (expr.text == "==") result = a == b;
                else if (expr.text == "!=") result = a != b;
                else if (expr.text == "<") result = a < b;
                else if (expr.text == "<=") result = a <= b;
                else if (expr.text == ">") result = a > b;
                else result = a >= b;
            }
            Value value;
            value.kind = TypeKind::Boolean;
            value.boolean = result;
            return value;
        }

        if (resultKind == TypeKind::Integer) {
            const std::int64_t a = integerOfValue(left);
            const std::int64_t b = integerOfValue(right);
            Value value;
            value.kind = TypeKind::Integer;
            if (expr.text == "+") value.integer = a + b;
            else if (expr.text == "-") value.integer = a - b;
            else if (expr.text == "*") value.integer = a * b;
            else if (expr.text == "/") value.integer = b == 0 ? 0 : a / b;
            else if (expr.text == "%") value.integer = b == 0 ? 0 : a % b;
            value.number = static_cast<double>(value.integer);
            value.rational = Rational(value.integer, 1);
            return value;
        }

        const double a = left.kind == TypeKind::Integer ? static_cast<double>(left.integer)
                                                        : left.number;
        const double b = right.kind == TypeKind::Integer ? static_cast<double>(right.integer)
                                                         : right.number;
        Value value;
        value.kind = TypeKind::Number;
        if (expr.text == "+") value.number = a + b;
        else if (expr.text == "-") value.number = a - b;
        else if (expr.text == "*") value.number = a * b;
        else if (expr.text == "/") value.number = b == 0.0 ? 0.0 : a / b;
        else if (expr.text == "%") value.number = b == 0.0 ? 0.0 : std::fmod(a, b);
        return value;
    }

    Value lowerRhythmLiteral(const Expr& expr, const std::string& label) {
        std::vector<RhythmEvent> events;
        Rational cursor(0, 1);
        for (const RhythmEntry& entry : expr.rhythm) {
            const Value duration = lowerExpr(entry.duration, label);
            if (duration.kind != TypeKind::Duration || !duration.rational.isPositive()) {
                error(entry.span, "type-duration", "rhythm entry duration must be positive");
                continue;
            }
            RhythmEvent event;
            event.onset = cursor;
            event.duration = duration.rational;
            event.rest = entry.rest;
            event.accent = entry.rest ? 0 : entry.accent;
            events.push_back(std::move(event));
            cursor += duration.rational;
        }
        Rational period = cursor;
        if (expr.explicitSpan != kInvalidExprId) {
            const Value span = lowerExpr(expr.explicitSpan, label);
            if (span.kind == TypeKind::Duration && span.rational.isPositive()) {
                if (span.rational < cursor) {
                    error(expr.span, "type-duration",
                          "rhythm span " + span.rational.toString() +
                              " is shorter than its content " + cursor.toString());
                } else {
                    period = span.rational;
                }
            }
        }
        Value value;
        if (!period.isPositive()) {
            error(expr.span, "type-rhythm-empty", "rhythm must have a positive logical length");
            return value;
        }
        try {
            value.kind = TypeKind::Rhythm;
            value.rhythm = project_.music().rhythmLiteral(
                Rhythm::fromEvents(std::move(events), period), label);
        } catch (const Error& rhythmError) {
            diagnostics_.error(rhythmError.code().empty() ? "rhythm" : rhythmError.code(),
                               expr.span, rhythmError.what());
            return Value{};
        }
        return value;
    }

    /// Handles every operator whose result is a Rhythm (including the rhythm
    /// overloads of sequence/repeat/rotate/shift/stretch). Keeping this out of
    /// lowerCall keeps the recursive fragment path's stack frame small: the
    /// 32-level function-call chain must not overflow the test stack.
    std::optional<Value> lowerRhythmCall(const Expr& expr, const std::string& label) {
        const std::string& name = expr.text;
        const auto rhythmArgument = [&](std::size_t index) {
            return index < expr.args.size() &&
                   hir_.typeOf(expr.args[index]).kind == TypeKind::Rhythm;
        };
        const auto rhythmValue = [&](ObjectId id) {
            Value value;
            value.kind = TypeKind::Rhythm;
            value.rhythm = id;
            return value;
        };

        if (name == "sequence" && rhythmArgument(0)) {
            std::vector<ObjectId> parts;
            for (const ExprId argument : expr.args) {
                const Value part = lowerExpr(argument, {});
                if (part.rhythm == kInvalidObjectId) {
                    return Value{};
                }
                parts.push_back(part.rhythm);
            }
            return rhythmValue(project_.music().rhythmSequence(parts, label));
        }
        if (name == "repeat" && rhythmArgument(0)) {
            const Value source = lowerExpr(expr.args[0], {});
            const int count = static_cast<int>(integerOfValue(lowerExpr(expr.args[1], {})));
            return rhythmValue(project_.music().rhythmRepeat(source.rhythm, count, label));
        }
        if (name == "rotate" && rhythmArgument(0)) {
            const Value source = lowerExpr(expr.args[0], {});
            const Rational amount = rationalOfValue(lowerExpr(expr.args[1], {}));
            return rhythmValue(project_.music().rhythmRotate(source.rhythm, amount, label));
        }
        if (name == "stretch" && rhythmArgument(0)) {
            const Value source = lowerExpr(expr.args[0], {});
            const Rational factor = rationalOfValue(lowerExpr(expr.args[1], {}));
            return rhythmValue(project_.music().rhythmStretch(source.rhythm, factor, label));
        }
        if (name == "shift" && rhythmArgument(0)) {
            const Value source = lowerExpr(expr.args[0], {});
            const Rational amount = rationalOfValue(lowerExpr(expr.args[1], {}));
            return rhythmValue(project_.music().rhythmShift(source.rhythm, amount, label));
        }
        if (name == "overlay") {
            std::vector<ObjectId> parts;
            for (const ExprId argument : expr.args) {
                const Value part = lowerExpr(argument, {});
                if (part.rhythm == kInvalidObjectId) {
                    return Value{};
                }
                parts.push_back(part.rhythm);
            }
            return rhythmValue(project_.music().rhythmOverlay(parts, label));
        }
        if (name == "subdivide") {
            const Value source = lowerExpr(expr.args[0], {});
            const int parts = static_cast<int>(integerOfValue(lowerExpr(expr.args[1], {})));
            return rhythmValue(project_.music().rhythmSubdivide(source.rhythm, parts, label));
        }
        if (name == "accent") {
            const Value source = lowerExpr(expr.args[0], {});
            std::vector<int> levels;
            const Value levelValue = lowerExpr(expr.args[1], {});
            if (levelValue.kind == TypeKind::Integer) {
                levels.push_back(static_cast<int>(levelValue.integer));
            } else {
                for (const Value& item : levelValue.items) {
                    levels.push_back(static_cast<int>(integerOfValue(item)));
                }
            }
            return rhythmValue(project_.music().rhythmAccent(source.rhythm, levels, label));
        }
        if (name == "realign") {
            const Value source = lowerExpr(expr.args[0], {});
            const Rational period = rationalOfValue(lowerExpr(expr.args[1], {}));
            return rhythmValue(project_.music().rhythmRealign(source.rhythm, period, label));
        }
        if (name == "quantize") {
            const Value source = lowerExpr(expr.args[0], {});
            const Rational grid = rationalOfValue(lowerExpr(expr.args[1], {}));
            return rhythmValue(project_.music().rhythmQuantize(source.rhythm, grid, label));
        }
        if (name == "onsetSet") {
            const Value source = lowerExpr(expr.args[0], {});
            const Rational unit = rationalOfValue(lowerExpr(expr.args[1], {}));
            return rhythmValue(project_.music().rhythmOnsetSet(source.rhythm, unit, label));
        }
        if (name == "retrigger") {
            const Value source = lowerExpr(expr.args[0], {});
            return rhythmValue(project_.music().rhythmRetrigger(source.rhythm, label));
        }
        if (name == "period") {
            const Value source = lowerExpr(expr.args[0], {});
            Value value;
            value.kind = TypeKind::Duration;
            value.rational = project_.music().evaluateRhythm(source.rhythm).logicalLength();
            value.number = value.rational.toDouble();
            return value;
        }
        if (name == "commonPeriod") {
            const Value left = lowerExpr(expr.args[0], {});
            const Value right = lowerExpr(expr.args[1], {});
            const std::optional<Rational> common = rhythmCommonPeriod(
                project_.music().evaluateRhythm(left.rhythm),
                project_.music().evaluateRhythm(right.rhythm));
            if (!common.has_value()) {
                error(expr.span, "rhythm-period-overflow",
                      "common period exceeds the musical rational limit");
                return Value{};
            }
            Value value;
            value.kind = TypeKind::Duration;
            value.rational = *common;
            value.number = common->toDouble();
            return value;
        }
        if (name == "metricAccent") {
            const Value source = lowerExpr(expr.args[0], {});
            const MetricScope& scope = metricContext();
            if (scope.map == nullptr) {
                error(expr.span, "context-not-declared",
                      "metricAccent requires a metric context");
                return Value{};
            }
            return rhythmValue(project_.music().rhythmAccentByMeter(
                source.rhythm, *scope.map, scope.anchor, label));
        }
        if (name == "metricWeight") {
            const Value offset = lowerExpr(expr.args[0], {});
            const MetricScope& scope = metricContext();
            const Position absolute(scope.anchor.inWholeNotes() + offset.rational);
            Value value;
            value.kind = TypeKind::Integer;
            value.integer = absolute.inWholeNotes().isNegative() || scope.map == nullptr
                                ? 0
                                : scope.map->metricWeight(absolute).level;
            value.number = static_cast<double>(value.integer);
            value.rational = Rational(value.integer, 1);
            return value;
        }
        if (name == "zip" || name == "cycle") {
            const Value rhythm = lowerExpr(expr.args[0], {});
            const Value pitches = lowerExpr(expr.args[1], {});
            const std::int64_t velocity = integerOfValue(lowerExpr(expr.args[2], {}));
            std::vector<Pitch> sequence;
            for (const Value& pitch : pitches.items) {
                try {
                    sequence.push_back(Pitch::parse(pitch.text));
                } catch (const Error& pitchError) {
                    diagnostics_.error("type-pitch", expr.span, pitchError.what());
                    return Value{};
                }
            }
            if (sequence.empty()) {
                error(expr.span, "rhythm-pitch", name + " requires at least one pitch");
                return Value{};
            }
            const Rhythm& rhythmValue = project_.music().evaluateRhythm(rhythm.rhythm);
            if (name == "zip" && rhythmValue.attackCount() != sequence.size()) {
                error(expr.span, "rhythm-pitch-count",
                      "strict rhythm-pitch combination has " +
                          std::to_string(rhythmValue.attackCount()) + " attacks but " +
                          std::to_string(sequence.size()) + " pitches");
                return Value{};
            }
            for (const RhythmEvent& event : rhythmValue.events()) {
                if (!event.rest && event.onset.isNegative()) {
                    error(expr.span, "rhythm-pitch",
                          "rhythm onset " + event.onset.toString() +
                              " is negative and cannot be placed on the fragment timeline");
                    return Value{};
                }
            }
            Value value;
            value.kind = TypeKind::Fragment;
            try {
                value.fragment = project_.music().rhythmCombine(
                    rhythm.rhythm, sequence,
                    static_cast<int>(std::clamp<std::int64_t>(velocity, 1, 127)),
                    name == "cycle", label);
            } catch (const Error& combination) {
                diagnostics_.error(
                    combination.code().empty() ? "rhythm-pitch" : combination.code(),
                    expr.span, combination.what());
                return Value{};
            }
            return value;
        }
        if (name == "intersect" || name == "union" || name == "difference") {
            const Value left = lowerExpr(expr.args[0], {});
            const Value right = lowerExpr(expr.args[1], {});
            std::optional<Rational> grid;
            if (expr.args.size() == 3) {
                grid = rationalOfValue(lowerExpr(expr.args[2], {}));
            }
            if (name == "intersect") {
                return rhythmValue(
                    project_.music().rhythmIntersect(left.rhythm, right.rhythm, grid, label));
            }
            if (name == "union") {
                return rhythmValue(
                    project_.music().rhythmUnion(left.rhythm, right.rhythm, grid, label));
            }
            return rhythmValue(
                project_.music().rhythmDifference(left.rhythm, right.rhythm, grid, label));
        }
        if (name == "complement") {
            const Value source = lowerExpr(expr.args[0], {});
            const Rational span = rationalOfValue(lowerExpr(expr.args[1], {}));
            const Rational grid = rationalOfValue(lowerExpr(expr.args[2], {}));
            return rhythmValue(
                project_.music().rhythmComplement(source.rhythm, span, grid, label));
        }
        if (name == "accentFollow") {
            const Value source = lowerExpr(expr.args[0], {});
            const Value target = lowerExpr(expr.args[1], {});
            std::optional<Rational> grid;
            if (expr.args.size() == 3) {
                grid = rationalOfValue(lowerExpr(expr.args[2], {}));
            }
            return rhythmValue(project_.music().rhythmAccentFollow(
                source.rhythm, target.rhythm, grid, label));
        }
        return std::nullopt;
    }

    Value lowerHarmonyLiteral(const Expr& expr, const std::string& label) {
        std::vector<HarmonyEvent> events;
        Rational contentEnd(0, 1);
        for (const HarmonyEntry& entry : expr.harmony) {
            HarmonyEvent event;
            try {
                event.chord = ChordSymbol::parse(entry.chord);
            } catch (const Error& chordError) {
                diagnostics_.error(
                    chordError.code().empty() ? "type-chord" : chordError.code(),
                    entry.span, chordError.what());
                continue;
            }
            const Value offset = lowerExpr(entry.offset, label);
            const Value duration = lowerExpr(entry.duration, label);
            if ((offset.kind != TypeKind::Position && offset.kind != TypeKind::Duration) ||
                duration.kind != TypeKind::Duration || !duration.rational.isPositive() ||
                offset.rational.isNegative()) {
                error(entry.span, "type-harmony",
                      "harmony entries need a non-negative position and a positive duration");
                continue;
            }
            event.start = offset.rational;
            event.duration = duration.rational;
            contentEnd = std::max(contentEnd, event.start + event.duration);
            events.push_back(std::move(event));
        }
        Rational logicalLength = contentEnd;
        if (expr.explicitSpan != kInvalidExprId) {
            const Value span = lowerExpr(expr.explicitSpan, label);
            if (span.kind == TypeKind::Duration && span.rational.isPositive()) {
                if (span.rational < contentEnd) {
                    error(expr.span, "type-duration",
                          "harmony span " + span.rational.toString() +
                              " is shorter than its content " + contentEnd.toString());
                } else {
                    logicalLength = span.rational;
                }
            }
        }
        Value value;
        if (!logicalLength.isPositive()) {
            error(expr.span, "type-harmony-empty",
                  "harmony literal must have a positive logical length");
            return value;
        }
        try {
            value.kind = TypeKind::Harmony;
            value.harmony = project_.music().harmonyLiteral(
                HarmonyTimeline::fromEvents(std::move(events), logicalLength), label);
        } catch (const Error& harmonyError) {
            diagnostics_.error(
                harmonyError.code().empty() ? "harmony" : harmonyError.code(), expr.span,
                harmonyError.what());
            return Value{};
        }
        return value;
    }

    std::optional<Value> lowerHarmonyCall(const Expr& expr, const std::string& label) {
        const std::string& name = expr.text;
        const auto valueOf = [&](std::size_t index) { return lowerExpr(expr.args[index], {}); };
        const auto pitchOf = [&](const Value& value) {
            return Pitch::parse(value.text);
        };
        const auto chordValue = [&](const ChordSymbol& chord, ObjectId node) {
            Value value;
            value.kind = TypeKind::Chord;
            value.chordStorage = std::make_shared<ChordSymbol>(chord);
            value.chordNode = node;
            return value;
        };
        const auto voicingValue = [&](const Voicing& voicing) {
            Value value;
            value.kind = TypeKind::Voicing;
            value.voicingStorage = std::make_shared<Voicing>(voicing);
            return value;
        };
        const auto pitchList = [&](const std::vector<Pitch>& pitches) {
            Value value;
            value.kind = TypeKind::List;
            for (const Pitch& pitch : pitches) {
                Value item;
                item.kind = TypeKind::Pitch;
                item.text = pitch.name();
                value.items.push_back(std::move(item));
            }
            return value;
        };
        const auto integerResult = [](std::int64_t number) {
            Value value;
            value.kind = TypeKind::Integer;
            value.integer = number;
            value.number = static_cast<double>(number);
            value.rational = Rational(number, 1);
            return value;
        };
        const auto parsePitchList = [&](const Value& list, std::vector<Pitch>& pitches) {
            for (const Value& item : list.items) {
                try {
                    pitches.push_back(Pitch::parse(item.text));
                } catch (const Error& pitchError) {
                    diagnostics_.error("type-pitch", expr.span, pitchError.what());
                    return false;
                }
            }
            return true;
        };

        if (name == "chord") {
            const Value text = valueOf(0);
            try {
                return chordValue(ChordSymbol::parse(text.text), kInvalidObjectId);
            } catch (const Error& chordError) {
                diagnostics_.error(
                    chordError.code().empty() ? "chord" : chordError.code(), expr.span,
                    chordError.what());
                return Value{};
            }
        }
        if (name == "chordAt") {
            const Value harmony = valueOf(0);
            const Value at = valueOf(1);
            const ObjectId query = project_.music().harmonyQuery(
                harmony.harmony, Position(at.rational), label);
            try {
                return chordValue(project_.music().evaluateChord(query), query);
            } catch (const Error& queryError) {
                diagnostics_.error(
                    queryError.code().empty() ? "harmony" : queryError.code(), expr.span,
                    queryError.what());
                return Value{};
            }
        }
        if (name == "chordAtBar") {
            const Value harmony = valueOf(0);
            const std::int64_t bar = integerOfValue(valueOf(1));
            const MetricScope& scope = metricContext();
            if (scope.map == nullptr) {
                error(expr.span, "context-not-declared", "chordAtBar requires a metric context");
                return Value{};
            }
            MetricDuration metric;
            metric.amount = Rational(bar, 1);
            metric.unit = MetricUnit::Bars;
            std::string message;
            const std::optional<Rational> offset =
                scope.map->resolveDuration(scope.anchor, metric, message);
            if (!offset.has_value()) {
                error(expr.span, "meter-metric-duration", message);
                return Value{};
            }
            const Position at(scope.anchor.inWholeNotes() + *offset);
            const ObjectId query = project_.music().harmonyQuery(harmony.harmony, at, label);
            try {
                return chordValue(project_.music().evaluateChord(query), query);
            } catch (const Error& queryError) {
                diagnostics_.error(
                    queryError.code().empty() ? "harmony" : queryError.code(), expr.span,
                    queryError.what());
                return Value{};
            }
        }
        if (name == "harmonyLength") {
            const Value harmony = valueOf(0);
            Value value;
            value.kind = TypeKind::Duration;
            value.rational =
                project_.music().evaluateHarmony(harmony.harmony).logicalLength();
            value.number = value.rational.toDouble();
            return value;
        }
        if (name == "harmonyOverlaps") {
            const Value harmony = valueOf(0);
            const Value at = valueOf(1);
            const HarmonyTimeline& timeline =
                project_.music().evaluateHarmony(harmony.harmony);
            return integerResult(static_cast<std::int64_t>(
                timeline.eventsAt(Position(at.rational)).size()));
        }
        if (name == "chordName") {
            const Value chord = valueOf(0);
            Value value;
            value.kind = TypeKind::String;
            value.text = chord.chordRef().name();
            return value;
        }
        if (name == "chordPitchClasses") {
            const Value chord = valueOf(0);
            Value value;
            value.kind = TypeKind::List;
            for (const int pitchClass : chord.chordRef().pitchClassSet().values) {
                Value item;
                item.kind = TypeKind::Integer;
                item.integer = pitchClass;
                item.number = static_cast<double>(pitchClass);
                item.rational = Rational(pitchClass, 1);
                value.items.push_back(std::move(item));
            }
            return value;
        }
        if (name == "chordContains" || name == "chordTone") {
            const Value chord = valueOf(0);
            const Value pitch = valueOf(1);
            Value value;
            value.kind = TypeKind::Boolean;
            try {
                value.boolean = chord.chordRef().contains(pitchOf(pitch).midi() % 12);
            } catch (const Error& pitchError) {
                diagnostics_.error("type-pitch", expr.span, pitchError.what());
                return Value{};
            }
            return value;
        }
        if (name == "voicing") {
            const Value chord = valueOf(0);
            const Value low = valueOf(1);
            const Value high = valueOf(2);
            const std::int64_t voices = integerOfValue(valueOf(3));
            const std::int64_t inversion = integerOfValue(valueOf(4));
            const std::int64_t spacing = integerOfValue(valueOf(5));
            try {
                return voicingValue(buildVoicing(
                    chord.chordRef(), pitchOf(low), pitchOf(high), static_cast<int>(voices),
                    static_cast<int>(inversion), static_cast<int>(spacing)));
            } catch (const Error& voicingError) {
                diagnostics_.error(
                    voicingError.code().empty() ? "voicing" : voicingError.code(), expr.span,
                    voicingError.what());
                return Value{};
            }
        }
        if (name == "voiceLead") {
            const Value previousValue = valueOf(0);
            const Value chord = valueOf(1);
            const Value low = valueOf(2);
            const Value high = valueOf(3);
            const std::int64_t maxMove = integerOfValue(valueOf(4));
            const std::int64_t spacing = integerOfValue(valueOf(5));
            const bool allowCrossing = valueOf(6).boolean;
            std::vector<Pitch> previous;
            if (!parsePitchList(previousValue, previous)) {
                return Value{};
            }
            try {
                return voicingValue(voiceLead(previous, chord.chordRef(), pitchOf(low),
                                              pitchOf(high), static_cast<int>(maxMove),
                                              static_cast<int>(spacing), allowCrossing));
            } catch (const Error& leadError) {
                diagnostics_.error(
                    leadError.code().empty() ? "voice-leading" : leadError.code(), expr.span,
                    leadError.what());
                return Value{};
            }
        }
        if (name == "voicingNotes") {
            return pitchList(valueOf(0).voicingRef().notes);
        }
        if (name == "voicingStatus") {
            Value value;
            value.kind = TypeKind::String;
            value.text = std::string(voicingStatusName(valueOf(0).voicingRef().status));
            return value;
        }
        if (name == "voicingMovement") {
            return integerResult(valueOf(0).voicingRef().totalMovement);
        }
        if (name == "voicingCrossings") {
            return integerResult(valueOf(0).voicingRef().crossings);
        }
        if (name == "voicingComplete") {
            Value value;
            value.kind = TypeKind::Boolean;
            value.boolean = valueOf(0).voicingRef().complete;
            return value;
        }
        if (name == "voicingSearchComplete") {
            Value value;
            value.kind = TypeKind::Boolean;
            value.boolean = valueOf(0).voicingRef().searchComplete;
            return value;
        }
        if (name == "voiceDistance") {
            std::vector<Pitch> first;
            std::vector<Pitch> second;
            if (!parsePitchList(valueOf(0), first) || !parsePitchList(valueOf(1), second)) {
                return Value{};
            }
            return integerResult(totalVoiceMovement(first, second));
        }
        if (name == "commonTones") {
            std::vector<Pitch> first;
            std::vector<Pitch> second;
            if (!parsePitchList(valueOf(0), first) || !parsePitchList(valueOf(1), second)) {
                return Value{};
            }
            return pitchList(commonTones(first, second));
        }
        if (name == "interval") {
            const Value first = valueOf(0);
            const Value second = valueOf(1);
            try {
                return integerResult(pitchOf(second).midi() - pitchOf(first).midi());
            } catch (const Error& pitchError) {
                diagnostics_.error("type-pitch", expr.span, pitchError.what());
                return Value{};
            }
        }
        if (name == "pitchClass") {
            const Value pitch = valueOf(0);
            try {
                return integerResult(pitchOf(pitch).midi() % 12);
            } catch (const Error& pitchError) {
                diagnostics_.error("type-pitch", expr.span, pitchError.what());
                return Value{};
            }
        }
        if (name == "pitchRange") {
            std::vector<Pitch> pitches;
            if (!parsePitchList(valueOf(0), pitches)) {
                return Value{};
            }
            if (pitches.empty()) {
                return integerResult(0);
            }
            int low = pitches.front().midi();
            int high = pitches.front().midi();
            for (const Pitch& pitch : pitches) {
                low = std::min(low, pitch.midi());
                high = std::max(high, pitch.midi());
            }
            return integerResult(high - low);
        }
        if (name == "noteOverlap") {
            const Value first = valueOf(0);
            const Value second = valueOf(1);
            try {
                return integerResult(noteIntervalOverlap(
                    project_.music().evaluate(first.fragment),
                    project_.music().evaluate(second.fragment)));
            } catch (const Error& evaluation) {
                diagnostics_.error(
                    evaluation.code().empty() ? "analysis" : evaluation.code(), expr.span,
                    evaluation.what());
                return Value{};
            }
        }
        if (name == "fragmentRange") {
            const Value fragment = valueOf(0);
            try {
                return integerResult(
                    soundingRange(project_.music().evaluate(fragment.fragment)));
            } catch (const Error& evaluation) {
                diagnostics_.error(
                    evaluation.code().empty() ? "analysis" : evaluation.code(), expr.span,
                    evaluation.what());
                return Value{};
            }
        }
        if (name == "parallelPerfects") {
            std::vector<Pitch> lower;
            std::vector<Pitch> upper;
            if (!parsePitchList(valueOf(0), lower) || !parsePitchList(valueOf(1), upper)) {
                return Value{};
            }
            return integerResult(parallelPerfects(lower, upper));
        }
        return std::nullopt;
    }

    Value lowerExpr(ExprId id, const std::string& label) {
        if (id == kInvalidExprId || id >= module_.expressions.size()) {
            return Value{};
        }
        const Expr& expr = module_.expr(id);
        switch (expr.kind) {
            case ExprKind::Integer:
            case ExprKind::Rational:
            case ExprKind::Number: {
                const Constant constant = hir_.constantOf(id);
                Value value;
                const TypeKind hinted = hir_.typeOf(id).kind;
                value.kind = hinted == TypeKind::Error ? TypeKind::Number : hinted;
                value.rational = resolveMetricConstant(constant, expr.span);
                value.number = constant.kind == Constant::Kind::Metric
                                   ? value.rational.toDouble()
                                   : constant.number;
                value.integer = constant.integer;
                return value;
            }
            case ExprKind::Boolean: {
                Value value;
                value.kind = TypeKind::Boolean;
                value.boolean = expr.boolean;
                return value;
            }
            case ExprKind::String: {
                Value value;
                value.kind = TypeKind::String;
                value.text = expr.text;
                return value;
            }
            case ExprKind::Identifier: {
                if (const std::optional<Value> local = lookupLocalValue(expr.text);
                    local.has_value()) {
                    return *local;
                }
                const SymbolId symbolId = hir_.symbolOf(id);
                if (symbolId != kInvalidSymbolId) {
                    ensureGlobal(symbolId);
                    Value value = symbolValues_[symbolId];
                    if (value.kind == TypeKind::Error) {
                        value.kind = hir_.symbolTypes[symbolId].kind;
                        value.text = hir_.symbols.get(symbolId).name;
                    }
                    return value;
                }
                if (hir_.typeOf(id).kind == TypeKind::Pitch) {
                    Value value;
                    value.kind = TypeKind::Pitch;
                    value.text = expr.text;
                    return value;
                }
                return Value{};
            }
            case ExprKind::Melody: {
                std::vector<std::pair<Pitch, Rational>> notes;
                for (const MelodyEntry& entry : expr.melody) {
                    try {
                        const Value duration = lowerExpr(entry.duration, label);
                        notes.emplace_back(Pitch::parse(entry.pitch),
                                           rationalOfValue(duration));
                    } catch (const Error& error) {
                        diagnostics_.error("type-pitch", entry.span, error.what());
                    }
                }
                Value value;
                value.kind = TypeKind::Fragment;
                value.fragment = project_.music().melody(notes, label);
                return value;
            }
            case ExprKind::Notes: {
                std::vector<Note> notes;
                for (const NoteEntry& entry : expr.notes) {
                    Note note;
                    try {
                        note.pitch = Pitch::parse(entry.pitch);
                    } catch (const Error& error) {
                        diagnostics_.error("type-pitch", entry.span, error.what());
                    }
                    note.offset = Duration(rationalOfValue(lowerExpr(entry.offset, label)));
                    note.duration = Duration(rationalOfValue(lowerExpr(entry.duration, label)));
                    note.velocity = entry.velocity;
                    notes.push_back(note);
                }
                std::optional<Duration> span;
                if (expr.explicitSpan != kInvalidExprId) {
                    span = Duration(rationalOfValue(lowerExpr(expr.explicitSpan, label)));
                }
                Value value;
                value.kind = TypeKind::Fragment;
                value.fragment = project_.music().notes(std::move(notes), span, label);
                return value;
            }
            case ExprKind::List: {
                Value value;
                value.kind = TypeKind::List;
                for (const ExprId item : expr.args) {
                    value.items.push_back(lowerExpr(item, label));
                }
                return value;
            }
            case ExprKind::Rhythm:
                return lowerRhythmLiteral(expr, label);
            case ExprKind::Harmony:
                return lowerHarmonyLiteral(expr, label);
            case ExprKind::UnaryMinus: {
                Value value = lowerExpr(expr.args.front(), label);
                value.rational = -value.rational;
                value.number = -value.number;
                value.integer = -value.integer;
                return value;
            }
            case ExprKind::Not: {
                Value value;
                value.kind = TypeKind::Boolean;
                value.boolean = !lowerExpr(expr.args.front(), label).boolean;
                return value;
            }
            case ExprKind::Binary:
                return lowerBinary(expr, id, label);
            case ExprKind::If: {
                const Value condition = lowerExpr(expr.args[0], label);
                return lowerExpr(condition.boolean ? expr.args[1] : expr.args[2], label);
            }
            case ExprKind::For: {
                const Value iterable = lowerExpr(expr.args[0], label);
                if (iterable.kind != TypeKind::List) {
                    error(expr.span, "lower-collection", "for iteration requires a collection");
                    return Value{};
                }
                if (iterable.items.size() > kMaxCollectionSize) {
                    error(expr.span, "limit-collection-size",
                          "collection exceeds " + std::to_string(kMaxCollectionSize));
                    return Value{};
                }
                Value result;
                result.kind = TypeKind::List;
                localScopes_.emplace_back();
                for (const Value& item : iterable.items) {
                    if (iterationCount_ >= kMaxLoopIterations) {
                        error(expr.span, "limit-loop-iterations",
                              "loop iteration budget exceeded");
                        break;
                    }
                    ++iterationCount_;
                    localScopes_.back()[expr.text] = item;
                    result.items.push_back(lowerExpr(expr.args[1], label));
                }
                localScopes_.pop_back();
                return result;
            }
            case ExprKind::Call:
                return lowerCall(expr, label);
            default:
                return Value{};
        }
    }

    Value lowerCall(const Expr& expr, const std::string& label) {
        const std::string& name = expr.text;
        if (const FunctionInfo* function = hir_.findFunction(name)) {
            return evaluateUserFunction(*function, expr);
        }
        const bool rhythmCandidate =
            isRhythmOperatorName(name) ||
            (isOverloadedRhythmName(name) && !expr.args.empty() &&
             hir_.typeOf(expr.args[0]).kind == TypeKind::Rhythm);
        if (rhythmCandidate) {
            if (std::optional<Value> rhythm = lowerRhythmCall(expr, label);
                rhythm.has_value()) {
                return std::move(*rhythm);
            }
        }
        if (isHarmonyOperatorName(name)) {
            if (std::optional<Value> harmony = lowerHarmonyCall(expr, label);
                harmony.has_value()) {
                return std::move(*harmony);
            }
        }

        Value result;
        if (name == "transpose") {
            const Value source = lowerExpr(expr.args[0], {});
            const int semitones = static_cast<int>(integerOfValue(lowerExpr(expr.args[1], {})));
            result.kind = TypeKind::Fragment;
            result.fragment = project_.music().transpose(
                source.fragment, Interval::fromSemitones(semitones), label);
            return result;
        }
        if (name == "reverse") {
            const Value source = lowerExpr(expr.args[0], {});
            if (hir_.typeOf(expr.args[0]).kind == TypeKind::Curve) {
                result.kind = TypeKind::Curve;
                result.curve = project_.temporal().reverse(source.curve, label);
            } else {
                result.kind = TypeKind::Fragment;
                result.fragment = project_.music().reverse(source.fragment, label);
            }
            return result;
        }
        if (name == "stretch") {
            const Value source = lowerExpr(expr.args[0], {});
            const Rational factor = rationalOfValue(lowerExpr(expr.args[1], {}));
            if (hir_.typeOf(expr.args[0]).kind == TypeKind::Curve) {
                result.kind = TypeKind::Curve;
                result.curve = project_.temporal().stretch(source.curve, factor, label);
            } else {
                result.kind = TypeKind::Fragment;
                result.fragment = project_.music().stretch(source.fragment, factor, label);
            }
            return result;
        }
        if (name == "repeat") {
            const Value source = lowerExpr(expr.args[0], {});
            const int count = static_cast<int>(integerOfValue(lowerExpr(expr.args[1], {})));
            if (hir_.typeOf(expr.args[0]).kind == TypeKind::Curve) {
                result.kind = TypeKind::Curve;
                result.curve = project_.temporal().repeat(source.curve, count, label);
            } else {
                result.kind = TypeKind::Fragment;
                result.fragment = project_.music().repeat(source.fragment, count, label);
            }
            return result;
        }
        if (name == "sequence" || name == "parallel") {
            std::vector<ObjectId> parts;
            for (const ExprId argument : expr.args) {
                const Value part = lowerExpr(argument, {});
                if (part.fragment == kInvalidObjectId) {
                    return Value{};
                }
                parts.push_back(part.fragment);
            }
            result.kind = TypeKind::Fragment;
            result.fragment = name == "sequence"
                                  ? project_.music().sequence(parts, label)
                                  : project_.music().parallel(parts, label);
            return result;
        }
        if (name == "diatonicTranspose") {
            const Value source = lowerExpr(expr.args[0], {});
            const int steps = static_cast<int>(integerOfValue(lowerExpr(expr.args[1], {})));
            result.kind = TypeKind::Fragment;
            result.fragment = project_.music().diatonicTranspose(source.fragment, steps, label);
            return result;
        }
        if (name == "harmonize") {
            const Value source = lowerExpr(expr.args[0], {});
            const Value degrees = lowerExpr(expr.args[1], {});
            std::vector<int> offsets;
            for (const Value& degree : degrees.items) {
                offsets.push_back(static_cast<int>(integerOfValue(degree)));
            }
            result.kind = TypeKind::Fragment;
            result.fragment = project_.music().harmonize(source.fragment, offsets, label);
            return result;
        }
        if (name == "syncopate") {
            const Value source = lowerExpr(expr.args[0], {});
            const Rational displacement = rationalOfValue(lowerExpr(expr.args[1], {}));
            const MetricScope& scope = metricContext();
            result.kind = TypeKind::Fragment;
            if (scope.map != nullptr) {
                result.fragment = project_.music().syncopate(source.fragment, displacement,
                                                             *scope.map, scope.anchor, label);
            } else {
                result.fragment = project_.music().syncopate(source.fragment, displacement,
                                                             label);
            }
            return result;
        }
        if (name == "shift") {
            const Value source = lowerExpr(expr.args[0], {});
            const Rational amount = rationalOfValue(lowerExpr(expr.args[1], {}));
            result.kind = TypeKind::Curve;
            result.curve = project_.temporal().shift(source.curve, amount, label);
            return result;
        }
        if (name == "rotate") {
            const Value source = lowerExpr(expr.args[0], {});
            const Rational amount = rationalOfValue(lowerExpr(expr.args[1], {}));
            result.kind = TypeKind::Fragment;
            result.fragment = project_.music().rotate(source.fragment, amount, label);
            return result;
        }
        if (name == "range") {
            const std::int64_t start = integerOfValue(lowerExpr(expr.args[0], {}));
            const std::int64_t count = integerOfValue(lowerExpr(expr.args[1], {}));
            if (count < 0 || count > kMaxCollectionSize) {
                error(expr.span, "limit-collection-size",
                      "range count must be in [0, " +
                          std::to_string(kMaxCollectionSize) + "]");
                return Value{};
            }
            result.kind = TypeKind::List;
            for (std::int64_t index = 0; index < count; ++index) {
                Value item;
                item.kind = TypeKind::Integer;
                item.integer = start + index;
                item.number = static_cast<double>(item.integer);
                item.rational = Rational(item.integer, 1);
                result.items.push_back(item);
            }
            return result;
        }
        if (name == "length") {
            const Value collection = lowerExpr(expr.args[0], {});
            result.kind = TypeKind::Integer;
            result.integer = static_cast<std::int64_t>(collection.items.size());
            result.number = static_cast<double>(result.integer);
            result.rational = Rational(result.integer, 1);
            return result;
        }
        if (name == "round") {
            const Value value = lowerExpr(expr.args[0], {});
            const double number =
                value.kind == TypeKind::Integer ? static_cast<double>(value.integer)
                                                : value.number;
            result.kind = TypeKind::Integer;
            result.integer = static_cast<std::int64_t>(std::llround(number));
            result.number = static_cast<double>(result.integer);
            result.rational = Rational(result.integer, 1);
            return result;
        }
        if (name == "note") {
            const Value pitch = lowerExpr(expr.args[0], {});
            const Rational duration =
                rationalOfValue(lowerExpr(expr.args[1], {}));
            const std::int64_t velocity =
                integerOfValue(lowerExpr(expr.args[2], {}));
            Note note;
            try {
                note.pitch = Pitch::parse(pitch.text);
            } catch (const Error& pitchError) {
                diagnostics_.error("type-pitch", expr.span, pitchError.what());
                return Value{};
            }
            note.offset = Duration(Rational(0, 1));
            note.duration = Duration(duration);
            note.velocity = static_cast<int>(std::clamp<std::int64_t>(velocity, 1, 127));
            result.kind = TypeKind::Fragment;
            result.fragment = project_.music().notes({note}, std::nullopt, label);
            return result;
        }
        if (name == "rest") {
            const Rational duration =
                rationalOfValue(lowerExpr(expr.args[0], {}));
            result.kind = TypeKind::Fragment;
            result.fragment =
                project_.music().notes({}, Duration(duration), label);
            return result;
        }
        if (name == "sequenceAll" || name == "parallelAll") {
            const Value collection = lowerExpr(expr.args[0], {});
            std::vector<ObjectId> parts;
            for (const Value& item : collection.items) {
                parts.push_back(item.fragment);
            }
            if (parts.empty()) {
                error(expr.span, "lower-empty-collection",
                      name + " requires at least one fragment");
                return Value{};
            }
            result.kind = TypeKind::Fragment;
            result.fragment = name == "sequenceAll"
                                  ? project_.music().sequence(parts, label)
                                  : project_.music().parallel(parts, label);
            return result;
        }
        if (name == "ratio") {
            const std::int64_t numerator =
                integerOfValue(lowerExpr(expr.args[0], {}));
            const std::int64_t denominator =
                integerOfValue(lowerExpr(expr.args[1], {}));
            if (denominator == 0) {
                error(expr.span, "sema-division-by-zero", "ratio denominator must not be zero");
                return Value{};
            }
            result.kind = TypeKind::Duration;
            result.rational = Rational(numerator, denominator);
            result.number = result.rational.toDouble();
            return result;
        }
        if (name == "random") {
            const std::int64_t seed = integerOfValue(lowerExpr(expr.args[0], {}));
            const std::int64_t index = integerOfValue(lowerExpr(expr.args[1], {}));
            std::uint64_t state = static_cast<std::uint64_t>(seed) ^
                                  (static_cast<std::uint64_t>(index) *
                                   0x9e3779b97f4a7c15ull);
            state += 0x9e3779b97f4a7c15ull;
            state = (state ^ (state >> 30)) * 0xbf58476d1ce4e5b9ull;
            state = (state ^ (state >> 27)) * 0x94d049bb133111ebull;
            state ^= state >> 31;
            result.kind = TypeKind::Number;
            result.number =
                static_cast<double>(state >> 11) * (1.0 / 9007199254740992.0);
            return result;
        }
        diagnostics_.error("type-unknown-operator", expr.span,
                           "unknown operator '" + name + "'");
        return Value{};
    }

    Rational rationalOf(ExprId id) {
        const Constant constant = hir_.constantOf(id);
        if (constant.kind == Constant::Kind::Integer) {
            return Rational(constant.integer, 1);
        }
        return resolveMetricConstant(constant, module_.expr(id).span);
    }

    double numberOf(ExprId id) const { return hir_.constantOf(id).number; }

    std::int64_t integerOf(ExprId id) const {
        const Constant constant = hir_.constantOf(id);
        if (constant.kind == Constant::Kind::Integer) {
            return constant.integer;
        }
        return static_cast<std::int64_t>(constant.number);
    }

    const Module& module_;
    const HirModule& hir_;
    DiagnosticSink& diagnostics_;
    Project project_;
    MeterMap baseMeterMap_;
    std::vector<MetricScope> metricScopes_;
    std::vector<Value> symbolValues_;
    std::vector<std::size_t> trackIndices_;
    std::vector<LoweredSection> loweredSections_;
    std::set<InstanceId> instanceIds_;
    std::set<SymbolId> globalResolving_;
    std::set<SymbolId> globalResolved_;
    std::vector<std::map<std::string, Value>> localScopes_;
    std::map<std::string, Value> functionCache_;
    int callDepth_ = 0;
    int callCount_ = 0;
    int iterationCount_ = 0;
    int callInstanceCounter_ = 0;
    mutable SymbolId lastSymbolId_ = kInvalidSymbolId;
};

}  // namespace

LoweringResult lowerToProject(const Module& module, const HirModule& hir,
                              DiagnosticSink& diagnostics) {
    Lowering lowering(module, hir, diagnostics);
    LoweringResult result = lowering.run();
    for (const ExportDecl& exportDecl : module.exports) {
        result.exports.push_back(ExportRequest{exportDecl.format, exportDecl.path});
    }
    return result;
}

}  // namespace mpl
