#pragma once

#include "mpl/core/Error.hpp"

#include <cmath>
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace mpltest {

struct TestCase {
    std::string name;
    std::function<void()> fn;
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;
    return tests;
}

/// When enabled, golden tests rewrite their expected files instead of failing.
inline bool& updateGoldenMode() {
    static bool enabled = false;
    return enabled;
}

struct Registrar {
    Registrar(std::string name, std::function<void()> fn) {
        registry().push_back(TestCase{std::move(name), std::move(fn)});
    }
};

class AssertionFailure : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

inline std::string toText(const std::string& value) {
    return "\"" + value + "\"";
}

template <typename T>
std::string toText(const T& value) {
    std::ostringstream stream;
    stream << value;
    return stream.str();
}

inline void fail(const char* file, int line, const std::string& message) {
    throw AssertionFailure(std::string(file) + ":" + std::to_string(line) + ": " + message);
}

}  // namespace mpltest

#define MPL_TEST(name)                                                             \
    static void mpl_test_##name();                                                 \
    static ::mpltest::Registrar mpl_test_registrar_##name(#name, mpl_test_##name); \
    static void mpl_test_##name()

#define MPL_CHECK(condition)                                                       \
    do {                                                                           \
        if (!(condition)) {                                                        \
            ::mpltest::fail(__FILE__, __LINE__, "check failed: " #condition);      \
        }                                                                          \
    } while (false)

#define MPL_CHECK_EQ(actual, expected)                                             \
    do {                                                                           \
        const auto& mpl_actual_value = (actual);                                   \
        const auto& mpl_expected_value = (expected);                               \
        if (!(mpl_actual_value == mpl_expected_value)) {                           \
            ::mpltest::fail(__FILE__, __LINE__,                                    \
                            std::string("expected ") +                             \
                                ::mpltest::toText(mpl_expected_value) +            \
                                " but got " + ::mpltest::toText(mpl_actual_value)); \
        }                                                                          \
    } while (false)

#define MPL_CHECK_NEAR(actual, expected, tolerance)                                \
    do {                                                                           \
        const double mpl_actual_value = (actual);                                  \
        const double mpl_expected_value = (expected);                              \
        if (std::fabs(mpl_actual_value - mpl_expected_value) > (tolerance)) {      \
            ::mpltest::fail(__FILE__, __LINE__,                                    \
                            std::string("expected ") +                             \
                                ::mpltest::toText(mpl_expected_value) +            \
                                " but got " + ::mpltest::toText(mpl_actual_value)); \
        }                                                                          \
    } while (false)

#define MPL_CHECK_THROWS(expression)                                               \
    do {                                                                           \
        bool mpl_threw = false;                                                    \
        try {                                                                      \
            (void)(expression);                                                    \
        } catch (const ::mpl::Error&) {                                            \
            mpl_threw = true;                                                      \
        }                                                                          \
        if (!mpl_threw) {                                                          \
            ::mpltest::fail(__FILE__, __LINE__,                                    \
                            "expected mpl::Error from: " #expression);             \
        }                                                                          \
    } while (false)
