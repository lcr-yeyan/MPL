#include "mpl/temporal/TemporalIR.hpp"

#include "mpl/core/Error.hpp"
#include "mpl/util/JsonWriter.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace mpl {
namespace {

template <typename... Visitors>
struct Overloaded : Visitors... {
    using Visitors::operator()...;
};

template <typename... Visitors>
Overloaded(Visitors...) -> Overloaded<Visitors...>;

std::string describeTemporal(TemporalId id) {
    return id == kInvalidTemporalId ? std::string("<invalid>")
                                    : "#" + std::to_string(id);
}

void requireSupportedDomain(TimeDomain domain, std::string_view operation) {
    if (domain == TimeDomain::Physical || domain == TimeDomain::Sample) {
        throw Error(std::string(operation) +
                        " supports Musical and Relative domains in V0.0.3; Physical and Sample "
                        "domains are reserved for the audio runtime",
                    "temporal-domain");
    }
}

void requirePositiveExtent(const Rational& extent, std::string_view operation) {
    if (!extent.isPositive()) {
        throw Error(std::string(operation) + " requires a positive extent, got " +
                        extent.toString(),
                    "temporal");
    }
}

void requireFinite(double value, std::string_view operation) {
    if (!std::isfinite(value)) {
        throw Error(std::string(operation) + " requires finite values", "temporal");
    }
}

void validateKeyframes(const std::vector<CurvePoint>& points, const Rational& extent,
                       std::string_view operation) {
    if (points.empty()) {
        throw Error(std::string(operation) + " requires at least one keyframe", "temporal");
    }
    bool first = true;
    Rational previous{0, 1};
    for (const CurvePoint& point : points) {
        if (point.time.isNegative() || point.time > extent) {
            throw Error(std::string(operation) + " keyframe time " + point.time.toString() +
                            " is outside [0, " + extent.toString() + "]",
                        "temporal");
        }
        if (!first && !(previous < point.time)) {
            throw Error(std::string(operation) +
                            " keyframe times must be strictly increasing",
                        "temporal");
        }
        requireFinite(point.value, operation);
        previous = point.time;
        first = false;
    }
}

void validateSegments(const Curve& curve, std::string_view operation) {
    requirePositiveExtent(curve.extent, operation);
    if (curve.segments.empty()) {
        throw Error(std::string(operation) + " produced an empty curve", "temporal");
    }
    for (const CurveSegment& segment : curve.segments) {
        if (!(segment.start < segment.end)) {
            throw Error(std::string(operation) + " produced a non-positive segment", "temporal");
        }
        requireFinite(segment.from, operation);
        requireFinite(segment.to, operation);
    }
}

Curve constantCurve(double value, TimeDomain domain, const Rational& extent) {
    Curve curve;
    curve.domain = domain;
    curve.extent = extent;
    curve.segments.push_back(CurveSegment{Rational(0, 1), extent, value, value, Interpolation::Step});
    validateSegments(curve, "constant");
    return curve;
}

Curve linearCurve(double from, double to, TimeDomain domain, const Rational& extent,
                  Interpolation interpolation) {
    Curve curve;
    curve.domain = domain;
    curve.extent = extent;
    curve.segments.push_back(CurveSegment{Rational(0, 1), extent, from, to, interpolation});
    validateSegments(curve, interpolation == Interpolation::Smoothstep ? "smoothstep" : "linear");
    return curve;
}

Curve stepCurve(double from, double to, const Rational& switchAt, TimeDomain domain,
                const Rational& extent) {
    if (switchAt <= Rational(0, 1)) {
        return constantCurve(to, domain, extent);
    }
    if (switchAt >= extent) {
        return constantCurve(from, domain, extent);
    }

    Curve curve;
    curve.domain = domain;
    curve.extent = extent;
    curve.segments.push_back(
        CurveSegment{Rational(0, 1), switchAt, from, to, Interpolation::Step});
    curve.segments.push_back(
        CurveSegment{switchAt, extent, to, to, Interpolation::Step});
    validateSegments(curve, "step");
    return curve;
}

Curve keyframesCurve(const std::vector<CurvePoint>& points, TimeDomain domain,
                     const Rational& extent) {
    validateKeyframes(points, extent, "keyframes");
    if (points.size() == 1) {
        return constantCurve(points.front().value, domain, extent);
    }

    Curve curve;
    curve.domain = domain;
    curve.extent = extent;

    if (points.front().time > Rational(0, 1)) {
        curve.segments.push_back(CurveSegment{Rational(0, 1), points.front().time,
                                              points.front().value, points.front().value,
                                              Interpolation::Step});
    }

    for (std::size_t index = 0; index + 1 < points.size(); ++index) {
        const CurvePoint& left = points[index];
        const CurvePoint& right = points[index + 1];
        curve.segments.push_back(
            CurveSegment{left.time, right.time, left.value, right.value, left.interpolation});
    }

    if (points.back().time < extent) {
        curve.segments.push_back(CurveSegment{points.back().time, extent,
                                              points.back().value, points.back().value,
                                              Interpolation::Step});
    }

    validateSegments(curve, "keyframes");
    return curve;
}

Curve shiftCurve(const Curve& input, const Rational& amount) {
    Curve result = input;
    for (CurveSegment& segment : result.segments) {
        segment.start += amount;
        segment.end += amount;
    }
    return result;
}

Curve stretchCurve(const Curve& input, const Rational& factor) {
    Curve result = input;
    result.extent = input.extent * factor;
    for (CurveSegment& segment : result.segments) {
        segment.start = segment.start * factor;
        segment.end = segment.end * factor;
    }
    return result;
}

Curve reverseCurve(const Curve& input) {
    Curve result;
    result.domain = input.domain;
    result.extent = input.extent;
    result.segments.reserve(input.segments.size());
    for (const CurveSegment& segment : input.segments) {
        if (segment.interpolation == Interpolation::Step) {
            // A mirrored step keeps the pre-step value on its right side; the
            // preceding segment carries the post-step value up to the jump.
            result.segments.push_back(CurveSegment{input.extent - segment.end,
                                                   input.extent - segment.start,
                                                   segment.from, segment.from,
                                                   Interpolation::Step});
        } else {
            result.segments.push_back(CurveSegment{input.extent - segment.end,
                                                   input.extent - segment.start,
                                                   segment.to, segment.from,
                                                   segment.interpolation});
        }
    }
    std::sort(result.segments.begin(), result.segments.end(),
              [](const CurveSegment& lhs, const CurveSegment& rhs) {
                  return lhs.start < rhs.start;
              });

    // A mirrored step is left-continuous; attach the original post-step value
    // to the endpoint of the segment immediately before the mirrored jump.
    for (const CurveSegment& segment : input.segments) {
        if (segment.interpolation != Interpolation::Step) {
            continue;
        }
        const Rational jump = input.extent - segment.end;
        if (jump <= Rational(0, 1) || jump >= input.extent) {
            continue;
        }
        for (CurveSegment& candidate : result.segments) {
            if (candidate.end == jump) {
                candidate.to = segment.to;
                break;
            }
        }
    }
    return result;
}

Curve repeatCurve(const Curve& input, int count) {
    Curve result;
    result.domain = input.domain;
    result.extent = input.extent * Rational(count, 1);
    result.segments.reserve(input.segments.size() * static_cast<std::size_t>(count));
    for (int copy = 0; copy < count; ++copy) {
        const Rational shift = input.extent * Rational(copy, 1);
        for (const CurveSegment& segment : input.segments) {
            result.segments.push_back(CurveSegment{segment.start + shift, segment.end + shift,
                                                   segment.from, segment.to,
                                                   segment.interpolation});
        }
    }
    return result;
}

}  // namespace

