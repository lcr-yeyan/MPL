#pragma once

#include "mpl/frontend/Ast.hpp"
#include "mpl/frontend/Symbols.hpp"
#include "mpl/frontend/Types.hpp"
#include "mpl/music/Key.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace mpl {

struct FunctionParamInfo {
    std::string name;
    Type type;
};

struct FunctionInfo {
    std::string name;
    std::vector<FunctionParamInfo> params;
    Type returnType;
    bool contextKey = false;
    bool contextMeter = false;
    std::size_t declarationIndex = 0;
    std::uint64_t version = 0;
    SourceSpan span;
};

/// Lightweight typed HIR: the AST remains the tree, while these parallel
/// vectors carry resolved symbols, types and constants for every expression.
struct HirModule {
    std::vector<Type> exprTypes;
    std::vector<SymbolId> exprSymbols;
    std::vector<Constant> exprConstants;

    SymbolTable symbols;
    std::vector<Type> symbolTypes;
    std::vector<Rational> symbolCurveExtents;

    std::vector<FunctionInfo> functions;
    std::unordered_map<std::string, std::size_t> functionIndex;

    /// Module identity/dependency records produced by the frontend linker.
    std::vector<ModuleRecord> modules;

    const FunctionInfo* findFunction(const std::string& name) const {
        const auto iterator = functionIndex.find(name);
        return iterator == functionIndex.end() ? nullptr : &functions[iterator->second];
    }

    /// Section-major order, aligned with SectionDecl::clips.
    std::vector<SymbolId> clipTrackSymbols;
    /// Aligned with Module::automations; invalid for master targets.
    std::vector<SymbolId> automationTrackSymbols;
    /// Aligned with ArrangementDecl::plays.
    std::vector<SymbolId> arrangementSymbols;

    double tempo = 120.0;
    Meter meter{4, 4};
    Key key;

    Type typeOf(ExprId id) const {
        return id < exprTypes.size() ? exprTypes[id] : Type{};
    }
    Constant constantOf(ExprId id) const {
        return id < exprConstants.size() ? exprConstants[id] : Constant{};
    }
    SymbolId symbolOf(ExprId id) const {
        return id < exprSymbols.size() ? exprSymbols[id] : kInvalidSymbolId;
    }
};

}  // namespace mpl
