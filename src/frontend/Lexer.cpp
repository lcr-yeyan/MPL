#include "mpl/frontend/Lexer.hpp"

#include <cctype>
#include <string>
#include <utility>

namespace mpl {

std::string_view tokenKindName(TokenKind kind) {
    switch (kind) {
        case TokenKind::End: return "end of file";
        case TokenKind::Identifier: return "identifier";
        case TokenKind::Integer: return "integer";
        case TokenKind::Rational: return "rational";
        case TokenKind::Number: return "number";
        case TokenKind::String: return "string";
        case TokenKind::LBrace: return "'{'";
        case TokenKind::RBrace: return "'}'";
        case TokenKind::LParen: return "'('";
        case TokenKind::RParen: return "')'";
        case TokenKind::LBracket: return "'['";
        case TokenKind::RBracket: return "']'";
        case TokenKind::Colon: return "':'";
        case TokenKind::Semicolon: return "';'";
        case TokenKind::Comma: return "','";
        case TokenKind::Dot: return "'.'";
        case TokenKind::Equal: return "'='";
        case TokenKind::At: return "'@'";
        case TokenKind::Minus: return "'-'";
        case TokenKind::Arrow: return "'->'";
        case TokenKind::Slash: return "'/'";
        case TokenKind::Plus: return "'+'";
        case TokenKind::Star: return "'*'";
        case TokenKind::Percent: return "'%'";
        case TokenKind::EqualEqual: return "'=='";
        case TokenKind::BangEqual: return "'!='";
        case TokenKind::Less: return "'<'";
        case TokenKind::LessEqual: return "'<='";
        case TokenKind::Greater: return "'>'";
        case TokenKind::GreaterEqual: return "'>='";
        case TokenKind::AmpAmp: return "'&&'";
        case TokenKind::PipePipe: return "'||'";
        case TokenKind::Bang: return "'!'";
        case TokenKind::Fn: return "'fn'";
        case TokenKind::Return: return "'return'";
        case TokenKind::If: return "'if'";
        case TokenKind::Else: return "'else'";
        case TokenKind::True: return "'true'";
        case TokenKind::False: return "'false'";
        case TokenKind::Project: return "'project'";
        case TokenKind::Module: return "'module'";
        case TokenKind::Import: return "'import'";
        case TokenKind::Tempo: return "'tempo'";
        case TokenKind::Meter: return "'meter'";
        case TokenKind::Key: return "'key'";
        case TokenKind::Asset: return "'asset'";
        case TokenKind::Instrument: return "'instrument'";
        case TokenKind::Sample: return "'sample'";
        case TokenKind::Let: return "'let'";
        case TokenKind::Curve: return "'curve'";
        case TokenKind::On: return "'on'";
        case TokenKind::For: return "'for'";
        case TokenKind::Automation: return "'automation'";
        case TokenKind::Track: return "'track'";
        case TokenKind::Section: return "'section'";
        case TokenKind::Clip: return "'clip'";
        case TokenKind::Play: return "'play'";
        case TokenKind::Arrangement: return "'arrangement'";
        case TokenKind::Export: return "'export'";
        case TokenKind::Midi: return "'midi'";
        case TokenKind::Master: return "'master'";
        case TokenKind::Processor: return "'processor'";
        case TokenKind::Bus: return "'bus'";
        case TokenKind::Insert: return "'insert'";
        case TokenKind::Send: return "'send'";
        case TokenKind::Sidechain: return "'sidechain'";
    }
    return "token";
}

Lexer::Lexer(std::string_view source, std::string file, DiagnosticSink& diagnostics)
    : source_(source), file_(std::move(file)), diagnostics_(diagnostics) {
    if (source_.size() >= 3 && static_cast<unsigned char>(source_[0]) == 0xEF &&
        static_cast<unsigned char>(source_[1]) == 0xBB &&
        static_cast<unsigned char>(source_[2]) == 0xBF) {
        offset_ = 3;  // UTF-8 BOM is not source text.
    }
}

char Lexer::peek(std::size_t lookahead) const noexcept {
    const std::size_t index = offset_ + lookahead;
    return index < source_.size() ? source_[index] : '\0';
}

char Lexer::advance() {
    const char character = source_[offset_++];
    if (character == '\n') {
        ++line_;
        column_ = 1;
    } else {
        ++column_;
    }
    return character;
}

bool Lexer::match(char expected) {
    if (peek() != expected) {
        return false;
    }
    advance();
    return true;
}

void Lexer::skipLineComment() {
    while (!atEnd() && peek() != '\n') {
        advance();
    }
}

void Lexer::skipBlockComment() {
    while (!atEnd()) {
        if (peek() == '*' && peek(1) == '/') {
            advance();
            advance();
            return;
        }
        advance();
    }
    diagnostics_.error("lex-unterminated-comment",
                       SourceSpan{file_, offset_, offset_, line_, column_, line_, column_},
                       "unterminated block comment");
}

void Lexer::skipTrivia() {
    while (!atEnd()) {
        const char character = peek();
        if (std::isspace(static_cast<unsigned char>(character)) != 0) {
            advance();
            continue;
        }
        if (character == '/' && peek(1) == '/') {
            advance();
            advance();
            skipLineComment();
            continue;
        }
        if (character == '/' && peek(1) == '*') {
            advance();
            advance();
            skipBlockComment();
            continue;
        }
        break;
    }
}

Token Lexer::makeToken(TokenKind kind, std::size_t startOffset, int startLine,
                       int startColumn, std::string text) const {
    Token token;
    token.kind = kind;
    token.text = std::move(text);
    token.span = SourceSpan{file_, startOffset, offset_, startLine, startColumn, line_, column_};
    return token;
}

Token Lexer::identifier(std::size_t startOffset, int startLine, int startColumn) {
    const std::size_t begin = offset_;
    while (!atEnd()) {
        const char character = peek();
        if (std::isalnum(static_cast<unsigned char>(character)) != 0 || character == '_' ||
            character == '#') {
            advance();
        } else {
            break;
        }
    }
    std::string text(source_.substr(begin, offset_ - begin));
    const TokenKind kind = [&text]() {
        if (text == "project") return TokenKind::Project;
        if (text == "module") return TokenKind::Module;
        if (text == "import") return TokenKind::Import;
        if (text == "tempo") return TokenKind::Tempo;
        if (text == "meter") return TokenKind::Meter;
        if (text == "key") return TokenKind::Key;
        if (text == "asset") return TokenKind::Asset;
        if (text == "instrument") return TokenKind::Instrument;
        if (text == "sample") return TokenKind::Sample;
        if (text == "let") return TokenKind::Let;
        if (text == "fn") return TokenKind::Fn;
        if (text == "return") return TokenKind::Return;
        if (text == "if") return TokenKind::If;
        if (text == "else") return TokenKind::Else;
        if (text == "true") return TokenKind::True;
        if (text == "false") return TokenKind::False;
        if (text == "curve") return TokenKind::Curve;
        if (text == "on") return TokenKind::On;
        if (text == "for") return TokenKind::For;
        if (text == "automation") return TokenKind::Automation;
        if (text == "track") return TokenKind::Track;
        if (text == "section") return TokenKind::Section;
        if (text == "clip") return TokenKind::Clip;
        if (text == "play") return TokenKind::Play;
        if (text == "arrangement") return TokenKind::Arrangement;
        if (text == "export") return TokenKind::Export;
        if (text == "midi") return TokenKind::Midi;
        if (text == "master") return TokenKind::Master;
        if (text == "processor") return TokenKind::Processor;
        if (text == "bus") return TokenKind::Bus;
        if (text == "insert") return TokenKind::Insert;
        if (text == "send") return TokenKind::Send;
        if (text == "sidechain") return TokenKind::Sidechain;
        return TokenKind::Identifier;
    }();
    return makeToken(kind, startOffset, startLine, startColumn, std::move(text));
}

Token Lexer::number(std::size_t startOffset, int startLine, int startColumn) {
    const std::size_t begin = offset_;
    while (std::isdigit(static_cast<unsigned char>(peek())) != 0) {
        advance();
    }

    bool rational = false;
    bool decimal = false;
    if (peek() == '/' && std::isdigit(static_cast<unsigned char>(peek(1))) != 0) {
        rational = true;
        advance();
        while (std::isdigit(static_cast<unsigned char>(peek())) != 0) {
            advance();
        }
    } else if (peek() == '.' && std::isdigit(static_cast<unsigned char>(peek(1))) != 0) {
        decimal = true;
        advance();
        while (std::isdigit(static_cast<unsigned char>(peek())) != 0) {
            advance();
        }
    }

    const std::string text(source_.substr(begin, offset_ - begin));
    Token token = makeToken(TokenKind::Integer, startOffset, startLine, startColumn, text);
    if (rational) {
        const std::size_t slash = text.find('/');
        const std::int64_t numerator = std::stoll(text.substr(0, slash));
        const std::int64_t denominator = std::stoll(text.substr(slash + 1));
        if (denominator == 0) {
            diagnostics_.error("lex-zero-denominator", token.span,
                               "rational literal has a zero denominator", "non-zero denominator",
                               "0");
        }
        token.kind = TokenKind::Rational;
        token.numerator = numerator;
        token.denominator = denominator;
        token.number = static_cast<double>(numerator) / static_cast<double>(denominator);
    } else if (decimal) {
        token.kind = TokenKind::Number;
        token.number = std::stod(text);
    } else {
        token.integer = std::stoll(text);
        token.number = static_cast<double>(token.integer);
    }
    return token;
}

Token Lexer::stringLiteral(std::size_t startOffset, int startLine, int startColumn) {
    advance();  // opening quote
    std::string text;
    while (!atEnd() && peek() != '"') {
        const char character = advance();
        if (character == '\\') {
            if (atEnd()) {
                break;
            }
            const char escaped = advance();
            switch (escaped) {
                case '"': text.push_back('"'); break;
                case '\\': text.push_back('\\'); break;
                case 'n': text.push_back('\n'); break;
                case 't': text.push_back('\t'); break;
                default:
                    text.push_back(escaped);
                    break;
            }
        } else {
            text.push_back(character);
        }
    }

    if (atEnd()) {
        Token token = makeToken(TokenKind::String, startOffset, startLine, startColumn, text);
        diagnostics_.error("lex-unterminated-string", token.span, "unterminated string literal");
        return token;
    }
    advance();  // closing quote
    return makeToken(TokenKind::String, startOffset, startLine, startColumn, std::move(text));
}

std::vector<Token> Lexer::lex() {
    std::vector<Token> tokens;
    while (true) {
        skipTrivia();
        if (atEnd()) {
            tokens.push_back(makeToken(TokenKind::End, offset_, line_, column_, {}));
            break;
        }

        const std::size_t startOffset = offset_;
        const int startLine = line_;
        const int startColumn = column_;
        const char character = peek();

        if (std::isalpha(static_cast<unsigned char>(character)) != 0 || character == '_') {
            tokens.push_back(identifier(startOffset, startLine, startColumn));
            continue;
        }
        if (std::isdigit(static_cast<unsigned char>(character)) != 0) {
            tokens.push_back(number(startOffset, startLine, startColumn));
            continue;
        }
        if (character == '"') {
            tokens.push_back(stringLiteral(startOffset, startLine, startColumn));
            continue;
        }

        advance();
        switch (character) {
            case '{': tokens.push_back(makeToken(TokenKind::LBrace, startOffset, startLine, startColumn, "{")); break;
            case '}': tokens.push_back(makeToken(TokenKind::RBrace, startOffset, startLine, startColumn, "}")); break;
            case '(': tokens.push_back(makeToken(TokenKind::LParen, startOffset, startLine, startColumn, "(")); break;
            case ')': tokens.push_back(makeToken(TokenKind::RParen, startOffset, startLine, startColumn, ")")); break;
            case '[': tokens.push_back(makeToken(TokenKind::LBracket, startOffset, startLine, startColumn, "[")); break;
            case ']': tokens.push_back(makeToken(TokenKind::RBracket, startOffset, startLine, startColumn, "]")); break;
            case ':': tokens.push_back(makeToken(TokenKind::Colon, startOffset, startLine, startColumn, ":")); break;
            case ';': tokens.push_back(makeToken(TokenKind::Semicolon, startOffset, startLine, startColumn, ";")); break;
            case ',': tokens.push_back(makeToken(TokenKind::Comma, startOffset, startLine, startColumn, ",")); break;
            case '.': tokens.push_back(makeToken(TokenKind::Dot, startOffset, startLine, startColumn, ".")); break;
            case '=':
                if (peek() == '=') {
                    advance();
                    tokens.push_back(makeToken(TokenKind::EqualEqual, startOffset, startLine,
                                               startColumn, "=="));
                } else {
                    tokens.push_back(makeToken(TokenKind::Equal, startOffset, startLine,
                                               startColumn, "="));
                }
                break;
            case '!':
                if (peek() == '=') {
                    advance();
                    tokens.push_back(makeToken(TokenKind::BangEqual, startOffset, startLine,
                                               startColumn, "!="));
                } else {
                    tokens.push_back(makeToken(TokenKind::Bang, startOffset, startLine,
                                               startColumn, "!"));
                }
                break;
            case '<':
                if (peek() == '=') {
                    advance();
                    tokens.push_back(makeToken(TokenKind::LessEqual, startOffset, startLine,
                                               startColumn, "<="));
                } else {
                    tokens.push_back(makeToken(TokenKind::Less, startOffset, startLine,
                                               startColumn, "<"));
                }
                break;
            case '>':
                if (peek() == '=') {
                    advance();
                    tokens.push_back(makeToken(TokenKind::GreaterEqual, startOffset, startLine,
                                               startColumn, ">="));
                } else {
                    tokens.push_back(makeToken(TokenKind::Greater, startOffset, startLine,
                                               startColumn, ">"));
                }
                break;
            case '&':
                if (peek() == '&') {
                    advance();
                    tokens.push_back(makeToken(TokenKind::AmpAmp, startOffset, startLine,
                                               startColumn, "&&"));
                } else {
                    Token token = makeToken(TokenKind::Identifier, startOffset, startLine,
                                            startColumn, "&");
                    diagnostics_.error("lex-unexpected-character", token.span,
                                       "unexpected character '&' (use '&&')");
                }
                break;
            case '|':
                if (peek() == '|') {
                    advance();
                    tokens.push_back(makeToken(TokenKind::PipePipe, startOffset, startLine,
                                               startColumn, "||"));
                } else {
                    Token token = makeToken(TokenKind::Identifier, startOffset, startLine,
                                            startColumn, "|");
                    diagnostics_.error("lex-unexpected-character", token.span,
                                       "unexpected character '|' (use '||')");
                }
                break;
            case '+': tokens.push_back(makeToken(TokenKind::Plus, startOffset, startLine, startColumn, "+")); break;
            case '*': tokens.push_back(makeToken(TokenKind::Star, startOffset, startLine, startColumn, "*")); break;
            case '%': tokens.push_back(makeToken(TokenKind::Percent, startOffset, startLine, startColumn, "%")); break;
            case '@': tokens.push_back(makeToken(TokenKind::At, startOffset, startLine, startColumn, "@")); break;
            case '-':
                if (peek() == '>') {
                    advance();
                    tokens.push_back(makeToken(TokenKind::Arrow, startOffset, startLine,
                                               startColumn, "->"));
                } else {
                    tokens.push_back(makeToken(TokenKind::Minus, startOffset, startLine,
                                               startColumn, "-"));
                }
                break;
            case '/': tokens.push_back(makeToken(TokenKind::Slash, startOffset, startLine, startColumn, "/")); break;
            default: {
                Token token = makeToken(TokenKind::Identifier, startOffset, startLine, startColumn,
                                        std::string(1, character));
                diagnostics_.error("lex-unexpected-character", token.span,
                                   std::string("unexpected character '") + character + "'");
                break;
            }
        }
    }
    return tokens;
}

}  // namespace mpl