std::string_view interpolationName(Interpolation interpolation) {
    switch (interpolation) {
        case Interpolation::Step: return "step";
        case Interpolation::Linear: return "linear";
        case Interpolation::Smoothstep: return "smoothstep";
    }
    return "unknown";
}

double Curve::sample(const Rational& time) const {
    if (segments.empty()) {
        throw Error("temporal curve has no segments", "temporal");
    }
    if (time <= segments.front().start) {
        return segments.front().from;
    }

    const CurveSegment* previous = nullptr;
    for (const CurveSegment& segment : segments) {
        if (time < segment.start) {
            return previous != nullptr ? previous->to : segment.from;
        }
        if (time < segment.end) {
            if (segment.interpolation == Interpolation::Step) {
                return segment.from;
            }
            const Rational length = segment.end - segment.start;
            const double x = ((time - segment.start) / length).toDouble();
            if (segment.interpolation == Interpolation::Smoothstep) {
                const double eased = x * x * (3.0 - 2.0 * x);
                return segment.from + (segment.to - segment.from) * eased;
            }
            return segment.from + (segment.to - segment.from) * x;
        }
        previous = &segment;
    }
    return segments.back().to;
}

double Curve::sampleDouble(double time) const {
    if (segments.empty()) {
        throw Error("temporal curve has no segments", "temporal");
    }
    if (time <= segments.front().start.toDouble()) {
        return segments.front().from;
    }

    const CurveSegment* previous = nullptr;
    for (const CurveSegment& segment : segments) {
        const double start = segment.start.toDouble();
        const double end = segment.end.toDouble();
        if (time < start) {
            return previous != nullptr ? previous->to : segment.from;
        }
        if (time < end) {
            if (segment.interpolation == Interpolation::Step) {
                return segment.from;
            }
            const double x = (time - start) / (end - start);
            if (segment.interpolation == Interpolation::Smoothstep) {
                const double eased = x * x * (3.0 - 2.0 * x);
                return segment.from + (segment.to - segment.from) * eased;
            }
            return segment.from + (segment.to - segment.from) * x;
        }
        previous = &segment;
    }
    return segments.back().to;
}

