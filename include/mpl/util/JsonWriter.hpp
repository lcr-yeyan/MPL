#pragma once

#include <cstdint>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

namespace mpl {

/// Minimal deterministic JSON writer used for compiler artifacts. Keys are
/// emitted in the order they are written, which keeps golden files stable.
class JsonWriter {
public:
    JsonWriter& beginObject();
    JsonWriter& endObject();
    JsonWriter& beginArray();
    JsonWriter& endArray();

    JsonWriter& key(std::string_view name);

    JsonWriter& value(std::string_view text);
    JsonWriter& value(const char* text);
    JsonWriter& value(std::int64_t number);
    JsonWriter& value(int number);
    JsonWriter& value(std::size_t number);
    JsonWriter& value(double number);
    JsonWriter& value(bool flag);

    const std::string& str() const noexcept { return out_; }
    std::string take() { return std::move(out_); }

private:
    struct Frame {
        bool isObject = true;
        bool first = true;
    };

    void beforeItem();
    void beginValue();
    void indent(std::size_t depth);
    static void appendEscaped(std::string& out, std::string_view text);

    std::string out_;
    std::vector<Frame> stack_;
    bool afterKey_ = false;
};

}  // namespace mpl
