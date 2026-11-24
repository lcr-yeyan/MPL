#include "mpl/frontend/Lowering.hpp"

#include "mpl/core/Error.hpp"
#include "mpl/frontend/ParameterRegistry.hpp"
#include "mpl/music/Pitch.hpp"

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace mpl {
namespace {

struct Value {
    TypeKind kind = TypeKind::Error;
    ObjectId fragment = kInvalidObjectId;
    TemporalId curve = kInvalidTemporalId;
    Rational rational{0, 1};
    double number = 0.0;
    std::int64_t integer = 0;
    std::string text;
    std::vector<int> integers;
};

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
    std::vector<LoweredClip> clips;
    SourceSpan span;
};

class Lowering {
public:
    Lowering(const Module& module, const HirModule& hir, DiagnosticSink& diagnostics)
        : module_(module), hir_(hir), diagnostics_(diagnostics) {}

    LoweringResult run() {
        project_.setTempo(hir_.tempo);
        project_.setMeter(hir_.meter.numerator, hir_.meter.denominator);
        project_.setKey(hir_.key);

        symbolValues_.assign(hir_.symbols.size(), Value{});
        loweredSections_.resize(module_.sections.size());

        addTracks();
        buildMix();
        lowerDeclarations();
        lowerArrangement();

        LoweringResult result;
        result.project = std::move(project_);
        return result;
    }

private:
    void error(const SourceSpan& span, std::string code, std::string message,
               std::string related = {}) {
        diagnostics_.error(std::move(code), span, std::move(message), {}, {}, std::move(related));
    }

    const Symbol* symbolFor(const std::string& name) const {
        const std::optional<SymbolId> id = hir_.symbols.find(name);
        if (!id.has_value()) {
            return nullptr;
        }
        lastSymbolId_ = *id;
        return &hir_.symbols.get(*id);
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
                case DeclKind::Let:
                    lowerLet(module_.lets[reference.index]);
                    break;
                case DeclKind::Curve:
                    lowerCurve(module_.curves[reference.index]);
                    break;
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

    void lowerSection(const SectionDecl& decl, std::size_t sectionIndex) {
        LoweredSection section;
        section.name = decl.name;
        section.span = decl.span;
        section.duration = rationalOf(decl.duration);

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
            Value part = lowerExpr(clip.play, clipName);
            if (part.fragment == kInvalidObjectId) {
                continue;
            }
            LoweredClip lowered;
            lowered.trackIndex = trackIndices_[track.declarationIndex];
            lowered.name = clipName;
            lowered.part = part.fragment;
            lowered.span = clip.span;
            lowered.offset = clip.at == kInvalidExprId ? Rational(0, 1) : rationalOf(clip.at);
            lowered.expression.velocity = lowerLane(clip.velocity);
            lowered.expression.timing = lowerLane(clip.timing);
            lowered.expression.gate = lowerLane(clip.gate);
            lowered.expression.pan = lowerLane(clip.pan);

            const Duration contentDuration =
                project_.music().evaluate(part.fragment).duration();
            if (lowered.offset + contentDuration.inWholeNotes() > section.duration) {
                diagnostics_.warning(
                    "sema-section-overflow", clip.span,
                    "clip content (" + contentDuration.toString() +
                        " at offset " + lowered.offset.toString() +
                        ") is longer than section '" + decl.name + "' (" +
                        section.duration.toString() + "); the clip will overlap later sections");
            }
            section.clips.push_back(std::move(lowered));
        }
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
            const int occurrence = occurrences[section.name]++;
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

        for (std::size_t trackIndex = 0; trackIndex < project_.tracks().size(); ++trackIndex) {
            if (project_.tracks()[trackIndex].clips.empty()) {
                const std::string& name = project_.tracks()[trackIndex].name;
                error(module_.tracks[trackIndex].span, "sema-empty-track",
                      "track '" + name + "' has no clips in the arrangement", name);
            }
        }
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
                value.kind = TypeKind::Number;
                value.rational = constant.rational;
                value.number = constant.number;
                value.integer = constant.integer;
                return value;
            }
            case ExprKind::String: {
                Value value;
                value.kind = TypeKind::String;
                value.text = expr.text;
                return value;
            }
            case ExprKind::Identifier: {
                const SymbolId symbolId = hir_.symbolOf(id);
                if (symbolId == kInvalidSymbolId) {
                    return Value{};
                }
                Value value = symbolValues_[symbolId];
                if (value.kind == TypeKind::Error) {
                    value.kind = hir_.symbolTypes[symbolId].kind;
                    value.text = hir_.symbols.get(symbolId).name;
                }
                return value;
            }
            case ExprKind::Melody: {
                std::vector<std::pair<Pitch, Rational>> notes;
                for (const MelodyEntry& entry : expr.melody) {
                    try {
                        notes.emplace_back(Pitch::parse(entry.pitch), rationalOf(entry.duration));
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
                    note.offset = Duration(rationalOf(entry.offset));
                    note.duration = Duration(rationalOf(entry.duration));
                    note.velocity = entry.velocity;
                    notes.push_back(note);
                }
                std::optional<Duration> span;
                if (expr.explicitSpan != kInvalidExprId) {
                    span = Duration(rationalOf(expr.explicitSpan));
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
                    value.integers.push_back(static_cast<int>(integerOf(item)));
                }
                return value;
            }
            case ExprKind::UnaryMinus: {
                Value value = lowerExpr(expr.args.front(), label);
                value.rational = -value.rational;
                value.number = -value.number;
                value.integer = -value.integer;
                return value;
            }
            case ExprKind::Call:
                return lowerCall(expr, label);
            default:
                return Value{};
        }
    }

    Value lowerCall(const Expr& expr, const std::string& label) {
        const std::string& name = expr.text;
        Value result;
        if (name == "transpose") {
            const Value source = lowerExpr(expr.args[0], {});
            const int semitones = static_cast<int>(integerOf(expr.args[1]));
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
            const Rational factor = rationalOf(expr.args[1]);
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
            const int count = static_cast<int>(integerOf(expr.args[1]));
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
                parts.push_back(lowerExpr(argument, {}).fragment);
            }
            result.kind = TypeKind::Fragment;
            result.fragment = name == "sequence"
                                  ? project_.music().sequence(parts, label)
                                  : project_.music().parallel(parts, label);
            return result;
        }
        if (name == "diatonicTranspose") {
            const Value source = lowerExpr(expr.args[0], {});
            result.kind = TypeKind::Fragment;
            result.fragment = project_.music().diatonicTranspose(
                source.fragment, static_cast<int>(integerOf(expr.args[1])), label);
            return result;
        }
        if (name == "harmonize") {
            const Value source = lowerExpr(expr.args[0], {});
            const Value degrees = lowerExpr(expr.args[1], {});
            result.kind = TypeKind::Fragment;
            result.fragment = project_.music().harmonize(source.fragment, degrees.integers, label);
            return result;
        }
        if (name == "syncopate") {
            const Value source = lowerExpr(expr.args[0], {});
            result.kind = TypeKind::Fragment;
            result.fragment = project_.music().syncopate(
                source.fragment, rationalOf(expr.args[1]), label);
            return result;
        }
        if (name == "shift") {
            const Value source = lowerExpr(expr.args[0], {});
            result.kind = TypeKind::Curve;
            result.curve = project_.temporal().shift(source.curve, rationalOf(expr.args[1]),
                                                     label);
            return result;
        }
        diagnostics_.error("type-unknown-operator", expr.span,
                           "unknown operator '" + name + "'");
        return Value{};
    }

    Rational rationalOf(ExprId id) const {
        const Constant constant = hir_.constantOf(id);
        if (constant.kind == Constant::Kind::Integer) {
            return Rational(constant.integer, 1);
        }
        return constant.rational;
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
    std::vector<Value> symbolValues_;
    std::vector<std::size_t> trackIndices_;
    std::vector<LoweredSection> loweredSections_;
    std::set<InstanceId> instanceIds_;
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
