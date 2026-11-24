#pragma once

#include <stdexcept>
#include <string>
#include <utility>

namespace mpl {

/// Root exception for all diagnostics raised by the compiler core.
class Error : public std::runtime_error {
public:
    explicit Error(std::string message, std::string code = {})
        : std::runtime_error(code.empty() ? message : code + ": " + message),
          code_(std::move(code)) {}

    const std::string& code() const noexcept { return code_; }

private:
    std::string code_;
};

}  // namespace mpl
