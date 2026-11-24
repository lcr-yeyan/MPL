#pragma once

#include "mpl/frontend/Ast.hpp"
#include "mpl/frontend/Diagnostic.hpp"
#include "mpl/frontend/Token.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace mpl {

class Parser {
public:
    Parser(std::vector<Token> tokens, DiagnosticSink& diagnostics);

    Module parse();

private:
    const Token& current() const;
    const Token& previous() const;
    bool check(TokenKind kind) const;
    bool checkIdentifier(std::string_view text) const;
    const Token& advance();
    bool match(TokenKind kind);
    Token expect(TokenKind kind, std::string code, std::string message);
    bool expectSemicolon();
    void errorAtCurrent(std::string code, std::string message, std::string expected = {},
                        std::string actual = {});
    void synchronizeStatement();
    void synchronizeDeclaration();
    bool isDeclarationStart(TokenKind kind) const;
    ExprId addExpr(Expr expr);
    ExprId invalidExpr(const SourceSpan& span);
    SourceSpan merge(const SourceSpan& begin, const SourceSpan& end) const;

    void parseProject(bool store);
    void parseAsset();
    void parseLet();
    void parseCurve();
    void parseProcessor();
    void parseBus();
    void parseMaster();
    void parseTrack();
    void parseSection();
    void parseClip(SectionDecl& section);
    void parseAutomation();
    void parseArrangement();
    void parseExport();

    ExprId parseExpr();
    ExprId parseUnary();
    ExprId parsePrimary();
    ExprId parseCall(const Token& callee);
    ExprId parseMelody(const Token& start);
    ExprId parseNotes(const Token& start);
    ExprId parseKeyframes(const Token& start);
    ExprId parseList(const Token& start);
    void parseOptionalUnit(Expr& expr);

    std::vector<Token> tokens_;
    DiagnosticSink& diagnostics_;
    std::size_t position_ = 0;
    Module module_;
    std::size_t projectDeclarationIndex_ = 0;
};

}  // namespace mpl