TemporalKind temporalKind(const TemporalPayload& payload) {
    switch (payload.index()) {
        case 0: return TemporalKind::Constant;
        case 1: return TemporalKind::Linear;
        case 2: return TemporalKind::Step;
        case 3: return TemporalKind::Smoothstep;
        case 4: return TemporalKind::Keyframes;
        case 5: return TemporalKind::Shift;
        case 6: return TemporalKind::Stretch;
        case 7: return TemporalKind::Reverse;
        case 8: return TemporalKind::Repeat;
        default: break;
    }
    throw Error("unknown temporal node payload", "temporal");
}

std::string_view temporalKindName(TemporalKind kind) {
    switch (kind) {
        case TemporalKind::Constant: return "constant";
        case TemporalKind::Linear: return "linear";
        case TemporalKind::Step: return "step";
        case TemporalKind::Smoothstep: return "smoothstep";
        case TemporalKind::Keyframes: return "keyframes";
        case TemporalKind::Shift: return "shift";
        case TemporalKind::Stretch: return "stretch";
        case TemporalKind::Reverse: return "reverse";
        case TemporalKind::Repeat: return "repeat";
    }
    return "unknown";
}

TemporalId TemporalIR::constant(double value, TimeDomain domain, Rational extent,
                                std::string label) {
    requireSupportedDomain(domain, "constant");
    requireFinite(value, "constant");
    requirePositiveExtent(extent, "constant");
    TemporalNode node;
    node.id = static_cast<TemporalId>(nodes_.size());
    node.label = std::move(label);
    node.domain = domain;
    node.extent = extent;
    node.payload = ConstantNode{value};
    nodes_.push_back(std::move(node));
    return nodes_.back().id;
}

TemporalId TemporalIR::linear(double from, double to, TimeDomain domain, Rational extent,
                              std::string label) {
    requireSupportedDomain(domain, "linear");
    requireFinite(from, "linear");
    requireFinite(to, "linear");
    requirePositiveExtent(extent, "linear");
    TemporalNode node;
    node.id = static_cast<TemporalId>(nodes_.size());
    node.label = std::move(label);
    node.domain = domain;
    node.extent = extent;
    node.payload = LinearNode{from, to};
    nodes_.push_back(std::move(node));
    return nodes_.back().id;
}

TemporalId TemporalIR::step(double from, double to, Rational switchAt, TimeDomain domain,
                            Rational extent, std::string label) {
    requireSupportedDomain(domain, "step");
    requireFinite(from, "step");
    requireFinite(to, "step");
    requirePositiveExtent(extent, "step");
    if (switchAt.isNegative() || switchAt > extent) {
        throw Error("step switchAt must be inside [0, extent]", "temporal");
    }
    TemporalNode node;
    node.id = static_cast<TemporalId>(nodes_.size());
    node.label = std::move(label);
    node.domain = domain;
    node.extent = extent;
    node.payload = StepNode{from, to, switchAt};
    nodes_.push_back(std::move(node));
    return nodes_.back().id;
}

TemporalId TemporalIR::smoothstep(double from, double to, TimeDomain domain, Rational extent,
                                  std::string label) {
    requireSupportedDomain(domain, "smoothstep");
    requireFinite(from, "smoothstep");
    requireFinite(to, "smoothstep");
    requirePositiveExtent(extent, "smoothstep");
    TemporalNode node;
    node.id = static_cast<TemporalId>(nodes_.size());
    node.label = std::move(label);
    node.domain = domain;
    node.extent = extent;
    node.payload = SmoothstepNode{from, to};
    nodes_.push_back(std::move(node));
    return nodes_.back().id;
}

