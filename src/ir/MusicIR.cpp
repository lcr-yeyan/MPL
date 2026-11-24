#include "mpl/ir/MusicIR.hpp"

#include "mpl/core/Error.hpp"
#include "mpl/util/JsonWriter.hpp"

#include <algorithm>
#include <utility>

namespace mpl {
namespace {

template <typename... Visitors>
struct Overloaded : Visitors... {
    using Visitors::operator()...;
};

template <typename... Visitors>
Overloaded(Visitors...) -> Overloaded<Visitors...>;

std::string describeObject(ObjectId id) {
    return id == kInvalidObjectId ? std::string("<invalid>") : "#" + std::to_string(id);
}

bool isPowerOfTwo(int value) {
    return value > 0 && (value & (value - 1)) == 0;
}

}  // namespace

NodeKind nodeKind(const NodePayload& payload) {
    switch (payload.index()) {
        case 0: return NodeKind::Literal;
        case 1: return NodeKind::Transpose;
        case 2: return NodeKind::Reverse;
        case 3: return NodeKind::Stretch;
        case 4: return NodeKind::Repeat;
        case 5: return NodeKind::Sequence;
        case 6: return NodeKind::Parallel;
        case 7: return NodeKind::DiatonicTranspose;
        case 8: return NodeKind::Harmonize;
        case 9: return NodeKind::Syncopate;
        default: break;
    }
    throw Error("unknown music IR node payload", "music-ir");
}

std::string_view nodeKindName(NodeKind kind) {
    switch (kind) {
        case NodeKind::Literal: return "literal";
        case NodeKind::Transpose: return "transpose";
        case NodeKind::Reverse: return "reverse";
        case NodeKind::Stretch: return "stretch";
        case NodeKind::Repeat: return "repeat";
        case NodeKind::Sequence: return "sequence";
        case NodeKind::Parallel: return "parallel";
        case NodeKind::DiatonicTranspose: return "diatonic-transpose";
        case NodeKind::Harmonize: return "harmonize";
        case NodeKind::Syncopate: return "syncopate";
    }
    return "unknown";
}

std::vector<ContextAspect> contextDependencies(NodeKind kind) {
    switch (kind) {
        case NodeKind::DiatonicTranspose:
        case NodeKind::Harmonize:
            return {ContextAspect::Key};
        case NodeKind::Syncopate:
            return {ContextAspect::Meter};
        default:
            return {};
    }
}

std::string_view contextAspectName(ContextAspect aspect) {
    return aspect == ContextAspect::Key ? "key" : "meter";
}

ObjectId MusicIR::melody(const std::vector<std::pair<Pitch, Rational>>& values,
                         std::string label) {
    const Fragment fragment = Fragment::fromMelody(values);
    return notes(fragment.notes(), fragment.duration(), std::move(label));
}

ObjectId MusicIR::notes(std::vector<Note> notes, std::optional<Duration> span,
                        std::string label) {
    MusicNode node;
    node.id = static_cast<ObjectId>(nodes_.size());
    node.label = std::move(label);
    for (Note& note : notes) {
        note.origin = node.id;
    }
    node.payload = LiteralNode{Fragment::fromNotes(std::move(notes), span)};
    nodes_.push_back(std::move(node));
    return nodes_.back().id;
}

ObjectId MusicIR::transpose(ObjectId input, Interval interval, std::string label) {
    requireInputs({input}, 1, "transpose");
    MusicNode node;
    node.id = static_cast<ObjectId>(nodes_.size());
    node.label = std::move(label);
    node.inputs = {input};
    node.payload = TransposeNode{interval};
    nodes_.push_back(std::move(node));
    return nodes_.back().id;
}

ObjectId MusicIR::reverse(ObjectId input, std::string label) {
    requireInputs({input}, 1, "reverse");
    MusicNode node;
    node.id = static_cast<ObjectId>(nodes_.size());
    node.label = std::move(label);
    node.inputs = {input};
    node.payload = ReverseNode{};
    nodes_.push_back(std::move(node));
    return nodes_.back().id;
}

ObjectId MusicIR::stretch(ObjectId input, const Rational& factor, std::string label) {
    requireInputs({input}, 1, "stretch");
    if (!factor.isPositive()) {
        throw Error("stretch factor must be positive, got " + factor.toString(), "stretch");
    }
    MusicNode node;
    node.id = static_cast<ObjectId>(nodes_.size());
    node.label = std::move(label);
    node.inputs = {input};
    node.payload = StretchNode{factor};
    nodes_.push_back(std::move(node));
    return nodes_.back().id;
}

ObjectId MusicIR::repeat(ObjectId input, int count, std::string label) {
    requireInputs({input}, 1, "repeat");
    if (count < 1) {
        throw Error("repeat count must be at least 1, got " + std::to_string(count), "repeat");
    }
    MusicNode node;
    node.id = static_cast<ObjectId>(nodes_.size());
    node.label = std::move(label);
    node.inputs = {input};
    node.payload = RepeatNode{count};
    nodes_.push_back(std::move(node));
    return nodes_.back().id;
}

ObjectId MusicIR::sequence(const std::vector<ObjectId>& inputs, std::string label) {
    requireInputs(inputs, 1, "sequence");
    MusicNode node;
    node.id = static_cast<ObjectId>(nodes_.size());
    node.label = std::move(label);
    node.inputs = inputs;
    node.payload = SequenceNode{};
    nodes_.push_back(std::move(node));
    return nodes_.back().id;
}

ObjectId MusicIR::parallel(const std::vector<ObjectId>& inputs, std::string label) {
    requireInputs(inputs, 1, "parallel");
    MusicNode node;
    node.id = static_cast<ObjectId>(nodes_.size());
    node.label = std::move(label);
    node.inputs = inputs;
    node.payload = ParallelNode{};
    nodes_.push_back(std::move(node));
    return nodes_.back().id;
}

ObjectId MusicIR::diatonicTranspose(ObjectId input, int scaleSteps, std::string label) {
    requireInputs({input}, 1, "diatonic-transpose");
    MusicNode node;
    node.id = static_cast<ObjectId>(nodes_.size());
    node.label = std::move(label);
    node.inputs = {input};
    node.payload = DiatonicTransposeNode{scaleSteps};
    nodes_.push_back(std::move(node));
    return nodes_.back().id;
}

ObjectId MusicIR::harmonize(ObjectId input, std::vector<int> degreeOffsets,
                            std::string label) {
    requireInputs({input}, 1, "harmonize");
    if (degreeOffsets.empty()) {
        throw Error("harmonize requires at least one scale-degree offset", "harmonize");
    }
    MusicNode node;
    node.id = static_cast<ObjectId>(nodes_.size());
    node.label = std::move(label);
    node.inputs = {input};
    node.payload = HarmonizeNode{std::move(degreeOffsets), 80};
    nodes_.push_back(std::move(node));
    return nodes_.back().id;
}

ObjectId MusicIR::syncopate(ObjectId input, const Rational& displacement,
                            std::string label) {
    requireInputs({input}, 1, "syncopate");
    MusicNode node;
    node.id = static_cast<ObjectId>(nodes_.size());
    node.label = std::move(label);
    node.inputs = {input};
    node.payload = SyncopateNode{displacement};
    nodes_.push_back(std::move(node));
    return nodes_.back().id;
}

void MusicIR::setKey(Key key) {
    key_ = std::move(key);
    invalidateContext(ContextAspect::Key);
}

void MusicIR::setMeter(Meter meter) {
    if (meter.numerator < 1 || !isPowerOfTwo(meter.denominator)) {
        throw Error("meter " + meter.toString() +
                        " must have a positive numerator and a power-of-two denominator",
                    "meter");
    }
    meter_ = meter;
    invalidateContext(ContextAspect::Meter);
}

void MusicIR::invalidateContext(ContextAspect aspect) {
    std::vector<ObjectId> affected;
    for (const MusicNode& node : nodes_) {
        const std::vector<ContextAspect> dependencies = contextDependencies(nodeKind(node.payload));
        if (std::find(dependencies.begin(), dependencies.end(), aspect) != dependencies.end()) {
            affected.push_back(node.id);
        }
    }
    for (const ObjectId id : affected) {
        invalidate(id);
    }
}

void MusicIR::requireInputs(const std::vector<ObjectId>& inputs, std::size_t minCount,
                            std::string_view operation) const {
    if (inputs.size() < minCount) {
        throw Error(std::string(operation) + " received an unsupported number of inputs",
                    std::string(operation));
    }
    for (const ObjectId input : inputs) {
        if (!contains(input)) {
            throw Error(std::string(operation) + " references unknown object " +
                            describeObject(input),
                        std::string(operation));
        }
    }
}

const MusicNode& MusicIR::node(ObjectId id) const {
    if (!contains(id)) {
        throw Error("unknown music IR object " + describeObject(id), "music-ir");
    }
    return nodes_[id];
}

bool MusicIR::contains(ObjectId id) const noexcept {
    return id < nodes_.size();
}

MusicNode& MusicIR::mutableNode(ObjectId id) {
    if (!contains(id)) {
        throw Error("unknown music IR object " + describeObject(id), "music-ir");
    }
    return nodes_[id];
}

Fragment MusicIR::evaluateNode(const MusicNode& node) {
    return std::visit(
        Overloaded{
            [&](const LiteralNode& literal) { return literal.fragment; },
            [&](const TransposeNode& transposeNode) {
                return mpl::transpose(evaluate(node.inputs[0]), transposeNode.interval);
            },
            [&](const ReverseNode&) { return mpl::reverse(evaluate(node.inputs[0])); },
            [&](const StretchNode& stretchNode) {
                return mpl::stretch(evaluate(node.inputs[0]), stretchNode.factor);
            },
            [&](const RepeatNode& repeatNode) {
                return mpl::repeat(evaluate(node.inputs[0]), repeatNode.count);
            },
            [&](const SequenceNode&) {
                std::vector<Fragment> parts;
                parts.reserve(node.inputs.size());
                for (const ObjectId input : node.inputs) {
                    parts.push_back(evaluate(input));
                }
                return mpl::sequence(parts);
            },
            [&](const ParallelNode&) {
                std::vector<Fragment> parts;
                parts.reserve(node.inputs.size());
                for (const ObjectId input : node.inputs) {
                    parts.push_back(evaluate(input));
                }
                return mpl::parallel(parts);
            },
            [&](const DiatonicTransposeNode& diatonicNode) {
                return mpl::diatonicTranspose(evaluate(node.inputs[0]),
                                               diatonicNode.scaleSteps, key_);
            },
            [&](const HarmonizeNode& harmonizeNode) {
                return mpl::harmonize(evaluate(node.inputs[0]), harmonizeNode.degreeOffsets,
                                      key_, harmonizeNode.velocity, node.id);
            },
            [&](const SyncopateNode& syncopateNode) {
                return mpl::syncopate(evaluate(node.inputs[0]), meter_,
                                      syncopateNode.displacement);
            },
        },
        node.payload);
}

const Fragment& MusicIR::evaluate(ObjectId id) {
    MusicNode& target = mutableNode(id);
    if (target.value.has_value()) {
        ++cacheHits_;
        return *target.value;
    }
    target.value = evaluateNode(target);
    ++evaluations_;
    return *target.value;
}

void MusicIR::evaluateAll() {
    for (std::size_t index = 0; index < nodes_.size(); ++index) {
        evaluate(static_cast<ObjectId>(index));
    }
}

std::size_t MusicIR::evaluatedCount() const noexcept {
    return static_cast<std::size_t>(
        std::count_if(nodes_.begin(), nodes_.end(),
                      [](const MusicNode& node) { return node.value.has_value(); }));
}

std::vector<ObjectId> MusicIR::dependents(ObjectId id) const {
    if (!contains(id)) {
        throw Error("unknown music IR object " + describeObject(id), "music-ir");
    }

    std::vector<ObjectId> result;
    std::vector<char> visited(nodes_.size(), 0);
    std::vector<ObjectId> frontier{id};
    visited[id] = 1;

    while (!frontier.empty()) {
        const ObjectId current = frontier.back();
        frontier.pop_back();
        for (const MusicNode& node : nodes_) {
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

void MusicIR::invalidate(ObjectId id) {
    std::vector<ObjectId> affected = dependents(id);
    affected.push_back(id);
    for (const ObjectId object : affected) {
        nodes_[object].value.reset();
    }
}

void MusicIR::updateLiteral(ObjectId id, std::vector<Note> notes,
                            std::optional<Duration> span) {
    MusicNode& target = mutableNode(id);
    auto* literal = std::get_if<LiteralNode>(&target.payload);
    if (literal == nullptr) {
        throw Error("object " + describeObject(id) + " is not a literal and cannot be edited",
                    "music-ir");
    }
    for (Note& note : notes) {
        note.origin = id;
    }
    literal->fragment = Fragment::fromNotes(std::move(notes), span);
    invalidate(id);
}

std::vector<ObjectId> MusicIR::evaluationOrder() const {
    std::vector<ObjectId> order;
    order.reserve(nodes_.size());
    for (const MusicNode& node : nodes_) {
        order.push_back(node.id);
    }
    return order;
}

std::string MusicIR::toJson() const {
    JsonWriter json;
    json.beginObject();
    json.key("format").value("mpl.music-ir");
    json.key("version").value(2);
    json.key("timeUnit").value("whole-note");

    json.key("context").beginObject();
    json.key("key").value(key_.name());
    json.key("meter").value(meter_.toString());
    json.endObject();

    json.key("objects").beginArray();

    for (const MusicNode& node : nodes_) {
        const NodeKind kind = nodeKind(node.payload);
        json.beginObject();
        json.key("id").value(static_cast<std::int64_t>(node.id));
        json.key("kind").value(nodeKindName(kind));
        json.key("label").value(node.label);
        json.key("inputs").beginArray();
        for (const ObjectId input : node.inputs) {
            json.value(static_cast<std::int64_t>(input));
        }
        json.endArray();

        json.key("contextDependencies").beginArray();
        for (const ContextAspect aspect : contextDependencies(kind)) {
            json.value(contextAspectName(aspect));
        }
        json.endArray();

        if (kind == NodeKind::Transpose) {
            json.key("parameters").beginObject();
            json.key("intervalSemitones").value(std::get<TransposeNode>(node.payload).interval.semitones);
            json.endObject();
        } else if (kind == NodeKind::Stretch) {
            json.key("parameters").beginObject();
            json.key("factor").value(std::get<StretchNode>(node.payload).factor.toString());
            json.endObject();
        } else if (kind == NodeKind::Repeat) {
            json.key("parameters").beginObject();
            json.key("count").value(std::get<RepeatNode>(node.payload).count);
            json.endObject();
        } else if (kind == NodeKind::DiatonicTranspose) {
            json.key("parameters").beginObject();
            json.key("scaleSteps").value(std::get<DiatonicTransposeNode>(node.payload).scaleSteps);
            json.endObject();
        } else if (kind == NodeKind::Harmonize) {
            const HarmonizeNode& harmonizeNode = std::get<HarmonizeNode>(node.payload);
            json.key("parameters").beginObject();
            json.key("degreeOffsets").beginArray();
            for (const int offset : harmonizeNode.degreeOffsets) {
                json.value(offset);
            }
            json.endArray();
            json.key("velocity").value(harmonizeNode.velocity);
            json.endObject();
        } else if (kind == NodeKind::Syncopate) {
            json.key("parameters").beginObject();
            json.key("displacement").value(std::get<SyncopateNode>(node.payload).displacement.toString());
            json.endObject();
        }

        if (node.value.has_value()) {
            const Fragment& value = *node.value;
            json.key("value").beginObject();
            json.key("duration").value(value.duration().toString());
            json.key("noteCount").value(static_cast<std::int64_t>(value.noteCount()));
            json.key("notes").beginArray();
            for (const Note& note : value.notes()) {
                json.beginObject();
                json.key("pitch").value(note.pitch.name());
                json.key("midi").value(note.pitch.midi());
                json.key("offset").value(note.offset.toString());
                json.key("duration").value(note.duration.toString());
                json.key("velocity").value(note.velocity);
                json.key("origin").value(note.origin == kInvalidObjectId
                                             ? std::int64_t{-1}
                                             : static_cast<std::int64_t>(note.origin));
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
