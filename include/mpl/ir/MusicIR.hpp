#pragma once

#include "mpl/core/Ids.hpp"
#include "mpl/core/Time.hpp"
#include "mpl/music/Fragment.hpp"
#include "mpl/music/Harmony.hpp"
#include "mpl/music/HarmonyTimeline.hpp"
#include "mpl/music/Key.hpp"
#include "mpl/music/Pitch.hpp"
#include "mpl/music/Rhythm.hpp"
#include "mpl/music/Transforms.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace mpl {

// Music IR is a typed dependency graph. Every object is a node; edges point
// from an object to the objects it was derived from. Because operators can only
// reference already existing objects, the graph is acyclic by construction.

struct LiteralNode {
    Fragment fragment;
};

struct TransposeNode {
    Interval interval;
};

struct ReverseNode {};

struct StretchNode {
    Rational factor{1, 1};
};

struct RepeatNode {
    int count = 1;
};

struct SequenceNode {};

struct ParallelNode {};

struct DiatonicTransposeNode {
    int scaleSteps = 0;
};

struct HarmonizeNode {
    std::vector<int> degreeOffsets;
    int velocity = 80;
};

struct SyncopateNode {
    Rational displacement{1, 8};
    /// When present, beat points come from this map and anchor instead of the
    /// single global meter. This is the explicit metric context required by
    /// variable-meter sections.
    std::optional<MeterMap> meterMap;
    Position anchor;
};

struct RotateNode {
    Rational amount{0, 1};
};

// Rhythm nodes are values in the same dependency graph as fragments. Keeping a
// single ObjectId space means rhythm transformations and rhythm-pitch
// combination participate in the normal invalidation and provenance model.
struct RhythmLiteralNode {
    Rhythm rhythm;
};

struct RhythmSequenceNode {};

struct RhythmOverlayNode {};

struct RhythmRepeatNode {
    int count = 1;
};

struct RhythmRotateNode {
    Rational amount{0, 1};
};

struct RhythmShiftNode {
    Rational amount{0, 1};
};

struct RhythmStretchNode {
    Rational factor{1, 1};
};

struct RhythmSubdivideNode {
    int parts = 1;
};

struct RhythmAccentNode {
    std::vector<int> levels;
};

struct RhythmRealignNode {
    Rational period{0, 1};
};

struct RhythmQuantizeNode {
    Rational grid{0, 1};
};

struct RhythmAccentByMeterNode {
    MeterMap meterMap;
    Position anchor;
};

struct RhythmIntersectNode {
    std::optional<Rational> grid;
};

struct RhythmUnionNode {
    std::optional<Rational> grid;
};

struct RhythmDifferenceNode {
    std::optional<Rational> grid;
};

struct RhythmComplementNode {
    Rational span{0, 1};
    Rational grid{0, 1};
};

struct RhythmAccentFollowNode {
    std::optional<Rational> grid;
};

/// Maps attack events of a rhythm onto a pitch sequence. `cyclic` selects
/// cyclic fill; strict mode requires equal attack and pitch counts. Rests do
/// not consume pitches.
struct RhythmCombineNode {
    std::vector<Pitch> pitches;
    int velocity = 96;
    bool cyclic = false;
};

/// Reduces a rhythm to its onset set: every attack keeps its onset/accent but
/// receives the explicit `unit` duration; rests are dropped.
struct RhythmOnsetSetNode {
    Rational unit{0, 1};
};

/// Clears split-continuation flags, turning every sounding segment into an
/// explicit retrigger.
struct RhythmRetriggerNode {};

struct HarmonyLiteralNode {
    HarmonyTimeline timeline;
};

/// Compile-time harmonic query. It keeps the timeline as an input so the trace
/// records which harmony produced the selected chord.
struct HarmonyQueryNode {
    Position at;
};

/// Explicit tie between the last sounding note of the first fragment and the
/// first sounding note of the second. It is intentionally not a merge of all
/// adjacent same-pitch notes.
struct TieNode {};

using NodePayload =
    std::variant<LiteralNode, TransposeNode, ReverseNode, StretchNode, RepeatNode, SequenceNode,
                 ParallelNode, DiatonicTransposeNode, HarmonizeNode, SyncopateNode, RotateNode,
                 RhythmLiteralNode, RhythmSequenceNode, RhythmOverlayNode, RhythmRepeatNode,
                 RhythmRotateNode, RhythmShiftNode, RhythmStretchNode, RhythmSubdivideNode,
                 RhythmAccentNode, RhythmRealignNode, RhythmQuantizeNode, RhythmAccentByMeterNode,
                 RhythmIntersectNode, RhythmUnionNode, RhythmDifferenceNode,
                 RhythmComplementNode, RhythmAccentFollowNode, RhythmCombineNode,
                 RhythmOnsetSetNode, RhythmRetriggerNode, HarmonyLiteralNode, HarmonyQueryNode,
                 TieNode>;