TemporalId TemporalIR::keyframes(std::vector<CurvePoint> points, TimeDomain domain,
                                 Rational extent, std::string label) {
    requireSupportedDomain(domain, "keyframes");
    requirePositiveExtent(extent, "keyframes");
    validateKeyframes(points, extent, "keyframes");
    TemporalNode node;
    node.id = static_cast<TemporalId>(nodes_.size());
    node.label = std::move(label);
    node.domain = domain;
    node.extent = extent;
    node.payload = KeyframesNode{std::move(points)};
    nodes_.push_back(std::move(node));
    return nodes_.back().id;
}

TemporalId TemporalIR::shift(TemporalId input, const Rational& amount, std::string label) {
    return appendTransform(input, CurveShiftNode{amount}, std::move(label), "shift");
}

TemporalId TemporalIR::stretch(TemporalId input, const Rational& factor, std::string label) {
    if (!factor.isPositive()) {
        throw Error("temporal stretch factor must be positive", "temporal");
    }
    return appendTransform(input, CurveStretchNode{factor}, std::move(label), "stretch");
}

TemporalId TemporalIR::reverse(TemporalId input, std::string label) {
    return appendTransform(input, CurveReverseNode{}, std::move(label), "reverse");
}

TemporalId TemporalIR::repeat(TemporalId input, int count, std::string label) {
    if (count < 1) {
        throw Error("temporal repeat count must be at least 1", "temporal");
    }
    return appendTransform(input, CurveRepeatNode{count}, std::move(label), "repeat");
}

TemporalId TemporalIR::appendTransform(TemporalId input, TemporalPayload payload,
                                       std::string label, std::string_view operation) {
    if (!contains(input)) {
        throw Error(std::string(operation) + " references unknown temporal object " +
                        describeTemporal(input),
                    "temporal");
    }
    TemporalNode node;
    node.id = static_cast<TemporalId>(nodes_.size());
    node.label = std::move(label);
    node.domain = nodes_[input].domain;
    node.extent = nodes_[input].extent;
    node.inputs = {input};
    node.payload = std::move(payload);
    if (auto* stretchNode = std::get_if<CurveStretchNode>(&node.payload)) {
        node.extent = node.extent * stretchNode->factor;
    } else if (auto* repeatNode = std::get_if<CurveRepeatNode>(&node.payload)) {
        node.extent = node.extent * Rational(repeatNode->count, 1);
    }
    nodes_.push_back(std::move(node));
    return nodes_.back().id;
}

const TemporalNode& TemporalIR::node(TemporalId id) const {
    if (!contains(id)) {
        throw Error("unknown temporal object " + describeTemporal(id), "temporal");
    }
    return nodes_[id];
}

TemporalNode& TemporalIR::mutableNode(TemporalId id) {
    if (!contains(id)) {
        throw Error("unknown temporal object " + describeTemporal(id), "temporal");
    }
    return nodes_[id];
}

Curve TemporalIR::evaluateNode(const TemporalNode& node) const {
    return std::visit(
        Overloaded{
            [&](const ConstantNode& constantNode) {
                return constantCurve(constantNode.value, node.domain, node.extent);
            },
            [&](const LinearNode& linearNode) {
                return linearCurve(linearNode.from, linearNode.to, node.domain, node.extent,
                                   Interpolation::Linear);
            },
            [&](const StepNode& stepNode) {
                return stepCurve(stepNode.from, stepNode.to, stepNode.switchAt, node.domain,
                                 node.extent);
            },
            [&](const SmoothstepNode& smoothstepNode) {
                return linearCurve(smoothstepNode.from, smoothstepNode.to, node.domain,
                                   node.extent, Interpolation::Smoothstep);
            },
            [&](const KeyframesNode& keyframesNode) {
                return keyframesCurve(keyframesNode.points, node.domain, node.extent);
            },
            [&](const CurveShiftNode& shiftNode) {
                return shiftCurve(cachedInput(node.inputs[0]), shiftNode.amount);
            },
            [&](const CurveStretchNode& stretchNode) {
                return stretchCurve(cachedInput(node.inputs[0]), stretchNode.factor);
            },
            [&](const CurveReverseNode&) { return reverseCurve(cachedInput(node.inputs[0])); },
            [&](const CurveRepeatNode& repeatNode) {
                return repeatCurve(cachedInput(node.inputs[0]), repeatNode.count);
            },
        },
        node.payload);
}

