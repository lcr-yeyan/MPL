#include "mpl/frontend/Parser.hpp"

#include <array>
#include <utility>

namespace mpl {
namespace {

bool isTimeUnit(std::string_view text) {
    return text == "bpm" || text == "st" || text == "bars" || text == "beats" ||
           text == "whole" || text == "Hz" || text == "dB" || text == "ms" || text == "s";
}

bool isExpressionField(std::string_view text) {
    return text == "velocity" || text == "timing" || text == "gate" || text == "pan";
}

bool isAutomationMode(std::string_view text) {
    return text == "replace" || text == "add" || text == "multiply";
}

}  // namespace

Parser::Parser(std::vector<Token> tokens, DiagnosticSink& diagnostics, bool requireProject)
    : tokens_(std::move(tokens)), diagnostics_(diagnostics), requireProject_(requireProject) {
    module_.file = tokens_.empty() ? std::string{} : tokens_.front().span.file;
    module_.isLibrary = !requireProject;
}

const Token& Parser::current() const {
    return tokens_[position_ < tokens_.size() ? position_ : tokens_.size() - 1];
}

const Token& Parser::previous() const {
    return tokens_[position_ == 0 ? 0 : position_ - 1];
}

bool Parser::check(TokenKind kind) const {
    return current().kind == kind;
}

bool Parser::checkIdentifier(std::string_view text) const {
    return current().kind == TokenKind::Identifier && current().text == text;
}

const Token& Parser::advance() {
    const Token& token = current();
    if (position_ + 1 < tokens_.size()) {
        ++position_;
    }
    return token;
}

bool Parser::match(TokenKind kind) {
    if (!check(kind)) {
        return false;
    }
    advance();
    return true;
}

Token Parser::expect(TokenKind kind, std::string code, std::string message) {
    if (check(kind)) {
        return advance();
    }
    errorAtCurrent(std::move(code), std::move(message), std::string(tokenKindName(kind)),
                   current().text.empty() ? std::string(tokenKindName(current().kind))
                                          : current().text);
    return current();
}

bool Parser::expectSemicolon() {
    if (match(TokenKind::Semicolon)) {
        return true;
    }
    errorAtCurrent("parse-expected-semicolon", "expected ';' after declaration or statement",
                   "';'", current().text.empty() ? std::string(tokenKindName(current().kind))
                                                 : current().text);
    synchronizeStatement();
    return false;
}

void Parser::errorAtCurrent(std::string code, std::string message, std::string expected,
                            std::string actual) {
    diagnostics_.error(std::move(code), current().span, std::move(message), std::move(expected),
                       actual.empty() && !current().text.empty() ? current().text
                                                                 : std::move(actual));
}

void Parser::synchronizeStatement() {
    while (!check(TokenKind::End) && !check(TokenKind::RBrace)) {
        if (check(TokenKind::Semicolon)) {
            advance();
            return;
        }
        advance();
    }
}

bool Parser::isDeclarationStart(TokenKind kind) const {
    switch (kind) {
        case TokenKind::Project:
        case TokenKind::Asset:
        case TokenKind::Let:
        case TokenKind::Curve:
        case TokenKind::Processor:
        case TokenKind::Bus:
        case TokenKind::Master:
        case TokenKind::Fn:
        case TokenKind::Track:
        case TokenKind::Section:
        case TokenKind::Automation:
        case TokenKind::Arrangement:
        case TokenKind::Export:
            return true;
        default:
            return false;
    }
}

void Parser::synchronizeDeclaration() {
    while (!check(TokenKind::End) && !isDeclarationStart(current().kind)) {
        advance();
    }
}

ExprId Parser::addExpr(Expr expr) {
    module_.expressions.push_back(std::move(expr));
    return static_cast<ExprId>(module_.expressions.size() - 1);
}

ExprId Parser::invalidExpr(const SourceSpan& span) {
    Expr expr;
    expr.kind = ExprKind::Invalid;
    expr.span = span;
    return addExpr(std::move(expr));
}

SourceSpan Parser::merge(const SourceSpan& begin, const SourceSpan& end) const {
    SourceSpan span = begin;
    span.end = end.end;
    span.endLine = end.endLine;
    span.endColumn = end.endColumn;
    return span;
}

Module Parser::parse() {
    module_.span = current().span;

    if (check(TokenKind::Module)) {
        advance();
        if (check(TokenKind::Identifier)) {
            module_.moduleName = advance().text;
        } else {
            errorAtCurrent("parse-expected-module-name", "expected a module name",
                           "identifier");
        }
        expectSemicolon();
    }
    while (check(TokenKind::Import)) {
        parseImport();
    }

    if (check(TokenKind::Project)) {
        parseProject(true);
    } else if (requireProject_) {
        errorAtCurrent("parse-expected-project", "a program must start with a project declaration",
                       "'project'", current().text);
    }

    while (!check(TokenKind::End)) {
        const std::size_t before = position_;
        const Token& token = current();
        switch (token.kind) {
            case TokenKind::Asset: parseAsset(); break;
            case TokenKind::Let: parseLet(); break;
            case TokenKind::Curve: parseCurve(); break;
            case TokenKind::Processor: parseProcessor(); break;
            case TokenKind::Bus: parseBus(); break;
            case TokenKind::Master: parseMaster(); break;
            case TokenKind::Fn: parseFunction(); break;
            case TokenKind::Track: parseTrack(); break;
            case TokenKind::Section: parseSection(); break;
            case TokenKind::Automation: parseAutomation(); break;
            case TokenKind::Arrangement: parseArrangement(); break;
            case TokenKind::Export: {
                const TokenKind next = position_ + 1 < tokens_.size()
                                           ? tokens_[position_ + 1].kind
                                           : TokenKind::End;
                if (next == TokenKind::Fn) {
                    advance();
                    parseFunction(true);
                } else if (next == TokenKind::Let) {
                    advance();
                    parseLet(true);
                } else if (next == TokenKind::Curve) {
                    advance();
                    parseCurve(true);
                } else {
                    parseExport();
                }
                break;
            }
            case TokenKind::Import:
                errorAtCurrent("parse-import-position",
                               "imports must appear before the project declaration");
                parseImport();
                break;
            case TokenKind::Module:
                errorAtCurrent("parse-module-position",
                               "the module declaration must be the first statement");
                advance();
                break;
            case TokenKind::Project:
                errorAtCurrent("parse-project-not-first", "project must be the first declaration");
                parseProject(false);
                break;
            default:
                errorAtCurrent("parse-unexpected-declaration",
                               "unexpected token at declaration scope");
                synchronizeDeclaration();
                if (check(TokenKind::End)) {
                    break;
                }
                break;
        }
        if (position_ == before && !check(TokenKind::End)) {
            advance();
        }
    }

    if (!tokens_.empty()) {
        module_.span = merge(tokens_.front().span, previous().span);
    }
    return std::move(module_);
}

void Parser::parseImport() {
    const Token& start = advance();
    ImportDecl decl;
    decl.span = start.span;
    if (check(TokenKind::String)) {
        decl.path = advance().text;
    } else {
        errorAtCurrent("parse-expected-import-path", "expected an import path string",
                       "string");
    }
    expectSemicolon();
    module_.imports.push_back(std::move(decl));
}

void Parser::parseProject(bool store) {
    const Token& start = advance();
    ProjectDecl project;
    project.span = start.span;
    if (check(TokenKind::Identifier)) {
        project.name = advance().text;
    }
    expect(TokenKind::LBrace, "parse-expected-lbrace", "expected '{' to start project block");

    while (!check(TokenKind::RBrace) && !check(TokenKind::End)) {
        const std::size_t before = position_;
        if (check(TokenKind::Tempo)) {
            advance();
            expect(TokenKind::Equal, "parse-expected-equal", "expected '=' after tempo");
            const ExprId value = parseExpr();
            expectSemicolon();
            if (project.tempo != kInvalidExprId) {
                diagnostics_.error("parse-duplicate-field", start.span,
                                   "duplicate project field 'tempo'");
            } else {
                project.tempo = value;
            }
        } else if (check(TokenKind::Meter)) {
            advance();
            expect(TokenKind::Equal, "parse-expected-equal", "expected '=' after meter");
            const ExprId value = parseExpr();
            expectSemicolon();
            if (project.meter != kInvalidExprId) {
                diagnostics_.error("parse-duplicate-field", start.span,
                                   "duplicate project field 'meter'");
            } else {
                project.meter = value;
            }
        } else if (check(TokenKind::Key)) {
            advance();
            expect(TokenKind::Equal, "parse-expected-equal", "expected '=' after key");
            const Token& keyStart = current();
            Expr key;
            key.kind = ExprKind::Key;
            key.span = keyStart.span;
            if (check(TokenKind::Identifier)) {
                key.text = advance().text;
                if (check(TokenKind::Identifier)) {
                    const Token& mode = advance();
                    key.text += " ";
                    key.text += mode.text;
                    key.span = merge(keyStart.span, mode.span);
                } else {
                    errorAtCurrent("parse-expected-key-mode",
                                   "expected 'major' or 'minor' after key tonic", "mode");
                }
            } else {
                errorAtCurrent("parse-expected-key", "expected a tonal key such as 'F minor'");
            }
            const ExprId value = addExpr(std::move(key));
            expectSemicolon();
            if (project.key != kInvalidExprId) {
                diagnostics_.error("parse-duplicate-field", start.span,
                                   "duplicate project field 'key'");
            } else {
                project.key = value;
            }
        } else {
            errorAtCurrent("parse-unexpected-project-field", "unknown project field");
            synchronizeStatement();
        }
        if (position_ == before && !check(TokenKind::RBrace) && !check(TokenKind::End)) {
            advance();
        }
    }

    if (check(TokenKind::RBrace)) {
        const Token& close = advance();
        project.span = merge(start.span, close.span);
    } else {
        errorAtCurrent("parse-expected-rbrace", "expected '}' to close project block", "'}'");
    }

    if (store) {
        if (module_.project.has_value()) {
            diagnostics_.error("parse-duplicate-project", project.span,
                               "duplicate project declaration");
        } else {
            module_.project = std::move(project);
        }
    }
}

void Parser::parseAsset() {
    const Token& start = advance();
    AssetDecl decl;
    decl.span = start.span;
    if (check(TokenKind::Identifier)) {
        decl.name = advance().text;
    } else {
        errorAtCurrent("parse-expected-name", "expected asset name", "identifier");
    }
    expect(TokenKind::Equal, "parse-expected-equal", "expected '=' after asset name");
    if (check(TokenKind::Instrument) || check(TokenKind::Sample)) {
        decl.kind = advance().text;
    } else {
        errorAtCurrent("parse-expected-asset-kind", "expected 'instrument' or 'sample'",
                       "'instrument' or 'sample'");
    }
    expect(TokenKind::LParen, "parse-expected-lparen", "expected '(' after asset kind");
    if (check(TokenKind::String)) {
        decl.provider = advance().text;
    } else {
        errorAtCurrent("parse-expected-string", "expected asset provider string", "string");
    }
    expect(TokenKind::RParen, "parse-expected-rparen", "expected ')' after asset provider");
    expectSemicolon();
    decl.span = merge(start.span, previous().span);
    module_.declarationOrder.push_back(DeclRef{DeclKind::Asset, module_.assets.size()});
    module_.assets.push_back(std::move(decl));
}

void Parser::parseLet(bool exported) {
    const Token& start = advance();
    LetDecl decl;
    decl.span = start.span;
    decl.exported = exported;
    if (check(TokenKind::Identifier)) {
        decl.name = advance().text;
    } else {
        errorAtCurrent("parse-expected-name", "expected binding name", "identifier");
    }
    expect(TokenKind::Equal, "parse-expected-equal", "expected '=' after binding name");
    decl.value = parseExpr();
    expectSemicolon();
    decl.span = merge(start.span, previous().span);
    module_.declarationOrder.push_back(DeclRef{DeclKind::Let, module_.lets.size()});
    module_.lets.push_back(std::move(decl));
}

void Parser::parseCurve(bool exported) {
    const Token& start = advance();
    CurveDecl decl;
    decl.span = start.span;
    decl.exported = exported;
    if (check(TokenKind::Identifier)) {
        decl.name = advance().text;
    } else {
        errorAtCurrent("parse-expected-name", "expected curve name", "identifier");
    }
    expect(TokenKind::On, "parse-expected-on", "expected 'on' and a curve domain");
    if (check(TokenKind::Identifier)) {
        decl.domain = advance().text;
    } else {
        errorAtCurrent("parse-expected-domain", "expected curve domain", "musical or relative");
        decl.domain = "relative";
    }
    if (match(TokenKind::For)) {
        decl.extent = parseExpr();
    }
    expect(TokenKind::Equal, "parse-expected-equal", "expected '=' after curve declaration");
    decl.value = parseExpr();
    expectSemicolon();
    decl.span = merge(start.span, previous().span);
    module_.declarationOrder.push_back(DeclRef{DeclKind::Curve, module_.curves.size()});
    module_.curves.push_back(std::move(decl));
}

void Parser::parseProcessor() {
    const Token& start = advance();
    ProcessorDecl decl;
    decl.span = start.span;
    if (check(TokenKind::Identifier)) {
        decl.name = advance().text;
    } else {
        errorAtCurrent("parse-expected-name", "expected processor name", "identifier");
    }
    expect(TokenKind::Equal, "parse-expected-equal", "expected '=' after processor name");
    if (check(TokenKind::Identifier)) {
        decl.kind = advance().text;
    } else {
        errorAtCurrent("parse-expected-processor-kind",
                       "expected a processor kind such as 'biquad'", "processor kind");
    }
    expect(TokenKind::LBrace, "parse-expected-lbrace",
           "expected '{' to start the processor body");
    while (!check(TokenKind::RBrace) && !check(TokenKind::End)) {
        const std::size_t before = position_;
        if (check(TokenKind::Identifier)) {
            const Token& field = advance();
            expect(TokenKind::Equal, "parse-expected-equal",
                   "expected '=' after processor field");
            if (field.text == "mode") {
                if (check(TokenKind::Identifier)) {
                    if (!decl.mode.empty()) {
                        diagnostics_.error("parse-duplicate-field", field.span,
                                           "duplicate processor field 'mode'");
                    }
                    decl.mode = advance().text;
                } else {
                    errorAtCurrent("parse-expected-mode", "expected a filter mode",
                                   "lowpass, highpass, peaking, lowshelf or highshelf");
                }
            } else {
                ProcessorField entry;
                entry.span = field.span;
                entry.name = field.text;
                entry.value = parseExpr();
                decl.fields.push_back(std::move(entry));
            }
            expectSemicolon();
        } else {
            errorAtCurrent("parse-unexpected-processor-field", "unknown processor field");
            synchronizeStatement();
        }
        if (position_ == before && !check(TokenKind::RBrace) && !check(TokenKind::End)) {
            advance();
        }
    }
    if (check(TokenKind::RBrace)) {
        decl.span = merge(start.span, advance().span);
    } else {
        errorAtCurrent("parse-expected-rbrace", "expected '}' to close processor", "'}'");
    }
    match(TokenKind::Semicolon);
    module_.declarationOrder.push_back(DeclRef{DeclKind::Processor, module_.processors.size()});
    module_.processors.push_back(std::move(decl));
}

void Parser::parseBus() {
    const Token& start = advance();
    BusDecl decl;
    decl.span = start.span;
    if (check(TokenKind::Identifier)) {
        decl.name = advance().text;
    } else {
        errorAtCurrent("parse-expected-name", "expected bus name", "identifier");
    }
    expect(TokenKind::LBrace, "parse-expected-lbrace", "expected '{' to start bus block");
    while (!check(TokenKind::RBrace) && !check(TokenKind::End)) {
        const std::size_t before = position_;
        if (check(TokenKind::Insert)) {
            advance();
            if (check(TokenKind::Identifier)) {
                decl.inserts.push_back(advance().text);
            } else {
                errorAtCurrent("parse-expected-name", "expected processor name", "identifier");
            }
            expectSemicolon();
        } else if (checkIdentifier("gain") || checkIdentifier("pan")) {
            const std::string field = advance().text;
            expect(TokenKind::Equal, "parse-expected-equal",
                   "expected '=' after bus field");
            const ExprId value = parseExpr();
            expectSemicolon();
            ExprId& target = field == "gain" ? decl.gain : decl.pan;
            if (target != kInvalidExprId) {
                diagnostics_.error("parse-duplicate-field", start.span,
                                   "duplicate bus field '" + field + "'");
            } else {
                target = value;
            }
        } else {
            errorAtCurrent("parse-unexpected-bus-field", "unknown bus field");
            synchronizeStatement();
        }
        if (position_ == before && !check(TokenKind::RBrace) && !check(TokenKind::End)) {
            advance();
        }
    }
    if (check(TokenKind::RBrace)) {
        decl.span = merge(start.span, advance().span);
    } else {
        errorAtCurrent("parse-expected-rbrace", "expected '}' to close bus block", "'}'");
    }
    match(TokenKind::Semicolon);
    module_.declarationOrder.push_back(DeclRef{DeclKind::Bus, module_.buses.size()});
    module_.buses.push_back(std::move(decl));
}

void Parser::parseMaster() {
    const Token& start = advance();
    MasterDecl decl;
    decl.span = start.span;
    expect(TokenKind::LBrace, "parse-expected-lbrace", "expected '{' to start master block");
    while (!check(TokenKind::RBrace) && !check(TokenKind::End)) {
        const std::size_t before = position_;
        if (check(TokenKind::Insert)) {
            advance();
            if (check(TokenKind::Identifier)) {
                decl.inserts.push_back(advance().text);
            } else {
                errorAtCurrent("parse-expected-name", "expected processor name", "identifier");
            }
            expectSemicolon();
        } else if (checkIdentifier("gain") || checkIdentifier("pan")) {
            const std::string field = advance().text;
            expect(TokenKind::Equal, "parse-expected-equal",
                   "expected '=' after master field");
            const ExprId value = parseExpr();
            expectSemicolon();
            ExprId& target = field == "gain" ? decl.gain : decl.pan;
            if (target != kInvalidExprId) {
                diagnostics_.error("parse-duplicate-field", start.span,
                                   "duplicate master field '" + field + "'");
            } else {
                target = value;
            }
        } else {
            errorAtCurrent("parse-unexpected-master-field", "unknown master field");
            synchronizeStatement();
        }
        if (position_ == before && !check(TokenKind::RBrace) && !check(TokenKind::End)) {
            advance();
        }
    }
    if (check(TokenKind::RBrace)) {
        decl.span = merge(start.span, advance().span);
    } else {
        errorAtCurrent("parse-expected-rbrace", "expected '}' to close master block", "'}'");
    }
    match(TokenKind::Semicolon);
    if (module_.master.has_value()) {
        diagnostics_.error("parse-duplicate-master", decl.span,
                           "duplicate master declaration");
    } else {
        module_.master = std::move(decl);
    }
}

std::string Parser::parseTypeName() {
    if (match(TokenKind::LBracket)) {
        const std::string element = parseTypeName();
        expect(TokenKind::RBracket, "parse-expected-rbracket", "expected ']' in list type");
        return "[" + element + "]";
    }
    if (check(TokenKind::Identifier)) {
        return advance().text;
    }
    if (check(TokenKind::Key)) {
        advance();
        return "Key";
    }
    if (check(TokenKind::Meter)) {
        advance();
        return "Meter";
    }
    errorAtCurrent("parse-expected-type", "expected a type name", "type");
    return "Number";
}

void Parser::parseFunction(bool exported) {
    const Token& start = advance();
    FunctionDecl decl;
    decl.span = start.span;
    decl.exported = exported;
    if (check(TokenKind::Identifier)) {
        decl.name = advance().text;
    } else {
        errorAtCurrent("parse-expected-name", "expected function name", "identifier");
    }
    expect(TokenKind::LParen, "parse-expected-lparen", "expected '(' after function name");
    while (!check(TokenKind::RParen) && !check(TokenKind::End)) {
        const std::size_t before = position_;
        ParamDecl param;
        param.span = current().span;
        if (check(TokenKind::Identifier)) {
            param.name = advance().text;
        } else {
            errorAtCurrent("parse-expected-name", "expected parameter name", "identifier");
        }
        expect(TokenKind::Colon, "parse-expected-colon", "expected ':' after parameter name");
        param.type = parseTypeName();
        decl.params.push_back(std::move(param));
        if (!match(TokenKind::Comma)) {
            break;
        }
        if (position_ == before) {
            advance();
        }
    }
    expect(TokenKind::RParen, "parse-expected-rparen", "expected ')' after parameters");
    expect(TokenKind::Arrow, "parse-expected-arrow", "expected '->' before return type");
    decl.returnType = parseTypeName();

    if (checkIdentifier("context")) {
        advance();
        while (!check(TokenKind::LBrace) && !check(TokenKind::End)) {
            if (check(TokenKind::Key)) {
                advance();
                decl.contextKey = true;
            } else if (check(TokenKind::Meter)) {
                advance();
                decl.contextMeter = true;
            } else {
                errorAtCurrent("parse-expected-context",
                               "expected 'key' or 'meter' in context declaration");
                break;
            }
            if (!match(TokenKind::Comma)) {
                break;
            }
        }
    }

    expect(TokenKind::LBrace, "parse-expected-lbrace", "expected '{' to start function body");
    while (!check(TokenKind::RBrace) && !check(TokenKind::End)) {
        const std::size_t before = position_;
        if (check(TokenKind::Let)) {
            const Token& letToken = advance();
            Stmt statement;
            statement.kind = StmtKind::Let;
            statement.span = letToken.span;
            if (check(TokenKind::Identifier)) {
                statement.name = advance().text;
            } else {
                errorAtCurrent("parse-expected-name", "expected local binding name", "identifier");
            }
            expect(TokenKind::Equal, "parse-expected-equal", "expected '=' after local binding");
            statement.value = parseExpr();
            expectSemicolon();
            decl.body.push_back(std::move(statement));
        } else if (check(TokenKind::Return)) {
            const Token& returnToken = advance();
            Stmt statement;
            statement.kind = StmtKind::Return;
            statement.span = returnToken.span;
            statement.value = parseExpr();
            expectSemicolon();
            decl.body.push_back(std::move(statement));
        } else {
            errorAtCurrent("parse-unexpected-function-item",
                           "expected 'let' or 'return' in function body");
            synchronizeStatement();
        }
        if (position_ == before && !check(TokenKind::RBrace) && !check(TokenKind::End)) {
            advance();
        }
    }
    if (check(TokenKind::RBrace)) {
        decl.span = merge(start.span, advance().span);
    } else {
        errorAtCurrent("parse-expected-rbrace", "expected '}' to close function", "'}'");
    }
    match(TokenKind::Semicolon);
    module_.functions.push_back(std::move(decl));
}

void Parser::parseTrack() {
    const Token& start = advance();
    TrackDecl decl;
    decl.span = start.span;
    if (check(TokenKind::Identifier)) {
        decl.name = advance().text;
    } else {
        errorAtCurrent("parse-expected-name", "expected track name", "identifier");
    }
    expect(TokenKind::LBrace, "parse-expected-lbrace", "expected '{' to start track block");
    while (!check(TokenKind::RBrace) && !check(TokenKind::End)) {
        const std::size_t before = position_;
        if (checkIdentifier("asset") || check(TokenKind::Asset)) {
            advance();
            expect(TokenKind::Equal, "parse-expected-equal", "expected '=' after asset");
            if (check(TokenKind::Identifier)) {
                decl.asset = advance().text;
            } else {
                errorAtCurrent("parse-expected-name", "expected asset name", "identifier");
            }
            expectSemicolon();
        } else if (checkIdentifier("role")) {
            advance();
            expect(TokenKind::Equal, "parse-expected-equal", "expected '=' after role");
            if (check(TokenKind::Identifier)) {
                decl.role = advance().text;
            } else {
                errorAtCurrent("parse-expected-role", "expected instrument role", "role");
            }
            expectSemicolon();
        } else if (checkIdentifier("gain") || checkIdentifier("pan")) {
            const std::string field = advance().text;
            expect(TokenKind::Equal, "parse-expected-equal",
                   "expected '=' after track field");
            const ExprId value = parseExpr();
            expectSemicolon();
            ExprId& target = field == "gain" ? decl.gain : decl.pan;
            if (target != kInvalidExprId) {
                diagnostics_.error("parse-duplicate-field", start.span,
                                   "duplicate track field '" + field + "'");
            } else {
                target = value;
            }
        } else if (check(TokenKind::Insert)) {
            advance();
            if (check(TokenKind::Identifier)) {
                decl.inserts.push_back(advance().text);
            } else {
                errorAtCurrent("parse-expected-name", "expected processor name", "identifier");
            }
            expectSemicolon();
        } else if (check(TokenKind::Send)) {
            const Token& sendToken = advance();
            SendDecl send;
            send.span = sendToken.span;
            send.level = parseExpr();
            if (checkIdentifier("to")) {
                advance();
            } else {
                errorAtCurrent("parse-expected-to", "expected 'to' after send level", "'to'");
            }
            if (check(TokenKind::Identifier)) {
                send.bus = advance().text;
            } else {
                errorAtCurrent("parse-expected-name", "expected bus name", "identifier");
            }
            expectSemicolon();
            decl.sends.push_back(std::move(send));
        } else if (check(TokenKind::Sidechain)) {
            const Token& sidechainToken = advance();
            SidechainDecl sidechain;
            sidechain.span = sidechainToken.span;
            if (check(TokenKind::Identifier)) {
                sidechain.processor = advance().text;
            } else {
                errorAtCurrent("parse-expected-name", "expected processor name", "identifier");
            }
            if (checkIdentifier("from")) {
                advance();
            } else {
                errorAtCurrent("parse-expected-from", "expected 'from' after sidechain processor",
                               "'from'");
            }
            if (check(TokenKind::Identifier)) {
                sidechain.source = advance().text;
            } else {
                errorAtCurrent("parse-expected-name", "expected source track name", "identifier");
            }
            expectSemicolon();
            decl.sidechains.push_back(std::move(sidechain));
        } else {
            errorAtCurrent("parse-unexpected-track-field", "unknown track field");
            synchronizeStatement();
        }
        if (position_ == before && !check(TokenKind::RBrace) && !check(TokenKind::End)) {
            advance();
        }
    }
    if (check(TokenKind::RBrace)) {
        decl.span = merge(start.span, advance().span);
    } else {
        errorAtCurrent("parse-expected-rbrace", "expected '}' to close track block", "'}'");
    }
    module_.declarationOrder.push_back(DeclRef{DeclKind::Track, module_.tracks.size()});
    module_.tracks.push_back(std::move(decl));
}

void Parser::parseSection() {
    const Token& start = advance();
    SectionDecl decl;
    decl.span = start.span;
    if (check(TokenKind::Identifier)) {
        decl.name = advance().text;
    } else {
        errorAtCurrent("parse-expected-name", "expected section name", "identifier");
    }
    expect(TokenKind::LParen, "parse-expected-lparen", "expected '(' after section name");
    decl.duration = parseExpr();
    expect(TokenKind::RParen, "parse-expected-rparen", "expected ')' after section duration");
    if (check(TokenKind::Meter)) {
        advance();
        decl.meter = parseMeterSpec();
    }
    expect(TokenKind::LBrace, "parse-expected-lbrace", "expected '{' to start section");
    while (!check(TokenKind::RBrace) && !check(TokenKind::End)) {
        const std::size_t before = position_;
        if (check(TokenKind::Clip)) {
            parseClip(decl);
        } else if (check(TokenKind::Meter)) {
            advance();
            MeterChange change;
            change.span = previous().span;
            const std::optional<MeterSpec> spec = parseMeterSpec();
            if (spec.has_value()) {
                change.meter = *spec;
            }
            if (checkIdentifier("at")) {
                advance();
                change.at = parseExpr();
            } else {
                errorAtCurrent("parse-expected-at",
                               "expected 'at <position>' after a section meter change", "'at'");
            }
            if (!change.meter.explicitGrouping && checkIdentifier("grouping")) {
                advance();
                change.meter.explicitGrouping = true;
                while (!check(TokenKind::End)) {
                    if (!check(TokenKind::Integer)) {
                        errorAtCurrent("parse-expected-integer",
                                       "expected a positive grouping component", "integer");
                        break;
                    }
                    change.meter.grouping.push_back(static_cast<int>(advance().integer));
                    if (!match(TokenKind::Plus)) {
                        break;
                    }
                }
            }
            expectSemicolon();
            decl.meterChanges.push_back(std::move(change));
        } else {
            errorAtCurrent("parse-unexpected-section-item",
                           "expected a clip or meter change in section");
            synchronizeStatement();
        }
        if (position_ == before && !check(TokenKind::RBrace) && !check(TokenKind::End)) {
            advance();
        }
    }
    if (check(TokenKind::RBrace)) {
        decl.span = merge(start.span, advance().span);
    } else {
        errorAtCurrent("parse-expected-rbrace", "expected '}' to close section", "'}'");
    }
    module_.declarationOrder.push_back(DeclRef{DeclKind::Section, module_.sections.size()});
    module_.sections.push_back(std::move(decl));
}

std::optional<MeterSpec> Parser::parseMeterSpec() {
    if (!check(TokenKind::Rational)) {
        errorAtCurrent("parse-expected-meter", "expected a meter such as '7/8'",
                       "'<numerator>/<denominator>'");
        return std::nullopt;
    }
    const Token& ratio = advance();
    MeterSpec spec;
    spec.span = ratio.span;
    spec.numerator = static_cast<int>(ratio.numerator);
    spec.denominator = static_cast<int>(ratio.denominator);
    if (checkIdentifier("grouping")) {
        advance();
        spec.explicitGrouping = true;
        while (!check(TokenKind::End)) {
            if (!check(TokenKind::Integer)) {
                errorAtCurrent("parse-expected-integer",
                               "expected a positive grouping component", "integer");
                break;
            }
            spec.grouping.push_back(static_cast<int>(advance().integer));
            if (!match(TokenKind::Plus)) {
                break;
            }
        }
        if (spec.grouping.empty()) {
            errorAtCurrent("parse-expected-grouping",
                           "grouping requires at least one component", "2+2+3");
        }
    }
    return spec;
}

void Parser::parseClip(SectionDecl& section) {
    const Token& start = advance();
    ClipDecl clip;
    clip.span = start.span;
    if (check(TokenKind::Identifier)) {
        clip.track = advance().text;
    } else {
        errorAtCurrent("parse-expected-name", "expected track name in clip", "identifier");
    }
    if (check(TokenKind::String)) {
        clip.name = advance().text;
    }
    if (checkIdentifier("at")) {
        advance();
        clip.at = parseExpr();
    }
    expect(TokenKind::LBrace, "parse-expected-lbrace", "expected '{' to start clip");
    while (!check(TokenKind::RBrace) && !check(TokenKind::End)) {
        const std::size_t before = position_;
        if (check(TokenKind::Play)) {
            advance();
            const ExprId value = parseExpr();
            expectSemicolon();
            if (clip.play != kInvalidExprId) {
                diagnostics_.error("parse-duplicate-play", start.span,
                                   "clip already has a play statement");
            } else {
                clip.play = value;
            }
        } else if (check(TokenKind::Identifier) &&
                   isExpressionField(current().text)) {
            const std::string field = advance().text;
            expect(TokenKind::Equal, "parse-expected-equal",
                   "expected '=' after expression lane");
            const ExprId value = parseExpr();
            expectSemicolon();
            ExprId* target = nullptr;
            if (field == "velocity") {
                target = &clip.velocity;
            } else if (field == "timing") {
                target = &clip.timing;
            } else if (field == "gate") {
                target = &clip.gate;
            } else {
                target = &clip.pan;
            }
            if (*target != kInvalidExprId) {
                diagnostics_.error("parse-duplicate-field", start.span,
                                   "duplicate expression lane '" + field + "'");
            } else {
                *target = value;
            }
        } else {
            errorAtCurrent("parse-unexpected-clip-item",
                           "expected play or an expression lane");
            synchronizeStatement();
        }
        if (position_ == before && !check(TokenKind::RBrace) && !check(TokenKind::End)) {
            advance();
        }
    }
    if (check(TokenKind::RBrace)) {
        clip.span = merge(start.span, advance().span);
    } else {
        errorAtCurrent("parse-expected-rbrace", "expected '}' to close clip", "'}'");
    }
    if (clip.play == kInvalidExprId) {
        diagnostics_.error("parse-missing-play", clip.span, "clip requires a play expression");
    }
    section.clips.push_back(std::move(clip));
}

void Parser::parseAutomation() {
    const Token& start = advance();
    AutomationDecl decl;
    decl.span = start.span;
    if (check(TokenKind::Master)) {
        advance();
        decl.scope = "master";
    } else if (check(TokenKind::Identifier)) {
        decl.track = advance().text;
        decl.scope = "track";
    } else {
        errorAtCurrent("parse-expected-target", "expected automation target", "track.parameter");
    }
    expect(TokenKind::Dot, "parse-expected-dot", "expected '.' in automation target");
    if (check(TokenKind::Identifier)) {
        decl.parameter = advance().text;
    } else {
        errorAtCurrent("parse-expected-parameter", "expected parameter name", "identifier");
    }
    if (match(TokenKind::Dot)) {
        decl.processor = decl.parameter;
        decl.parameter.clear();
        if (check(TokenKind::Identifier)) {
            decl.parameter = advance().text;
        } else {
            errorAtCurrent("parse-expected-parameter", "expected parameter name", "identifier");
        }
    }
    expect(TokenKind::Equal, "parse-expected-equal", "expected '=' after automation target");
    decl.curve = parseExpr();
    if (check(TokenKind::Identifier) && isAutomationMode(current().text)) {
        decl.mode = advance().text;
    } else {
        decl.mode = "replace";
    }
    expectSemicolon();
    decl.span = merge(start.span, previous().span);
    module_.declarationOrder.push_back(DeclRef{DeclKind::Automation, module_.automations.size()});
    module_.automations.push_back(std::move(decl));
}

void Parser::parseArrangement() {
    const Token& start = advance();
    ArrangementDecl decl;
    decl.span = start.span;
    expect(TokenKind::LBrace, "parse-expected-lbrace", "expected '{' to start arrangement");
    while (!check(TokenKind::RBrace) && !check(TokenKind::End)) {
        const std::size_t before = position_;
        if (check(TokenKind::Play)) {
            advance();
            if (check(TokenKind::Identifier)) {
                const Token& section = advance();
                decl.plays.emplace_back(section.text, section.span);
            } else {
                errorAtCurrent("parse-expected-section", "expected section name after play",
                               "identifier");
            }
            expectSemicolon();
        } else if (check(TokenKind::Semicolon)) {
            advance();
        } else {
            errorAtCurrent("parse-unexpected-arrangement-item",
                           "expected 'play <section>;' in arrangement");
            synchronizeStatement();
        }
        if (position_ == before && !check(TokenKind::RBrace) && !check(TokenKind::End)) {
            advance();
        }
    }
    if (check(TokenKind::RBrace)) {
        decl.span = merge(start.span, advance().span);
    } else {
        errorAtCurrent("parse-expected-rbrace", "expected '}' to close arrangement", "'}'");
    }
    if (module_.arrangement.has_value()) {
        diagnostics_.error("parse-duplicate-arrangement", decl.span,
                           "duplicate arrangement declaration");
    } else {
        module_.arrangement = std::move(decl);
    }
}

void Parser::parseExport() {
    const Token& start = advance();
    ExportDecl decl;
    decl.span = start.span;
    if (check(TokenKind::Midi)) {
        decl.format = "midi";
        advance();
    } else if (checkIdentifier("wav") || checkIdentifier("audio")) {
        decl.format = "wav";
        advance();
    } else {
        errorAtCurrent("parse-expected-format", "expected export format 'midi' or 'wav'",
                       "'midi' or 'wav'");
        decl.format = "midi";
    }
    expect(TokenKind::LParen, "parse-expected-lparen", "expected '(' after export format");
    if (check(TokenKind::String)) {
        decl.path = advance().text;
    } else {
        errorAtCurrent("parse-expected-string", "expected export path string", "string");
    }
    expect(TokenKind::RParen, "parse-expected-rparen", "expected ')' after export path");
    expectSemicolon();
    decl.span = merge(start.span, previous().span);
    module_.exports.push_back(std::move(decl));
}

ExprId Parser::parseExpr() { return parseOr(); }

ExprId Parser::parseOr() {
    ExprId left = parseAnd();
    while (check(TokenKind::PipePipe)) {
        const Token& op = advance();
        const ExprId right = parseAnd();
        Expr expr;
        expr.kind = ExprKind::Binary;
        expr.text = "||";
        expr.span = merge(module_.expr(left).span, module_.expr(right).span);
        expr.args = {left, right};
        left = addExpr(std::move(expr));
        (void)op;
    }
    return left;
}

ExprId Parser::parseAnd() {
    ExprId left = parseEquality();
    while (check(TokenKind::AmpAmp)) {
        advance();
        const ExprId right = parseEquality();
        Expr expr;
        expr.kind = ExprKind::Binary;
        expr.text = "&&";
        expr.span = merge(module_.expr(left).span, module_.expr(right).span);
        expr.args = {left, right};
        left = addExpr(std::move(expr));
    }
    return left;
}

ExprId Parser::parseEquality() {
    ExprId left = parseComparison();
    while (check(TokenKind::EqualEqual) || check(TokenKind::BangEqual)) {
        const std::string op = advance().text;
        const ExprId right = parseComparison();
        Expr expr;
        expr.kind = ExprKind::Binary;
        expr.text = op;
        expr.span = merge(module_.expr(left).span, module_.expr(right).span);
        expr.args = {left, right};
        left = addExpr(std::move(expr));
    }
    return left;
}

ExprId Parser::parseComparison() {
    ExprId left = parseAdditive();
    while (check(TokenKind::Less) || check(TokenKind::LessEqual) ||
           check(TokenKind::Greater) || check(TokenKind::GreaterEqual)) {
        const std::string op = advance().text;
        const ExprId right = parseAdditive();
        Expr expr;
        expr.kind = ExprKind::Binary;
        expr.text = op;
        expr.span = merge(module_.expr(left).span, module_.expr(right).span);
        expr.args = {left, right};
        left = addExpr(std::move(expr));
    }
    return left;
}

ExprId Parser::parseAdditive() {
    ExprId left = parseMultiplicative();
    while (check(TokenKind::Plus) || check(TokenKind::Minus)) {
        const std::string op = advance().text;
        const ExprId right = parseMultiplicative();
        Expr expr;
        expr.kind = ExprKind::Binary;
        expr.text = op;
        expr.span = merge(module_.expr(left).span, module_.expr(right).span);
        expr.args = {left, right};
        left = addExpr(std::move(expr));
    }
    return left;
}

ExprId Parser::parseMultiplicative() {
    ExprId left = parseUnary();
    while (check(TokenKind::Star) || check(TokenKind::Slash) ||
           check(TokenKind::Percent)) {
        const std::string op = advance().text;
        const ExprId right = parseUnary();
        Expr expr;
        expr.kind = ExprKind::Binary;
        expr.text = op;
        expr.span = merge(module_.expr(left).span, module_.expr(right).span);
        expr.args = {left, right};
        left = addExpr(std::move(expr));
    }
    return left;
}

ExprId Parser::parseUnary() {
    if (check(TokenKind::Minus)) {
        const Token& start = advance();
        const ExprId operand = parseUnary();
        Expr expr;
        expr.kind = ExprKind::UnaryMinus;
        expr.span = merge(start.span, module_.expr(operand).span);
        expr.args.push_back(operand);
        return addExpr(std::move(expr));
    }
    if (check(TokenKind::Bang)) {
        const Token& start = advance();
        const ExprId operand = parseUnary();
        Expr expr;
        expr.kind = ExprKind::Not;
        expr.span = merge(start.span, module_.expr(operand).span);
        expr.args.push_back(operand);
        return addExpr(std::move(expr));
    }
    return parsePostfix();
}

ExprId Parser::parsePostfix() {
    ExprId expression = parsePrimary();
    while (check(TokenKind::LBracket)) {
        const Token& start = advance();
        expression = parseIndex(expression, start);
    }
    return expression;
}

ExprId Parser::parseIndex(ExprId base, const Token& start) {
    Expr expr;
    expr.kind = ExprKind::Index;
    expr.span = start.span;
    expr.args.push_back(base);

    ExprId first = kInvalidExprId;
    if (!check(TokenKind::Colon)) {
        first = parseExpr();
        expr.args.push_back(first);
    }
    if (match(TokenKind::Colon)) {
        expr.text = "slice";
        if (first == kInvalidExprId) {
            Expr zero;
            zero.kind = ExprKind::Integer;
            zero.span = start.span;
            zero.integer = 0;
            zero.number = 0.0;
            first = addExpr(std::move(zero));
            expr.args.push_back(first);
        }
        if (!check(TokenKind::RBracket)) {
            expr.args.push_back(parseExpr());
        } else {
            expr.args.push_back(kInvalidExprId);
        }
    } else if (first == kInvalidExprId) {
        errorAtCurrent("parse-expected-index", "expected list index or slice", "index");
        expr.args.push_back(invalidExpr(start.span));
    }
    const Token close = expect(TokenKind::RBracket, "parse-expected-rbracket",
                               "expected ']' after list index");
    expr.span = merge(start.span, close.span);
    return addExpr(std::move(expr));
}

ExprId Parser::parsePrimary() {
    const Token& token = current();
    switch (token.kind) {
        case TokenKind::True:
        case TokenKind::False: {
            advance();
            Expr expr;
            expr.kind = ExprKind::Boolean;
            expr.span = token.span;
            expr.boolean = token.kind == TokenKind::True;
            return addExpr(std::move(expr));
        }
        case TokenKind::If: return parseIfExpr();
        case TokenKind::For: return parseForExpr();
        case TokenKind::Integer:
        case TokenKind::Rational:
        case TokenKind::Number: {
            advance();
            Expr expr;
            expr.span = token.span;
            if (token.kind == TokenKind::Integer) {
                expr.kind = ExprKind::Integer;
                expr.integer = token.integer;
                expr.number = token.number;
            } else if (token.kind == TokenKind::Rational) {
                expr.kind = ExprKind::Rational;
                expr.numerator = token.numerator;
                expr.denominator = token.denominator;
                expr.number = token.number;
            } else {
                expr.kind = ExprKind::Number;
                expr.number = token.number;
            }
            parseOptionalUnit(expr);
            return addExpr(std::move(expr));
        }
        case TokenKind::String: {
            advance();
            Expr expr;
            expr.kind = ExprKind::String;
            expr.span = token.span;
            expr.text = token.text;
            return addExpr(std::move(expr));
        }
        case TokenKind::LBracket:
            return parseList(token);
        case TokenKind::Identifier:
            if (token.text == "melody" && position_ + 1 < tokens_.size() &&
                tokens_[position_ + 1].kind == TokenKind::LBrace) {
                return parseMelody(token);
            }
            if (token.text == "notes") {
                return parseNotes(token);
            }
            if (token.text == "rhythm" &&
                (position_ + 1 < tokens_.size()) &&
                (tokens_[position_ + 1].kind == TokenKind::LBrace ||
                 (tokens_[position_ + 1].kind == TokenKind::Identifier &&
                  tokens_[position_ + 1].text == "span"))) {
                return parseRhythm(token);
            }
            if (token.text == "harmony" &&
                (position_ + 1 < tokens_.size()) &&
                (tokens_[position_ + 1].kind == TokenKind::LBrace ||
                 (tokens_[position_ + 1].kind == TokenKind::Identifier &&
                  tokens_[position_ + 1].text == "span"))) {
                return parseHarmony(token);
            }
            if (token.text == "keyframes" && position_ + 1 < tokens_.size() &&
                tokens_[position_ + 1].kind == TokenKind::LBrace) {
                return parseKeyframes(token);
            }
            if (position_ + 1 < tokens_.size() &&
                tokens_[position_ + 1].kind == TokenKind::LParen) {
                return parseCall(token);
            }
            advance();
            {
                Expr expr;
                expr.kind = ExprKind::Identifier;
                expr.span = token.span;
                expr.text = token.text;
                return addExpr(std::move(expr));
            }
        default:
            break;
    }

    errorAtCurrent("parse-expected-expression", "expected an expression", "expression",
                   token.text);
    const ExprId invalid = invalidExpr(token.span);
    if (!check(TokenKind::End) && !check(TokenKind::Semicolon) && !check(TokenKind::RBrace) &&
        !check(TokenKind::RParen) && !check(TokenKind::RBracket) && !check(TokenKind::Comma)) {
        advance();
    }
    return invalid;
}

ExprId Parser::parseIfExpr() {
    const Token& start = advance();
    const ExprId condition = parseExpr();
    expect(TokenKind::LBrace, "parse-expected-lbrace", "expected '{' after if condition");
    const ExprId thenValue = parseExpr();
    expect(TokenKind::RBrace, "parse-expected-rbrace", "expected '}' after if branch");
    expect(TokenKind::Else, "parse-expected-else", "expected 'else' after if branch");
    expect(TokenKind::LBrace, "parse-expected-lbrace", "expected '{' after else");
    const ExprId elseValue = parseExpr();
    expect(TokenKind::RBrace, "parse-expected-rbrace", "expected '}' after else branch");
    Expr expr;
    expr.kind = ExprKind::If;
    expr.span = merge(start.span, previous().span);
    expr.args = {condition, thenValue, elseValue};
    return addExpr(std::move(expr));
}

ExprId Parser::parseForExpr() {
    const Token& start = advance();
    Expr expr;
    expr.kind = ExprKind::For;
    expr.span = start.span;
    if (check(TokenKind::Identifier)) {
        expr.text = advance().text;
    } else {
        errorAtCurrent("parse-expected-name", "expected loop variable", "identifier");
    }
    if (checkIdentifier("in")) {
        advance();
    } else {
        errorAtCurrent("parse-expected-in", "expected 'in' after loop variable", "'in'");
    }
    const ExprId iterable = parseExpr();
    expect(TokenKind::LBrace, "parse-expected-lbrace", "expected '{' after for iterable");
    const ExprId body = parseExpr();
    expect(TokenKind::RBrace, "parse-expected-rbrace", "expected '}' after for body");
    expr.args = {iterable, body};
    expr.span = merge(start.span, previous().span);
    return addExpr(std::move(expr));
}

ExprId Parser::parseCall(const Token& callee) {
    advance();  // callee
    Expr expr;
    expr.kind = ExprKind::Call;
    expr.text = callee.text;
    expr.span = callee.span;
    expect(TokenKind::LParen, "parse-expected-lparen", "expected '(' after operator name");
    if (!check(TokenKind::RParen)) {
        while (!check(TokenKind::End)) {
            const std::size_t before = position_;
            expr.args.push_back(parseExpr());
            if (!match(TokenKind::Comma)) {
                break;
            }
            if (position_ == before) {
                advance();
            }
        }
    }
    if (check(TokenKind::RParen)) {
        const Token& close = advance();
        expr.span = merge(callee.span, close.span);
    } else {
        errorAtCurrent("parse-expected-rparen", "expected ')' after operator arguments", "')'");
    }
    return addExpr(std::move(expr));
}

ExprId Parser::parseMelody(const Token& start) {
    advance();  // melody
    Expr expr;
    expr.kind = ExprKind::Melody;
    expr.span = start.span;
    expect(TokenKind::LBrace, "parse-expected-lbrace", "expected '{' after melody");
    while (!check(TokenKind::RBrace) && !check(TokenKind::End)) {
        const std::size_t before = position_;
        MelodyEntry entry;
        entry.span = current().span;
        if (check(TokenKind::Identifier)) {
            entry.pitch = advance().text;
        } else {
            errorAtCurrent("parse-expected-pitch", "expected a pitch such as 'F4'", "pitch");
            synchronizeStatement();
            continue;
        }
        expect(TokenKind::Colon, "parse-expected-colon", "expected ':' after pitch");
        entry.duration = parseExpr();
        expr.melody.push_back(std::move(entry));
        if (!match(TokenKind::Comma)) {
            break;
        }
        if (position_ == before) {
            advance();
        }
    }
    if (check(TokenKind::RBrace)) {
        expr.span = merge(start.span, advance().span);
    } else {
        errorAtCurrent("parse-expected-rbrace", "expected '}' to close melody", "'}'");
    }
    return addExpr(std::move(expr));
}

ExprId Parser::parseNotes(const Token& start) {
    advance();  // notes
    Expr expr;
    expr.kind = ExprKind::Notes;
    expr.span = start.span;
    if (checkIdentifier("span")) {
        advance();
        expr.explicitSpan = parseExpr();
    }
    expect(TokenKind::LBrace, "parse-expected-lbrace", "expected '{' after notes");
    while (!check(TokenKind::RBrace) && !check(TokenKind::End)) {
        const std::size_t before = position_;
        NoteEntry entry;
        entry.span = current().span;
        if (check(TokenKind::Identifier)) {
            entry.pitch = advance().text;
        } else {
            errorAtCurrent("parse-expected-pitch", "expected a pitch such as 'C2'", "pitch");
            synchronizeStatement();
            continue;
        }
        expect(TokenKind::At, "parse-expected-at", "expected '@' before note offset");
        entry.offset = parseExpr();
        expect(TokenKind::For, "parse-expected-for", "expected 'for' before note duration");
        entry.duration = parseExpr();
        if (checkIdentifier("vel")) {
            advance();
            if (check(TokenKind::Integer)) {
                entry.velocity = static_cast<int>(advance().integer);
            } else {
                errorAtCurrent("parse-expected-integer", "expected velocity integer", "integer");
            }
        }
        expr.notes.push_back(std::move(entry));
        if (!match(TokenKind::Comma)) {
            break;
        }
        if (position_ == before) {
            advance();
        }
    }
    if (check(TokenKind::RBrace)) {
        expr.span = merge(start.span, advance().span);
    } else {
        errorAtCurrent("parse-expected-rbrace", "expected '}' to close notes", "'}'");
    }
    return addExpr(std::move(expr));
}

ExprId Parser::parseRhythm(const Token& start) {
    advance();  // rhythm
    Expr expr;
    expr.kind = ExprKind::Rhythm;
    expr.span = start.span;
    if (checkIdentifier("span")) {
        advance();
        expr.explicitSpan = parseExpr();
    }
    expect(TokenKind::LBrace, "parse-expected-lbrace", "expected '{' after rhythm");
    while (!check(TokenKind::RBrace) && !check(TokenKind::End)) {
        const std::size_t before = position_;
        RhythmEntry entry;
        entry.span = current().span;
        if (checkIdentifier("hit")) {
            advance();
            entry.rest = false;
            entry.duration = parseExpr();
            if (checkIdentifier("accent")) {
                advance();
                if (check(TokenKind::Integer)) {
                    entry.accent = static_cast<int>(advance().integer);
                } else {
                    errorAtCurrent("parse-expected-integer",
                                   "expected an integer accent level", "integer");
                }
            }
        } else if (checkIdentifier("rest")) {
            advance();
            entry.rest = true;
            entry.duration = parseExpr();
        } else {
            errorAtCurrent("parse-expected-rhythm-entry",
                           "expected 'hit <duration>' or 'rest <duration>'", "'hit' or 'rest'");
            synchronizeStatement();
            continue;
        }
        expr.rhythm.push_back(std::move(entry));
        if (!match(TokenKind::Comma)) {
            break;
        }
        if (position_ == before) {
            advance();
        }
    }
    if (check(TokenKind::RBrace)) {
        expr.span = merge(start.span, advance().span);
    } else {
        errorAtCurrent("parse-expected-rbrace", "expected '}' to close rhythm", "'}'");
    }
    return addExpr(std::move(expr));
}

ExprId Parser::parseHarmony(const Token& start) {
    advance();  // harmony
    Expr expr;
    expr.kind = ExprKind::Harmony;
    expr.span = start.span;
    if (checkIdentifier("span")) {
        advance();
        expr.explicitSpan = parseExpr();
    }
    expect(TokenKind::LBrace, "parse-expected-lbrace", "expected '{' after harmony");
    while (!check(TokenKind::RBrace) && !check(TokenKind::End)) {
        const std::size_t before = position_;
        HarmonyEntry entry;
        entry.span = current().span;
        if (check(TokenKind::String)) {
            entry.chord = advance().text;
        } else {
            errorAtCurrent("parse-expected-chord",
                           "expected a chord string such as \"Dm7\"", "string");
            synchronizeStatement();
            continue;
        }
        expect(TokenKind::At, "parse-expected-at", "expected '@' before harmony start");
        entry.offset = parseExpr();
        expect(TokenKind::For, "parse-expected-for", "expected 'for' before harmony duration");
        entry.duration = parseExpr();
        expr.harmony.push_back(std::move(entry));
        if (!match(TokenKind::Comma)) {
            break;
        }
        if (position_ == before) {
            advance();
        }
    }
    if (check(TokenKind::RBrace)) {
        expr.span = merge(start.span, advance().span);
    } else {
        errorAtCurrent("parse-expected-rbrace", "expected '}' to close harmony", "'}'");
    }
    return addExpr(std::move(expr));
}

ExprId Parser::parseKeyframes(const Token& start) {
    advance();  // keyframes
    Expr expr;
    expr.kind = ExprKind::Keyframes;
    expr.span = start.span;
    expect(TokenKind::LBrace, "parse-expected-lbrace", "expected '{' after keyframes");
    while (!check(TokenKind::RBrace) && !check(TokenKind::End)) {
        const std::size_t before = position_;
        KeyframeEntry entry;
        entry.span = current().span;
        entry.time = parseExpr();
        expect(TokenKind::Colon, "parse-expected-colon", "expected ':' after keyframe time");
        bool negative = false;
        if (match(TokenKind::Minus)) {
            negative = true;
        }
        if (check(TokenKind::Number) || check(TokenKind::Integer)) {
            entry.value = advance().number;
            if (negative) {
                entry.value = -entry.value;
            }
        } else {
            errorAtCurrent("parse-expected-number", "expected keyframe value", "number");
        }
        if (check(TokenKind::Identifier)) {
            entry.interpolation = advance().text;
        } else {
            errorAtCurrent("parse-expected-interpolation",
                           "expected 'step', 'linear' or 'smoothstep'", "interpolation");
            entry.interpolation = "linear";
        }
        expr.keyframes.push_back(std::move(entry));
        if (!match(TokenKind::Comma)) {
            break;
        }
        if (position_ == before) {
            advance();
        }
    }
    if (check(TokenKind::RBrace)) {
        expr.span = merge(start.span, advance().span);
    } else {
        errorAtCurrent("parse-expected-rbrace", "expected '}' to close keyframes", "'}'");
    }
    return addExpr(std::move(expr));
}

ExprId Parser::parseList(const Token& start) {
    advance();  // [
    Expr expr;
    expr.kind = ExprKind::List;
    expr.span = start.span;
    if (!check(TokenKind::RBracket)) {
        while (!check(TokenKind::End)) {
            const std::size_t before = position_;
            expr.args.push_back(parseExpr());
            if (!match(TokenKind::Comma)) {
                break;
            }
            if (position_ == before) {
                advance();
            }
        }
    }
    if (check(TokenKind::RBracket)) {
        expr.span = merge(start.span, advance().span);
    } else {
        errorAtCurrent("parse-expected-rbracket", "expected ']' after list", "']'");
    }
    return addExpr(std::move(expr));
}

void Parser::parseOptionalUnit(Expr& expr) {
    if (check(TokenKind::Identifier) && isTimeUnit(current().text)) {
        expr.unit = advance().text;
    }
}

}  // namespace mpl
