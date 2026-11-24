#pragma once

#include "mpl/frontend/Source.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace mpl {

using ExprId = std::uint32_t;

inline constexpr ExprId kInvalidExprId = 0xFFFFFFFFu;

enum class ExprKind {
    Invalid,
    Integer,
    Rational,
    Number,
    String,
    Identifier,
    Key,
    Call,
    UnaryMinus,
    List,
    Melody,
    Notes,
    Keyframes,
};

struct MelodyEntry {
    SourceSpan span;
    std::string pitch;
    ExprId duration = kInvalidExprId;
};

struct NoteEntry {
    SourceSpan span;
    std::string pitch;
    ExprId offset = kInvalidExprId;
    ExprId duration = kInvalidExprId;
    int velocity = 96;
};

struct KeyframeEntry {
    SourceSpan span;
    ExprId time = kInvalidExprId;
    double value = 0.0;
    std::string interpolation;
};

struct Expr {
    ExprKind kind = ExprKind::Invalid;
    SourceSpan span;
    std::string text;
    double number = 0.0;
    std::int64_t integer = 0;
    std::int64_t numerator = 0;
    std::int64_t denominator = 1;
    std::string unit;
    std::vector<ExprId> args;
    std::vector<MelodyEntry> melody;
    std::vector<NoteEntry> notes;
    std::vector<KeyframeEntry> keyframes;
    ExprId explicitSpan = kInvalidExprId;
};

struct ProjectDecl {
    SourceSpan span;
    std::string name;
    ExprId tempo = kInvalidExprId;
    ExprId meter = kInvalidExprId;
    ExprId key = kInvalidExprId;
};

struct AssetDecl {
    SourceSpan span;
    std::string name;
    std::string kind;
    std::string provider;
};

struct LetDecl {
    SourceSpan span;
    std::string name;
    ExprId value = kInvalidExprId;
};

struct CurveDecl {
    SourceSpan span;
    std::string name;
    std::string domain;
    ExprId extent = kInvalidExprId;
    ExprId value = kInvalidExprId;
};

struct SendDecl {
    SourceSpan span;
    std::string bus;
    ExprId level = kInvalidExprId;
};

struct SidechainDecl {
    SourceSpan span;
    std::string processor;
    std::string source;
};

struct TrackDecl {
    SourceSpan span;
    std::string name;
    std::string asset;
    std::string role;
    ExprId gain = kInvalidExprId;
    ExprId pan = kInvalidExprId;
    std::vector<std::string> inserts;
    std::vector<SendDecl> sends;
    std::vector<SidechainDecl> sidechains;
};

struct ProcessorField {
    SourceSpan span;
    std::string name;
    ExprId value = kInvalidExprId;
};

struct ProcessorDecl {
    SourceSpan span;
    std::string name;
    std::string kind;
    std::string mode;
    std::vector<ProcessorField> fields;
};

struct BusDecl {
    SourceSpan span;
    std::string name;
    ExprId gain = kInvalidExprId;
    ExprId pan = kInvalidExprId;
    std::vector<std::string> inserts;
};

struct MasterDecl {
    SourceSpan span;
    ExprId gain = kInvalidExprId;
    ExprId pan = kInvalidExprId;
    std::vector<std::string> inserts;
};

struct ClipDecl {
    SourceSpan span;
    std::string track;
    std::string name;
    ExprId at = kInvalidExprId;
    ExprId play = kInvalidExprId;
    ExprId velocity = kInvalidExprId;
    ExprId timing = kInvalidExprId;
    ExprId gate = kInvalidExprId;
    ExprId pan = kInvalidExprId;
};

struct SectionDecl {
    SourceSpan span;
    std::string name;
    ExprId duration = kInvalidExprId;
    std::vector<ClipDecl> clips;
};

struct AutomationDecl {
    SourceSpan span;
    std::string scope;
    std::string track;
    std::string processor;
    std::string parameter;
    ExprId curve = kInvalidExprId;
    std::string mode;
};

struct ArrangementDecl {
    SourceSpan span;
    std::vector<std::pair<std::string, SourceSpan>> plays;
};

struct ExportDecl {
    SourceSpan span;
    std::string format;
    std::string path;
};

enum class DeclKind { Asset, Let, Curve, Track, Section, Automation, Processor, Bus };

struct DeclRef {
    DeclKind kind = DeclKind::Asset;
    std::size_t index = 0;
};

struct Module {
    SourceSpan span;
    std::string file;
    std::optional<ProjectDecl> project;
    std::vector<AssetDecl> assets;
    std::vector<LetDecl> lets;
    std::vector<CurveDecl> curves;
    std::vector<TrackDecl> tracks;
    std::vector<SectionDecl> sections;
    std::vector<AutomationDecl> automations;
    std::vector<ProcessorDecl> processors;
    std::vector<BusDecl> buses;
    std::optional<MasterDecl> master;
    std::optional<ArrangementDecl> arrangement;
    std::vector<ExportDecl> exports;
    std::vector<DeclRef> declarationOrder;
    std::vector<Expr> expressions;

    const Expr& expr(ExprId id) const { return expressions[id]; }
};

std::string_view exprKindName(ExprKind kind);

std::string astToJson(const Module& module);

}  // namespace mpl
