#pragma once

#include "mpl/frontend/Ast.hpp"
#include "mpl/frontend/Diagnostic.hpp"
#include "mpl/frontend/Hir.hpp"

#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace mpl {

class Sema {
public:
    Sema(const Module& module, DiagnosticSink& diagnostics);

    HirModule analyze();

private:
    Type checkExpr(ExprId id, std::optional<Type> expected = std::nullopt);
    Type checkNumeric(ExprId id, const Expr& expr, std::optional<Type> expected);
    Type checkIdentifier(ExprId id, const Expr& expr, std::optional<Type> expected);
    Type checkCall(const Expr& expr);
    Type checkBinary(ExprId id, const Expr& expr);
    Type checkIf(ExprId id, const Expr& expr);
    Type checkFor(ExprId id, const Expr& expr);
    Type checkMelody(const Expr& expr);
    Type checkNotes(const Expr& expr);
    Type checkRhythm(const Expr& expr);
    Type checkHarmony(const Expr& expr);
    Type checkList(const Expr& expr, std::optional<Type> expected = std::nullopt);
    Type checkIndex(const Expr& expr);
    Type checkCurveLiteral(ExprId id, TimeDomain domain, const Rational& extent);
    Type checkCurveKeyframes(ExprId id, TimeDomain domain, const Rational& extent);

    void checkProject();
    void checkAsset(const AssetDecl& decl);
    void checkLet(const LetDecl& decl);
    void checkCurve(const CurveDecl& decl);
    void checkProcessor(const ProcessorDecl& decl);
    void checkBus(const BusDecl& decl);
    void checkMaster(const MasterDecl& decl);
    void checkTrack(const TrackDecl& decl);
    void checkInsert(const std::string& insert, const SourceSpan& span, const std::string& owner);
    void checkStripLevels(ExprId gain, ExprId pan, const std::string& owner);
    void checkSection(const SectionDecl& decl);
    bool checkMeterSpec(const MeterSpec& spec, const std::string& owner);
    void checkAutomation(const AutomationDecl& decl);
    void checkArrangement(const ArrangementDecl& decl);
    void checkExport(const ExportDecl& decl);

    void collectFunctionSignatures();
    void checkFunctionBody(const FunctionInfo& info, const FunctionDecl& decl);
    void checkRecursion();
    Type parseTypeName(const std::string& name, const SourceSpan& span);
    bool assignable(const Type& expected, const Type& actual, const SourceSpan& span,
                    std::string_view context);
    bool isNumeric(TypeKind kind) const;
    std::optional<Type> lookupLocal(const std::string& name) const;
    bool declareLocal(const std::string& name, Type type, const SourceSpan& span);
    void pushScope();
    void popScope();
    void requireContext(bool key, bool meter, const SourceSpan& span, std::string_view operation);

    bool addSymbol(const std::string& name, SymbolKind kind, const SourceSpan& span,
                   std::size_t declarationIndex, Type type,
                   const Rational& curveExtent = Rational(1, 1));
    std::optional<SymbolId> findSymbol(const std::string& name) const;
    bool requireKind(const Expr& expr, const Type& type, TypeKind expected,
                     std::string_view expectation);
    bool curveValuesInRange(ExprId id, double minimum, double maximum) const;
    std::optional<Rational> curveExtentOf(ExprId id) const;
    bool curveBounds(ExprId id, double& minimum, double& maximum) const;
    Rational durationConstant(ExprId id) const;
    bool isPositiveDuration(ExprId id) const;
    void setType(ExprId id, Type type) { hir_.exprTypes[id] = type; }
    void setConstant(ExprId id, Constant constant) { hir_.exprConstants[id] = constant; }
    void errorAt(const SourceSpan& span, std::string code, std::string message,
                 std::string expected = {}, std::string actual = {}, std::string related = {});

    const Module& module_;
    DiagnosticSink& diagnostics_;
    HirModule hir_;
    bool inCurveDeclaration_ = false;
    std::map<std::string, int> replaceCounts_;
    std::vector<char> checked_;
    std::vector<std::map<std::string, Type>> localScopes_;
    const FunctionInfo* currentFunction_ = nullptr;
    std::map<std::string, std::set<std::string>> callGraph_;
};

}  // namespace mpl
