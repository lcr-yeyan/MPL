#pragma once

#include "mpl/core/Rational.hpp"
#include "mpl/core/TimeTypes.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace mpl {

enum class TypeKind {
    Error,
    Number,
    Integer,
    Boolean,
    Scalar,
    Duration,
    Position,
    Interval,
    ScaleDegree,
    Tempo,
    Frequency,
    Gain,
    PhysicalTime,
    Pitch,
    Key,
    String,
    Fragment,
    Rhythm,
    Chord,
    Harmony,
    Voicing,
    Curve,
    List,
    Asset,
    Track,
    Section,
    Processor,
    Bus,
    Function,
};

struct Type {
    TypeKind kind = TypeKind::Error;
    TypeKind element = TypeKind::Error;  // List element
    TimeDomain domain = TimeDomain::Relative;  // Curve domain

    static Type simple(TypeKind kind) { return Type{kind, TypeKind::Error, TimeDomain::Relative}; }
    static Type curve(TimeDomain domain) {
        return Type{TypeKind::Curve, TypeKind::Error, domain};
    }
    static Type list(TypeKind element) {
        return Type{TypeKind::List, element, TimeDomain::Relative};
    }

    bool isError() const noexcept { return kind == TypeKind::Error; }
    bool isFragment() const noexcept { return kind == TypeKind::Fragment; }
    bool isCurve() const noexcept { return kind == TypeKind::Curve; }
};

std::string_view typeKindName(TypeKind kind);

std::string typeName(const Type& type);

struct Constant {
    enum class Kind { None, Rational, Number, Integer, Text, Metric };
    enum class MetricUnit { None, Bars, Beats };
    Kind kind = Kind::None;
    Rational rational{0, 1};
    double number = 0.0;
    std::int64_t integer = 0;
    std::string text;
    /// Set when the literal was written with a metric unit. `rational` then
    /// holds the value folded under the project meter for static validation;
    /// lowering re-resolves `metricAmount` against the actual meter context.
    MetricUnit metricUnit = MetricUnit::None;
    Rational metricAmount{0, 1};

    static Constant none() { return Constant{}; }
    static Constant rationalValue(const Rational& value) {
        Constant constant;
        constant.kind = Kind::Rational;
        constant.rational = value;
        constant.number = value.toDouble();
        return constant;
    }
    static Constant numberValue(double value) {
        Constant constant;
        constant.kind = Kind::Number;
        constant.number = value;
        return constant;
    }
    static Constant integerValue(std::int64_t value) {
        Constant constant;
        constant.kind = Kind::Integer;
        constant.integer = value;
        constant.number = static_cast<double>(value);
        constant.rational = Rational(value, 1);
        return constant;
    }
    static Constant metricValue(const Rational& amount, MetricUnit unit,
                                const Rational& folded) {
        Constant constant;
        constant.kind = Kind::Metric;
        constant.metricAmount = amount;
        constant.metricUnit = unit;
        constant.rational = folded;
        constant.number = folded.toDouble();
        return constant;
    }
};

}  // namespace mpl
