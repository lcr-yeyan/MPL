#include "mpl/frontend/Sema.hpp"

#include "mpl/core/Error.hpp"
#include "mpl/frontend/ParameterRegistry.hpp"
#include "mpl/ir/MixIR.hpp"
#include "mpl/music/Key.hpp"
#include "mpl/music/Pitch.hpp"

#include <algorithm>
#include <cmath>
#include <set>
#include <string>
#include <utility>

namespace mpl {
namespace {

const Type kErrorType = Type::simple(TypeKind::Error);
const Type kScalarType = Type::simple(TypeKind::Scalar);
const Type kDurationType = Type::simple(TypeKind::Duration);
const Type kPositionType = Type::simple(TypeKind::Position);
const Type kIntervalType = Type::simple(TypeKind::Interval);
const Type kScaleDegreeType = Type::simple(TypeKind::ScaleDegree);
const Type kTempoType = Type::simple(TypeKind::Tempo);
const Type kFrequencyType = Type::simple(TypeKind::Frequency);
const Type kGainType = Type::simple(TypeKind::Gain);
const Type kPhysicalTimeType = Type::simple(TypeKind::PhysicalTime);
const Type kFragmentType = Type::simple(TypeKind::Fragment);
const Type kStringType = Type::simple(TypeKind::String);

bool isBiquadMode(std::string_view text) {
    return text == "lowpass" || text == "highpass" || text == "peaking" ||
           text == "lowshelf" || text == "highshelf";
}

Type expectedTypeForUnit(ParameterUnit unit) {
    switch (unit) {
        case ParameterUnit::Decibel:
            return kGainType;
        case ParameterUnit::Hertz:
            return kFrequencyType;
        case ParameterUnit::Seconds:
        case ParameterUnit::Milliseconds:
            return kPhysicalTimeType;
        default:
            return kScalarType;
    }
}

const ProcessorParameterDescriptor* findProcessorDescriptor(ProcessorKind kind,
                                                            std::string_view name) {
    for (const ProcessorParameterDescriptor& descriptor : processorParameters(kind)) {
        if (descriptor.name == name) {
            return &descriptor;
        }
    }
    return nullptr;
}

bool isPowerOfTwo(int value) {
    return value > 0 && (value & (value - 1)) == 0;
}

bool isBaseCurveCall(std::string_view name) {
    return name == "constant" || name == "linear" || name == "step" ||
           name == "smoothstep" || name == "keyframes";
}

std::string expectedText(const Type& type) {
    return typeName(type);
}}  // namespace

Sema::Sema(const Module& module, DiagnosticSink& diagnostics)
    : module_(module), diagnostics_(diagnostics) {
    const std::size_t count = module_.expressions.size();
    hir_.exprTypes.assign(count, Type{});
    hir_.exprSymbols.assign(count, kInvalidSymbolId);
    hir_.exprConstants.assign(count, Constant{});
    checked_.assign(count, 0);
}

HirModule Sema::analyze() {
    if (module_.project.has_value()) {
        checkProject();
    } else {
        diagnostics_.error("sema-missing-project", module_.span,
                           "program has no project declaration");
    }

    for (const DeclRef& reference : module_.declarationOrder) {
        switch (reference.kind) {
            case DeclKind::Asset: checkAsset(module_.assets[reference.index]); break;
            case DeclKind::Let: checkLet(module_.lets[reference.index]); break;
            case DeclKind::Curve: checkCurve(module_.curves[reference.index]); break;
            case DeclKind::Processor:
                checkProcessor(module_.processors[reference.index]);
                break;
            case DeclKind::Bus: checkBus(module_.buses[reference.index]); break;
            case DeclKind::Track: checkTrack(module_.tracks[reference.index]); break;
            case DeclKind::Section: checkSection(module_.sections[reference.index]); break;
            case DeclKind::Automation:
                checkAutomation(module_.automations[reference.index]);
                break;
        }
    }

    if (module_.master.has_value()) {
        checkMaster(*module_.master);
    }

    if (module_.arrangement.has_value()) {
        checkArrangement(*module_.arrangement);
    } else {
        diagnostics_.error("sema-missing-arrangement", module_.span,
                           "program has no arrangement declaration");
    }
    if (module_.exports.empty()) {
        diagnostics_.error("sema-missing-export", module_.span,
                           "program has no export declaration");
    } else {
        for (const ExportDecl& exportDecl : module_.exports) {
            checkExport(exportDecl);
        }
    }

    return std::move(hir_);
}

void Sema::errorAt(const SourceSpan& span, std::string code, std::string message,
                   std::string expected, std::string actual, std::string related) {
    diagnostics_.error(std::move(code), span, std::move(message), std::move(expected),
                       std::move(actual), std::move(related));
}

bool Sema::addSymbol(const std::string& name, SymbolKind kind, const SourceSpan& span,
                     std::size_t declarationIndex, Type type,
                     const Rational& curveExtent) {
    if (name.empty()) {
        errorAt(span, "sema-invalid-name", "declaration has an empty name");
        return false;
    }
    if (hir_.symbols.find(name).has_value()) {
        errorAt(span, "sema-duplicate-symbol",
                "duplicate declaration '" + name + "'", "unique name", name);
        return false;
    }
    const SymbolId id = hir_.symbols.add(Symbol{name, kind, span, declarationIndex});
    (void)id;
    hir_.symbolTypes.push_back(type);
    hir_.symbolCurveExtents.push_back(curveExtent);
    return true;
}

std::optional<SymbolId> Sema::findSymbol(const std::string& name) const {
    return hir_.symbols.find(name);
}

bool Sema::requireKind(const Expr& expr, const Type& type, TypeKind expectedType,
                       std::string_view expectation) {
    if (type.isError()) {
        return false;
    }
    if (type.kind == expectedType) {
        return true;
    }
    errorAt(expr.span, "type-mismatch",
            "type mismatch: expected " + std::string(expectation),
            std::string(expectation), typeName(type), expr.text);
    return false;
}

void Sema::checkProject() {
    const ProjectDecl& project = *module_.project;
    if (project.tempo == kInvalidExprId) {
        errorAt(project.span, "sema-missing-field", "project is missing 'tempo'");
    } else {
        const Type type = checkExpr(project.tempo, kTempoType);
        if (!type.isError()) {
            if (type.kind != TypeKind::Tempo) {
                errorAt(module_.expr(project.tempo).span, "type-mismatch",
                        "tempo must be a value with 'bpm'", "tempo (bpm)", typeName(type));
            } else {
                const Constant constant = hir_.constantOf(project.tempo);
                if (!(constant.number > 0.0) || constant.number > 1000.0) {
                    errorAt(module_.expr(project.tempo).span, "sema-tempo-range",
                            "tempo must be in (0, 1000] bpm");
                }
                hir_.tempo = constant.number;
            }
        }
    }

    if (project.meter == kInvalidExprId) {
        errorAt(project.span, "sema-missing-field", "project is missing 'meter'");
    } else {
        const Expr& meter = module_.expr(project.meter);
        if (meter.kind != ExprKind::Rational) {
            errorAt(meter.span, "type-meter", "meter must be written as '<numerator>/<denominator>'",
                    "rational meter", std::string(exprKindName(meter.kind)));
        } else {
            const int numerator = static_cast<int>(meter.numerator);
            const int denominator = static_cast<int>(meter.denominator);
            if (numerator < 1 || denominator < 1 || !isPowerOfTwo(denominator)) {
                errorAt(meter.span, "type-meter",
                        "meter must have a positive numerator and a power-of-two denominator");
            } else {
                hir_.meter = Meter{numerator, denominator};
            }
        }
    }

    if (project.key == kInvalidExprId) {
        errorAt(project.span, "sema-missing-field", "project is missing 'key'");
    } else {
        const Expr& key = module_.expr(project.key);
        if (key.kind == ExprKind::Key) {
            try {
                hir_.key = Key::parse(key.text);
            } catch (const Error& error) {
                errorAt(key.span, "type-key", error.what());
            }
        } else {
            errorAt(key.span, "type-key", "expected a key such as 'F minor'");
        }
    }
}

void Sema::checkAsset(const AssetDecl& decl) {
    if (decl.kind != "instrument" && decl.kind != "sample") {
        errorAt(decl.span, "type-asset", "asset kind must be 'instrument' or 'sample'");
    }
    if (decl.provider.empty()) {
        errorAt(decl.span, "type-asset", "asset provider must not be empty");
    }
    addSymbol(decl.name, SymbolKind::Asset, decl.span,
              static_cast<std::size_t>(&decl - module_.assets.data()),
              Type::simple(TypeKind::Asset));
}

void Sema::checkLet(const LetDecl& decl) {
    const Type type = checkExpr(decl.value);
    if (!type.isError() && type.kind != TypeKind::Fragment && type.kind != TypeKind::Curve) {
        errorAt(decl.span, "type-mismatch",
                "let bindings must hold a fragment or a curve", "fragment or curve",
                typeName(type), decl.name);
    }
    addSymbol(decl.name, SymbolKind::Let, decl.span,
              static_cast<std::size_t>(&decl - module_.lets.data()), type,
              type.kind == TypeKind::Curve ? curveExtentOf(decl.value).value_or(Rational(1, 1))
                                           : Rational(1, 1));
}

void Sema::checkCurve(const CurveDecl& decl) {
    TimeDomain domain = TimeDomain::Relative;
    if (decl.domain == "musical") {
        domain = TimeDomain::Musical;
    } else if (decl.domain == "relative") {
        domain = TimeDomain::Relative;
    } else if (decl.domain == "physical" || decl.domain == "sample") {
        errorAt(decl.span, "type-domain-unsupported",
                "physical/sample-domain curves are reserved for the audio runtime");
    } else {
        errorAt(decl.span, "type-domain", "unknown curve domain '" + decl.domain + "'",
                "musical or relative", decl.domain);
    }

    Rational extent(1, 1);
    if (domain == TimeDomain::Relative) {
        if (decl.extent != kInvalidExprId) {
            errorAt(module_.expr(decl.extent).span, "type-domain",
                    "relative curves are normalized to extent 1");
        }
    } else if (decl.extent != kInvalidExprId) {
        const Type extentType = checkExpr(decl.extent, kDurationType);
        if (!extentType.isError()) {
            if (extentType.kind != TypeKind::Duration) {
                errorAt(module_.expr(decl.extent).span, "type-mismatch",
                        "musical curve extent must be a duration", "duration",
                        typeName(extentType));
            } else if (const Rational value = durationConstant(decl.extent);
                       !value.isPositive()) {
                errorAt(module_.expr(decl.extent).span, "type-domain",
                        "curve extent must be positive");
            } else {
                extent = value;
            }
        }
    }

    inCurveDeclaration_ = true;
    const Type valueType = checkCurveLiteral(decl.value, domain, extent);
    inCurveDeclaration_ = false;
    if (!valueType.isError() && valueType.kind != TypeKind::Curve) {
        errorAt(module_.expr(decl.value).span, "type-mismatch",
                "curve declaration requires a curve value", "curve", typeName(valueType));
    }

    addSymbol(decl.name, SymbolKind::Curve, decl.span,
              static_cast<std::size_t>(&decl - module_.curves.data()), Type::curve(domain),
              extent);
}

void Sema::checkInsert(const std::string& insert, const SourceSpan& span,
                       const std::string& owner) {
    if (insert.empty()) {
        errorAt(span, "sema-unknown-name",
                "strip '" + owner + "' inserts an empty processor name");
        return;
    }
    const std::optional<SymbolId> symbol = findSymbol(insert);
    if (!symbol.has_value()) {
        errorAt(span, "sema-unknown-name", "unknown processor '" + insert + "'", "processor",
                insert, owner);
    } else if (hir_.symbolTypes[*symbol].kind != TypeKind::Processor) {
        errorAt(span, "type-mismatch", "'" + insert + "' is not a processor", "processor",
                typeName(hir_.symbolTypes[*symbol]), owner);
    }
}

void Sema::checkStripLevels(ExprId gain, ExprId pan, const SourceSpan& span,
                            const std::string& owner) {
    if (gain != kInvalidExprId) {
        const Type type = checkExpr(gain, kGainType);
        if (!type.isError() && type.kind != TypeKind::Gain) {
            errorAt(module_.expr(gain).span, "type-mismatch",
                    "strip '" + owner + "' gain must be in dB", "dB", typeName(type), owner);
        }
    }
    if (pan != kInvalidExprId) {
        const Type type = checkExpr(pan, kScalarType);
        if (!type.isError()) {
            if (type.kind != TypeKind::Scalar) {
                errorAt(module_.expr(pan).span, "type-mismatch",
                        "strip '" + owner + "' pan must be a scalar", "scalar",
                        typeName(type), owner);
            } else {
                const double value = hir_.constantOf(pan).number;
                if (value < -1.0 || value > 1.0) {
                    errorAt(module_.expr(pan).span, "sema-parameter-range",
                            "strip '" + owner + "' pan must be in [-1, 1]");
                }
            }
        }
    }
    (void)span;
}

void Sema::checkProcessor(const ProcessorDecl& decl) {
    ProcessorKind kind = ProcessorKind::Gain;
    const bool known = processorKindFromName(decl.kind, kind);
    if (!known) {
        errorAt(decl.span, "type-processor",
                "unknown processor kind '" + decl.kind + "'",
                "gain, pan, biquad, compressor, delay or reverb", decl.kind);
    } else if (kind == ProcessorKind::Biquad) {
        if (decl.mode.empty()) {
            errorAt(decl.span, "sema-processor-mode", "biquad requires a filter mode",
                    "lowpass, highpass, peaking, lowshelf or highshelf");
        } else if (!isBiquadMode(decl.mode)) {
            errorAt(decl.span, "sema-processor-mode",
                    "unknown filter mode '" + decl.mode + "'",
                    "lowpass, highpass, peaking, lowshelf or highshelf", decl.mode);
        }
    } else if (!decl.mode.empty()) {
        errorAt(decl.span, "sema-processor-mode",
                "processor kind '" + decl.kind + "' does not accept a mode", "no mode",
                decl.mode);
    }

    std::set<std::string> seen;
    for (const ProcessorField& field : decl.fields) {
        if (!seen.insert(field.name).second) {
            errorAt(field.span, "sema-duplicate-field",
                    "duplicate processor parameter '" + field.name + "'");
            continue;
        }
        if (!known) {
            continue;
        }
        const ProcessorParameterDescriptor* descriptor =
            findProcessorDescriptor(kind, field.name);
        if (descriptor == nullptr) {
            errorAt(field.span, "sema-processor-parameter",
                    "unknown parameter '" + field.name + "' for processor kind '" +
                        decl.kind + "'");
            continue;
        }
        const Type expected = expectedTypeForUnit(descriptor->unit);
        const Type actual = checkExpr(field.value, expected);
        if (actual.isError()) {
            continue;
        }
        if (actual.kind != expected.kind) {
            errorAt(module_.expr(field.value).span, "type-mismatch",
                    "parameter '" + field.name + "' of '" + decl.name + "' expects " +
                        typeName(expected),
                    typeName(expected), typeName(actual), decl.name);
            continue;
        }
        const Constant value = hir_.constantOf(field.value);
        if (value.kind != Constant::Kind::None &&
            (value.number < descriptor->minimum - 1e-9 ||
             value.number > descriptor->maximum + 1e-9)) {
            errorAt(module_.expr(field.value).span, "sema-parameter-range",
                    "parameter '" + field.name + "' of '" + decl.name +
                        "' is outside its range");
        }
    }

    addSymbol(decl.name, SymbolKind::Processor, decl.span,
              static_cast<std::size_t>(&decl - module_.processors.data()),
              Type::simple(TypeKind::Processor));
}

void Sema::checkBus(const BusDecl& decl) {
    for (const std::string& insert : decl.inserts) {
        checkInsert(insert, decl.span, decl.name);
    }
    checkStripLevels(decl.gain, decl.pan, decl.span, decl.name);
    addSymbol(decl.name, SymbolKind::Bus, decl.span,
              static_cast<std::size_t>(&decl - module_.buses.data()),
              Type::simple(TypeKind::Bus));
}

void Sema::checkMaster(const MasterDecl& decl) {
    for (const std::string& insert : decl.inserts) {
        checkInsert(insert, decl.span, "master");
    }
    checkStripLevels(decl.gain, decl.pan, decl.span, "master");
}

void Sema::checkTrack(const TrackDecl& decl) {
    if (decl.asset.empty()) {
        errorAt(decl.span, "sema-missing-field", "track '" + decl.name + "' is missing 'asset'");
    } else if (const std::optional<SymbolId> asset = findSymbol(decl.asset);
               !asset.has_value()) {
        errorAt(decl.span, "sema-unknown-name", "unknown asset '" + decl.asset + "'", "asset",
                decl.asset, decl.name);
    } else if (hir_.symbolTypes[*asset].kind != TypeKind::Asset) {
        errorAt(decl.span, "type-mismatch", "'" + decl.asset + "' is not an asset", "asset",
                typeName(hir_.symbolTypes[*asset]), decl.name);
    }
    if (!isInstrumentRole(decl.role)) {
        errorAt(decl.span, "type-role",
                "unknown instrument role '" + decl.role + "'",
                "melodic, bass, harmony, percussion or texture", decl.role, decl.name);
    }
    checkStripLevels(decl.gain, decl.pan, decl.span, decl.name);
    for (const std::string& insert : decl.inserts) {
        checkInsert(insert, decl.span, decl.name);
    }
    for (const SendDecl& send : decl.sends) {
        if (send.level != kInvalidExprId) {
            const Type type = checkExpr(send.level, kScalarType);
            if (!type.isError()) {
                if (type.kind != TypeKind::Scalar) {
                    errorAt(module_.expr(send.level).span, "type-mismatch",
                            "send level must be a linear scalar", "scalar", typeName(type),
                            decl.name);
                } else {
                    const double value = hir_.constantOf(send.level).number;
                    if (value < 0.0 || value > 1.0) {
                        errorAt(module_.expr(send.level).span, "sema-parameter-range",
                                "send level must be in [0, 1]");
                    }
                }
            }
        }
        if (const std::optional<SymbolId> bus = findSymbol(send.bus);
            !bus.has_value()) {
            errorAt(send.span, "sema-unknown-name", "unknown bus '" + send.bus + "'", "bus",
                    send.bus, decl.name);
        } else if (hir_.symbolTypes[*bus].kind != TypeKind::Bus) {
            errorAt(send.span, "type-mismatch", "'" + send.bus + "' is not a bus", "bus",
                    typeName(hir_.symbolTypes[*bus]), decl.name);
        }
    }
    for (const SidechainDecl& sidechain : decl.sidechains) {
        if (const std::optional<SymbolId> processorId = findSymbol(sidechain.processor);
            !processorId.has_value()) {
            errorAt(sidechain.span, "sema-unknown-name",
                    "unknown processor '" + sidechain.processor + "'", "processor",
                    sidechain.processor, decl.name);
        } else if (hir_.symbolTypes[*processorId].kind != TypeKind::Processor) {
            errorAt(sidechain.span, "type-mismatch",
                    "'" + sidechain.processor + "' is not a processor", "processor",
                    typeName(hir_.symbolTypes[*processorId]), decl.name);
        } else {
            const Symbol& processorSymbol = hir_.symbols.get(*processorId);
            const ProcessorDecl& processorDecl =
                module_.processors[processorSymbol.declarationIndex];
            ProcessorKind kind = ProcessorKind::Gain;
            if (processorKindFromName(processorDecl.kind, kind) &&
                kind != ProcessorKind::Compressor) {
                errorAt(sidechain.span, "type-sidechain",
                        "sidechain target '" + sidechain.processor +
                            "' must be a compressor",
                        "compressor", processorDecl.kind, decl.name);
            }
            if (std::find(decl.inserts.begin(), decl.inserts.end(), sidechain.processor) ==
                decl.inserts.end()) {
                errorAt(sidechain.span, "sema-unknown-insert",
                        "sidechain processor '" + sidechain.processor +
                            "' is not inserted on track '" + decl.name + "'",
                        "inserted processor", sidechain.processor, decl.name);
            }
        }
        if (const std::optional<SymbolId> source = findSymbol(sidechain.source);
            !source.has_value()) {
            errorAt(sidechain.span, "sema-unknown-name",
                    "unknown sidechain source track '" + sidechain.source + "'", "track",
                    sidechain.source, decl.name);
        } else if (hir_.symbolTypes[*source].kind != TypeKind::Track) {
            errorAt(sidechain.span, "type-mismatch",
                    "'" + sidechain.source + "' is not a track", "track",
                    typeName(hir_.symbolTypes[*source]), decl.name);
        }
    }
    addSymbol(decl.name, SymbolKind::Track, decl.span,
              static_cast<std::size_t>(&decl - module_.tracks.data()),
              Type::simple(TypeKind::Track));
}

void Sema::checkSection(const SectionDecl& decl) {
    if (decl.duration == kInvalidExprId) {
        errorAt(decl.span, "sema-missing-field", "section '" + decl.name + "' has no duration");
    } else {
        const Type type = checkExpr(decl.duration, kDurationType);
        if (!type.isError()) {
            if (type.kind != TypeKind::Duration) {
                errorAt(module_.expr(decl.duration).span, "type-mismatch",
                        "section duration must be a duration", "duration", typeName(type),
                        decl.name);
            } else if (!isPositiveDuration(decl.duration)) {
                errorAt(module_.expr(decl.duration).span, "type-duration",
                        "section duration must be positive");
            }
        }
    }

    std::set<std::string> tracksInSection;
    for (const ClipDecl& clip : decl.clips) {
        SymbolId trackSymbol = kInvalidSymbolId;
        if (clip.track.empty()) {
            errorAt(clip.span, "sema-unknown-name", "clip has no track", "track");
        } else if (const std::optional<SymbolId> symbol = findSymbol(clip.track);
                   !symbol.has_value()) {
            errorAt(clip.span, "sema-unknown-name", "unknown track '" + clip.track + "'", "track",
                    clip.track, decl.name);
        } else if (hir_.symbolTypes[*symbol].kind != TypeKind::Track) {
            errorAt(clip.span, "type-mismatch", "'" + clip.track + "' is not a track", "track",
                    typeName(hir_.symbolTypes[*symbol]), decl.name);
        } else {
            trackSymbol = *symbol;
            tracksInSection.insert(clip.track);
        }
        hir_.clipTrackSymbols.push_back(trackSymbol);

        if (clip.at != kInvalidExprId) {
            const Type atType = checkExpr(clip.at, kDurationType);
            if (!atType.isError()) {
                if (atType.kind != TypeKind::Duration) {
                    errorAt(module_.expr(clip.at).span, "type-mismatch",
                            "clip offset must be a duration", "duration", typeName(atType));
                } else if (hir_.constantOf(clip.at).rational.isNegative()) {
                    errorAt(module_.expr(clip.at).span, "type-duration",
                            "clip offset must not be negative");
                }
            }
        }

        if (clip.play != kInvalidExprId) {
            const Type type = checkExpr(clip.play, kFragmentType);
            if (!type.isError()) {
                requireKind(module_.expr(clip.play), type, TypeKind::Fragment, "fragment");
            }
        }

        const auto checkLane = [&](ExprId lane, std::string_view laneName) {
            if (lane == kInvalidExprId) {
                return;
            }
            const ExpressionLaneInfo* info = findExpressionLane(laneName);
            if (info == nullptr) {
                return;
            }
            const Type type = checkExpr(lane, Type::curve(TimeDomain::Relative));
            if (type.isError()) {
                return;
            }
            if (type.kind != TypeKind::Curve || type.domain != TimeDomain::Relative) {
                errorAt(module_.expr(lane).span, "type-domain",
                        "expression lane '" + std::string(laneName) +
                            "' requires a relative curve",
                        "curve<relative>", typeName(type));
                return;
            }
            const std::optional<Rational> extent = curveExtentOf(lane);
            if (extent.has_value() && *extent != Rational(1, 1)) {
                errorAt(module_.expr(lane).span, "type-expression-extent",
                        "expression lane '" + std::string(laneName) +
                            "' must be normalized to extent 1",
                        "extent 1", extent->toString());
            }
            if (!curveValuesInRange(lane, info->minimum, info->maximum)) {
                errorAt(module_.expr(lane).span, "sema-parameter-range",
                        "expression lane '" + std::string(laneName) +
                            "' leaves its parameter range");
            }
        };
        checkLane(clip.velocity, "velocity");
        checkLane(clip.timing, "timing");
        checkLane(clip.gate, "gate");
        checkLane(clip.pan, "pan");
    }

    addSymbol(decl.name, SymbolKind::Section, decl.span,
              static_cast<std::size_t>(&decl - module_.sections.data()),
              Type::simple(TypeKind::Section));
}

void Sema::checkAutomation(const AutomationDecl& decl) {
    const std::string mode = decl.mode.empty() ? "replace" : decl.mode;
    if (mode != "replace" && mode != "add" && mode != "multiply") {
        errorAt(decl.span, "sema-automation-mode",
                "unknown automation mode '" + mode + "'", "replace, add or multiply", mode);
    }

    if (!decl.processor.empty()) {
        const bool masterTarget = decl.scope == "master";
        SymbolId targetSymbol = kInvalidSymbolId;
        if (!masterTarget) {
            if (const std::optional<SymbolId> symbol = findSymbol(decl.track);
                !symbol.has_value()) {
                errorAt(decl.span, "sema-unknown-name", "unknown automation target '" +
                                                           decl.track + "'",
                        "track or bus", decl.track);
            } else if (hir_.symbolTypes[*symbol].kind != TypeKind::Track &&
                       hir_.symbolTypes[*symbol].kind != TypeKind::Bus) {
                errorAt(decl.span, "type-mismatch",
                        "'" + decl.track + "' is not a track or bus", "track or bus",
                        typeName(hir_.symbolTypes[*symbol]));
            } else {
                targetSymbol = *symbol;
            }
        }
        hir_.automationTrackSymbols.push_back(masterTarget ? kInvalidSymbolId : targetSymbol);
        const std::string targetName = masterTarget ? "master" : decl.track;

        SymbolId processorSymbol = kInvalidSymbolId;
        if (const std::optional<SymbolId> symbol = findSymbol(decl.processor);
            !symbol.has_value()) {
            errorAt(decl.span, "sema-unknown-name",
                    "unknown processor '" + decl.processor + "'", "processor", decl.processor);
        } else if (hir_.symbolTypes[*symbol].kind != TypeKind::Processor) {
            errorAt(decl.span, "type-mismatch", "'" + decl.processor + "' is not a processor",
                    "processor", typeName(hir_.symbolTypes[*symbol]));
        } else {
            processorSymbol = *symbol;
        }

        const ProcessorParameterDescriptor* descriptor = nullptr;
        ProcessorKind kind = ProcessorKind::Gain;
        if (processorSymbol != kInvalidSymbolId) {
            const Symbol& symbol = hir_.symbols.get(processorSymbol);
            const ProcessorDecl& processor =
                module_.processors[symbol.declarationIndex];
            const bool known = processorKindFromName(processor.kind, kind);
            if (known) {
                descriptor = findProcessorDescriptor(kind, decl.parameter);
                if (descriptor == nullptr) {
                    errorAt(decl.span, "sema-unknown-parameter",
                            "unknown parameter '" + decl.parameter + "' for processor '" +
                                decl.processor + "'",
                            "processor parameter", decl.parameter);
                }
            }

            const std::vector<std::string>* inserts = nullptr;
            if (masterTarget) {
                if (module_.master.has_value()) {
                    inserts = &module_.master->inserts;
                }
            } else if (targetSymbol != kInvalidSymbolId) {
                const Symbol& target = hir_.symbols.get(targetSymbol);
                if (target.kind == SymbolKind::Track) {
                    inserts = &module_.tracks[target.declarationIndex].inserts;
                } else if (target.kind == SymbolKind::Bus) {
                    inserts = &module_.buses[target.declarationIndex].inserts;
                }
            }
            if (inserts != nullptr &&
                std::find(inserts->begin(), inserts->end(), decl.processor) ==
                    inserts->end()) {
                errorAt(decl.span, "sema-unknown-insert",
                        "processor '" + decl.processor + "' is not inserted on '" +
                            targetName + "'",
                        "inserted processor", decl.processor, targetName);
            }
        }

        const Type type = checkExpr(decl.curve, Type::curve(TimeDomain::Musical));
        if (!type.isError()) {
            if (type.kind != TypeKind::Curve) {
                errorAt(module_.expr(decl.curve).span, "type-mismatch",
                        "automation requires a curve", "curve", typeName(type));
            } else if (type.domain != TimeDomain::Musical) {
                errorAt(module_.expr(decl.curve).span, "type-domain",
                        "processor automation requires a musical-domain curve",
                        "curve<musical>", typeName(type));
            } else if (descriptor != nullptr &&
                       !curveValuesInRange(decl.curve, descriptor->minimum,
                                           descriptor->maximum)) {
                errorAt(module_.expr(decl.curve).span, "sema-parameter-range",
                        "automation curve leaves the range of '" + decl.parameter + "'");
            }
        }

        if (mode == "replace") {
            const std::string key = targetName + "|" + decl.processor + "|" +
                                    decl.parameter + "|musical";
            if (++replaceCounts_[key] > 1) {
                errorAt(decl.span, "sema-automation-conflict",
                        "multiple replace bindings target '" + decl.processor + "." +
                            decl.parameter + "'");
            }
        }
        return;
    }

    const ParameterInfo* parameter = findParameter(decl.parameter);
    if (parameter == nullptr) {
        errorAt(decl.span, "sema-unknown-parameter",
                "unknown automation parameter '" + decl.parameter + "'",
                "registered parameter", decl.parameter);
    } else if (decl.scope == "track" && !parameter->trackScope) {
        errorAt(decl.span, "sema-parameter-scope",
                "parameter '" + decl.parameter + "' does not support track scope");
    } else if (decl.scope == "master" && !parameter->masterScope) {
        errorAt(decl.span, "sema-parameter-scope",
                "parameter '" + decl.parameter + "' does not support master scope");
    }

    SymbolId trackSymbol = kInvalidSymbolId;
    if (decl.scope == "track") {
        if (const std::optional<SymbolId> symbol = findSymbol(decl.track);
            !symbol.has_value()) {
            errorAt(decl.span, "sema-unknown-name", "unknown track '" + decl.track + "'",
                    "track", decl.track);
        } else if (hir_.symbolTypes[*symbol].kind != TypeKind::Track) {
            errorAt(decl.span, "type-mismatch", "'" + decl.track + "' is not a track", "track",
                    typeName(hir_.symbolTypes[*symbol]));
        } else {
            trackSymbol = *symbol;
        }
    }
    hir_.automationTrackSymbols.push_back(trackSymbol);

    const Type type = checkExpr(decl.curve, Type::curve(TimeDomain::Musical));
    if (!type.isError()) {
        if (type.kind != TypeKind::Curve) {
            errorAt(module_.expr(decl.curve).span, "type-mismatch",
                    "automation requires a curve", "curve", typeName(type));
        } else if (type.domain != TimeDomain::Musical) {
            errorAt(module_.expr(decl.curve).span, "type-domain",
                    "V0.1.0 automation requires a musical-domain curve", "curve<musical>",
                    typeName(type));
        } else if (parameter != nullptr &&
                   !curveValuesInRange(decl.curve, parameter->minimum, parameter->maximum)) {
            errorAt(module_.expr(decl.curve).span, "sema-parameter-range",
                    "automation curve leaves the range of '" + decl.parameter + "'");
        }
    }

    if (mode == "replace") {
        const std::string key =
            decl.scope + "|" + decl.track + "|" + decl.parameter + "|musical";
        if (++replaceCounts_[key] > 1) {
            errorAt(decl.span, "sema-automation-conflict",
                    "multiple replace bindings target '" + decl.parameter + "'");
        }
    }
}

void Sema::checkArrangement(const ArrangementDecl& decl) {
    if (decl.plays.empty()) {
        errorAt(decl.span, "sema-empty-arrangement", "arrangement must play at least one section");
    }
    for (const auto& play : decl.plays) {
        if (const std::optional<SymbolId> symbol = findSymbol(play.first);
            !symbol.has_value()) {
            errorAt(play.second, "sema-unknown-name", "unknown section '" + play.first + "'",
                    "section", play.first);
            hir_.arrangementSymbols.push_back(kInvalidSymbolId);
        } else if (hir_.symbolTypes[*symbol].kind != TypeKind::Section) {
            errorAt(play.second, "type-mismatch", "'" + play.first + "' is not a section",
                    "section", typeName(hir_.symbolTypes[*symbol]));
            hir_.arrangementSymbols.push_back(kInvalidSymbolId);
        } else {
            hir_.arrangementSymbols.push_back(*symbol);
        }
    }
}

void Sema::checkExport(const ExportDecl& decl) {
    if (decl.format != "midi" && decl.format != "wav") {
        errorAt(decl.span, "type-export", "unknown export format '" + decl.format + "'",
                "midi or wav", decl.format);
    }
    if (decl.path.empty()) {
        errorAt(decl.span, "type-export", "export path must not be empty");
    }
}

Type Sema::checkExpr(ExprId id, std::optional<Type> expected) {
    if (id == kInvalidExprId || id >= module_.expressions.size()) {
        return kErrorType;
    }
    if (checked_[id] != 0) {
        return hir_.exprTypes[id];
    }
    checked_[id] = 1;

    const Expr& expr = module_.expr(id);
    Type type = kErrorType;
    switch (expr.kind) {
        case ExprKind::Integer:
        case ExprKind::Rational:
        case ExprKind::Number:
            type = checkNumeric(id, expr, expected);
            break;
        case ExprKind::String:
            type = kStringType;
            break;
        case ExprKind::Identifier:
            type = checkIdentifier(id, expr);
            break;
        case ExprKind::Key:
            type = Type::simple(TypeKind::Key);
            break;
        case ExprKind::Call:
            type = checkCall(expr);
            break;
        case ExprKind::UnaryMinus: {
            if (expr.args.empty()) {
                break;
            }
            type = checkExpr(expr.args.front(), expected);
            if (!type.isError()) {
                Constant constant = hir_.constantOf(expr.args.front());
                if (constant.kind == Constant::Kind::Rational) {
                    constant.rational = -constant.rational;
                    constant.number = -constant.number;
                } else if (constant.kind == Constant::Kind::Number) {
                    constant.number = -constant.number;
                } else if (constant.kind == Constant::Kind::Integer) {
                    constant.integer = -constant.integer;
                    constant.number = -constant.number;
                    constant.rational = Rational(constant.integer, 1);
                }
                hir_.exprConstants[id] = constant;
            }
            break;
        }
        case ExprKind::List:
            type = checkList(expr, TypeKind::ScaleDegree);
            break;
        case ExprKind::Melody:
            type = checkMelody(expr);
            break;
        case ExprKind::Notes:
            type = checkNotes(expr);
            break;
        case ExprKind::Keyframes:
            errorAt(expr.span, "type-curve-context",
                    "keyframes are only valid inside a curve declaration");
            break;
        case ExprKind::Invalid:
            break;
    }
    hir_.exprTypes[id] = type;
    return type;
}

Type Sema::checkNumeric(ExprId id, const Expr& expr, std::optional<Type> expected) {
    Constant constant;
    if (expr.kind == ExprKind::Integer) {
        constant = Constant::integerValue(expr.integer);
    } else if (expr.kind == ExprKind::Rational) {
        constant = Constant::rationalValue(Rational(expr.numerator, expr.denominator));
    } else {
        if (!std::isfinite(expr.number)) {
            errorAt(expr.span, "type-number", "numeric literal must be finite");
            return kErrorType;
        }
        constant = Constant::numberValue(expr.number);
    }

    if (!expr.unit.empty()) {
        if (expr.unit == "st") {
            if (expr.kind != ExprKind::Integer) {
                errorAt(expr.span, "type-unit", "intervals must be whole semitones", "integer",
                        "fractional value");
                return kErrorType;
            }
            hir_.exprConstants[id] = constant;
            return kIntervalType;
        }
        if (expr.unit == "bpm") {
            if (expr.kind != ExprKind::Integer && expr.kind != ExprKind::Number) {
                errorAt(expr.span, "type-unit", "tempo must be a number", "number",
                        std::string(exprKindName(expr.kind)));
                return kErrorType;
            }
            hir_.exprConstants[id] = constant;
            return kTempoType;
        }
        if (expr.unit == "bars" || expr.unit == "beats" || expr.unit == "whole") {
            if (expr.kind == ExprKind::Number) {
                errorAt(expr.span, "type-unit", "durations must be exact rational values",
                        "integer or rational", "decimal");
                return kErrorType;
            }
            Rational value = constant.rational;
            if (expr.unit == "bars") {
                value = value * hir_.meter.barLength();
            } else if (expr.unit == "beats") {
                value = value * hir_.meter.beatLength();
            }
            hir_.exprConstants[id] = Constant::rationalValue(value);
            return kDurationType;
        }
        if (expr.unit == "Hz") {
            hir_.exprConstants[id] = constant;
            return Type::simple(TypeKind::Frequency);
        }
        if (expr.unit == "dB") {
            hir_.exprConstants[id] = constant;
            return Type::simple(TypeKind::Gain);
        }
        if (expr.unit == "ms" || expr.unit == "s") {
            const double seconds = expr.unit == "ms" ? constant.number / 1000.0 : constant.number;
            hir_.exprConstants[id] = Constant::numberValue(seconds);
            return Type::simple(TypeKind::PhysicalTime);
        }
        errorAt(expr.span, "type-unit", "unknown unit '" + expr.unit + "'",
                "bpm, st, bars, beats, whole, Hz, dB, ms or s", expr.unit);
        return kErrorType;
    }

    if (expected.has_value() && !expected->isError()) {
        switch (expected->kind) {
            case TypeKind::Duration:
            case TypeKind::Position:
                if (expr.kind == ExprKind::Number) {
                    errorAt(expr.span, "type-unit",
                            "time positions and durations must be exact rational values",
                            "integer or rational", "decimal");
                    return kErrorType;
                }
                hir_.exprConstants[id] = Constant::rationalValue(constant.rational);
                return Type::simple(expected->kind);
            case TypeKind::ScaleDegree:
                if (expr.kind != ExprKind::Integer) {
                    errorAt(expr.span, "type-unit", "scale degrees must be integers", "integer",
                            std::string(exprKindName(expr.kind)));
                    return kErrorType;
                }
                hir_.exprConstants[id] = constant;
                return kScaleDegreeType;
            case TypeKind::Scalar:
            case TypeKind::Number:
                hir_.exprConstants[id] = constant;
                return Type::simple(expected->kind);
            case TypeKind::Tempo:
                errorAt(expr.span, "type-unit", "tempo requires the 'bpm' unit", "number bpm",
                        std::string(exprKindName(expr.kind)));
                return kErrorType;
            case TypeKind::Interval:
                errorAt(expr.span, "type-unit", "intervals require the 'st' unit", "number st",
                        std::string(exprKindName(expr.kind)));
                return kErrorType;
            default:
                break;
        }
    }
    hir_.exprConstants[id] = constant;
    return Type::simple(TypeKind::Number);
}

Type Sema::checkIdentifier(ExprId id, const Expr& expr) {
    const std::optional<SymbolId> symbol = findSymbol(expr.text);
    if (!symbol.has_value()) {
        errorAt(expr.span, "sema-unknown-name", "unknown name '" + expr.text + "'");
        return kErrorType;
    }
    const Type type = hir_.symbolTypes[*symbol];
    hir_.exprSymbols[id] = *symbol;
    return type;
}

Type Sema::checkList(const Expr& expr, TypeKind element) {
    for (const ExprId item : expr.args) {
        const Type itemType = checkExpr(item, Type::simple(element));
        if (!itemType.isError()) {
            requireKind(module_.expr(item), itemType, element, typeKindName(element));
        }
    }
    const Type type = Type::list(element);
    hir_.exprTypes[static_cast<ExprId>(&expr - module_.expressions.data())] = type;
    return type;
}

Type Sema::checkMelody(const Expr& expr) {
    for (const MelodyEntry& entry : expr.melody) {
        try {
            (void)Pitch::parse(entry.pitch);
        } catch (const Error& error) {
            errorAt(entry.span, "type-pitch", error.what(), "pitch literal", entry.pitch);
        }
        const Type duration = checkExpr(entry.duration, kDurationType);
        if (!duration.isError()) {
            if (duration.kind != TypeKind::Duration) {
                errorAt(module_.expr(entry.duration).span, "type-mismatch",
                        "melody note duration must be a duration", "duration",
                        typeName(duration));
            } else if (!isPositiveDuration(entry.duration)) {
                errorAt(module_.expr(entry.duration).span, "type-duration",
                        "melody note duration must be positive");
            }
        }
    }
    return kFragmentType;
}

Type Sema::checkNotes(const Expr& expr) {
    Rational contentEnd(0, 1);
    for (const NoteEntry& entry : expr.notes) {
        try {
            (void)Pitch::parse(entry.pitch);
        } catch (const Error& error) {
            errorAt(entry.span, "type-pitch", error.what(), "pitch literal", entry.pitch);
        }
        const Type offset = checkExpr(entry.offset, kPositionType);
        if (!offset.isError() && offset.kind != TypeKind::Position) {
            errorAt(module_.expr(entry.offset).span, "type-mismatch",
                    "note offset must be a position", "position", typeName(offset));
        }
        const Rational offsetValue = hir_.constantOf(entry.offset).rational;
        if (offsetValue.isNegative()) {
            errorAt(module_.expr(entry.offset).span, "type-duration",
                    "note offset must not be negative");
        }

        const Type duration = checkExpr(entry.duration, kDurationType);
        if (!duration.isError() && duration.kind != TypeKind::Duration) {
            errorAt(module_.expr(entry.duration).span, "type-mismatch",
                    "note duration must be a duration", "duration", typeName(duration));
        }
        const Rational durationValue = hir_.constantOf(entry.duration).rational;
        if (!durationValue.isPositive()) {
            errorAt(module_.expr(entry.duration).span, "type-duration",
                    "note duration must be positive");
        }
        if (entry.velocity < 1 || entry.velocity > 127) {
            errorAt(entry.span, "type-velocity", "note velocity must be in 1..127");
        }
        contentEnd = std::max(contentEnd, offsetValue + durationValue);
    }

    if (expr.explicitSpan != kInvalidExprId) {
        const Type span = checkExpr(expr.explicitSpan, kDurationType);
        if (!span.isError()) {
            if (span.kind != TypeKind::Duration) {
                errorAt(module_.expr(expr.explicitSpan).span, "type-mismatch",
                        "notes span must be a duration", "duration", typeName(span));
            } else {
                const Rational spanValue = hir_.constantOf(expr.explicitSpan).rational;
                if (spanValue < contentEnd) {
                    errorAt(module_.expr(expr.explicitSpan).span, "type-duration",
                            "notes span is shorter than its content", contentEnd.toString(),
                            spanValue.toString());
                }
            }
        }
    }
    return kFragmentType;
}

Type Sema::checkCall(const Expr& expr) {
    const std::string& name = expr.text;
    const auto argument = [&](std::size_t index, std::optional<Type> expected = std::nullopt) {
        return index < expr.args.size() ? checkExpr(expr.args[index], expected) : kErrorType;
    };
    const auto arity = [&](std::size_t required) {
        if (expr.args.size() != required) {
            errorAt(expr.span, "type-arity",
                    "operator '" + name + "' expects " + std::to_string(required) +
                        " argument(s), got " + std::to_string(expr.args.size()),
                    std::to_string(required) + " arguments",
                    std::to_string(expr.args.size()) + " arguments", name);
            return false;
        }
        return true;
    };

    if (name == "transpose") {
        if (!arity(2)) return kErrorType;
        const Type source = argument(0, kFragmentType);
        const Type interval = argument(1, kIntervalType);
        if (!requireKind(module_.expr(expr.args[0]), source, TypeKind::Fragment, "fragment") ||
            !requireKind(module_.expr(expr.args[1]), interval, TypeKind::Interval, "interval")) {
            return kErrorType;
        }
        return kFragmentType;
    }
    if (name == "reverse") {
        if (!arity(1)) return kErrorType;
        const Type source = argument(0);
        if (source.kind == TypeKind::Fragment) return kFragmentType;
        if (source.kind == TypeKind::Curve) return source;
        errorAt(expr.span, "type-mismatch", "reverse expects a fragment or curve",
                "fragment or curve", typeName(source));
        return kErrorType;
    }
    if (name == "stretch") {
        if (!arity(2)) return kErrorType;
        const Type source = argument(0);
        const Type factor = argument(1, kScalarType);
        if (!requireKind(module_.expr(expr.args[1]), factor, TypeKind::Scalar, "scalar")) {
            return kErrorType;
        }
        const Constant factorConstant = hir_.constantOf(expr.args[1]);
        if (factorConstant.kind == Constant::Kind::Number) {
            errorAt(module_.expr(expr.args[1]).span, "type-unit",
                    "stretch factor must be an exact rational value");
        } else if (factorConstant.kind != Constant::Kind::None &&
                   !(factorConstant.number > 0.0)) {
            errorAt(module_.expr(expr.args[1]).span, "type-duration",
                    "stretch factor must be positive");
        }
        if (source.kind == TypeKind::Fragment || source.kind == TypeKind::Curve) {
            return source;
        }
        errorAt(expr.span, "type-mismatch", "stretch expects a fragment or curve",
                "fragment or curve", typeName(source));
        return kErrorType;
    }
    if (name == "repeat") {
        if (!arity(2)) return kErrorType;
        const Type source = argument(0);
        const Type count = argument(1, kScaleDegreeType);
        if (!requireKind(module_.expr(expr.args[1]), count, TypeKind::ScaleDegree, "integer")) {
            return kErrorType;
        }
        const Constant countConstant = hir_.constantOf(expr.args[1]);
        if (countConstant.kind != Constant::Kind::None && countConstant.integer < 1) {
            errorAt(module_.expr(expr.args[1]).span, "type-duration",
                    "repeat count must be at least 1");
        }
        if (source.kind == TypeKind::Fragment || source.kind == TypeKind::Curve) {
            return source;
        }
        errorAt(expr.span, "type-mismatch", "repeat expects a fragment or curve",
                "fragment or curve", typeName(source));
        return kErrorType;
    }
    if (name == "sequence" || name == "parallel") {
        if (expr.args.empty()) {
            errorAt(expr.span, "type-arity", "operator '" + name + "' needs at least one argument");
            return kErrorType;
        }
        for (const ExprId argumentId : expr.args) {
            const Type type = checkExpr(argumentId, kFragmentType);
            if (!requireKind(module_.expr(argumentId), type, TypeKind::Fragment, "fragment")) {
                return kErrorType;
            }
        }
        return kFragmentType;
    }
    if (name == "diatonicTranspose") {
        if (!arity(2)) return kErrorType;
        const Type source = argument(0, kFragmentType);
        const Type steps = argument(1, kScaleDegreeType);
        if (!requireKind(module_.expr(expr.args[0]), source, TypeKind::Fragment, "fragment") ||
            !requireKind(module_.expr(expr.args[1]), steps, TypeKind::ScaleDegree,
                         "integer scale degree")) {
            return kErrorType;
        }
        return kFragmentType;
    }
    if (name == "harmonize") {
        if (!arity(2)) return kErrorType;
        const Type source = argument(0, kFragmentType);
        if (!requireKind(module_.expr(expr.args[0]), source, TypeKind::Fragment, "fragment")) {
            return kErrorType;
        }
        const Expr& listExpr = module_.expr(expr.args[1]);
        const Type listType = checkList(listExpr, TypeKind::ScaleDegree);
        if (listExpr.args.empty()) {
            errorAt(listExpr.span, "type-arity", "harmonize needs at least one scale degree");
        }
        return listType.kind == TypeKind::List ? kFragmentType : kErrorType;
    }
    if (name == "syncopate") {
        if (!arity(2)) return kErrorType;
        const Type source = argument(0, kFragmentType);
        const Type duration = argument(1, kDurationType);
        if (!requireKind(module_.expr(expr.args[0]), source, TypeKind::Fragment, "fragment") ||
            !requireKind(module_.expr(expr.args[1]), duration, TypeKind::Duration, "duration")) {
            return kErrorType;
        }
        return kFragmentType;
    }
    if (name == "shift") {
        if (!arity(2)) return kErrorType;
        const Type source = argument(0);
        if (source.kind != TypeKind::Curve) {
            errorAt(expr.span, "type-mismatch", "shift expects a curve", "curve",
                    typeName(source));
            return kErrorType;
        }
        const Type timeType = source.domain == TimeDomain::Musical ? kDurationType : kScalarType;
        const Type time = argument(1, timeType);
        if (!time.isError()) {
            requireKind(module_.expr(expr.args[1]), time, timeType.kind,
                        timeType.kind == TypeKind::Duration ? "duration" : "scalar");
        }
        return source;
    }
    if (isBaseCurveCall(name)) {
        if (!inCurveDeclaration_) {
            errorAt(expr.span, "type-curve-context",
                    "curve constructors are only valid in a curve declaration");
        }
        return kErrorType;
    }

    errorAt(expr.span, "type-unknown-operator", "unknown operator '" + name + "'", "operator",
            name);
    return kErrorType;
}

Type Sema::checkCurveLiteral(ExprId id, TimeDomain domain, const Rational& extent) {
    const Expr& expr = module_.expr(id);
    if (expr.kind == ExprKind::Keyframes) {
        return checkCurveKeyframes(id, domain, extent);
    }
    if (expr.kind != ExprKind::Call) {
        errorAt(expr.span, "type-mismatch", "curve declaration requires a curve literal",
                "constant/linear/step/smoothstep/keyframes", std::string(exprKindName(expr.kind)));
        return kErrorType;
    }

    const std::string& name = expr.text;
    if (name == "keyframes") {
        return kErrorType;
    }
    const std::size_t required = name == "constant" ? 1 : (name == "step" ? 3 : 2);
    if (expr.args.size() != required) {
        errorAt(expr.span, "type-arity",
                "curve constructor '" + name + "' expects " + std::to_string(required) +
                    " argument(s)");
        return kErrorType;
    }
    if (name != "constant" && name != "linear" && name != "step" && name != "smoothstep") {
        errorAt(expr.span, "type-mismatch", "unknown curve constructor '" + name + "'",
                "curve constructor", name);
        return kErrorType;
    }

    for (std::size_t index = 0; index < required; ++index) {
        const bool isTime = name == "step" && index == 2;
        const Type expectedType =
            isTime ? (domain == TimeDomain::Musical ? kDurationType : kScalarType) : kScalarType;
        const Type argumentType = checkExpr(expr.args[index], expectedType);
        if (!argumentType.isError()) {
            requireKind(module_.expr(expr.args[index]), argumentType, expectedType.kind,
                        expectedType.kind == TypeKind::Duration ? "duration" : "scalar");
        }
    }

    if (name == "step") {
        const Constant time = hir_.constantOf(expr.args[2]);
        if (time.kind == Constant::Kind::Number) {
            errorAt(module_.expr(expr.args[2]).span, "type-domain",
                    "step switch time must be an exact rational value");
        } else if (time.kind == Constant::Kind::Rational) {
            if (time.rational.isNegative() || time.rational > extent) {
                errorAt(module_.expr(expr.args[2]).span, "type-domain",
                        "step switch time is outside the curve extent");
            }
        }
    }

    hir_.exprTypes[id] = Type::curve(domain);
    return Type::curve(domain);
}

Type Sema::checkCurveKeyframes(ExprId id, TimeDomain domain, const Rational& extent) {
    const Expr& expr = module_.expr(id);
    if (expr.keyframes.empty()) {
        errorAt(expr.span, "type-duration", "keyframes curve needs at least one keyframe");
        return kErrorType;
    }

    Rational previous;
    bool first = true;
    for (const KeyframeEntry& entry : expr.keyframes) {
        const Type expected = domain == TimeDomain::Musical ? kDurationType : kScalarType;
        const Type type = checkExpr(entry.time, expected);
        if (!type.isError() &&
            !requireKind(module_.expr(entry.time), type, expected.kind,
                         expected.kind == TypeKind::Duration ? "duration" : "scalar")) {
            return kErrorType;
        }
        const Constant time = hir_.constantOf(entry.time);
        if (time.kind == Constant::Kind::Number) {
            errorAt(module_.expr(entry.time).span, "type-domain",
                    "keyframe times must be exact rational values");
            return kErrorType;
        }
        const Rational value = time.rational;
        if (value.isNegative() || value > extent) {
            errorAt(module_.expr(entry.time).span, "type-domain",
                    "keyframe time is outside the curve extent");
        }
        if (!first && !(previous < value)) {
            errorAt(module_.expr(entry.time).span, "type-domain",
                    "keyframe times must be strictly increasing");
        }
        previous = value;
        first = false;

        if (!std::isfinite(entry.value)) {
            errorAt(entry.span, "type-number", "keyframe value must be finite");
        }
        if (entry.interpolation != "step" && entry.interpolation != "linear" &&
            entry.interpolation != "smoothstep") {
            errorAt(entry.span, "type-domain",
                    "unknown interpolation '" + entry.interpolation + "'",
                    "step, linear or smoothstep", entry.interpolation);
        }
    }
    hir_.exprTypes[id] = Type::curve(domain);
    return Type::curve(domain);
}

bool Sema::curveBounds(ExprId id, double& minimum, double& maximum) const {
    if (id == kInvalidExprId || id >= module_.expressions.size()) {
        return false;
    }
    const Expr& expr = module_.expr(id);
    if (expr.kind == ExprKind::Identifier) {
        const SymbolId symbol = hir_.symbolOf(id);
        if (symbol == kInvalidSymbolId) {
            return false;
        }
        const Symbol& resolved = hir_.symbols.get(symbol);
        if (resolved.kind == SymbolKind::Curve) {
            return curveBounds(module_.curves[resolved.declarationIndex].value, minimum, maximum);
        }
        if (resolved.kind == SymbolKind::Let) {
            return curveBounds(module_.lets[resolved.declarationIndex].value, minimum, maximum);
        }
        return false;
    }
    if (expr.kind == ExprKind::Keyframes) {
        if (expr.keyframes.empty()) {
            return false;
        }
        minimum = maximum = expr.keyframes.front().value;
        for (const KeyframeEntry& entry : expr.keyframes) {
            minimum = std::min(minimum, entry.value);
            maximum = std::max(maximum, entry.value);
        }
        return true;
    }
    if (expr.kind == ExprKind::UnaryMinus && !expr.args.empty()) {
        double innerMin = 0.0;
        double innerMax = 0.0;
        if (!curveBounds(expr.args.front(), innerMin, innerMax)) {
            return false;
        }
        minimum = -innerMax;
        maximum = -innerMin;
        return true;
    }
    if (expr.kind != ExprKind::Call) {
        return false;
    }
    if (expr.text == "constant" && expr.args.size() == 1) {
        const Constant value = hir_.constantOf(expr.args[0]);
        if (value.kind == Constant::Kind::None) {
            return false;
        }
        minimum = maximum = value.number;
        return true;
    }
    if ((expr.text == "linear" || expr.text == "smoothstep") && expr.args.size() == 2) {
        const Constant from = hir_.constantOf(expr.args[0]);
        const Constant to = hir_.constantOf(expr.args[1]);
        if (from.kind == Constant::Kind::None || to.kind == Constant::Kind::None) {
            return false;
        }
        minimum = std::min(from.number, to.number);
        maximum = std::max(from.number, to.number);
        return true;
    }
    if (expr.text == "step" && expr.args.size() == 3) {
        const Constant from = hir_.constantOf(expr.args[0]);
        const Constant to = hir_.constantOf(expr.args[1]);
        if (from.kind == Constant::Kind::None || to.kind == Constant::Kind::None) {
            return false;
        }
        minimum = std::min(from.number, to.number);
        maximum = std::max(from.number, to.number);
        return true;
    }
    if ((expr.text == "shift" || expr.text == "reverse") && !expr.args.empty()) {
        return curveBounds(expr.args[0], minimum, maximum);
    }
    if ((expr.text == "stretch" || expr.text == "repeat") && !expr.args.empty()) {
        return curveBounds(expr.args[0], minimum, maximum);
    }
    return false;
}

bool Sema::curveValuesInRange(ExprId id, double minimum, double maximum) const {
    double curveMin = 0.0;
    double curveMax = 0.0;
    if (!curveBounds(id, curveMin, curveMax)) {
        return true;
    }
    return curveMin >= minimum && curveMax <= maximum;
}

std::optional<Rational> Sema::curveExtentOf(ExprId id) const {
    if (id == kInvalidExprId || id >= module_.expressions.size()) {
        return std::nullopt;
    }
    const Expr& expr = module_.expr(id);
    if (expr.kind == ExprKind::Identifier) {
        const SymbolId symbol = hir_.symbolOf(id);
        if (symbol == kInvalidSymbolId) {
            return std::nullopt;
        }
        const Symbol& resolved = hir_.symbols.get(symbol);
        if (resolved.kind == SymbolKind::Curve) {
            return hir_.symbolCurveExtents[symbol];
        }
        if (resolved.kind == SymbolKind::Let) {
            return curveExtentOf(module_.lets[resolved.declarationIndex].value);
        }
        return std::nullopt;
    }
    if (expr.kind != ExprKind::Call || expr.args.empty()) {
        return std::nullopt;
    }
    if (expr.text == "shift" || expr.text == "reverse") {
        return curveExtentOf(expr.args[0]);
    }
    if (expr.text == "stretch" && expr.args.size() == 2) {
        const Rational extent = curveExtentOf(expr.args[0]).value_or(Rational(1, 1));
        const Constant factor = hir_.constantOf(expr.args[1]);
        if (factor.kind == Constant::Kind::None) {
            return std::nullopt;
        }
        return extent * factor.rational;
    }
    if (expr.text == "repeat" && expr.args.size() == 2) {
        const Rational extent = curveExtentOf(expr.args[0]).value_or(Rational(1, 1));
        const Constant count = hir_.constantOf(expr.args[1]);
        if (count.kind == Constant::Kind::None) {
            return std::nullopt;
        }
        return extent * Rational(count.integer, 1);
    }
    return std::nullopt;
}

Rational Sema::durationConstant(ExprId id) const {
    return hir_.constantOf(id).rational;
}

bool Sema::isPositiveDuration(ExprId id) const {
    return durationConstant(id).isPositive();
}

}  // namespace mpl
