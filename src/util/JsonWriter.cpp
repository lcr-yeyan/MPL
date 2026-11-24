#include "mpl/util/JsonWriter.hpp"

#include <cstdio>
#include <utility>

namespace mpl {

void JsonWriter::indent(std::size_t depth) {
    out_.append(depth * 2, ' ');
}

void JsonWriter::beforeItem() {
    if (stack_.empty()) {
        return;
    }
    Frame& frame = stack_.back();
    if (!frame.first) {
        out_.push_back(',');
    }
    frame.first = false;
    out_.push_back('\n');
    indent(stack_.size());
}

void JsonWriter::beginValue() {
    if (afterKey_) {
        afterKey_ = false;
        return;
    }
    beforeItem();
}

JsonWriter& JsonWriter::beginObject() {
    beginValue();
    out_.push_back('{');
    stack_.push_back(Frame{true, true});
    return *this;
}

JsonWriter& JsonWriter::endObject() {
    Frame frame = stack_.back();
    stack_.pop_back();
    if (!frame.first) {
        out_.push_back('\n');
        indent(stack_.size());
    }
    out_.push_back('}');
    return *this;
}

JsonWriter& JsonWriter::beginArray() {
    beginValue();
    out_.push_back('[');
    stack_.push_back(Frame{false, true});
    return *this;
}

JsonWriter& JsonWriter::endArray() {
    Frame frame = stack_.back();
    stack_.pop_back();
    if (!frame.first) {
        out_.push_back('\n');
        indent(stack_.size());
    }
    out_.push_back(']');
    return *this;
}

JsonWriter& JsonWriter::key(std::string_view name) {
    beforeItem();
    appendEscaped(out_, name);
    out_.push_back(':');
    out_.push_back(' ');
    afterKey_ = true;
    return *this;
}

void JsonWriter::appendEscaped(std::string& out, std::string_view text) {
    out.push_back('"');
    for (const char character : text) {
        switch (character) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(character) < 0x20) {
                    char buffer[8];
                    std::snprintf(buffer, sizeof(buffer), "\\u%04x",
                                  static_cast<unsigned int>(static_cast<unsigned char>(character)));
                    out += buffer;
                } else {
                    out.push_back(character);
                }
                break;
        }
    }
    out.push_back('"');
}

JsonWriter& JsonWriter::value(std::string_view text) {
    beginValue();
    appendEscaped(out_, text);
    return *this;
}

JsonWriter& JsonWriter::value(const char* text) {
    return value(std::string_view(text == nullptr ? "" : text));
}

JsonWriter& JsonWriter::value(std::int64_t number) {
    beginValue();
    out_ += std::to_string(number);
    return *this;
}

JsonWriter& JsonWriter::value(int number) {
    return value(static_cast<std::int64_t>(number));
}

JsonWriter& JsonWriter::value(std::size_t number) {
    beginValue();
    out_ += std::to_string(number);
    return *this;
}

JsonWriter& JsonWriter::value(double number) {
    beginValue();
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.10g", number);
    out_ += buffer;
    return *this;
}

JsonWriter& JsonWriter::value(bool flag) {
    beginValue();
    out_ += flag ? "true" : "false";
    return *this;
}

}  // namespace mpl
