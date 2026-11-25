#include "mpl/frontend/Sema.hpp"

#include "mpl/core/Error.hpp"
#include "mpl/frontend/ParameterRegistry.hpp"
#include "mpl/ir/MixIR.hpp"
#include "mpl/music/Key.hpp"
#include "mpl/music/Pitch.hpp"
#include "mpl/music/Rhythm.hpp"
#include "mpl/music/Chord.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <set>
#include <string>
#include <utility>

namespace mpl {
namespace {

const Type kErrorType = Type::simple(TypeKind::Error);
const Type kScalarType = Type::simple(TypeKind::Scalar);
const Type kNumberType = Type::simple(TypeKind::Number);
const Type kIntegerType = Type::simple(TypeKind::Integer);
const Type kBooleanType = Type::simple(TypeKind::Boolean);
const Type kPitchType = Type::simple(TypeKind::Pitch);
const Type kDurationType = Type::simple(TypeKind::Duration);
const Type kPositionType = Type::simple(TypeKind::Position);
const Type kIntervalType = Type::simple(TypeKind::Interval);
const Type kScaleDegreeType = Type::simple(TypeKind::ScaleDegree);
const Type kTempoType = Type::simple(TypeKind::Tempo);
const Type kFrequencyType = Type::simple(TypeKind::Frequency);
const Type kGainType = Type::simple(TypeKind::Gain);
const Type kPhysicalTimeType = Type::simple(TypeKind::PhysicalTime);
const Type kFragmentType = Type::simple(TypeKind::Fragment);
const Type kRhythmType = Type::simple(TypeKind::Rhythm);
const Type kChordType = Type::simple(TypeKind::Chord);
const Type kHarmonyType = Type::simple(TypeKind::Harmony);
const Type kVoicingType = Type::simple(TypeKind::Voicing);
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

void hashMix(std::uint64_t& hash, std::uint64_t value) {
    hash ^= value + 0x9e3779b97f4a7c15ull + (hash << 6) + (hash >> 2);
}

void hashText(std::uint64_t& hash, std::string_view text) {
    for (const char character : text) {
        hash ^= static_cast<unsigned char>(character);
        hash *= 1099511628211ull;
    }
    hashMix(hash, text.size());
}

std::uint64_t hashExpr(const Module& module, ExprId id, int depth = 0) {
    if (id == kInvalidExprId || depth > 64 || id >= module.expressions.size()) {
        return 0;
    }
    const Expr& expr = module.expr(id);
    std::uint64_t hash = 1469598103934665603ull;
    hashMix(hash, static_cast<std::uint64_t>(expr.kind));
    hashText(hash, expr.text);
    hashText(hash, expr.unit);
    hashMix(hash, static_cast<std::uint64_t>(expr.integer));
    hashMix(hash, static_cast<std::uint64_t>(expr.numerator));
    hashMix(hash, static_cast<std::uint64_t>(expr.denominator));
    hashMix(hash, static_cast<std::uint64_t>(expr.number * 1000003.0));
    hashMix(hash, expr.boolean ? 1 : 0);
    for (const ExprId argument : expr.args) {
        hashMix(hash, hashExpr(module, argument, depth + 1));
    }
    for (const MelodyEntry& entry : expr.melody) {
        hashText(hash, entry.pitch);
        hashMix(hash, hashExpr(module, entry.duration, depth + 1));
    }
    for (const NoteEntry& entry : expr.notes) {
        hashText(hash, entry.pitch);
        hashMix(hash, hashExpr(module, entry.offset, depth + 1));
        hashMix(hash, hashExpr(module, entry.duration, depth + 1));
        hashMix(hash, static_cast<std::uint64_t>(entry.velocity));
    }
    for (const KeyframeEntry& entry : expr.keyframes) {
        hashMix(hash, hashExpr(module, entry.time, depth + 1));
        hashMix(hash, static_cast<std::uint64_t>(entry.value * 1000003.0));
        hashText(hash, entry.interpolation);
    }
    return hash;
}

std::uint64_t functionVersion(const Module& module, const FunctionDecl& decl) {
    std::uint64_t hash = 1469598103934665603ull;
    hashText(hash, decl.name);
    hashText(hash, decl.returnType);
    for (const ParamDecl& param : decl.params) {
        hashText(hash, param.name);
        hashText(hash, param.type);
    }
    hashMix(hash, decl.contextKey ? 1 : 0);
    hashMix(hash, decl.contextMeter ? 1 : 0);
    for (const Stmt& statement : decl.body) {
        hashMix(hash, static_cast<std::uint64_t>(statement.kind));
        hashText(hash, statement.name);
        hashMix(hash, hashExpr(module, statement.value));
    }
    return hash;
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
    collectFunctionSignatures();

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

    for (std::size_t index = 0; index < hir_.functions.size(); ++index) {
        checkFunctionBody(hir_.functions[index], module_.functions[index]);
    }
    checkRecursion();

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

void Sema::pushScope() { localScopes_.emplace_back(); }

void Sema::popScope() {
    if (!localScopes_.empty()) {
        localScopes_.pop_back();
    }
}

std::optional<Type> Sema::lookupLocal(const std::string& name) const {
    for (auto scope = localScopes_.rbegin(); scope != localScopes_.rend(); ++scope) {
        const auto iterator = scope->find(name);
        if (iterator != scope->end()) {
            return iterator->second;
        }
    }
    return std::nullopt;
}

bool Sema::declareLocal(const std::string& name, Type type, const SourceSpan& span) {
    if (localScopes_.empty()) {
        pushScope();
    }
    auto& scope = localScopes_.back();
    if (scope.find(name) != scope.end()) {
        errorAt(span, "sema-duplicate-local", "duplicate local binding '" + name + "'");
        return false;
    }
    scope[name] = type;
    return true;
}

bool Sema::isNumeric(TypeKind kind) const {
    return kind == TypeKind::Integer || kind == TypeKind::Number ||
           kind == TypeKind::Scalar || kind == TypeKind::ScaleDegree;
}

Type Sema::parseTypeName(const std::string& name, const SourceSpan& span) {
    if (name.size() >= 2 && name.front() == '[' && name.back() == ']') {
        const Type element = parseTypeName(name.substr(1, name.size() - 2), span);
        return Type::list(element.kind);
    }
    if (name == "Fragment") return Type::simple(TypeKind::Fragment);
    if (name == "Rhythm") return Type::simple(TypeKind::Rhythm);
    if (name == "Chord") return Type::simple(TypeKind::Chord);
    if (name == "Harmony") return Type::simple(TypeKind::Harmony);
    if (name == "Voicing") return Type::simple(TypeKind::Voicing);
    if (name == "Curve") return Type::curve(TimeDomain::Relative);
    if (name == "Duration") return Type::simple(TypeKind::Duration);
    if (name == "Position") return Type::simple(TypeKind::Position);
    if (name == "Pitch") return Type::simple(TypeKind::Pitch);
    if (name == "Int") return Type::simple(TypeKind::Integer);
    if (name == "Number") return Type::simple(TypeKind::Number);
    if (name == "Scalar") return Type::simple(TypeKind::Scalar);
    if (name == "Bool") return Type::simple(TypeKind::Boolean);
    if (name == "String") return Type::simple(TypeKind::String);
    if (name == "Key") return Type::simple(TypeKind::Key);
    errorAt(span, "type-name", "unknown type '" + name + "'",
            "Fragment, Rhythm, Chord, Harmony, Voicing, Curve, Duration, Position, Pitch, "
            "Int, Number, Scalar, Bool, String, Key or [T]",
            name);
    return kErrorType;
}

bool Sema::assignable(const Type& expected, const Type& actual, const SourceSpan& span,
                      std::string_view context) {
    if (expected.isError() || actual.isError()) {
        return true;
    }
    if (expected.kind == TypeKind::List && actual.kind == TypeKind::List) {
        if (expected.element == TypeKind::Error || actual.element == TypeKind::Error) {
            return true;
        }
        if (expected.element == actual.element ||
            (isNumeric(expected.element) && isNumeric(actual.element))) {
            return true;
        }
    } else if (expected.kind == actual.kind) {
        return true;
    } else if (isNumeric(expected.kind) && isNumeric(actual.kind)) {
        return true;
    }
    errorAt(span, "type-mismatch",
            std::string(context) + " expects " + typeName(expected), typeName(expected),
            typeName(actual));
    return false;
}

void Sema::requireContext(bool key, bool meter, const SourceSpan& span,
                          std::string_view operation) {
    if (currentFunction_ == nullptr) {
        return;
    }
    if (key && !currentFunction_->contextKey) {
        errorAt(span, "context-not-declared",
                std::string(operation) + " requires 'context key' on function '" +
                    currentFunction_->name + "'",
                "context key", "none", currentFunction_->name);
    }
    if (meter && !currentFunction_->contextMeter) {
        errorAt(span, "context-not-declared",
                std::string(operation) + " requires 'context meter' on function '" +
                    currentFunction_->name + "'",
                "context meter", "none", currentFunction_->name);
    }
}

void Sema::collectFunctionSignatures() {
    for (std::size_t index = 0; index < module_.functions.size(); ++index) {
        const FunctionDecl& decl = module_.functions[index];
        if (hir_.functionIndex.find(decl.name) != hir_.functionIndex.end()) {
            errorAt(decl.span, "sema-duplicate-symbol",
                    "duplicate function declaration '" + decl.name + "'", "unique name",
                    decl.name);
            continue;
        }
        FunctionInfo info;
        info.name = decl.name;
        info.contextKey = decl.contextKey;
        info.contextMeter = decl.contextMeter;
        info.declarationIndex = index;
        info.version = functionVersion(module_, decl);
        info.span = decl.span;
        std::set<std::string> parameterNames;
        for (const ParamDecl& param : decl.params) {
            if (!parameterNames.insert(param.name).second) {
                errorAt(param.span, "sema-duplicate-parameter",
                        "duplicate parameter '" + param.name + "'");
            }
            FunctionParamInfo parameter;
            parameter.name = param.name;
            parameter.type = parseTypeName(param.type, param.span);
            info.params.push_back(std::move(parameter));
        }
        info.returnType = parseTypeName(decl.returnType, decl.span);
        hir_.functionIndex[decl.name] = hir_.functions.size();
        hir_.functions.push_back(std::move(info));
        addSymbol(decl.name, SymbolKind::Function, decl.span, index,
                  Type::simple(TypeKind::Function));
    }
}

void Sema::checkFunctionBody(const FunctionInfo& info, const FunctionDecl& decl) {
    if (decl.body.empty() || decl.body.back().kind != StmtKind::Return) {
        errorAt(decl.span, "sema-function-return",
                "function '" + decl.name + "' must end with a return statement");
    }
    currentFunction_ = &info;
    pushScope();
    for (std::size_t index = 0; index < info.params.size(); ++index) {
        const SourceSpan annSpan =
            index < decl.params.size() ? decl.params[index].span : decl.span;
        declareLocal(info.params[index].name, info.params[index].type, annSpan);
    }
    for (std::size_t index = 0; index < decl.body.size(); ++index) {
        const Stmt& statement = decl.body[index];
        if (statement.kind == StmtKind::Let) {
            const Type type = checkExpr(statement.value);
            if (!type.isError()) {
                const bool valueType =
                    type.kind == TypeKind::Fragment || type.kind == TypeKind::Rhythm ||
                    type.kind == TypeKind::Chord || type.kind == TypeKind::Harmony ||
                    type.kind == TypeKind::Voicing || type.kind == TypeKind::Curve ||
                    type.kind == TypeKind::Duration || type.kind == TypeKind::Position ||
                    type.kind == TypeKind::Integer || type.kind == TypeKind::Number ||
                    type.kind == TypeKind::Scalar || type.kind == TypeKind::Boolean ||
                    type.kind == TypeKind::Pitch || type.kind == TypeKind::String ||
                    type.kind == TypeKind::List;
                if (!valueType) {
                    errorAt(statement.span, "sema-local-type",
                            "local binding '" + statement.name +
                                "' has a non-value type",
                            "fragment, curve, scalar, pitch or collection",
                            typeName(type));
                } else {
                    declareLocal(statement.name, type, statement.span);
                }
            }
        } else {
            if (index + 1 != decl.body.size()) {
                errorAt(statement.span, "sema-return-not-last",
                        "return must be the last statement of a function");
            }
            const Type actual = checkExpr(statement.value, info.returnType);
            assignable(info.returnType, actual, module_.expr(statement.value).span,
                       "return value of '" + info.name + "'");
        }
    }
    popScope();
    currentFunction_ = nullptr;
}

void Sema::checkRecursion() {
    std::map<std::string, int> state;
    std::vector<std::string> stack;
    std::function<bool(const std::string&)> visit = [&](const std::string& name) {
        state[name] = 1;
        stack.push_back(name);
        for (const std::string& next : callGraph_[name]) {
            if (state[next] == 1) {
                const FunctionInfo* function = hir_.findFunction(name);
                errorAt(function != nullptr ? function->span : module_.span,
                        "sema-recursion-unsupported",
                        "recursion is not supported in V0.3.0-alpha: '" + name + "' -> '" +
                            next + "'",
                        "acyclic function calls", next, name);
                stack.pop_back();
                state[name] = 2;
                return true;
            }
            if (state[next] == 0 && visit(next)) {
                stack.pop_back();
                state[name] = 2;
                return true;
            }
        }
        stack.pop_back();
        state[name] = 2;
        return false;
    };
    for (const FunctionInfo& function : hir_.functions) {
        if (state[function.name] == 0) {
            (void)visit(function.name);
        }
    }
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
    if (!type.isError()) {
        const bool valueType =
            type.kind == TypeKind::Fragment || type.kind == TypeKind::Rhythm ||
            type.kind == TypeKind::Chord || type.kind == TypeKind::Harmony ||
            type.kind == TypeKind::Voicing || type.kind == TypeKind::Curve ||
            type.kind == TypeKind::Duration || type.kind == TypeKind::Position ||
            type.kind == TypeKind::Integer || type.kind == TypeKind::Number ||
            type.kind == TypeKind::Scalar || type.kind == TypeKind::Boolean ||
            type.kind == TypeKind::Pitch || type.kind == TypeKind::String ||
            type.kind == TypeKind::List;
        if (!valueType) {
            errorAt(decl.span, "type-mismatch",
                    "let bindings must hold a music value, curve or scalar",
                    "fragment, curve, scalar or collection", typeName(type), decl.name);
        }
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

void Sema::checkStripLevels(ExprId gain, ExprId pan, const std::string& owner) {
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
}

void Sema::checkProcessor(const ProcessorDecl& decl) {
    ProcessorKind kind = ProcessorKind::Gain;
    const bool known = processorKindFromName(decl.kind, kind);
    if (!known) {
        errorAt(decl.span, "type-processor",
                "unknown processor kind '" + decl.kind + "'",
                "gain, pan, biquad, compressor, delay, reverb or limiter", decl.kind);
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
    checkStripLevels(decl.gain, decl.pan, decl.name);
    addSymbol(decl.name, SymbolKind::Bus, decl.span,
              static_cast<std::size_t>(&decl - module_.buses.data()),
              Type::simple(TypeKind::Bus));
}

void Sema::checkMaster(const MasterDecl& decl) {
    for (const std::string& insert : decl.inserts) {
        checkInsert(insert, decl.span, "master");
    }
    checkStripLevels(decl.gain, decl.pan, "master");
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
    checkStripLevels(decl.gain, decl.pan, decl.name);
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

bool Sema::checkMeterSpec(const MeterSpec& spec, const std::string& owner) {
    bool ok = true;
    if (spec.numerator < 1 || spec.denominator < 1 ||
        !isPowerOfTwo(spec.denominator)) {
        errorAt(spec.span, "type-meter",
                "meter of '" + owner +
                    "' must have a positive numerator and a power-of-two denominator");
        ok = false;
    }
    if (spec.explicitGrouping) {
        int total = 0;
        for (const int group : spec.grouping) {
            if (group < 1) {
                errorAt(spec.span, "sema-meter-grouping",
                        "metric grouping components must be positive");
                ok = false;
                break;
            }
            total += group;
        }
        if (ok && total != spec.numerator) {
            errorAt(spec.span, "sema-meter-grouping",
                    "metric grouping sums to " + std::to_string(total) + ", expected " +
                        std::to_string(spec.numerator),
                    std::to_string(spec.numerator), std::to_string(total));
            ok = false;
        }
    }
    return ok;
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

    if (decl.meter.has_value()) {
        checkMeterSpec(*decl.meter, decl.name);
    }
    for (const MeterChange& change : decl.meterChanges) {
        checkMeterSpec(change.meter, decl.name);
        if (change.at == kInvalidExprId) {
            errorAt(change.span, "sema-missing-field",
                    "meter change in section '" + decl.name + "' has no position");
        } else {
            const Type atType = checkExpr(change.at, kDurationType);
            if (!atType.isError()) {
                if (atType.kind != TypeKind::Duration) {
                    errorAt(module_.expr(change.at).span, "type-mismatch",
                            "meter change position must be a duration", "duration",
                            typeName(atType));
                } else if (hir_.constantOf(change.at).rational.isNegative()) {
                    errorAt(module_.expr(change.at).span, "type-duration",
                            "meter change position must not be negative");
                }
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
                } else if (kind == ProcessorKind::Limiter &&
                           decl.parameter == "attack") {
                    errorAt(decl.span, "sema-processor-automation",
                            "limiter attack defines the processor lookahead latency and "
                            "cannot be automated",
                            "static attack", decl.parameter);
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
        case ExprKind::Boolean:
            type = kBooleanType;
            break;
        case ExprKind::Identifier:
            type = checkIdentifier(id, expr, expected);
            break;
        case ExprKind::Key:
            type = Type::simple(TypeKind::Key);
            break;
        case ExprKind::Binary:
            type = checkBinary(id, expr);
            break;
        case ExprKind::Not: {
            if (expr.args.empty()) {
                break;
            }
            const Type operand = checkExpr(expr.args.front(), kBooleanType);
            if (!operand.isError() && operand.kind != TypeKind::Boolean) {
                errorAt(expr.span, "type-mismatch", "'!' expects a boolean", "boolean",
                        typeName(operand));
                type = kErrorType;
            } else {
                type = kBooleanType;
            }
            break;
        }
        case ExprKind::If:
            type = checkIf(id, expr);
            break;
        case ExprKind::For:
            type = checkFor(id, expr);
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
                } else if (constant.kind == Constant::Kind::Metric) {
                    constant.metricAmount = -constant.metricAmount;
                    constant.rational = -constant.rational;
                    constant.number = -constant.number;
                }
                hir_.exprConstants[id] = constant;
            }
            break;
        }
        case ExprKind::List:
            type = checkList(expr);
            break;
        case ExprKind::Melody:
            type = checkMelody(expr);
            break;
        case ExprKind::Notes:
            type = checkNotes(expr);
            break;
        case ExprKind::Rhythm:
            type = checkRhythm(expr);
            break;
        case ExprKind::Harmony:
            type = checkHarmony(expr);
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
            const Rational amount = constant.rational;
            const Rational folded =
                expr.unit == "bars"
                    ? amount * hir_.meter.barLength()
                    : expr.unit == "beats" ? amount * hir_.meter.beatLength() : amount;
            hir_.exprConstants[id] =
                Constant::metricValue(amount,
                                      expr.unit == "bars"
                                          ? Constant::MetricUnit::Bars
                                          : expr.unit == "beats"
                                                ? Constant::MetricUnit::Beats
                                                : Constant::MetricUnit::None,
                                      folded);
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
            case TypeKind::Integer:
                if (expr.kind != ExprKind::Integer) {
                    errorAt(expr.span, "type-unit", "expected an integer value", "integer",
                            std::string(exprKindName(expr.kind)));
                    return kErrorType;
                }
                hir_.exprConstants[id] = constant;
                return kIntegerType;
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

Type Sema::checkIdentifier(ExprId id, const Expr& expr, std::optional<Type> expected) {
    if (const std::optional<Type> local = lookupLocal(expr.text); local.has_value()) {
        return *local;
    }
    const std::optional<SymbolId> symbol = findSymbol(expr.text);
    if (!symbol.has_value()) {
        if (expected.has_value() && expected->kind == TypeKind::Pitch) {
            try {
                (void)Pitch::parse(expr.text);
                return kPitchType;
            } catch (const Error&) {
            }
        }
        errorAt(expr.span, "sema-unknown-name", "unknown name '" + expr.text + "'");
        return kErrorType;
    }
    const Type type = hir_.symbolTypes[*symbol];
    hir_.exprSymbols[id] = *symbol;
    return type;
}

Type Sema::checkList(const Expr& expr, std::optional<Type> expected) {
    Type element = expected.value_or(Type{});
    bool elementKnown = expected.has_value() && !expected->isError();
    for (const ExprId item : expr.args) {
        const Type itemType =
            checkExpr(item, elementKnown ? std::optional<Type>(element) : std::nullopt);
        if (itemType.isError()) {
            continue;
        }
        if (elementKnown) {
            assignable(element, itemType, module_.expr(item).span, "list element");
        } else {
            element = itemType;
            elementKnown = true;
        }
    }
    if (!elementKnown) {
        element = kNumberType;
    }
    const Type type = Type::list(element.kind);
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

Type Sema::checkRhythm(const Expr& expr) {
    if (expr.rhythm.empty()) {
        errorAt(expr.span, "type-rhythm-empty", "rhythm literal needs at least one entry");
        return kRhythmType;
    }
    Rational contentEnd(0, 1);
    bool allAbsolute = true;
    for (const RhythmEntry& entry : expr.rhythm) {
        const Type duration = checkExpr(entry.duration, kDurationType);
        if (!duration.isError()) {
            if (duration.kind != TypeKind::Duration) {
                errorAt(module_.expr(entry.duration).span, "type-mismatch",
                        "rhythm entry duration must be a duration", "duration",
                        typeName(duration));
            } else if (!isPositiveDuration(entry.duration)) {
                errorAt(module_.expr(entry.duration).span, "type-duration",
                        "rhythm entry duration must be positive");
            }
            const Constant constant = hir_.constantOf(entry.duration);
            if (constant.kind == Constant::Kind::Metric) {
                allAbsolute = false;
            }
            contentEnd += durationConstant(entry.duration);
        }
        if (entry.accent < 0 || entry.accent > kMaxAccentLevel) {
            errorAt(entry.span, "sema-accent-range",
                    "rhythm accent must be in [0, " + std::to_string(kMaxAccentLevel) + "]");
        }
    }

    if (expr.explicitSpan != kInvalidExprId) {
        const Type span = checkExpr(expr.explicitSpan, kDurationType);
        if (!span.isError()) {
            if (span.kind != TypeKind::Duration) {
                errorAt(module_.expr(expr.explicitSpan).span, "type-mismatch",
                        "rhythm span must be a duration", "duration", typeName(span));
            } else if (!isPositiveDuration(expr.explicitSpan)) {
                errorAt(module_.expr(expr.explicitSpan).span, "type-duration",
                        "rhythm span must be positive");
            } else {
                const Constant spanConstant = hir_.constantOf(expr.explicitSpan);
                if (spanConstant.kind == Constant::Kind::Metric) {
                    allAbsolute = false;
                }
                if (allAbsolute && durationConstant(expr.explicitSpan) < contentEnd) {
                    errorAt(module_.expr(expr.explicitSpan).span, "type-duration",
                            "rhythm span is shorter than its content", contentEnd.toString(),
                            durationConstant(expr.explicitSpan).toString());
                }
            }
        }
    }
    return kRhythmType;
}

Type Sema::checkHarmony(const Expr& expr) {
    if (expr.harmony.empty()) {
        errorAt(expr.span, "type-harmony-empty", "harmony literal needs at least one entry");
        return kHarmonyType;
    }
    Rational contentEnd(0, 1);
    bool allAbsolute = true;
    for (const HarmonyEntry& entry : expr.harmony) {
        try {
            (void)ChordSymbol::parse(entry.chord);
        } catch (const Error& chordError) {
            errorAt(entry.span, "type-chord", chordError.what());
        }
        const Type offset = checkExpr(entry.offset, kPositionType);
        if (!offset.isError() && offset.kind != TypeKind::Position &&
            offset.kind != TypeKind::Duration) {
            errorAt(module_.expr(entry.offset).span, "type-mismatch",
                    "harmony event start must be a position or duration", "position",
                    typeName(offset));
        }
        const Rational offsetValue = hir_.constantOf(entry.offset).rational;
        if (offsetValue.isNegative()) {
            errorAt(module_.expr(entry.offset).span, "type-duration",
                    "harmony event start must not be negative");
        }
        const Type duration = checkExpr(entry.duration, kDurationType);
        if (!duration.isError() && duration.kind != TypeKind::Duration) {
            errorAt(module_.expr(entry.duration).span, "type-mismatch",
                    "harmony event duration must be a duration", "duration",
                    typeName(duration));
        }
        const Rational durationValue = hir_.constantOf(entry.duration).rational;
        if (!durationValue.isPositive()) {
            errorAt(module_.expr(entry.duration).span, "type-duration",
                    "harmony event duration must be positive");
        }
        if (hir_.constantOf(entry.offset).kind == Constant::Kind::Metric ||
            hir_.constantOf(entry.duration).kind == Constant::Kind::Metric) {
            allAbsolute = false;
        }
        contentEnd = std::max(contentEnd, offsetValue + durationValue);
    }
    if (expr.explicitSpan != kInvalidExprId) {
        const Type span = checkExpr(expr.explicitSpan, kDurationType);
        if (!span.isError()) {
            if (span.kind != TypeKind::Duration) {
                errorAt(module_.expr(expr.explicitSpan).span, "type-mismatch",
                        "harmony span must be a duration", "duration", typeName(span));
            } else if (!isPositiveDuration(expr.explicitSpan)) {
                errorAt(module_.expr(expr.explicitSpan).span, "type-duration",
                        "harmony span must be positive");
            } else if (allAbsolute &&
                       durationConstant(expr.explicitSpan) < contentEnd) {
                errorAt(module_.expr(expr.explicitSpan).span, "type-duration",
                        "harmony span is shorter than its content", contentEnd.toString(),
                        durationConstant(expr.explicitSpan).toString());
            }
        }
    }
    return kHarmonyType;
}

Type Sema::checkBinary(ExprId id, const Expr& expr) {
    if (expr.args.size() != 2) {
        return kErrorType;
    }
    const std::string& op = expr.text;
    const ExprId leftId = expr.args[0];
    const ExprId rightId = expr.args[1];
    if (op == "&&" || op == "||") {
        const Type left = checkExpr(leftId, kBooleanType);
        const Type right = checkExpr(rightId, kBooleanType);
        if (!left.isError()) {
            assignable(kBooleanType, left, module_.expr(leftId).span, "logical operand");
        }
        if (!right.isError()) {
            assignable(kBooleanType, right, module_.expr(rightId).span, "logical operand");
        }
        return kBooleanType;
    }

    const bool comparison = op == "==" || op == "!=" || op == "<" || op == "<=" ||
                            op == ">" || op == ">=";
    if (comparison) {
        const Type left = checkExpr(leftId);
        Type right;
        if (left.kind == TypeKind::Integer && module_.expr(rightId).kind == ExprKind::Integer) {
            right = checkExpr(rightId, kIntegerType);
        } else if (left.kind == TypeKind::Number &&
                   module_.expr(rightId).kind == ExprKind::Integer) {
            right = checkExpr(rightId, kNumberType);
        } else {
            right = checkExpr(rightId);
        }
        if (left.isError() || right.isError()) {
            return kBooleanType;
        }
        const bool numeric = isNumeric(left.kind) && isNumeric(right.kind);
        const bool same = left.kind == right.kind &&
                          (left.kind == TypeKind::Duration || left.kind == TypeKind::Position ||
                           left.kind == TypeKind::Interval || left.kind == TypeKind::Pitch ||
                           left.kind == TypeKind::String || left.kind == TypeKind::Boolean ||
                           left.kind == TypeKind::ScaleDegree);
        if (!numeric && !same) {
            errorAt(expr.span, "type-mismatch",
                    "cannot compare " + typeName(left) + " with " + typeName(right));
        }
        return kBooleanType;
    }

    const Type left = checkExpr(leftId);
    Type right;
    if (left.kind == TypeKind::Integer && module_.expr(rightId).kind == ExprKind::Integer) {
        right = checkExpr(rightId, kIntegerType);
    } else if (isNumeric(left.kind) && module_.expr(rightId).kind == ExprKind::Integer) {
        right = checkExpr(rightId, kNumberType);
    } else {
        right = checkExpr(rightId);
    }
    if (left.isError() || right.isError()) {
        return kErrorType;
    }
    if (!isNumeric(left.kind) || !isNumeric(right.kind)) {
        errorAt(expr.span, "type-mismatch",
                "arithmetic operator '" + op + "' expects numeric operands",
                "number", typeName(left) + " and " + typeName(right));
        return kErrorType;
    }
    if (op == "%" && (left.kind != TypeKind::Integer || right.kind != TypeKind::Integer)) {
        errorAt(expr.span, "type-mismatch", "'%' expects integer operands", "integer",
                typeName(left) + " and " + typeName(right));
        return kErrorType;
    }
    if ((op == "/" || op == "%") && module_.expr(rightId).kind == ExprKind::Integer) {
        const Constant rightConstant = hir_.constantOf(rightId);
        if (rightConstant.kind == Constant::Kind::Integer && rightConstant.integer == 0) {
            errorAt(module_.expr(rightId).span, "sema-division-by-zero",
                    "division or modulo by zero");
            return kErrorType;
        }
    }
    if (left.kind == TypeKind::Integer && right.kind == TypeKind::Integer) {
        return kIntegerType;
    }
    return kNumberType;
}

Type Sema::checkIf(ExprId, const Expr& expr) {
    if (expr.args.size() != 3) {
        return kErrorType;
    }
    const ExprId conditionId = expr.args[0];
    const Type condition = checkExpr(conditionId, kBooleanType);
    if (!condition.isError()) {
        assignable(kBooleanType, condition, module_.expr(conditionId).span, "if condition");
    }
    const Type thenType = checkExpr(expr.args[1]);
    const Type elseType = checkExpr(expr.args[2]);
    if (thenType.isError()) {
        return elseType;
    }
    if (elseType.isError()) {
        return thenType;
    }
    if (thenType.kind == elseType.kind) {
        if (thenType.kind == TypeKind::List) {
            const Type combined = isNumeric(thenType.element) && isNumeric(elseType.element)
                                      ? Type::list(TypeKind::Number)
                                      : thenType;
            return combined;
        }
        return thenType;
    }
    if (isNumeric(thenType.kind) && isNumeric(elseType.kind)) {
        return (thenType.kind == TypeKind::Integer && elseType.kind == TypeKind::Integer)
                   ? kIntegerType
                   : kNumberType;
    }
    errorAt(expr.span, "type-mismatch",
            "if branches have different types: " + typeName(thenType) + " and " +
                typeName(elseType),
            typeName(thenType), typeName(elseType));
    return kErrorType;
}

Type Sema::checkFor(ExprId, const Expr& expr) {
    if (expr.args.size() != 2) {
        return kErrorType;
    }
    const Type iterableType = checkExpr(expr.args[0]);
    if (iterableType.isError()) {
        return kErrorType;
    }
    if (iterableType.kind != TypeKind::List) {
        errorAt(expr.span, "type-mismatch", "for iteration requires a collection", "list",
                typeName(iterableType));
        return kErrorType;
    }
    const SourceSpan bodySpan = module_.expr(expr.args[1]).span;
    pushScope();
    declareLocal(expr.text, Type::simple(iterableType.element), expr.span);
    const Type bodyType = checkExpr(expr.args[1]);
    popScope();
    (void)bodySpan;
    if (bodyType.isError()) {
        return kErrorType;
    }
    return Type::list(bodyType.kind);
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

    if (const FunctionInfo* function = hir_.findFunction(name)) {
        if (expr.args.size() != function->params.size()) {
            errorAt(expr.span, "type-arity",
                    "function '" + name + "' expects " +
                        std::to_string(function->params.size()) + " argument(s), got " +
                        std::to_string(expr.args.size()),
                    std::to_string(function->params.size()) + " arguments",
                    std::to_string(expr.args.size()) + " arguments", name);
        }
        const std::size_t count = std::min(expr.args.size(), function->params.size());
        for (std::size_t index = 0; index < count; ++index) {
            const Type actual = checkExpr(expr.args[index], function->params[index].type);
            assignable(function->params[index].type, actual,
                       module_.expr(expr.args[index]).span,
                       "argument '" + function->params[index].name + "' of '" + name + "'");
        }
        if (currentFunction_ != nullptr) {
            if (function->contextKey && !currentFunction_->contextKey) {
                errorAt(expr.span, "context-not-declared",
                        "calling '" + name + "' requires 'context key' on function '" +
                            currentFunction_->name + "'",
                        "context key", "none", currentFunction_->name);
            }
            if (function->contextMeter && !currentFunction_->contextMeter) {
                errorAt(expr.span, "context-not-declared",
                        "calling '" + name + "' requires 'context meter' on function '" +
                            currentFunction_->name + "'",
                        "context meter", "none", currentFunction_->name);
            }
            callGraph_[currentFunction_->name].insert(name);
        }
        return function->returnType;
    }

    if (name == "range") {
        if (!arity(2)) return kErrorType;
        const Type start = argument(0, kIntegerType);
        const Type count = argument(1, kIntegerType);
        if (!requireKind(module_.expr(expr.args[0]), start, TypeKind::Integer, "integer") ||
            !requireKind(module_.expr(expr.args[1]), count, TypeKind::Integer, "integer")) {
            return kErrorType;
        }
        const Constant countConstant = hir_.constantOf(expr.args[1]);
        if (countConstant.kind == Constant::Kind::Integer &&
            (countConstant.integer < 0 || countConstant.integer > 1024)) {
            errorAt(module_.expr(expr.args[1]).span, "sema-collection-limit",
                    "range count must be in [0, 1024]");
        }
        return Type::list(TypeKind::Integer);
    }
    if (name == "length") {
        if (!arity(1)) return kErrorType;
        const Type collection = argument(0);
        if (!collection.isError() && collection.kind != TypeKind::List) {
            errorAt(expr.span, "type-mismatch", "length expects a collection", "list",
                    typeName(collection));
        }
        return kIntegerType;
    }
    if (name == "round") {
        if (!arity(1)) return kErrorType;
        const Type value = argument(0);
        if (!value.isError() && !isNumeric(value.kind)) {
            errorAt(expr.span, "type-mismatch", "round expects a number", "number",
                    typeName(value));
        }
        return kIntegerType;
    }
    if (name == "note") {
        if (!arity(3)) return kErrorType;
        const Type pitch = argument(0, kPitchType);
        const Type duration = argument(1, kDurationType);
        const Type velocity = argument(2, kIntegerType);
        assignable(kPitchType, pitch, module_.expr(expr.args[0]).span, "note pitch");
        assignable(kDurationType, duration, module_.expr(expr.args[1]).span, "note duration");
        assignable(kIntegerType, velocity, module_.expr(expr.args[2]).span, "note velocity");
        return kFragmentType;
    }
    if (name == "rest") {
        if (!arity(1)) return kErrorType;
        const Type duration = argument(0, kDurationType);
        assignable(kDurationType, duration, module_.expr(expr.args[0]).span, "rest duration");
        return kFragmentType;
    }
    if (name == "rotate") {
        if (!arity(2)) return kErrorType;
        const Type source = argument(0);
        const Type amount = argument(1, kDurationType);
        assignable(kDurationType, amount, module_.expr(expr.args[1]).span, "rotate amount");
        if (source.kind == TypeKind::Rhythm) {
            return kRhythmType;
        }
        if (source.kind == TypeKind::Fragment) {
            return kFragmentType;
        }
        errorAt(expr.span, "type-mismatch", "rotate expects a fragment or rhythm",
                "fragment or rhythm", typeName(source));
        return kErrorType;
    }
    if (name == "sequenceAll" || name == "parallelAll") {
        if (!arity(1)) return kErrorType;
        const Type collection = argument(0);
        if (!collection.isError() &&
            (collection.kind != TypeKind::List || collection.element != TypeKind::Fragment)) {
            errorAt(expr.span, "type-mismatch",
                    name + " expects a collection of fragments", "list<fragment>",
                    typeName(collection));
        }
        return kFragmentType;
    }
    if (name == "ratio") {
        if (!arity(2)) return kErrorType;
        const Type numerator = argument(0, kIntegerType);
        const Type denominator = argument(1, kIntegerType);
        assignable(kIntegerType, numerator, module_.expr(expr.args[0]).span, "ratio numerator");
        assignable(kIntegerType, denominator, module_.expr(expr.args[1]).span,
                   "ratio denominator");
        const Constant denominatorConstant = hir_.constantOf(expr.args[1]);
        if (denominatorConstant.kind == Constant::Kind::Integer &&
            denominatorConstant.integer == 0) {
            errorAt(module_.expr(expr.args[1]).span, "sema-division-by-zero",
                    "ratio denominator must not be zero");
        }
        return kDurationType;
    }
    if (name == "random") {
        if (!arity(2)) return kErrorType;
        const Type seed = argument(0, kIntegerType);
        const Type index = argument(1, kIntegerType);
        assignable(kIntegerType, seed, module_.expr(expr.args[0]).span, "random seed");
        assignable(kIntegerType, index, module_.expr(expr.args[1]).span, "random index");
        return kNumberType;
    }

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
        if (source.kind == TypeKind::Fragment || source.kind == TypeKind::Curve ||
            source.kind == TypeKind::Rhythm) {
            return source;
        }
        errorAt(expr.span, "type-mismatch", "stretch expects a fragment, curve or rhythm",
                "fragment, curve or rhythm", typeName(source));
        return kErrorType;
    }
    if (name == "repeat") {
        if (!arity(2)) return kErrorType;
        const Type source = argument(0);
        const Type count = argument(1, kIntegerType);
        if (!requireKind(module_.expr(expr.args[1]), count, TypeKind::Integer, "integer")) {
            return kErrorType;
        }
        const Constant countConstant = hir_.constantOf(expr.args[1]);
        if (countConstant.kind != Constant::Kind::None && countConstant.integer < 1) {
            errorAt(module_.expr(expr.args[1]).span, "type-duration",
                    "repeat count must be at least 1");
        }
        if (source.kind == TypeKind::Fragment || source.kind == TypeKind::Curve ||
            source.kind == TypeKind::Rhythm) {
            return source;
        }
        errorAt(expr.span, "type-mismatch", "repeat expects a fragment, curve or rhythm",
                "fragment, curve or rhythm", typeName(source));
        return kErrorType;
    }
    if (name == "sequence" || name == "parallel") {
        if (expr.args.empty()) {
            errorAt(expr.span, "type-arity", "operator '" + name + "' needs at least one argument");
            return kErrorType;
        }
        const Type first = checkExpr(expr.args.front());
        if (first.isError()) {
            return kErrorType;
        }
        const bool rhythmMode = first.kind == TypeKind::Rhythm;
        for (const ExprId argumentId : expr.args) {
            const Type type = checkExpr(argumentId, rhythmMode ? kRhythmType : kFragmentType);
            if (!requireKind(module_.expr(argumentId), type,
                             rhythmMode ? TypeKind::Rhythm : TypeKind::Fragment,
                             rhythmMode ? "rhythm" : "fragment")) {
                return kErrorType;
            }
        }
        return rhythmMode ? kRhythmType : kFragmentType;
    }
    if (name == "overlay") {
        if (expr.args.empty()) {
            errorAt(expr.span, "type-arity", "overlay needs at least one rhythm");
            return kErrorType;
        }
        for (const ExprId argumentId : expr.args) {
            const Type type = checkExpr(argumentId, kRhythmType);
            if (!requireKind(module_.expr(argumentId), type, TypeKind::Rhythm, "rhythm")) {
                return kErrorType;
            }
        }
        return kRhythmType;
    }
    if (name == "subdivide") {
        if (!arity(2)) return kErrorType;
        const Type source = argument(0, kRhythmType);
        const Type parts = argument(1, kIntegerType);
        if (!requireKind(module_.expr(expr.args[0]), source, TypeKind::Rhythm, "rhythm") ||
            !requireKind(module_.expr(expr.args[1]), parts, TypeKind::Integer, "integer")) {
            return kErrorType;
        }
        const Constant partsConstant = hir_.constantOf(expr.args[1]);
        if (partsConstant.kind == Constant::Kind::Integer &&
            (partsConstant.integer < 1 || partsConstant.integer > kMaxRhythmSubdivision)) {
            errorAt(module_.expr(expr.args[1]).span, "sema-rhythm-limit",
                    "subdivide parts must be in [1, " +
                        std::to_string(kMaxRhythmSubdivision) + "]");
        }
        return kRhythmType;
    }
    if (name == "accent") {
        if (!arity(2)) return kErrorType;
        const Type source = argument(0, kRhythmType);
        if (!requireKind(module_.expr(expr.args[0]), source, TypeKind::Rhythm, "rhythm")) {
            return kErrorType;
        }
        const Type levels = module_.expr(expr.args[1]).kind == ExprKind::List
                                ? checkList(module_.expr(expr.args[1]), kIntegerType)
                                : checkExpr(expr.args[1], kIntegerType);
        if (!levels.isError() && levels.kind == TypeKind::Integer) {
            const Constant constant = hir_.constantOf(expr.args[1]);
            if (constant.kind == Constant::Kind::Integer &&
                (constant.integer < 0 || constant.integer > kMaxAccentLevel)) {
                errorAt(module_.expr(expr.args[1]).span, "sema-accent-range",
                        "accent level must be in [0, " + std::to_string(kMaxAccentLevel) + "]");
            }
            return kRhythmType;
        }
        if (!levels.isError() && levels.kind == TypeKind::List &&
            levels.element == TypeKind::Integer) {
            if (module_.expr(expr.args[1]).args.empty()) {
                errorAt(module_.expr(expr.args[1]).span, "type-arity",
                        "accent level list must not be empty");
            }
            for (const ExprId levelId : module_.expr(expr.args[1]).args) {
                const Constant constant = hir_.constantOf(levelId);
                if (constant.kind == Constant::Kind::Integer &&
                    (constant.integer < 0 || constant.integer > kMaxAccentLevel)) {
                    errorAt(module_.expr(levelId).span, "sema-accent-range",
                            "accent level must be in [0, " +
                                std::to_string(kMaxAccentLevel) + "]");
                }
            }
            return kRhythmType;
        }
        errorAt(expr.span, "type-mismatch", "accent expects a level or a list of levels",
                "integer or list<integer>", typeName(levels));
        return kErrorType;
    }
    if (name == "realign") {
        if (!arity(2)) return kErrorType;
        const Type source = argument(0, kRhythmType);
        const Type period = argument(1, kDurationType);
        if (!requireKind(module_.expr(expr.args[0]), source, TypeKind::Rhythm, "rhythm") ||
            !requireKind(module_.expr(expr.args[1]), period, TypeKind::Duration, "duration")) {
            return kErrorType;
        }
        const Constant constant = hir_.constantOf(expr.args[1]);
        if (constant.kind != Constant::Kind::None && !constant.rational.isPositive()) {
            errorAt(module_.expr(expr.args[1]).span, "type-duration",
                    "realign period must be positive");
        }
        return kRhythmType;
    }
    if (name == "period") {
        if (!arity(1)) return kErrorType;
        const Type source = argument(0, kRhythmType);
        if (!requireKind(module_.expr(expr.args[0]), source, TypeKind::Rhythm, "rhythm")) {
            return kErrorType;
        }
        return kDurationType;
    }
    if (name == "commonPeriod") {
        if (!arity(2)) return kErrorType;
        for (std::size_t index = 0; index < 2; ++index) {
            const Type source = argument(index, kRhythmType);
            if (!requireKind(module_.expr(expr.args[index]), source, TypeKind::Rhythm,
                             "rhythm")) {
                return kErrorType;
            }
        }
        return kDurationType;
    }
    if (name == "quantize") {
        if (!arity(2)) return kErrorType;
        const Type source = argument(0, kRhythmType);
        const Type grid = argument(1, kDurationType);
        if (!requireKind(module_.expr(expr.args[0]), source, TypeKind::Rhythm, "rhythm") ||
            !requireKind(module_.expr(expr.args[1]), grid, TypeKind::Duration, "duration")) {
            return kErrorType;
        }
        const Constant constant = hir_.constantOf(expr.args[1]);
        if (constant.kind != Constant::Kind::None && !constant.rational.isPositive()) {
            errorAt(module_.expr(expr.args[1]).span, "type-duration",
                    "quantize grid must be positive");
        }
        return kRhythmType;
    }
    if (name == "onsetSet") {
        if (!arity(2)) return kErrorType;
        const Type source = argument(0, kRhythmType);
        const Type unit = argument(1, kDurationType);
        if (!requireKind(module_.expr(expr.args[0]), source, TypeKind::Rhythm, "rhythm") ||
            !requireKind(module_.expr(expr.args[1]), unit, TypeKind::Duration, "duration")) {
            return kErrorType;
        }
        const Constant constant = hir_.constantOf(expr.args[1]);
        if (constant.kind != Constant::Kind::None && !constant.rational.isPositive()) {
            errorAt(module_.expr(expr.args[1]).span, "type-duration",
                    "onset-set unit must be positive");
        }
        return kRhythmType;
    }
    if (name == "retrigger") {
        if (!arity(1)) return kErrorType;
        const Type source = argument(0, kRhythmType);
        if (!requireKind(module_.expr(expr.args[0]), source, TypeKind::Rhythm, "rhythm")) {
            return kErrorType;
        }
        return kRhythmType;
    }
    if (name == "metricAccent") {
        if (!arity(1)) return kErrorType;
        requireContext(false, true, expr.span, "metricAccent");
        const Type source = argument(0, kRhythmType);
        if (!requireKind(module_.expr(expr.args[0]), source, TypeKind::Rhythm, "rhythm")) {
            return kErrorType;
        }
        return kRhythmType;
    }
    if (name == "metricWeight") {
        if (!arity(1)) return kErrorType;
        requireContext(false, true, expr.span, "metricWeight");
        const Type offset = argument(0, kDurationType);
        if (!requireKind(module_.expr(expr.args[0]), offset, TypeKind::Duration, "duration")) {
            return kErrorType;
        }
        return kIntegerType;
    }
    if (name == "zip" || name == "cycle") {
        if (!arity(3)) return kErrorType;
        const Type rhythmType = argument(0, kRhythmType);
        const Type pitchesType = module_.expr(expr.args[1]).kind == ExprKind::List
                                     ? checkList(module_.expr(expr.args[1]), kPitchType)
                                     : checkExpr(expr.args[1], Type::list(TypeKind::Pitch));
        const Type velocity = argument(2, kIntegerType);
        if (!requireKind(module_.expr(expr.args[0]), rhythmType, TypeKind::Rhythm, "rhythm") ||
            !requireKind(module_.expr(expr.args[2]), velocity, TypeKind::Integer, "integer")) {
            return kErrorType;
        }
        if (!pitchesType.isError()) {
            if (pitchesType.kind != TypeKind::List ||
                pitchesType.element != TypeKind::Pitch) {
                errorAt(module_.expr(expr.args[1]).span, "type-mismatch",
                        name + " expects a list of pitches", "list<pitch>",
                        typeName(pitchesType));
            } else if (module_.expr(expr.args[1]).kind == ExprKind::List &&
                       module_.expr(expr.args[1]).args.empty()) {
                errorAt(module_.expr(expr.args[1]).span, "type-arity",
                        name + " requires at least one pitch");
            }
        }
        const Constant velocityConstant = hir_.constantOf(expr.args[2]);
        if (velocityConstant.kind == Constant::Kind::Integer &&
            (velocityConstant.integer < 1 || velocityConstant.integer > 127)) {
            errorAt(module_.expr(expr.args[2]).span, "sema-velocity",
                    "base velocity must be in 1..127");
        }
        return kFragmentType;
    }
    if (name == "intersect" || name == "union" || name == "difference") {
        if (expr.args.size() < 2 || expr.args.size() > 3) {
            errorAt(expr.span, "type-arity",
                    name + " expects 2 or 3 arguments, got " +
                        std::to_string(expr.args.size()));
            return kErrorType;
        }
        for (std::size_t index = 0; index < 2; ++index) {
            const Type source = argument(index, kRhythmType);
            if (!requireKind(module_.expr(expr.args[index]), source, TypeKind::Rhythm,
                             "rhythm")) {
                return kErrorType;
            }
        }
        if (expr.args.size() == 3) {
            const Type grid = argument(2, kDurationType);
            if (!requireKind(module_.expr(expr.args[2]), grid, TypeKind::Duration,
                             "duration")) {
                return kErrorType;
            }
            const Constant constant = hir_.constantOf(expr.args[2]);
            if (constant.kind != Constant::Kind::None && !constant.rational.isPositive()) {
                errorAt(module_.expr(expr.args[2]).span, "type-duration",
                        "relation grid must be positive");
            }
        }
        return kRhythmType;
    }
    if (name == "complement") {
        if (!arity(3)) return kErrorType;
        const Type source = argument(0, kRhythmType);
        const Type span = argument(1, kDurationType);
        const Type grid = argument(2, kDurationType);
        if (!requireKind(module_.expr(expr.args[0]), source, TypeKind::Rhythm, "rhythm") ||
            !requireKind(module_.expr(expr.args[1]), span, TypeKind::Duration, "duration") ||
            !requireKind(module_.expr(expr.args[2]), grid, TypeKind::Duration, "duration")) {
            return kErrorType;
        }
        const Constant spanConstant = hir_.constantOf(expr.args[1]);
        if (spanConstant.kind != Constant::Kind::None && !spanConstant.rational.isPositive()) {
            errorAt(module_.expr(expr.args[1]).span, "type-duration",
                    "complement span must be positive");
        }
        const Constant gridConstant = hir_.constantOf(expr.args[2]);
        if (gridConstant.kind != Constant::Kind::None && !gridConstant.rational.isPositive()) {
            errorAt(module_.expr(expr.args[2]).span, "type-duration",
                    "complement grid must be positive");
        }
        return kRhythmType;
    }
    if (name == "accentFollow") {
        if (expr.args.size() < 2 || expr.args.size() > 3) {
            errorAt(expr.span, "type-arity",
                    "accentFollow expects 2 or 3 arguments, got " +
                        std::to_string(expr.args.size()));
            return kErrorType;
        }
        for (std::size_t index = 0; index < 2; ++index) {
            const Type source = argument(index, kRhythmType);
            if (!requireKind(module_.expr(expr.args[index]), source, TypeKind::Rhythm,
                             "rhythm")) {
                return kErrorType;
            }
        }
        if (expr.args.size() == 3) {
            const Type grid = argument(2, kDurationType);
            if (!requireKind(module_.expr(expr.args[2]), grid, TypeKind::Duration,
                             "duration")) {
                return kErrorType;
            }
        }
        return kRhythmType;
    }
    if (name == "chord") {
        if (!arity(1)) return kErrorType;
        const Type text = argument(0, kStringType);
        if (!requireKind(module_.expr(expr.args[0]), text, TypeKind::String, "string")) {
            return kErrorType;
        }
        if (module_.expr(expr.args[0]).kind == ExprKind::String) {
            try {
                (void)ChordSymbol::parse(module_.expr(expr.args[0]).text);
            } catch (const Error& error) {
                errorAt(expr.span, "type-chord", error.what());
            }
        }
        return kChordType;
    }
    if (name == "chordAt") {
        if (!arity(2)) return kErrorType;
        const Type harmony = argument(0, kHarmonyType);
        const Type at = argument(1, kDurationType);
        if (!requireKind(module_.expr(expr.args[0]), harmony, TypeKind::Harmony, "harmony") ||
            !requireKind(module_.expr(expr.args[1]), at, TypeKind::Duration, "duration")) {
            return kErrorType;
        }
        return kChordType;
    }
    if (name == "chordAtBar") {
        if (!arity(2)) return kErrorType;
        requireContext(false, true, expr.span, "chordAtBar");
        const Type harmony = argument(0, kHarmonyType);
        const Type bar = argument(1, kIntegerType);
        if (!requireKind(module_.expr(expr.args[0]), harmony, TypeKind::Harmony, "harmony") ||
            !requireKind(module_.expr(expr.args[1]), bar, TypeKind::Integer, "integer")) {
            return kErrorType;
        }
        return kChordType;
    }
    if (name == "harmonyLength") {
        if (!arity(1)) return kErrorType;
        const Type harmony = argument(0, kHarmonyType);
        if (!requireKind(module_.expr(expr.args[0]), harmony, TypeKind::Harmony, "harmony")) {
            return kErrorType;
        }
        return kDurationType;
    }
    if (name == "harmonyOverlaps") {
        if (!arity(2)) return kErrorType;
        const Type harmony = argument(0, kHarmonyType);
        const Type at = argument(1, kDurationType);
        if (!requireKind(module_.expr(expr.args[0]), harmony, TypeKind::Harmony, "harmony") ||
            !requireKind(module_.expr(expr.args[1]), at, TypeKind::Duration, "duration")) {
            return kErrorType;
        }
        return kIntegerType;
    }
    if (name == "chordName") {
        if (!arity(1)) return kErrorType;
        const Type chord = argument(0, kChordType);
        if (!requireKind(module_.expr(expr.args[0]), chord, TypeKind::Chord, "chord")) {
            return kErrorType;
        }
        return kStringType;
    }
    if (name == "chordPitchClasses") {
        if (!arity(1)) return kErrorType;
        const Type chord = argument(0, kChordType);
        if (!requireKind(module_.expr(expr.args[0]), chord, TypeKind::Chord, "chord")) {
            return kErrorType;
        }
        return Type::list(TypeKind::Integer);
    }
    if (name == "chordContains" || name == "chordTone") {
        if (!arity(2)) return kErrorType;
        const Type chord = argument(0, kChordType);
        const Type pitch = argument(1, kPitchType);
        if (!requireKind(module_.expr(expr.args[0]), chord, TypeKind::Chord, "chord") ||
            !requireKind(module_.expr(expr.args[1]), pitch, TypeKind::Pitch, "pitch")) {
            return kErrorType;
        }
        return kBooleanType;
    }
    if (name == "voicing") {
        if (!arity(6)) return kErrorType;
        const Type chord = argument(0, kChordType);
        const Type low = argument(1, kPitchType);
        const Type high = argument(2, kPitchType);
        const Type voices = argument(3, kIntegerType);
        const Type inversion = argument(4, kIntegerType);
        const Type spacing = argument(5, kIntegerType);
        for (std::size_t index = 0; index < 6; ++index) {
            const TypeKind expected = index == 0   ? TypeKind::Chord
                                      : index <= 2 ? TypeKind::Pitch
                                                   : TypeKind::Integer;
            if (!requireKind(module_.expr(expr.args[index]),
                             checkExpr(expr.args[index], Type::simple(expected)), expected,
                             expected == TypeKind::Chord
                                 ? "chord"
                                 : expected == TypeKind::Pitch ? "pitch" : "integer")) {
                return kErrorType;
            }
        }
        return kVoicingType;
    }
    if (name == "voiceLead") {
        if (!arity(7)) return kErrorType;
        const Type chord = argument(1, kChordType);
        const Type low = argument(2, kPitchType);
        const Type high = argument(3, kPitchType);
        const Type maxMove = argument(4, kIntegerType);
        const Type spacing = argument(5, kIntegerType);
        const Type crossing = argument(6, kBooleanType);
        (void)maxMove;
        (void)spacing;
        const Type previous = module_.expr(expr.args[0]).kind == ExprKind::List
                                  ? checkList(module_.expr(expr.args[0]), kPitchType)
                                  : checkExpr(expr.args[0], Type::list(TypeKind::Pitch));
        if (!previous.isError() &&
            (previous.kind != TypeKind::List || previous.element != TypeKind::Pitch)) {
            errorAt(expr.span, "type-mismatch",
                    "voiceLead previous voices must be a list of pitches", "list<pitch>",
                    typeName(previous));
        }
        if (!requireKind(module_.expr(expr.args[1]), chord, TypeKind::Chord, "chord") ||
            !requireKind(module_.expr(expr.args[2]), low, TypeKind::Pitch, "pitch") ||
            !requireKind(module_.expr(expr.args[3]), high, TypeKind::Pitch, "pitch") ||
            !requireKind(module_.expr(expr.args[6]), crossing, TypeKind::Boolean, "boolean")) {
            return kErrorType;
        }
        return kVoicingType;
    }
    if (name == "voicingNotes") {
        if (!arity(1)) return kErrorType;
        const Type voicing = argument(0, kVoicingType);
        if (!requireKind(module_.expr(expr.args[0]), voicing, TypeKind::Voicing, "voicing")) {
            return kErrorType;
        }
        return Type::list(TypeKind::Pitch);
    }
    if (name == "voicingStatus") {
        if (!arity(1)) return kErrorType;
        const Type voicing = argument(0, kVoicingType);
        if (!requireKind(module_.expr(expr.args[0]), voicing, TypeKind::Voicing, "voicing")) {
            return kErrorType;
        }
        return kStringType;
    }
    if (name == "voicingMovement" || name == "voicingCrossings") {
        if (!arity(1)) return kErrorType;
        const Type voicing = argument(0, kVoicingType);
        if (!requireKind(module_.expr(expr.args[0]), voicing, TypeKind::Voicing, "voicing")) {
            return kErrorType;
        }
        return kIntegerType;
    }
    if (name == "voicingComplete") {
        if (!arity(1)) return kErrorType;
        const Type voicing = argument(0, kVoicingType);
        if (!requireKind(module_.expr(expr.args[0]), voicing, TypeKind::Voicing, "voicing")) {
            return kErrorType;
        }
        return kBooleanType;
    }
    if (name == "voicingSearchComplete") {
        if (!arity(1)) return kErrorType;
        const Type voicing = argument(0, kVoicingType);
        if (!requireKind(module_.expr(expr.args[0]), voicing, TypeKind::Voicing, "voicing")) {
            return kErrorType;
        }
        return kBooleanType;
    }
    if (name == "voiceDistance") {
        if (!arity(2)) return kErrorType;
        for (std::size_t index = 0; index < 2; ++index) {
            const Type list = module_.expr(expr.args[index]).kind == ExprKind::List
                                  ? checkList(module_.expr(expr.args[index]), kPitchType)
                                  : checkExpr(expr.args[index], Type::list(TypeKind::Pitch));
            if (!list.isError() &&
                (list.kind != TypeKind::List || list.element != TypeKind::Pitch)) {
                errorAt(expr.span, "type-mismatch",
                        "voiceDistance expects lists of pitches", "list<pitch>",
                        typeName(list));
            }
        }
        return kIntegerType;
    }
    if (name == "commonTones") {
        if (!arity(2)) return kErrorType;
        for (std::size_t index = 0; index < 2; ++index) {
            const Type list = module_.expr(expr.args[index]).kind == ExprKind::List
                                  ? checkList(module_.expr(expr.args[index]), kPitchType)
                                  : checkExpr(expr.args[index], Type::list(TypeKind::Pitch));
            if (!list.isError() &&
                (list.kind != TypeKind::List || list.element != TypeKind::Pitch)) {
                errorAt(expr.span, "type-mismatch", "commonTones expects lists of pitches",
                        "list<pitch>", typeName(list));
            }
        }
        return Type::list(TypeKind::Pitch);
    }
    if (name == "interval") {
        if (!arity(2)) return kErrorType;
        const Type first = argument(0, kPitchType);
        const Type second = argument(1, kPitchType);
        if (!requireKind(module_.expr(expr.args[0]), first, TypeKind::Pitch, "pitch") ||
            !requireKind(module_.expr(expr.args[1]), second, TypeKind::Pitch, "pitch")) {
            return kErrorType;
        }
        return kIntegerType;
    }
    if (name == "pitchClass") {
        if (!arity(1)) return kErrorType;
        const Type pitch = argument(0, kPitchType);
        if (!requireKind(module_.expr(expr.args[0]), pitch, TypeKind::Pitch, "pitch")) {
            return kErrorType;
        }
        return kIntegerType;
    }
    if (name == "pitchRange") {
        if (!arity(1)) return kErrorType;
        const Type list = module_.expr(expr.args[0]).kind == ExprKind::List
                              ? checkList(module_.expr(expr.args[0]), kPitchType)
                              : checkExpr(expr.args[0], Type::list(TypeKind::Pitch));
        if (!list.isError() &&
            (list.kind != TypeKind::List || list.element != TypeKind::Pitch)) {
            errorAt(expr.span, "type-mismatch", "pitchRange expects a list of pitches",
                    "list<pitch>", typeName(list));
        }
        return kIntegerType;
    }
    if (name == "noteOverlap") {
        if (!arity(2)) return kErrorType;
        for (std::size_t index = 0; index < 2; ++index) {
            const Type fragment = argument(index, kFragmentType);
            if (!requireKind(module_.expr(expr.args[index]), fragment, TypeKind::Fragment,
                             "fragment")) {
                return kErrorType;
            }
        }
        return kIntegerType;
    }
    if (name == "fragmentRange") {
        if (!arity(1)) return kErrorType;
        const Type fragment = argument(0, kFragmentType);
        if (!requireKind(module_.expr(expr.args[0]), fragment, TypeKind::Fragment,
                         "fragment")) {
            return kErrorType;
        }
        return kIntegerType;
    }
    if (name == "parallelPerfects") {
        if (!arity(2)) return kErrorType;
        for (std::size_t index = 0; index < 2; ++index) {
            const Type list = module_.expr(expr.args[index]).kind == ExprKind::List
                                  ? checkList(module_.expr(expr.args[index]), kPitchType)
                                  : checkExpr(expr.args[index], Type::list(TypeKind::Pitch));
            if (!list.isError() &&
                (list.kind != TypeKind::List || list.element != TypeKind::Pitch)) {
                errorAt(expr.span, "type-mismatch",
                        "parallelPerfects expects lists of pitches", "list<pitch>",
                        typeName(list));
            }
        }
        return kIntegerType;
    }
    if (name == "diatonicTranspose") {
        if (!arity(2)) return kErrorType;
        requireContext(true, false, expr.span, "diatonicTranspose");
        const Type source = argument(0, kFragmentType);
        const Type steps = argument(1, kIntegerType);
        if (!requireKind(module_.expr(expr.args[0]), source, TypeKind::Fragment, "fragment") ||
            !requireKind(module_.expr(expr.args[1]), steps, TypeKind::Integer,
                         "integer scale degree")) {
            return kErrorType;
        }
        return kFragmentType;
    }
    if (name == "harmonize") {
        if (!arity(2)) return kErrorType;
        requireContext(true, false, expr.span, "harmonize");
        const Type source = argument(0, kFragmentType);
        if (!requireKind(module_.expr(expr.args[0]), source, TypeKind::Fragment, "fragment")) {
            return kErrorType;
        }
        const Expr& listExpr = module_.expr(expr.args[1]);
        const Type listType = checkList(listExpr, kIntegerType);
        if (listExpr.args.empty()) {
            errorAt(listExpr.span, "type-arity", "harmonize needs at least one scale degree");
        }
        return listType.kind == TypeKind::List ? kFragmentType : kErrorType;
    }
    if (name == "syncopate") {
        if (!arity(2)) return kErrorType;
        requireContext(false, true, expr.span, "syncopate");
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
        if (source.kind == TypeKind::Rhythm) {
            const Type amount = argument(1, kDurationType);
            if (!requireKind(module_.expr(expr.args[1]), amount, TypeKind::Duration,
                             "duration")) {
                return kErrorType;
            }
            return kRhythmType;
        }
        if (source.kind != TypeKind::Curve) {
            errorAt(expr.span, "type-mismatch", "shift expects a curve or rhythm",
                    "curve or rhythm", typeName(source));
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
