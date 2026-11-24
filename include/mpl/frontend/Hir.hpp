#pragma once

#include "mpl/frontend/Ast.hpp"
#include "mpl/frontend/Symbols.hpp"
#include "mpl/frontend/Types.hpp"
#include "mpl/music/Key.hpp"

#include <string>
#include <vector>

namespace mpl {

/// Lightweight typed HIR: the AST remains the tree, while these parallel
/// vectors carry resolved symbols, types and constants for every expression.
struct HirModule {
    std::vector<Type> exprTypes;
    std::vector<SymbolId> exprSymbols;
    std::vector<Constant> exprConstants;

    SymbolTable symbols;
    std::vector<Type> symbolTypes;
    std::vector<Rational> symbolCurveExtents;

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
