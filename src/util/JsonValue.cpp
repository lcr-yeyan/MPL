#include "mpl/util/JsonValue.hpp"

#include <cmath>
#include <cstdlib>
#include <sstream>
#include <utility>

namespace mpl {
namespace {

class Parser {
public:
    Parser(std::string_view text, std::string& error)
        : text_(text), error_(error) {}

    bool parse(JsonValue& value) {
        skipWhitespace();
        if (!parseValue(value)) {
            return false;
        }
        skipWhitespace();
        if (position_ != text_.size()) {
            return fail("trailing characters after JSON value");
        }
        return true;
    }

private:
    bool fail(std::string message) {
        error_ = std::move(message) + " at byte " + std::to_string(position_);
        return false;
    }

    void skipWhitespace() {
        while (position_ < text_.size()) {
            const char character = text_[position_];
            if (character == ' ' || character == '\t' || character == '\n' ||
                character == '\r') {
                ++position_;
            } else {
                break;
            }
        }
    }

    bool parseString(std::string& out) {
        if (position_ >= text_.size() || text_[position_] != '"') {
            return fail("expected string");
        }
        ++position_;
        while (position_ < text_.size()) {
            const char character = text_[position_++];
            if (character == '"') {
                return true;
            }
            if (character == '\\') {
                if (position_ >= text_.size()) {
                    return fail("unterminated escape");
                }
                const char escaped = text_[position_++];
                switch (escaped) {
                    case '"': out += '"'; break;
                    case '\\': out += '\\'; break;
                    case '/': out += '/'; break;
                    case 'b': out += '\b'; break;
                    case 'f': out += '\f'; break;
                    case 'n': out += '\n'; break;
                    case 'r': out += '\r'; break;
                    case 't': out += '\t'; break;
                    case 'u': {
                        if (position_ + 4 > text_.size()) {
                            return fail("bad unicode escape");
                        }
                        int code = 0;
                        for (int index = 0; index < 4; ++index) {
                            const char digit = text_[position_++];
                            code *= 16;
                            if (digit >= '0' && digit <= '9') {
                                code += digit - '0';
                            } else if (digit >= 'a' && digit <= 'f') {
                                code += digit - 'a' + 10;
                            } else if (digit >= 'A' && digit <= 'F') {
                                code += digit - 'A' + 10;
                            } else {
                                return fail("bad unicode escape");
                            }
                        }
                        // Minimal UTF-8 encoding; surrogate pairs are not needed
                        // by task files.
                        if (code <= 0x7F) {
                            out += static_cast<char>(code);
                        } else if (code <= 0x7FF) {
                            out += static_cast<char>(0xC0 | (code >> 6));
                            out += static_cast<char>(0x80 | (code & 0x3F));
                        } else {
                            out += static_cast<char>(0xE0 | (code >> 12));
                            out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
                            out += static_cast<char>(0x80 | (code & 0x3F));
                        }
                        break;
                    }
                    default:
                        return fail("bad escape");
                }
            } else {
                out += character;
            }
        }
        return fail("unterminated string");
    }

    bool parseNumber(JsonValue& value) {
        const std::size_t start = position_;
        if (position_ < text_.size() && text_[position_] == '-') {
            ++position_;
        }
        while (position_ < text_.size() &&
               (text_[position_] >= '0' && text_[position_] <= '9')) {
            ++position_;
        }
        if (position_ < text_.size() && text_[position_] == '.') {
            ++position_;
            while (position_ < text_.size() &&
                   (text_[position_] >= '0' && text_[position_] <= '9')) {
                ++position_;
            }
        }
        if (position_ < text_.size() &&
            (text_[position_] == 'e' || text_[position_] == 'E')) {
            ++position_;
            if (position_ < text_.size() &&
                (text_[position_] == '+' || text_[position_] == '-')) {
                ++position_;
            }
            while (position_ < text_.size() &&
                   (text_[position_] >= '0' && text_[position_] <= '9')) {
                ++position_;
            }
        }
        if (position_ == start) {
            return fail("expected number");
        }
        const std::string number(text_.substr(start, position_ - start));
        char* end = nullptr;
        value.type = JsonValue::Type::Number;
        value.number = std::strtod(number.c_str(), &end);
        return end != nullptr;
    }

