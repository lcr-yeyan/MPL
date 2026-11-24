#pragma once

#include "mpl/core/Ids.hpp"
#include "mpl/core/Time.hpp"
#include "mpl/music/Fragment.hpp"
#include "mpl/music/Harmony.hpp"
#include "mpl/music/Key.hpp"
#include "mpl/music/Pitch.hpp"
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
};

using NodePayload =
    std::variant<LiteralNode, TransposeNode, ReverseNode, StretchNode, RepeatNode, SequenceNode,
                 ParallelNode, DiatonicTransposeNode, HarmonizeNode, SyncopateNode>;

struct MusicNode {
    ObjectId id = kInvalidObjectId;
    std::string label;
    std::vector<ObjectId> inputs;
    NodePayload payload = ReverseNode{};

    /// Cached evaluation result. Cleared by invalidate() when a dependency
    /// changes, which is what enables incremental recompilation.
    std::optional<Fragment> value;
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

    /// Experimental V0.0.2 operators. Their evaluation reads the current IR
    /// context, so the node records a context dependency for invalidation.
    ObjectId diatonicTranspose(ObjectId input, int scaleSteps, std::string label = {});
    ObjectId harmonize(ObjectId input, std::vector<int> degreeOffsets,
                       std::string label = {});
    ObjectId syncopate(ObjectId input, const Rational& displacement, std::string label = {});

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
    Fragment evaluateNode(const MusicNode& node);
    void invalidateContext(ContextAspect aspect);

    std::vector<MusicNode> nodes_;
    Key key_;
    Meter meter_{4, 4};
    std::size_t evaluations_ = 0;
    std::size_t cacheHits_ = 0;
};

NodeKind nodeKind(const NodePayload& payload);

std::string_view nodeKindName(NodeKind kind);

std::vector<ContextAspect> contextDependencies(NodeKind kind);

std::string_view contextAspectName(ContextAspect aspect);

}  // namespace mpl