struct MusicNode {
    ObjectId id = kInvalidObjectId;
    std::string label;
    std::vector<ObjectId> inputs;
    NodePayload payload = ReverseNode{};

    /// Cached evaluation result. Cleared by invalidate() when a dependency
    /// changes, which is what enables incremental recompilation.
    std::optional<Fragment> value;
    std::optional<Rhythm> rhythmValue;
    std::optional<HarmonyTimeline> harmonyValue;
    std::optional<ChordSymbol> chordValue;
};

enum class NodeKind {
    Literal,
    Transpose,
    Reverse,
    Stretch,
    Repeat,
    Sequence,
    Parallel,
    DiatonicTranspose,
    Harmonize,
    Syncopate,
    Rotate,
    RhythmLiteral,
    RhythmSequence,
    RhythmOverlay,
    RhythmRepeat,
    RhythmRotate,
    RhythmShift,
    RhythmStretch,
    RhythmSubdivide,
    RhythmAccent,
    RhythmRealign,
    RhythmQuantize,
    RhythmAccentByMeter,
    RhythmIntersect,
    RhythmUnion,
    RhythmDifference,
    RhythmComplement,
    RhythmAccentFollow,
    RhythmCombine,
    RhythmOnsetSet,
    RhythmRetrigger,
    HarmonyLiteral,
    HarmonyQuery,
    Tie,
};

/// Non-graph dependencies that an operator reads while evaluating. Changing a
/// context aspect invalidates only the nodes that declare that aspect.
enum class ContextAspect { Key, Meter };

class MusicIR {
public:
    /// A sequential melody literal: durations are laid out one after another.
    ObjectId melody(const std::vector<std::pair<Pitch, Rational>>& values,
                    std::string label = {});

    /// A literal with explicit note offsets and an optional span.
    ObjectId notes(std::vector<Note> notes, std::optional<Duration> span = std::nullopt,
                   std::string label = {});

    ObjectId transpose(ObjectId input, Interval interval, std::string label = {});
    ObjectId reverse(ObjectId input, std::string label = {});
    ObjectId stretch(ObjectId input, const Rational& factor, std::string label = {});
    ObjectId repeat(ObjectId input, int count, std::string label = {});
    ObjectId sequence(const std::vector<ObjectId>& inputs, std::string label = {});
    ObjectId parallel(const std::vector<ObjectId>& inputs, std::string label = {});

    /// Explicit tie across a fragment boundary. The boundary notes must share
    /// pitch, be the only sounding notes at their respective boundaries, and
    /// meet exactly at the first fragment's span end.
    ObjectId tie(ObjectId first, ObjectId second, std::string label = {});

    /// Experimental V0.0.2 operators. Their evaluation reads the current IR
    /// context, so the node records a context dependency for invalidation.
    ObjectId diatonicTranspose(ObjectId input, int scaleSteps, std::string label = {});
    ObjectId harmonize(ObjectId input, std::vector<int> degreeOffsets,
                       std::string label = {});
    ObjectId syncopate(ObjectId input, const Rational& displacement, std::string label = {});
    ObjectId syncopate(ObjectId input, const Rational& displacement, const MeterMap& meterMap,
                       const Position& anchor, std::string label = {});

    /// Cyclic phase shift inside the fragment span.
    ObjectId rotate(ObjectId input, const Rational& amount, std::string label = {});

    // Rhythm value constructors and operators. All return rhythm objects in the
    // same ObjectId space; evaluating a rhythm object yields a Rhythm, not a
    // Fragment.
    ObjectId rhythmLiteral(Rhythm rhythm, std::string label = {});
    ObjectId rhythmSequence(const std::vector<ObjectId>& inputs, std::string label = {});
    ObjectId rhythmOverlay(const std::vector<ObjectId>& inputs, std::string label = {});
    ObjectId rhythmRepeat(ObjectId input, int count, std::string label = {});
    ObjectId rhythmRotate(ObjectId input, const Rational& amount, std::string label = {});
    ObjectId rhythmShift(ObjectId input, const Rational& amount, std::string label = {});
    ObjectId rhythmStretch(ObjectId input, const Rational& factor, std::string label = {});
    ObjectId rhythmSubdivide(ObjectId input, int parts, std::string label = {});
    ObjectId rhythmAccent(ObjectId input, std::vector<int> levels, std::string label = {});
    ObjectId rhythmRealign(ObjectId input, const Rational& period, std::string label = {});
    ObjectId rhythmQuantize(ObjectId input, const Rational& grid, std::string label = {});
    ObjectId rhythmAccentByMeter(ObjectId input, const MeterMap& meterMap,
                                 const Position& anchor, std::string label = {});
    ObjectId rhythmIntersect(ObjectId lhs, ObjectId rhs,
                             std::optional<Rational> grid, std::string label = {});
    ObjectId rhythmUnion(ObjectId lhs, ObjectId rhs, std::optional<Rational> grid,
                         std::string label = {});
    ObjectId rhythmDifference(ObjectId lhs, ObjectId rhs,
                              std::optional<Rational> grid, std::string label = {});
    ObjectId rhythmComplement(ObjectId input, const Rational& span, const Rational& grid,
                              std::string label = {});
    ObjectId rhythmAccentFollow(ObjectId source, ObjectId target,
                                std::optional<Rational> grid, std::string label = {});

