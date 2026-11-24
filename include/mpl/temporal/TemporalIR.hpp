#pragma once

#include "mpl/core/Ids.hpp"
#include "mpl/core/Rational.hpp"
#include "mpl/core/Time.hpp"
#include "mpl/core/TimeTypes.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace mpl {

using TemporalId = std::uint32_t;

inline constexpr TemporalId kInvalidTemporalId = 0xFFFFFFFFu;

enum class Interpolation { Step, Linear, Smoothstep };

std::string_view interpolationName(Interpolation interpolation);

/// One entry of a serializable keyframe list. `interpolation` describes the
/// segment that starts at this point.
struct CurvePoint {
    Rational time{0, 1};
    double value = 0.0;
    Interpolation interpolation = Interpolation::Linear;

    friend bool operator==(const CurvePoint& lhs, const CurvePoint& rhs) {
        return lhs.time == rhs.time && lhs.value == rhs.value &&
               lhs.interpolation == rhs.interpolation;
    }
};

/// Evaluated, serializable curve. Segments are sorted, non-overlapping and
/// evaluated right-continuously: a segment covers [start, end).
struct CurveSegment {
    Rational start{0, 1};
    Rational end{0, 1};
    double from = 0.0;
    double to = 0.0;
    Interpolation interpolation = Interpolation::Linear;
};

struct Curve {
    TimeDomain domain = TimeDomain::Relative;
    Rational extent{1, 1};
    std::vector<CurveSegment> segments;

    /// Samples the curve at a time expressed in the curve's own domain unit.
    /// Times outside the covered range hold the first/last value.
    double sample(const Rational& time) const;

    /// Double-precision variant used by audio-rate control evaluation.
    double sampleDouble(double time) const;
};

struct ConstantNode {
    double value = 0.0;
};

struct LinearNode {
    double from = 0.0;
    double to = 1.0;
};

struct StepNode {
    double from = 0.0;
    double to = 1.0;
    Rational switchAt{1, 2};
};

struct SmoothstepNode {
    double from = 0.0;
    double to = 1.0;
};

struct KeyframesNode {
    std::vector<CurvePoint> points;
};

struct CurveShiftNode {
    Rational amount{0, 1};
};

struct CurveStretchNode {
    Rational factor{1, 1};
};

struct CurveReverseNode {};

struct CurveRepeatNode {
    int count = 1;
};

using TemporalPayload =
    std::variant<ConstantNode, LinearNode, StepNode, SmoothstepNode, KeyframesNode,
                 CurveShiftNode, CurveStretchNode, CurveReverseNode, CurveRepeatNode>;

enum class TemporalKind {
    Constant,
    Linear,
    Step,
    Smoothstep,
    Keyframes,
    Shift,
    Stretch,
    Reverse,
    Repeat,
};

struct TemporalNode {
    TemporalId id = kInvalidTemporalId;
    std::string label;
    TimeDomain domain = TimeDomain::Relative;
    Rational extent{1, 1};
    std::vector<TemporalId> inputs;
    TemporalPayload payload = ConstantNode{};
    mutable std::optional<Curve> value;
};

/// Minimal temporal dependency graph. Curves are serializable value graphs, not
/// arbitrary function objects. V0.0.3 supports the Musical and Relative
/// domains; Physical and Sample domains are reserved for the audio runtime.
class TemporalIR {
public:
    TemporalId constant(double value, TimeDomain domain = TimeDomain::Relative,
                        Rational extent = {1, 1}, std::string label = {});
    TemporalId linear(double from, double to, TimeDomain domain = TimeDomain::Relative,
                      Rational extent = {1, 1}, std::string label = {});
    TemporalId step(double from, double to, Rational switchAt,
                    TimeDomain domain = TimeDomain::Relative, Rational extent = {1, 1},
                    std::string label = {});
    TemporalId smoothstep(double from, double to, TimeDomain domain = TimeDomain::Relative,
                          Rational extent = {1, 1}, std::string label = {});
    TemporalId keyframes(std::vector<CurvePoint> points,
                         TimeDomain domain = TimeDomain::Relative, Rational extent = {1, 1},
                         std::string label = {});

    TemporalId shift(TemporalId input, const Rational& amount, std::string label = {});
    TemporalId stretch(TemporalId input, const Rational& factor, std::string label = {});
    TemporalId reverse(TemporalId input, std::string label = {});
    TemporalId repeat(TemporalId input, int count, std::string label = {});

    const std::vector<TemporalNode>& nodes() const noexcept { return nodes_; }
    const TemporalNode& node(TemporalId id) const;
    bool contains(TemporalId id) const noexcept { return id < nodes_.size(); }
    std::size_t size() const noexcept { return nodes_.size(); }
    bool empty() const noexcept { return nodes_.empty(); }

    const Curve& evaluate(TemporalId id) const;
    void evaluateAll() const;

    std::size_t evaluatedCount() const noexcept;
    std::size_t evaluationCount() const noexcept { return evaluations_; }
    std::size_t cacheHitCount() const noexcept { return cacheHits_; }

    TimeDomain domain(TemporalId id) const;
    const Rational& extent(TemporalId id) const;

    /// Samples at a time expressed in the curve domain's unit.
    double sample(TemporalId id, const Rational& domainTime) const;
    double sampleDouble(TemporalId id, double domainTime) const;
    double sampleAt(TemporalId id, const MusicalTime& time) const;
    double sampleAt(TemporalId id, const RelativeTime& time) const;
    double sampleAt(TemporalId id, const PhysicalTime& time) const;
    double sampleAt(TemporalId id, const SampleTime& time) const;

    std::vector<TemporalId> dependents(TemporalId id) const;
    void invalidate(TemporalId id);

    /// Parameter edits kept for incremental-compilation experiments.
    void updateConstant(TemporalId id, double value);
    void updateLinear(TemporalId id, double from, double to);
    void updateKeyframes(TemporalId id, std::vector<CurvePoint> points);

    std::string toJson() const;

private:
    TemporalNode& mutableNode(TemporalId id);
    TemporalId append(TemporalNode node);
    TemporalId appendTransform(TemporalId input, TemporalPayload payload, std::string label,
                               std::string_view operation);
    Curve evaluateNode(const TemporalNode& node) const;
    const Curve& cachedInput(TemporalId input) const;

    std::vector<TemporalNode> nodes_;
    mutable std::size_t evaluations_ = 0;
    mutable std::size_t cacheHits_ = 0;
};

TemporalKind temporalKind(const TemporalPayload& payload);

std::string_view temporalKindName(TemporalKind kind);

}  // namespace mpl