    bool parseValue(JsonValue& value) {
        skipWhitespace();
        if (position_ >= text_.size()) {
            return fail("unexpected end of JSON");
        }
        const char character = text_[position_];
        if (character == '{') {
            ++position_;
            value.type = JsonValue::Type::Object;
            skipWhitespace();
            if (position_ < text_.size() && text_[position_] == '}') {
                ++position_;
                return true;
            }
            while (true) {
                skipWhitespace();
                std::string key;
                if (!parseString(key)) {
                    return false;
                }
                skipWhitespace();
                if (position_ >= text_.size() || text_[position_] != ':') {
                    return fail("expected ':'");
                }
                ++position_;
                JsonValue child;
                if (!parseValue(child)) {
                    return false;
                }
                value.object.emplace(std::move(key), std::move(child));
                skipWhitespace();
                if (position_ < text_.size() && text_[position_] == ',') {
                    ++position_;
                    continue;
                }
                if (position_ < text_.size() && text_[position_] == '}') {
                    ++position_;
                    return true;
                }
                return fail("expected ',' or '}'");
            }
        }
        if (character == '[') {
            ++position_;
            value.type = JsonValue::Type::Array;
            skipWhitespace();
            if (position_ < text_.size() && text_[position_] == ']') {
                ++position_;
                return true;
            }
            while (true) {
                JsonValue child;
                if (!parseValue(child)) {
                    return false;
                }
                value.array.push_back(std::move(child));
                skipWhitespace();
                if (position_ < text_.size() && text_[position_] == ',') {
                    ++position_;
                    continue;
                }
                if (position_ < text_.size() && text_[position_] == ']') {
                    ++position_;
                    return true;
                }
                return fail("expected ',' or ']'");
            }
        }
        if (character == '"') {
            value.type = JsonValue::Type::String;
            return parseString(value.text);
        }
        if (text_.compare(position_, 4, "true") == 0) {
            position_ += 4;
            value.type = JsonValue::Type::Bool;
            value.boolean = true;
            return true;
        }
        if (text_.compare(position_, 5, "false") == 0) {
            position_ += 5;
            value.type = JsonValue::Type::Bool;
            value.boolean = false;
            return true;
        }
        if (text_.compare(position_, 4, "null") == 0) {
            position_ += 4;
            value.type = JsonValue::Type::Null;
            return true;
        }
        return parseNumber(value);
    }

    std::string_view text_;
    std::string& error_;
    std::size_t position_ = 0;
};

void writeString(std::string& out, std::string_view text) {
    out += '"';
    for (const char character : text) {
        switch (character) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += character; break;
        }
    }
    out += '"';
}

void writeCanonical(std::string& out, const JsonValue& value) {
    switch (value.type) {
        case JsonValue::Type::Null: out += "null"; break;
        case JsonValue::Type::Bool: out += value.boolean ? "true" : "false"; break;
        case JsonValue::Type::Number: {
            std::ostringstream stream;
            stream.precision(17);
            stream << value.number;
            out += stream.str();
            break;
        }
        case JsonValue::Type::String: writeString(out, value.text); break;
        case JsonValue::Type::Array:
            out += '[';
            for (std::size_t index = 0; index < value.array.size(); ++index) {
                if (index != 0) {
                    out += ',';
                }
                writeCanonical(out, value.array[index]);
            }
            out += ']';
            break;
        case JsonValue::Type::Object:
            out += '{';
            {
                bool first = true;
                for (const auto& [key, child] : value.object) {
                    if (!first) {
                        out += ',';
                    }
                    first = false;
                    writeString(out, key);
                    out += ':';
                    writeCanonical(out, child);
                }
            }
            out += '}';
            break;
    }
}

}  // namespace

const JsonValue* JsonValue::find(std::string_view key) const {
    const auto found = object.find(std::string(key));
    return found == object.end() ? nullptr : &found->second;
}

std::string JsonValue::canonical() const {
    std::string out;
    writeCanonical(out, *this);
    return out;
}

bool parseJson(std::string_view text, JsonValue& value, std::string& error) {
    Parser parser(text, error);
    return parser.parse(value);
}

bool jsonNumbersEqual(const JsonValue& lhs, const JsonValue& rhs, double tolerance) {
    if (!lhs.isNumber() || !rhs.isNumber()) {
        return false;
    }
    return std::fabs(lhs.number - rhs.number) <= tolerance;
}

}  // namespace mpl
