#include "mpl/ir/MusicIR.hpp"

#include "mpl/core/Error.hpp"
#include "mpl/util/JsonWriter.hpp"

#include <algorithm>
#include <utility>

namespace mpl {
namespace {

std::string describeObject(ObjectId id) {
    return id == kInvalidObjectId ? std::string("<invalid>") : "#" + std::to_string(id);
}

bool isPowerOfTwo(int value) {
    return value > 0 && (value & (value - 1)) == 0;
}

Fragment tieFragments(const Fragment& first, const Fragment& second) {
    if (first.notes().empty() || second.notes().empty()) {
        throw Error("tie requires two non-empty fragments", "tie-empty");
    }
    std::vector<std::size_t> ending;
    for (std::size_t index = 0; index < first.notes().size(); ++index) {
        const Note& note = first.notes()[index];
        if (note.offset + note.duration == first.duration()) {
            ending.push_back(index);
        }
    }
    std::vector<std::size_t> starting;
    for (std::size_t index = 0; index < second.notes().size(); ++index) {
        if (second.notes()[index].offset.inWholeNotes() == Rational(0, 1)) {
            starting.push_back(index);
        }
    }
    if (ending.size() != 1 || starting.size() != 1) {
        throw Error("tie boundary must contain exactly one sounding note on each side",
                    "tie-ambiguous");
    }
    const std::size_t last = ending.front();
    const std::size_t firstSecond = starting.front();
    if (first.notes()[last].pitch.midi() != second.notes()[firstSecond].pitch.midi()) {
        throw Error("tie boundary pitches differ", "tie-pitch");
    }
    std::vector<Note> merged = first.notes();
    merged[last].duration = merged[last].duration + second.notes()[firstSecond].duration;
    for (std::size_t index = 0; index < second.notes().size(); ++index) {
        if (index == firstSecond) {
            continue;
        }
        Note note = second.notes()[index];
        note.offset = note.offset + first.duration();
        merged.push_back(std::move(note));
    }
    return Fragment::fromNotes(std::move(merged),
                               first.duration() + second.duration());
}

/// Beat-point syncopation under an explicit meter map. A note is displaced when
/// its absolute position lands exactly on a denominator-unit beat, so 7/8 and
/// 5/8 sections use their own beat grid instead of one global meter.
Fragment syncopateWithMap(const Fragment& input, const MeterMap& map, const Position& anchor,
                          const Rational& displacement) {
    std::vector<Note> notes = input.notes();
    Rational span = input.duration().inWholeNotes();
    for (Note& note : notes) {
        const Rational absolute = anchor.inWholeNotes() + note.offset.inWholeNotes();
        if (absolute.isNegative() || !map.isBeatStart(Position(absolute))) {
            continue;
        }
        Rational shifted = note.offset.inWholeNotes() + displacement;
        if (shifted.isNegative()) {
            shifted = Rational(0, 1);
        }
        note.offset = Duration(shifted);
        span = std::max(span, shifted + note.duration.inWholeNotes());
    }
    return Fragment::fromNotes(std::move(notes), Duration(span));
}

void writeRhythmJson(JsonWriter& json, const Rhythm& rhythm) {
    json.beginObject();
    json.key("logicalLength").value(rhythm.logicalLength().toString());
    json.key("eventCount").value(static_cast<std::int64_t>(rhythm.eventCount()));
    json.key("attackCount").value(static_cast<std::int64_t>(rhythm.attackCount()));
    json.key("events").beginArray();
    for (const RhythmEvent& event : rhythm.events()) {
        json.beginObject();
        json.key("onset").value(event.onset.toString());
        json.key("duration").value(event.duration.toString());
        json.key("accent").value(event.accent);
        json.key("rest").value(event.rest);
        if (event.continuation) {
            json.key("continuation").value(true);
        }
        json.key("origin").value(event.origin == kInvalidObjectId
                                     ? std::int64_t{-1}
                                     : static_cast<std::int64_t>(event.origin));
        json.endObject();
    }
    json.endArray();
    json.endObject();
}

void writeHarmonyJson(JsonWriter& json, const HarmonyTimeline& harmony) {
    json.beginObject();
    json.key("logicalLength").value(harmony.logicalLength().toString());
    json.key("eventCount").value(static_cast<std::int64_t>(harmony.size()));
    json.key("events").beginArray();
    for (const HarmonyEvent& event : harmony.events()) {
        json.beginObject();
        json.key("start").value(event.start.toString());
        json.key("duration").value(event.duration.toString());
        json.key("chord").value(event.chord.name());
        json.key("pitchClasses").value(event.chord.pitchClassSet().name());
        json.key("origin").value(event.origin == kInvalidObjectId
                                     ? std::int64_t{-1}
                                     : static_cast<std::int64_t>(event.origin));
        json.endObject();
    }
    json.endArray();
    json.endObject();
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
        case 10: return NodeKind::Rotate;
        case 11: return NodeKind::RhythmLiteral;
        case 12: return NodeKind::RhythmSequence;
        case 13: return NodeKind::RhythmOverlay;
        case 14: return NodeKind::RhythmRepeat;
        case 15: return NodeKind::RhythmRotate;
        case 16: return NodeKind::RhythmShift;
        case 17: return NodeKind::RhythmStretch;
        case 18: return NodeKind::RhythmSubdivide;
        case 19: return NodeKind::RhythmAccent;
        case 20: return NodeKind::RhythmRealign;
        case 21: return NodeKind::RhythmQuantize;
        case 22: return NodeKind::RhythmAccentByMeter;
        case 23: return NodeKind::RhythmIntersect;
        case 24: return NodeKind::RhythmUnion;
        case 25: return NodeKind::RhythmDifference;
        case 26: return NodeKind::RhythmComplement;
        case 27: return NodeKind::RhythmAccentFollow;
        case 28: return NodeKind::RhythmCombine;
        case 29: return NodeKind::RhythmOnsetSet;
        case 30: return NodeKind::RhythmRetrigger;
        case 31: return NodeKind::HarmonyLiteral;
        case 32: return NodeKind::HarmonyQuery;
        case 33: return NodeKind::Tie;
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
        case NodeKind::Rotate: return "rotate";
        case NodeKind::RhythmLiteral: return "rhythm-literal";
        case NodeKind::RhythmSequence: return "rhythm-sequence";
        case NodeKind::RhythmOverlay: return "rhythm-overlay";
        case NodeKind::RhythmRepeat: return "rhythm-repeat";
        case NodeKind::RhythmRotate: return "rhythm-rotate";
        case NodeKind::RhythmShift: return "rhythm-shift";
        case NodeKind::RhythmStretch: return "rhythm-stretch";
        case NodeKind::RhythmSubdivide: return "rhythm-subdivide";
        case NodeKind::RhythmAccent: return "rhythm-accent";
        case NodeKind::RhythmRealign: return "rhythm-realign";
        case NodeKind::RhythmQuantize: return "rhythm-quantize";
        case NodeKind::RhythmAccentByMeter: return "rhythm-accent-by-meter";
        case NodeKind::RhythmIntersect: return "rhythm-intersect";
        case NodeKind::RhythmUnion: return "rhythm-union";
        case NodeKind::RhythmDifference: return "rhythm-difference";
        case NodeKind::RhythmComplement: return "rhythm-complement";
        case NodeKind::RhythmAccentFollow: return "rhythm-accent-follow";
        case NodeKind::RhythmCombine: return "rhythm-combine";
        case NodeKind::RhythmOnsetSet: return "rhythm-onset-set";
        case NodeKind::RhythmRetrigger: return "rhythm-retrigger";
        case NodeKind::HarmonyLiteral: return "harmony-literal";
        case NodeKind::HarmonyQuery: return "harmony-query";
        case NodeKind::Tie: return "tie";
    }
    return "unknown";
}

bool isRhythmKind(NodeKind kind) {
    switch (kind) {
        case NodeKind::RhythmLiteral:
        case NodeKind::RhythmSequence:
        case NodeKind::RhythmOverlay:
        case NodeKind::RhythmRepeat:
        case NodeKind::RhythmRotate:
        case NodeKind::RhythmShift:
        case NodeKind::RhythmStretch:
        case NodeKind::RhythmSubdivide:
        case NodeKind::RhythmAccent:
        case NodeKind::RhythmRealign:
        case NodeKind::RhythmQuantize:
        case NodeKind::RhythmAccentByMeter:
        case NodeKind::RhythmIntersect:
        case NodeKind::RhythmUnion:
        case NodeKind::RhythmDifference:
        case NodeKind::RhythmComplement:
        case NodeKind::RhythmAccentFollow:
        case NodeKind::RhythmOnsetSet:
        case NodeKind::RhythmRetrigger:
            return true;
        default:
            return false;
    }
}

bool isHarmonyKind(NodeKind kind) {
    return kind == NodeKind::HarmonyLiteral;
}

bool isChordKind(NodeKind kind) {
    return kind == NodeKind::HarmonyQuery;
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

ObjectId MusicIR::tie(ObjectId first, ObjectId second, std::string label) {
    requireInputs({first, second}, 2, "tie");
    MusicNode node;
    node.id = static_cast<ObjectId>(nodes_.size());
    node.label = std::move(label);
    node.inputs = {first, second};
    node.payload = TieNode{};
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
    SyncopateNode payload;
    payload.displacement = displacement;
    node.payload = std::move(payload);
    nodes_.push_back(std::move(node));
    return nodes_.back().id;
}

ObjectId MusicIR::syncopate(ObjectId input, const Rational& displacement,
                            const MeterMap& meterMap, const Position& anchor,
                            std::string label) {
    requireInputs({input}, 1, "syncopate");
    MusicNode node;
    node.id = static_cast<ObjectId>(nodes_.size());
    node.label = std::move(label);
    node.inputs = {input};
    SyncopateNode payload;
    payload.displacement = displacement;
    payload.meterMap = meterMap;
    payload.anchor = anchor;
    node.payload = std::move(payload);
    nodes_.push_back(std::move(node));
    return nodes_.back().id;
}

ObjectId MusicIR::rotate(ObjectId input, const Rational& amount, std::string label) {
    requireInputs({input}, 1, "rotate");
    MusicNode node;
    node.id = static_cast<ObjectId>(nodes_.size());
    node.label = std::move(label);
    node.inputs = {input};
    node.payload = RotateNode{amount};
    nodes_.push_back(std::move(node));
    return nodes_.back().id;
}

ObjectId MusicIR::rhythmLiteral(Rhythm rhythm, std::string label) {
    MusicNode node;
    node.id = static_cast<ObjectId>(nodes_.size());
    node.label = std::move(label);
    node.payload = RhythmLiteralNode{std::move(rhythm)};
    nodes_.push_back(std::move(node));
    return nodes_.back().id;
}

ObjectId MusicIR::rhythmSequence(const std::vector<ObjectId>& inputs, std::string label) {
    requireInputs(inputs, 1, "rhythm-sequence");
    for (const ObjectId input : inputs) {
        requireRhythmInput(input, "rhythm-sequence");
    }
    MusicNode node;
    node.id = static_cast<ObjectId>(nodes_.size());
    node.label = std::move(label);
    node.inputs = inputs;
    node.payload = RhythmSequenceNode{};
    nodes_.push_back(std::move(node));
    return nodes_.back().id;
}

ObjectId MusicIR::rhythmOverlay(const std::vector<ObjectId>& inputs, std::string label) {
    requireInputs(inputs, 1, "rhythm-overlay");
    for (const ObjectId input : inputs) {
        requireRhythmInput(input, "rhythm-overlay");
    }
    MusicNode node;
    node.id = static_cast<ObjectId>(nodes_.size());
    node.label = std::move(label);
    node.inputs = inputs;
    node.payload = RhythmOverlayNode{};
    nodes_.push_back(std::move(node));
    return nodes_.back().id;
}

ObjectId MusicIR::rhythmRepeat(ObjectId input, int count, std::string label) {
    requireInputs({input}, 1, "rhythm-repeat");
    requireRhythmInput(input, "rhythm-repeat");
    if (count < 1 || count > kMaxRhythmRepeat) {
        throw Error("rhythm repeat count must be in [1, " +
                        std::to_string(kMaxRhythmRepeat) + "]",
                    "rhythm");
    }
    MusicNode node;
    node.id = static_cast<ObjectId>(nodes_.size());
    node.label = std::move(label);
    node.inputs = {input};
    node.payload = RhythmRepeatNode{count};
    nodes_.push_back(std::move(node));
    return nodes_.back().id;
}

ObjectId MusicIR::rhythmRotate(ObjectId input, const Rational& amount, std::string label) {
    requireInputs({input}, 1, "rhythm-rotate");
    requireRhythmInput(input, "rhythm-rotate");
    MusicNode node;
    node.id = static_cast<ObjectId>(nodes_.size());
    node.label = std::move(label);
    node.inputs = {input};
    node.payload = RhythmRotateNode{amount};
    nodes_.push_back(std::move(node));
    return nodes_.back().id;
}

ObjectId MusicIR::rhythmShift(ObjectId input, const Rational& amount, std::string label) {
    requireInputs({input}, 1, "rhythm-shift");
    requireRhythmInput(input, "rhythm-shift");
    MusicNode node;
    node.id = static_cast<ObjectId>(nodes_.size());
    node.label = std::move(label);
    node.inputs = {input};
    node.payload = RhythmShiftNode{amount};
    nodes_.push_back(std::move(node));
    return nodes_.back().id;
}

ObjectId MusicIR::rhythmStretch(ObjectId input, const Rational& factor, std::string label) {
    requireInputs({input}, 1, "rhythm-stretch");
    requireRhythmInput(input, "rhythm-stretch");
    if (!factor.isPositive()) {
        throw Error("rhythm stretch factor must be positive", "rhythm");
    }
    MusicNode node;
    node.id = static_cast<ObjectId>(nodes_.size());
    node.label = std::move(label);
    node.inputs = {input};
    node.payload = RhythmStretchNode{factor};
    nodes_.push_back(std::move(node));
    return nodes_.back().id;
}

ObjectId MusicIR::rhythmSubdivide(ObjectId input, int parts, std::string label) {
    requireInputs({input}, 1, "rhythm-subdivide");
    requireRhythmInput(input, "rhythm-subdivide");
    if (parts < 1 || parts > kMaxRhythmSubdivision) {
        throw Error("rhythm subdivide parts must be in [1, " +
                        std::to_string(kMaxRhythmSubdivision) + "]",
                    "rhythm");
    }
    MusicNode node;
    node.id = static_cast<ObjectId>(nodes_.size());
    node.label = std::move(label);
    node.inputs = {input};
    node.payload = RhythmSubdivideNode{parts};
    nodes_.push_back(std::move(node));
    return nodes_.back().id;
}

ObjectId MusicIR::rhythmAccent(ObjectId input, std::vector<int> levels, std::string label) {
    requireInputs({input}, 1, "rhythm-accent");
    requireRhythmInput(input, "rhythm-accent");
    if (levels.empty()) {
        throw Error("rhythm accent requires at least one level", "rhythm");
    }
    MusicNode node;
    node.id = static_cast<ObjectId>(nodes_.size());
    node.label = std::move(label);
    node.inputs = {input};
    node.payload = RhythmAccentNode{std::move(levels)};
    nodes_.push_back(std::move(node));
    return nodes_.back().id;
}

ObjectId MusicIR::rhythmRealign(ObjectId input, const Rational& period, std::string label) {
    requireInputs({input}, 1, "rhythm-realign");
    requireRhythmInput(input, "rhythm-realign");
    MusicNode node;
    node.id = static_cast<ObjectId>(nodes_.size());
    node.label = std::move(label);
    node.inputs = {input};
    node.payload = RhythmRealignNode{period};
    nodes_.push_back(std::move(node));
    return nodes_.back().id;
}

ObjectId MusicIR::rhythmQuantize(ObjectId input, const Rational& grid, std::string label) {
    requireInputs({input}, 1, "rhythm-quantize");
    requireRhythmInput(input, "rhythm-quantize");
    MusicNode node;
    node.id = static_cast<ObjectId>(nodes_.size());
    node.label = std::move(label);
    node.inputs = {input};
    node.payload = RhythmQuantizeNode{grid};
    nodes_.push_back(std::move(node));
    return nodes_.back().id;
}

ObjectId MusicIR::rhythmAccentByMeter(ObjectId input, const MeterMap& meterMap,
                                      const Position& anchor, std::string label) {
    requireInputs({input}, 1, "rhythm-accent-by-meter");
    requireRhythmInput(input, "rhythm-accent-by-meter");
    MusicNode node;
    node.id = static_cast<ObjectId>(nodes_.size());
    node.label = std::move(label);
    node.inputs = {input};
    RhythmAccentByMeterNode payload;
    payload.meterMap = meterMap;
    payload.anchor = anchor;
    node.payload = std::move(payload);
    nodes_.push_back(std::move(node));
    return nodes_.back().id;
}

ObjectId MusicIR::rhythmIntersect(ObjectId lhs, ObjectId rhs,
                                  std::optional<Rational> grid, std::string label) {
    requireInputs({lhs, rhs}, 2, "rhythm-intersect");
    requireRhythmInput(lhs, "rhythm-intersect");
    requireRhythmInput(rhs, "rhythm-intersect");
    MusicNode node;
    node.id = static_cast<ObjectId>(nodes_.size());
    node.label = std::move(label);
    node.inputs = {lhs, rhs};
    node.payload = RhythmIntersectNode{std::move(grid)};
    nodes_.push_back(std::move(node));
    return nodes_.back().id;
}

ObjectId MusicIR::rhythmUnion(ObjectId lhs, ObjectId rhs, std::optional<Rational> grid,
                              std::string label) {
    requireInputs({lhs, rhs}, 2, "rhythm-union");
    requireRhythmInput(lhs, "rhythm-union");
    requireRhythmInput(rhs, "rhythm-union");
    MusicNode node;
    node.id = static_cast<ObjectId>(nodes_.size());
    node.label = std::move(label);
    node.inputs = {lhs, rhs};
    node.payload = RhythmUnionNode{std::move(grid)};
    nodes_.push_back(std::move(node));
    return nodes_.back().id;
}

ObjectId MusicIR::rhythmDifference(ObjectId lhs, ObjectId rhs,
                                   std::optional<Rational> grid, std::string label) {
    requireInputs({lhs, rhs}, 2, "rhythm-difference");
    requireRhythmInput(lhs, "rhythm-difference");
    requireRhythmInput(rhs, "rhythm-difference");
    MusicNode node;
    node.id = static_cast<ObjectId>(nodes_.size());
    node.label = std::move(label);
    node.inputs = {lhs, rhs};
    node.payload = RhythmDifferenceNode{std::move(grid)};
    nodes_.push_back(std::move(node));
    return nodes_.back().id;
}

ObjectId MusicIR::rhythmComplement(ObjectId input, const Rational& span, const Rational& grid,
                                   std::string label) {
    requireInputs({input}, 1, "rhythm-complement");
    requireRhythmInput(input, "rhythm-complement");
    MusicNode node;
    node.id = static_cast<ObjectId>(nodes_.size());
    node.label = std::move(label);
    node.inputs = {input};
    node.payload = RhythmComplementNode{span, grid};
    nodes_.push_back(std::move(node));
    return nodes_.back().id;
}

ObjectId MusicIR::rhythmAccentFollow(ObjectId source, ObjectId target,
                                     std::optional<Rational> grid, std::string label) {
    requireInputs({source, target}, 2, "rhythm-accent-follow");
    requireRhythmInput(source, "rhythm-accent-follow");
    requireRhythmInput(target, "rhythm-accent-follow");
    MusicNode node;
    node.id = static_cast<ObjectId>(nodes_.size());
    node.label = std::move(label);
    node.inputs = {source, target};
    node.payload = RhythmAccentFollowNode{std::move(grid)};
    nodes_.push_back(std::move(node));
    return nodes_.back().id;
}

ObjectId MusicIR::rhythmOnsetSet(ObjectId input, const Rational& unit, std::string label) {
    requireInputs({input}, 1, "rhythm-onset-set");
    requireRhythmInput(input, "rhythm-onset-set");
    if (!unit.isPositive()) {
        throw Error("rhythm onset-set unit must be positive", "rhythm");
    }
    MusicNode node;
    node.id = static_cast<ObjectId>(nodes_.size());
    node.label = std::move(label);
    node.inputs = {input};
    node.payload = RhythmOnsetSetNode{unit};
    nodes_.push_back(std::move(node));
    return nodes_.back().id;
}

ObjectId MusicIR::rhythmRetrigger(ObjectId input, std::string label) {
    requireInputs({input}, 1, "rhythm-retrigger");
    requireRhythmInput(input, "rhythm-retrigger");
    MusicNode node;
    node.id = static_cast<ObjectId>(nodes_.size());
    node.label = std::move(label);
    node.inputs = {input};
    node.payload = RhythmRetriggerNode{};
    nodes_.push_back(std::move(node));
    return nodes_.back().id;
}

ObjectId MusicIR::harmonyLiteral(HarmonyTimeline timeline, std::string label) {
    MusicNode node;
    node.id = static_cast<ObjectId>(nodes_.size());
    node.label = std::move(label);
    node.payload = HarmonyLiteralNode{std::move(timeline)};
    nodes_.push_back(std::move(node));
    return nodes_.back().id;
}

ObjectId MusicIR::harmonyQuery(ObjectId harmony, const Position& at, std::string label) {
    requireInputs({harmony}, 1, "harmony-query");
    requireHarmonyInput(harmony, "harmony-query");
    MusicNode node;
    node.id = static_cast<ObjectId>(nodes_.size());
    node.label = std::move(label);
    node.inputs = {harmony};
    node.payload = HarmonyQueryNode{at};
    nodes_.push_back(std::move(node));
    return nodes_.back().id;
}

ObjectId MusicIR::rhythmCombine(ObjectId rhythm, std::vector<Pitch> pitches, int velocity,
                                bool cyclic, std::string label) {
    requireInputs({rhythm}, 1, "rhythm-combine");
    requireRhythmInput(rhythm, "rhythm-combine");
    if (pitches.empty()) {
        throw Error("rhythm-combine requires at least one pitch", "rhythm-pitch");
    }
    if (velocity < 1 || velocity > 127) {
        throw Error("rhythm-combine base velocity must be in 1..127", "rhythm-pitch");
    }
    MusicNode node;
    node.id = static_cast<ObjectId>(nodes_.size());
    node.label = std::move(label);
    node.inputs = {rhythm};
    node.payload = RhythmCombineNode{std::move(pitches), velocity, cyclic};
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

void MusicIR::requireRhythmInput(ObjectId id, std::string_view operation) const {
    if (!contains(id)) {
        throw Error(std::string(operation) + " references unknown object " +
                        describeObject(id),
                    std::string(operation));
    }
    if (!isRhythmKind(nodeKind(nodes_[id].payload))) {
        throw Error(std::string(operation) + " expects a rhythm object but got " +
                        describeObject(id),
                    std::string(operation));
    }
}

void MusicIR::requireHarmonyInput(ObjectId id, std::string_view operation) const {
    if (!contains(id)) {
        throw Error(std::string(operation) + " references unknown object " +
                        describeObject(id),
                    std::string(operation));
    }
    if (!isHarmonyKind(nodeKind(nodes_[id].payload))) {
        throw Error(std::string(operation) + " expects a harmony object but got " +
                        describeObject(id),
                    std::string(operation));
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
    const NodeKind kind = nodeKind(node.payload);
    switch (kind) {
        case NodeKind::Literal:
            return std::get<LiteralNode>(node.payload).fragment;
        case NodeKind::Transpose:
            return mpl::transpose(evaluate(node.inputs[0]),
                                  std::get<TransposeNode>(node.payload).interval);
        case NodeKind::Reverse:
            return mpl::reverse(evaluate(node.inputs[0]));
        case NodeKind::Stretch:
            return mpl::stretch(evaluate(node.inputs[0]),
                                std::get<StretchNode>(node.payload).factor);
        case NodeKind::Repeat:
            return mpl::repeat(evaluate(node.inputs[0]),
                               std::get<RepeatNode>(node.payload).count);
        case NodeKind::Sequence: {
            std::vector<Fragment> parts;
            parts.reserve(node.inputs.size());
            for (const ObjectId input : node.inputs) {
                parts.push_back(evaluate(input));
            }
            return mpl::sequence(parts);
        }
        case NodeKind::Parallel: {
            std::vector<Fragment> parts;
            parts.reserve(node.inputs.size());
            for (const ObjectId input : node.inputs) {
                parts.push_back(evaluate(input));
            }
            return mpl::parallel(parts);
        }
        case NodeKind::Tie:
            return tieFragments(evaluate(node.inputs[0]), evaluate(node.inputs[1]));
        case NodeKind::DiatonicTranspose:
            return mpl::diatonicTranspose(evaluate(node.inputs[0]),
                                          std::get<DiatonicTransposeNode>(node.payload).scaleSteps,
                                          key_);
        case NodeKind::Harmonize: {
            const HarmonizeNode& payload = std::get<HarmonizeNode>(node.payload);
            return mpl::harmonize(evaluate(node.inputs[0]), payload.degreeOffsets, key_,
                                  payload.velocity, node.id);
        }
        case NodeKind::Syncopate: {
            const SyncopateNode& payload = std::get<SyncopateNode>(node.payload);
            if (payload.meterMap.has_value()) {
                return syncopateWithMap(evaluate(node.inputs[0]), *payload.meterMap,
                                        payload.anchor, payload.displacement);
            }
            return mpl::syncopate(evaluate(node.inputs[0]), meter_, payload.displacement);
        }
        case NodeKind::Rotate:
            return mpl::rotate(evaluate(node.inputs[0]),
                               std::get<RotateNode>(node.payload).amount);
        case NodeKind::RhythmCombine: {
            const RhythmCombineNode& payload = std::get<RhythmCombineNode>(node.payload);
            const Rhythm& rhythm = evaluateRhythm(node.inputs[0]);
            if (!rhythm.logicalLength().isPositive()) {
                throw Error("rhythm-combine requires a positive rhythm period",
                            "rhythm-pitch");
            }
            std::vector<const RhythmEvent*> attacks;
            for (const RhythmEvent& event : rhythm.events()) {
                if (isRhythmAttack(event)) {
                    attacks.push_back(&event);
                }
            }
            if (attacks.empty()) {
                return Fragment::fromNotes({}, Duration(rhythm.logicalLength()));
            }
            if (!payload.cyclic && attacks.size() != payload.pitches.size()) {
                throw Error("strict rhythm-pitch combination has " +
                                std::to_string(attacks.size()) + " attacks but " +
                                std::to_string(payload.pitches.size()) + " pitches",
                            "rhythm-pitch-count");
            }
            std::vector<Note> notes;
            notes.reserve(attacks.size());
            for (std::size_t index = 0; index < attacks.size(); ++index) {
                const RhythmEvent& event = *attacks[index];
                if (event.onset.isNegative()) {
                    throw Error("rhythm onset " + event.onset.toString() +
                                    " is negative and cannot be placed on the fragment timeline",
                                "rhythm-pitch");
                }
                Note note;
                note.pitch = payload.pitches[index % payload.pitches.size()];
                note.offset = Duration(event.onset);
                note.duration = Duration(event.duration);
                note.velocity = std::clamp(payload.velocity + event.accent * 8, 1, 127);
                note.origin = node.id;
                note.rhythmOrigin = event.origin;
                note.rhythmEventIndex = static_cast<std::uint32_t>(
                    &event - rhythm.events().data());
                notes.push_back(std::move(note));
            }
            return Fragment::fromNotes(std::move(notes), Duration(rhythm.logicalLength()));
        }
        default:
            throw Error("music IR object " + describeObject(node.id) +
                            " is a rhythm and does not evaluate to a fragment",
                        "music-ir");
    }
}

Rhythm MusicIR::evaluateRhythmNode(const MusicNode& node) {
    const NodeKind kind = nodeKind(node.payload);
    Rhythm result;
    switch (kind) {
        case NodeKind::RhythmLiteral:
            result = std::get<RhythmLiteralNode>(node.payload).rhythm;
            break;
        case NodeKind::RhythmSequence: {
            std::vector<Rhythm> parts;
            parts.reserve(node.inputs.size());
            for (const ObjectId input : node.inputs) {
                parts.push_back(evaluateRhythm(input));
            }
            result = mpl::rhythmSequence(parts);
            break;
        }
        case NodeKind::RhythmOverlay: {
            std::vector<Rhythm> parts;
            parts.reserve(node.inputs.size());
            for (const ObjectId input : node.inputs) {
                parts.push_back(evaluateRhythm(input));
            }
            result = mpl::rhythmOverlay(parts);
            break;
        }
        case NodeKind::RhythmRepeat:
            result = mpl::rhythmRepeat(evaluateRhythm(node.inputs[0]),
                                  std::get<RhythmRepeatNode>(node.payload).count);
            break;
        case NodeKind::RhythmRotate:
            result = mpl::rhythmRotate(evaluateRhythm(node.inputs[0]),
                                  std::get<RhythmRotateNode>(node.payload).amount);
            break;
        case NodeKind::RhythmShift:
            result = mpl::rhythmShift(evaluateRhythm(node.inputs[0]),
                                 std::get<RhythmShiftNode>(node.payload).amount);
            break;
        case NodeKind::RhythmStretch:
            result = mpl::rhythmStretch(evaluateRhythm(node.inputs[0]),
                                   std::get<RhythmStretchNode>(node.payload).factor);
            break;
        case NodeKind::RhythmSubdivide:
            result = mpl::rhythmSubdivide(evaluateRhythm(node.inputs[0]),
                                     std::get<RhythmSubdivideNode>(node.payload).parts);
            break;
        case NodeKind::RhythmAccent:
            result = mpl::rhythmAccent(evaluateRhythm(node.inputs[0]),
                                  std::get<RhythmAccentNode>(node.payload).levels);
            break;
        case NodeKind::RhythmRealign:
            result = mpl::rhythmRealign(evaluateRhythm(node.inputs[0]),
                                   std::get<RhythmRealignNode>(node.payload).period);
            break;
        case NodeKind::RhythmQuantize:
            result = mpl::rhythmQuantize(evaluateRhythm(node.inputs[0]),
                                    std::get<RhythmQuantizeNode>(node.payload).grid);
            break;
        case NodeKind::RhythmAccentByMeter: {
            const RhythmAccentByMeterNode& payload =
                std::get<RhythmAccentByMeterNode>(node.payload);
            result = mpl::rhythmAccentByMeter(evaluateRhythm(node.inputs[0]), payload.meterMap,
                                         payload.anchor);
            break;
        }
        case NodeKind::RhythmIntersect:
            result = mpl::rhythmIntersect(
                evaluateRhythm(node.inputs[0]), evaluateRhythm(node.inputs[1]),
                std::get<RhythmIntersectNode>(node.payload).grid);
            break;
        case NodeKind::RhythmUnion:
            result = mpl::rhythmUnion(evaluateRhythm(node.inputs[0]), evaluateRhythm(node.inputs[1]),
                                 std::get<RhythmUnionNode>(node.payload).grid);
            break;
        case NodeKind::RhythmDifference:
            result = mpl::rhythmDifference(
                evaluateRhythm(node.inputs[0]), evaluateRhythm(node.inputs[1]),
                std::get<RhythmDifferenceNode>(node.payload).grid);
            break;
        case NodeKind::RhythmComplement: {
            const RhythmComplementNode& payload =
                std::get<RhythmComplementNode>(node.payload);
            result = mpl::rhythmComplement(evaluateRhythm(node.inputs[0]), payload.span,
                                      payload.grid);
            break;
        }
        case NodeKind::RhythmAccentFollow:
            result = mpl::rhythmAccentFollow(
                evaluateRhythm(node.inputs[0]), evaluateRhythm(node.inputs[1]),
                std::get<RhythmAccentFollowNode>(node.payload).grid);
            break;
        case NodeKind::RhythmOnsetSet: {
            const RhythmOnsetSetNode& payload = std::get<RhythmOnsetSetNode>(node.payload);
            const Rhythm& input = evaluateRhythm(node.inputs[0]);
            std::vector<RhythmEvent> events;
            for (const RhythmEvent& event : input.events()) {
                if (event.rest) {
                    continue;
                }
                RhythmEvent onset = event;
                onset.duration = payload.unit;
                events.push_back(std::move(onset));
            }
            result = Rhythm::fromEvents(std::move(events), input.logicalLength());
            break;
        }
        case NodeKind::RhythmRetrigger:
            result = mpl::rhythmRetrigger(evaluateRhythm(node.inputs[0]));
            break;
        default:
            throw Error("music IR object " + describeObject(node.id) +
                            " is not a rhythm",
                        "music-ir");
    }
    return result.withOriginFallback(node.id);
}

HarmonyTimeline MusicIR::evaluateHarmonyNode(const MusicNode& node) {
    if (nodeKind(node.payload) != NodeKind::HarmonyLiteral) {
        throw Error("music IR object " + describeObject(node.id) +
                        " is not a harmony literal",
                    "music-ir");
    }
    return std::get<HarmonyLiteralNode>(node.payload).timeline.withOriginFallback(node.id);
}

ChordSymbol MusicIR::evaluateChordNode(const MusicNode& node) {
    if (nodeKind(node.payload) != NodeKind::HarmonyQuery) {
        throw Error("music IR object " + describeObject(node.id) +
                        " is not a harmony query",
                    "music-ir");
    }
    const HarmonyQueryNode& query = std::get<HarmonyQueryNode>(node.payload);
    const HarmonyTimeline& timeline = evaluateHarmony(node.inputs[0]);
    const HarmonyEvent* event = timeline.eventAt(query.at);
    return event == nullptr ? ChordSymbol::none() : event->chord;
}

const Fragment& MusicIR::evaluate(ObjectId id) {
    MusicNode& target = mutableNode(id);
    const NodeKind kind = nodeKind(target.payload);
    if (isRhythmKind(kind) || isHarmonyKind(kind) || isChordKind(kind)) {
        throw Error("music IR object " + describeObject(id) +
                        " does not evaluate to a fragment",
                    "music-ir");
    }
    if (target.value.has_value()) {
        ++cacheHits_;
        return *target.value;
    }
    target.value = evaluateNode(target);
    ++evaluations_;
    return *target.value;
}

const Rhythm& MusicIR::evaluateRhythm(ObjectId id) {
    MusicNode& target = mutableNode(id);
    if (!isRhythmKind(nodeKind(target.payload))) {
        throw Error("music IR object " + describeObject(id) +
                        " is not a rhythm",
                    "music-ir");
    }
    if (target.rhythmValue.has_value()) {
        ++cacheHits_;
        return *target.rhythmValue;
    }
    target.rhythmValue = evaluateRhythmNode(target);
    ++evaluations_;
    return *target.rhythmValue;
}

const HarmonyTimeline& MusicIR::evaluateHarmony(ObjectId id) {
    MusicNode& target = mutableNode(id);
    if (!isHarmonyKind(nodeKind(target.payload))) {
        throw Error("music IR object " + describeObject(id) +
                        " is not a harmony timeline",
                    "music-ir");
    }
    if (target.harmonyValue.has_value()) {
        ++cacheHits_;
        return *target.harmonyValue;
    }
    target.harmonyValue = evaluateHarmonyNode(target);
    ++evaluations_;
    return *target.harmonyValue;
}

const ChordSymbol& MusicIR::evaluateChord(ObjectId id) {
    MusicNode& target = mutableNode(id);
    if (!isChordKind(nodeKind(target.payload))) {
        throw Error("music IR object " + describeObject(id) +
                        " is not a chord query",
                    "music-ir");
    }
    if (target.chordValue.has_value()) {
        ++cacheHits_;
        return *target.chordValue;
    }
    target.chordValue = evaluateChordNode(target);
    ++evaluations_;
    return *target.chordValue;
}

bool MusicIR::isRhythmObject(ObjectId id) const {
    return contains(id) && isRhythmKind(nodeKind(nodes_[id].payload));
}

bool MusicIR::isHarmonyObject(ObjectId id) const {
    return contains(id) && isHarmonyKind(nodeKind(nodes_[id].payload));
}

bool MusicIR::isChordObject(ObjectId id) const {
    return contains(id) && isChordKind(nodeKind(nodes_[id].payload));
}

const Rhythm* MusicIR::cachedRhythm(ObjectId id) const {
    if (!contains(id) || !nodes_[id].rhythmValue.has_value()) {
        return nullptr;
    }
    return &*nodes_[id].rhythmValue;
}

const HarmonyTimeline* MusicIR::cachedHarmony(ObjectId id) const {
    if (!contains(id) || !nodes_[id].harmonyValue.has_value()) {
        return nullptr;
    }
    return &*nodes_[id].harmonyValue;
}

const ChordSymbol* MusicIR::cachedChord(ObjectId id) const {
    if (!contains(id) || !nodes_[id].chordValue.has_value()) {
        return nullptr;
    }
    return &*nodes_[id].chordValue;
}

void MusicIR::evaluateAll() {
    for (std::size_t index = 0; index < nodes_.size(); ++index) {
        const ObjectId id = static_cast<ObjectId>(index);
        const NodeKind kind = nodeKind(nodes_[index].payload);
        if (isRhythmKind(kind)) {
            (void)evaluateRhythm(id);
        } else if (isHarmonyKind(kind)) {
            (void)evaluateHarmony(id);
        } else if (isChordKind(kind)) {
            (void)evaluateChord(id);
        } else {
            (void)evaluate(id);
        }
    }
}

std::size_t MusicIR::evaluatedCount() const noexcept {
    return static_cast<std::size_t>(std::count_if(
        nodes_.begin(), nodes_.end(), [](const MusicNode& node) {
            return node.value.has_value() || node.rhythmValue.has_value() ||
                   node.harmonyValue.has_value() || node.chordValue.has_value();
        }));
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
        nodes_[object].rhythmValue.reset();
        nodes_[object].harmonyValue.reset();
        nodes_[object].chordValue.reset();
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
    json.key("version").value(3);
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
            const SyncopateNode& syncopateNode = std::get<SyncopateNode>(node.payload);
            json.key("parameters").beginObject();
            json.key("displacement").value(syncopateNode.displacement.toString());
            if (syncopateNode.meterMap.has_value()) {
                json.key("metricContext").value(syncopateNode.meterMap->describe());
                json.key("anchor").value(syncopateNode.anchor.toString());
            }
            json.endObject();
        } else if (kind == NodeKind::Rotate) {
            json.key("parameters").beginObject();
            json.key("amount").value(std::get<RotateNode>(node.payload).amount.toString());
            json.endObject();
        } else if (kind == NodeKind::RhythmRepeat) {
            json.key("parameters").beginObject();
            json.key("count").value(std::get<RhythmRepeatNode>(node.payload).count);
            json.endObject();
        } else if (kind == NodeKind::RhythmRotate) {
            json.key("parameters").beginObject();
            json.key("amount").value(std::get<RhythmRotateNode>(node.payload).amount.toString());
            json.endObject();
        } else if (kind == NodeKind::RhythmShift) {
            json.key("parameters").beginObject();
            json.key("amount").value(std::get<RhythmShiftNode>(node.payload).amount.toString());
            json.endObject();
        } else if (kind == NodeKind::RhythmStretch) {
            json.key("parameters").beginObject();
            json.key("factor").value(std::get<RhythmStretchNode>(node.payload).factor.toString());
            json.endObject();
        } else if (kind == NodeKind::RhythmSubdivide) {
            json.key("parameters").beginObject();
            json.key("parts").value(std::get<RhythmSubdivideNode>(node.payload).parts);
            json.endObject();
        } else if (kind == NodeKind::RhythmAccent) {
            json.key("parameters").beginObject();
            json.key("levels").beginArray();
            for (const int level : std::get<RhythmAccentNode>(node.payload).levels) {
                json.value(level);
            }
            json.endArray();
            json.endObject();
        } else if (kind == NodeKind::RhythmRealign) {
            json.key("parameters").beginObject();
            json.key("period").value(std::get<RhythmRealignNode>(node.payload).period.toString());
            json.endObject();
        } else if (kind == NodeKind::RhythmQuantize) {
            json.key("parameters").beginObject();
            json.key("grid").value(std::get<RhythmQuantizeNode>(node.payload).grid.toString());
            json.endObject();
        } else if (kind == NodeKind::RhythmAccentByMeter) {
            const RhythmAccentByMeterNode& payload =
                std::get<RhythmAccentByMeterNode>(node.payload);
            json.key("parameters").beginObject();
            json.key("metricContext").value(payload.meterMap.describe());
            json.key("anchor").value(payload.anchor.toString());
            json.endObject();
        } else if (kind == NodeKind::RhythmIntersect ||
                   kind == NodeKind::RhythmUnion ||
                   kind == NodeKind::RhythmDifference ||
                   kind == NodeKind::RhythmAccentFollow) {
            std::optional<Rational> grid;
            if (kind == NodeKind::RhythmIntersect) {
                grid = std::get<RhythmIntersectNode>(node.payload).grid;
            } else if (kind == NodeKind::RhythmUnion) {
                grid = std::get<RhythmUnionNode>(node.payload).grid;
            } else if (kind == NodeKind::RhythmDifference) {
                grid = std::get<RhythmDifferenceNode>(node.payload).grid;
            } else {
                grid = std::get<RhythmAccentFollowNode>(node.payload).grid;
            }
            json.key("parameters").beginObject();
            if (grid.has_value()) {
                json.key("grid").value(grid->toString());
            } else {
                json.key("grid").nullValue();
            }
            json.endObject();
        } else if (kind == NodeKind::RhythmComplement) {
            const RhythmComplementNode& payload = std::get<RhythmComplementNode>(node.payload);
            json.key("parameters").beginObject();
            json.key("span").value(payload.span.toString());
            json.key("grid").value(payload.grid.toString());
            json.endObject();
        } else if (kind == NodeKind::RhythmOnsetSet) {
            json.key("parameters").beginObject();
            json.key("unit").value(std::get<RhythmOnsetSetNode>(node.payload).unit.toString());
            json.endObject();
        } else if (kind == NodeKind::HarmonyQuery) {
            json.key("parameters").beginObject();
            json.key("at").value(std::get<HarmonyQueryNode>(node.payload).at.toString());
            json.endObject();
        } else if (kind == NodeKind::RhythmCombine) {
            const RhythmCombineNode& payload = std::get<RhythmCombineNode>(node.payload);
            json.key("parameters").beginObject();
            json.key("velocity").value(payload.velocity);
            json.key("cyclic").value(payload.cyclic);
            json.key("pitches").beginArray();
            for (const Pitch& pitch : payload.pitches) {
                json.value(pitch.name());
            }
            json.endArray();
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
                if (note.rhythmOrigin != kInvalidObjectId) {
                    json.key("rhythmOrigin").value(static_cast<std::int64_t>(note.rhythmOrigin));
                }
                if (note.rhythmEventIndex != kInvalidRhythmEvent) {
                    json.key("rhythmEvent").value(static_cast<std::int64_t>(note.rhythmEventIndex));
                }
                json.endObject();
            }
            json.endArray();
            json.endObject();
        }

        if (node.rhythmValue.has_value()) {
            json.key("rhythmValue");
            writeRhythmJson(json, *node.rhythmValue);
        }

        if (node.harmonyValue.has_value()) {
            json.key("harmonyValue");
            writeHarmonyJson(json, *node.harmonyValue);
        }

        if (node.chordValue.has_value()) {
            json.key("chordValue").beginObject();
            json.key("name").value(node.chordValue->name());
            json.key("pitchClasses").value(node.chordValue->pitchClassSet().name());
            json.key("intervals").beginArray();
            for (const int interval : node.chordValue->intervals) {
                json.value(interval);
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