const Curve& TemporalIR::cachedInput(TemporalId input) const {
    return evaluate(input);
}

const Curve& TemporalIR::evaluate(TemporalId id) const {
    const TemporalNode& target = node(id);
    if (target.value.has_value()) {
        ++cacheHits_;
        return *target.value;
    }
    Curve evaluated = evaluateNode(target);
    nodes_[id].value = std::move(evaluated);
    ++evaluations_;
    return *nodes_[id].value;
}

void TemporalIR::evaluateAll() const {
    for (std::size_t index = 0; index < nodes_.size(); ++index) {
        evaluate(static_cast<TemporalId>(index));
    }
}

std::size_t TemporalIR::evaluatedCount() const noexcept {
    return static_cast<std::size_t>(
        std::count_if(nodes_.begin(), nodes_.end(),
                      [](const TemporalNode& node) { return node.value.has_value(); }));
}

TimeDomain TemporalIR::domain(TemporalId id) const {
    return node(id).domain;
}

const Rational& TemporalIR::extent(TemporalId id) const {
    return node(id).extent;
}

double TemporalIR::sample(TemporalId id, const Rational& domainTime) const {
    return evaluate(id).sample(domainTime);
}

double TemporalIR::sampleDouble(TemporalId id, double domainTime) const {
    return evaluate(id).sampleDouble(domainTime);
}

double TemporalIR::sampleAt(TemporalId id, const MusicalTime& time) const {
    const Curve& curve = evaluate(id);
    if (curve.domain != TimeDomain::Musical) {
        throw Error("temporal object " + describeTemporal(id) + " is not in the musical domain",
                    "temporal-domain");
    }
    return curve.sample(time.wholeNotes());
}

double TemporalIR::sampleAt(TemporalId id, const RelativeTime& time) const {
    const Curve& curve = evaluate(id);
    if (curve.domain != TimeDomain::Relative) {
        throw Error("temporal object " + describeTemporal(id) + " is not in the relative domain",
                    "temporal-domain");
    }
    return curve.sample(time.fraction());
}

double TemporalIR::sampleAt(TemporalId id, const PhysicalTime& time) const {
    const Curve& curve = evaluate(id);
    if (curve.domain != TimeDomain::Physical) {
        throw Error("physical-domain temporal curves are reserved for the audio runtime",
                    "temporal-domain");
    }
    return curve.sample(Rational(static_cast<std::int64_t>(time.seconds() * 1000000.0), 1000000));
}

double TemporalIR::sampleAt(TemporalId id, const SampleTime& time) const {
    const Curve& curve = evaluate(id);
    if (curve.domain != TimeDomain::Sample) {
        throw Error("sample-domain temporal curves are reserved for the audio runtime",
                    "temporal-domain");
    }
    return curve.sample(Rational(time.samples(), 1));
}

