#pragma once

#include "mpl/frontend/Source.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace mpl {

enum class TokenKind {
    End,
    Identifier,
    Integer,
    Rational,
    Number,
    String,
    LBrace,
    RBrace,
    LParen,
    RParen,
    LBracket,
    RBracket,
    Colon,
    Semicolon,
    Comma,
    Dot,
    Equal,
    At,
    Minus,
    Slash,
    Project,
    Tempo,
    Meter,
    Key,
    Asset,
    Instrument,
    Sample,
    Let,
    Curve,
    On,
    For,
    Automation,
    Track,
    Section,
    Clip,
    Play,
    Arrangement,
    Export,
    Midi,
    Master,
    Processor,
    Bus,
    Insert,
    Send,
    Sidechain,
};

std::string_view tokenKindName(TokenKind kind);

struct Token {
    TokenKind kind = TokenKind::End;
    std::string text;
    SourceSpan span;
    std::int64_t integer = 0;
    std::int64_t numerator = 0;
    std::int64_t denominator = 1;
    double number = 0.0;
};

}  // namespace mpl