    /// Explicit onset-set extraction. Each attack is reduced to the given unit
    /// duration; rests are dropped. Used by cross-voice set relations.
    ObjectId rhythmOnsetSet(ObjectId input, const Rational& unit, std::string label = {});

    /// Converts split continuations into explicit attacks.
    ObjectId rhythmRetrigger(ObjectId input, std::string label = {});

    ObjectId harmonyLiteral(HarmonyTimeline timeline, std::string label = {});
    ObjectId harmonyQuery(ObjectId harmony, const Position& at, std::string label = {});

    /// Combines a rhythm with explicit pitches. Strict mode requires one pitch
    /// per attack; cyclic mode repeats the pitch sequence. Rests never consume
    /// a pitch. The result is a Fragment whose span is the rhythm period.
    ObjectId rhythmCombine(ObjectId rhythm, std::vector<Pitch> pitches, int velocity,
                           bool cyclic, std::string label = {});

    const Key& key() const noexcept { return key_; }
    const Meter& meter() const noexcept { return meter_; }

    void setKey(Key key);
    void setMeter(Meter meter);

    const std::vector<MusicNode>& nodes() const noexcept { return nodes_; }
    const MusicNode& node(ObjectId id) const;
    bool contains(ObjectId id) const noexcept;
    std::size_t size() const noexcept { return nodes_.size(); }
    bool empty() const noexcept { return nodes_.empty(); }

    /// Evaluates an object and caches the result in the node.
    const Fragment& evaluate(ObjectId id);

    /// Evaluates a rhythm object and caches the Rhythm value.
    const Rhythm& evaluateRhythm(ObjectId id);

    /// True when the object produces a Rhythm rather than a Fragment.
    bool isRhythmObject(ObjectId id) const;

    /// Cached rhythm value, or nullptr when the object is not a rhythm or has
    /// not been evaluated yet. Compilation.evaluateAll() populates this.
    const Rhythm* cachedRhythm(ObjectId id) const;

    /// Evaluates a harmony timeline object.
    const HarmonyTimeline& evaluateHarmony(ObjectId id);

    /// Evaluates a harmony-query object to its chord symbol.
    const ChordSymbol& evaluateChord(ObjectId id);

    bool isHarmonyObject(ObjectId id) const;
    bool isChordObject(ObjectId id) const;
    const HarmonyTimeline* cachedHarmony(ObjectId id) const;
    const ChordSymbol* cachedChord(ObjectId id) const;

    /// Evaluates every object in dependency order.
    void evaluateAll();

    std::size_t evaluatedCount() const noexcept;
    std::size_t evaluationCount() const noexcept { return evaluations_; }
    std::size_t cacheHitCount() const noexcept { return cacheHits_; }

    /// Objects that directly or transitively consume `id`, sorted ascending.
    std::vector<ObjectId> dependents(ObjectId id) const;

    /// Drops the cached value of `id` and everything derived from it.
    void invalidate(ObjectId id);

    /// Replaces the content of a literal object and invalidates every derived
    /// object. This is the entry point for incremental recompilation: after
    /// editing the motif, evaluateAll() recomputes only the affected subgraph.
    void updateLiteral(ObjectId id, std::vector<Note> notes,
                       std::optional<Duration> span = std::nullopt);

    /// Ids are allocated in creation order, so ascending id order is a valid
    /// topological order.
    std::vector<ObjectId> evaluationOrder() const;

    std::string toJson() const;

private:
    MusicNode& mutableNode(ObjectId id);
    void requireInputs(const std::vector<ObjectId>& inputs, std::size_t minCount,
                       std::string_view operation) const;
    void requireRhythmInput(ObjectId id, std::string_view operation) const;
    void requireHarmonyInput(ObjectId id, std::string_view operation) const;
    Fragment evaluateNode(const MusicNode& node);
    Rhythm evaluateRhythmNode(const MusicNode& node);
    HarmonyTimeline evaluateHarmonyNode(const MusicNode& node);
    ChordSymbol evaluateChordNode(const MusicNode& node);
    void invalidateContext(ContextAspect aspect);

    std::vector<MusicNode> nodes_;
    Key key_;
    Meter meter_{4, 4};
    std::size_t evaluations_ = 0;
    std::size_t cacheHits_ = 0;
};

NodeKind nodeKind(const NodePayload& payload);

std::string_view nodeKindName(NodeKind kind);

/// True for every node kind whose evaluated value is a Rhythm.
bool isRhythmKind(NodeKind kind);

bool isHarmonyKind(NodeKind kind);

bool isChordKind(NodeKind kind);

std::vector<ContextAspect> contextDependencies(NodeKind kind);

std::string_view contextAspectName(ContextAspect aspect);

}  // namespace mpl
