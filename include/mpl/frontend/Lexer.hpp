#pragma once

#include "mpl/frontend/Diagnostic.hpp"
#include "mpl/frontend/Token.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace mpl {

class Lexer {
public:
    Lexer(std::string_view source, std::string file, DiagnosticSink& diagnostics);

    std::vector<Token> lex();

private:
    bool atEnd() const noexcept { return offset_ >= source_.size(); }
    char peek(std::size_t lookahead = 0) const noexcept;
    char advance();
    bool match(char expected);
    void skipTrivia();
    void skipLineComment();
    void skipBlockComment();
    Token makeToken(TokenKind kind, std::size_t startOffset, int startLine, int startColumn,
                    std::string text) const;
    Token identifier(std::size_t startOffset, int startLine, int startColumn);
    Token number(std::size_t startOffset, int startLine, int startColumn);
    Token stringLiteral(std::size_t startOffset, int startLine, int startColumn);

    std::string_view source_;
    std::string file_;
    DiagnosticSink& diagnostics_;
    std::size_t offset_ = 0;
    int line_ = 1;
    int column_ = 1;
};

}  // namespace mpl
