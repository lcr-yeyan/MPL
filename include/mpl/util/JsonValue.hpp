#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace mpl {

/// Minimal JSON value used by the external task verifier and experiment
/// harness. It intentionally supports only the JSON subset needed by task
/// files; it is not a general-purpose JSON library.
struct JsonValue {
    enum class Type { Null, Bool, Number, String, Array, Object };

    Type type = Type::Null;
    bool boolean = false;
    double number = 0.0;
    std::string text;
    std::vector<JsonValue> array;
    std::map<std::string, JsonValue> object;

    bool isNull() const { return type == Type::Null; }
    bool isBool() const { return type == Type::Bool; }
    bool isNumber() const { return type == Type::Number; }
    bool isString() const { return type == Type::String; }
    bool isArray() const { return type == Type::Array; }
    bool isObject() const { return type == Type::Object; }

    const JsonValue* find(std::string_view key) const;
    std::string canonical() const;
};

/// Parses JSON; returns false and fills `error` on malformed input.
bool parseJson(std::string_view text, JsonValue& value, std::string& error);

bool jsonNumbersEqual(const JsonValue& lhs, const JsonValue& rhs, double tolerance);

}  // namespace mpl