std::vector<TemporalId> TemporalIR::dependents(TemporalId id) const {
    if (!contains(id)) {
        throw Error("unknown temporal object " + describeTemporal(id), "temporal");
    }

    std::vector<TemporalId> result;
    std::vector<char> visited(nodes_.size(), 0);
    std::vector<TemporalId> frontier{id};
    visited[id] = 1;

    while (!frontier.empty()) {
        const TemporalId current = frontier.back();
        frontier.pop_back();
        for (const TemporalNode& node : nodes_) {
            if (visited[node.id] != 0) {
                continue;
            }
            if (std::find(node.inputs.begin(), node.inputs.end(), current) != node.inputs.end()) {
                visited[node.id] = 1;
                result.push_back(node.id);
                frontier.push_back(node.id);
            }
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

void TemporalIR::invalidate(TemporalId id) {
    std::vector<TemporalId> affected = dependents(id);
    affected.push_back(id);
    for (const TemporalId object : affected) {
        nodes_[object].value.reset();
    }
}

void TemporalIR::updateConstant(TemporalId id, double value) {
    requireFinite(value, "constant");
    TemporalNode& target = mutableNode(id);
    auto* constantNode = std::get_if<ConstantNode>(&target.payload);
    if (constantNode == nullptr) {
        throw Error("temporal object " + describeTemporal(id) + " is not a constant", "temporal");
    }
    constantNode->value = value;
    invalidate(id);
}

void TemporalIR::updateLinear(TemporalId id, double from, double to) {
    requireFinite(from, "linear");
    requireFinite(to, "linear");
    TemporalNode& target = mutableNode(id);
    auto* linearNode = std::get_if<LinearNode>(&target.payload);
    if (linearNode == nullptr) {
        throw Error("temporal object " + describeTemporal(id) + " is not a linear curve",
                    "temporal");
    }
    linearNode->from = from;
    linearNode->to = to;
    invalidate(id);
}

void TemporalIR::updateKeyframes(TemporalId id, std::vector<CurvePoint> points) {
    TemporalNode& target = mutableNode(id);
    auto* keyframesNode = std::get_if<KeyframesNode>(&target.payload);
    if (keyframesNode == nullptr) {
        throw Error("temporal object " + describeTemporal(id) + " is not a keyframe curve",
                    "temporal");
    }
    validateKeyframes(points, target.extent, "keyframes");
    keyframesNode->points = std::move(points);
    invalidate(id);
}

std::string TemporalIR::toJson() const {
    JsonWriter json;
    json.beginObject();
    json.key("format").value("mpl.temporal-ir");
    json.key("version").value(1);
    json.key("objects").beginArray();

    for (const TemporalNode& node : nodes_) {
        const TemporalKind kind = temporalKind(node.payload);
        json.beginObject();
        json.key("id").value(static_cast<std::int64_t>(node.id));
        json.key("kind").value(temporalKindName(kind));
        json.key("label").value(node.label);
        json.key("domain").value(timeDomainName(node.domain));
        json.key("extent").value(node.extent.toString());
        json.key("inputs").beginArray();
        for (const TemporalId input : node.inputs) {
            json.value(static_cast<std::int64_t>(input));
        }
        json.endArray();

        if (kind == TemporalKind::Constant) {
            json.key("parameters").beginObject();
            json.key("value").value(std::get<ConstantNode>(node.payload).value);
            json.endObject();
        } else if (kind == TemporalKind::Linear) {
            json.key("parameters").beginObject();
            json.key("from").value(std::get<LinearNode>(node.payload).from);
            json.key("to").value(std::get<LinearNode>(node.payload).to);
            json.endObject();
        } else if (kind == TemporalKind::Step) {
            const StepNode& stepNode = std::get<StepNode>(node.payload);
            json.key("parameters").beginObject();
            json.key("from").value(stepNode.from);
            json.key("to").value(stepNode.to);
            json.key("switchAt").value(stepNode.switchAt.toString());
            json.endObject();
        } else if (kind == TemporalKind::Smoothstep) {
            json.key("parameters").beginObject();
            json.key("from").value(std::get<SmoothstepNode>(node.payload).from);
            json.key("to").value(std::get<SmoothstepNode>(node.payload).to);
            json.endObject();
        } else if (kind == TemporalKind::Keyframes) {
            json.key("parameters").beginObject();
            json.key("points").beginArray();
            for (const CurvePoint& point : std::get<KeyframesNode>(node.payload).points) {
                json.beginObject();
                json.key("time").value(point.time.toString());
                json.key("value").value(point.value);
                json.key("interpolation").value(interpolationName(point.interpolation));
                json.endObject();
            }
            json.endArray();
            json.endObject();
        } else if (kind == TemporalKind::Shift) {
            json.key("parameters").beginObject();
            json.key("amount").value(std::get<CurveShiftNode>(node.payload).amount.toString());
            json.endObject();
        } else if (kind == TemporalKind::Stretch) {
            json.key("parameters").beginObject();
            json.key("factor").value(std::get<CurveStretchNode>(node.payload).factor.toString());
            json.endObject();
        } else if (kind == TemporalKind::Repeat) {
            json.key("parameters").beginObject();
            json.key("count").value(std::get<CurveRepeatNode>(node.payload).count);
            json.endObject();
        }

        if (node.value.has_value()) {
            json.key("value").beginObject();
            json.key("extent").value(node.value->extent.toString());
            json.key("segments").beginArray();
            for (const CurveSegment& segment : node.value->segments) {
                json.beginObject();
                json.key("start").value(segment.start.toString());
                json.key("end").value(segment.end.toString());
                json.key("from").value(segment.from);
                json.key("to").value(segment.to);
                json.key("interpolation").value(interpolationName(segment.interpolation));
                json.endObject();
            }
            json.endArray();
            json.endObject();
        }

        json.endObject();
    }

    json.endArray();
    json.endObject();
    return json.take();
}

}  // namespace mpl
