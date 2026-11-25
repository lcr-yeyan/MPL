#include "TestHarness.hpp"

#include "mpl/Compiler.hpp"
#include "mpl/backend/MidiBackend.hpp"
#include "mpl/frontend/Frontend.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

using namespace mpl;

namespace {

std::string readText(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

std::string header() {
    return "project T { tempo = 120 bpm; meter = 4/4; key = C major; }\n"
           "asset PianoAsset = instrument(\"GrandPiano\");\n";
}

std::string program(const std::string& body) {
    return header() + body +
           "track Piano { asset = PianoAsset; role = melodic; }\n"
           "section S(1 bars) { clip Piano { play M; } }\n"
           "arrangement { play S; }\n"
           "export midi(\"x.mid\");\n";
}

bool hasFrontendCode(const FrontendResult& result, const std::string& code) {
    for (const FrontendDiagnostic& diagnostic : result.diagnostics) {
        if (diagnostic.code == code) {
            return true;
        }
    }
    return false;
}

std::vector<std::uint8_t> midiOf(const Compilation& compilation) {
    const MidiBackend backend(MidiOptions{480, "MPL Conductor"});
    return backend.serialize(compilation.production, compilation.temporal);
}

Compilation compileText(const std::string& source) {
    FrontendResult result = compileSource(source, "<functions>");
    if (!result.ok()) {
        ::mpltest::fail(__FILE__, __LINE__, "program failed to compile");
    }
    const Compiler compiler;
    return compiler.compile(std::move(result.program->project));
}

std::string functionMotif() {
    return "fn motif(root: Pitch, step: Int) -> Fragment context key {\n"
           "  let a = note(root, 1/8, 96);\n"
           "  let b = diatonicTranspose(a, step);\n"
           "  return sequence(a, b, a);\n"
           "}\n"
           "let M = motif(C4, 1);\n";
}

}  // namespace

MPL_TEST(FunctionInlineEquivalence) {
    const std::string functionSource = program(functionMotif());
    const std::string inlineSource = program(
        "let a = note(C4, 1/8, 96);\n"
        "let b = diatonicTranspose(a, 1);\n"
        "let M = sequence(a, b, a);\n");
    const Compilation functionCompilation = compileText(functionSource);
    const Compilation inlineCompilation = compileText(inlineSource);
    MPL_CHECK(midiOf(functionCompilation) == midiOf(inlineCompilation));
    MPL_CHECK_EQ(functionCompilation.production.noteCount(),
                 inlineCompilation.production.noteCount());
}

MPL_TEST(FunctionParameterAndReturnDiagnostics) {
    {
        const FrontendResult result = compileSource(
            program("fn motif(root: Pitch, step: Int) -> Fragment {\n"
                    "  let a = note(root, 1/8, 96);\n"
                    "  return diatonicTranspose(a, step);\n"
                    "}\n"
                    "let M = motif(C4, 1);\n"),
            "<test>");
        MPL_CHECK(hasFrontendCode(result, "context-not-declared"));
    }
    {
        const FrontendResult result = compileSource(
            program("fn motif(root: Pitch) -> Fragment { return 1; }\n"
                    "let M = motif(C4);\n"),
            "<test>");
        MPL_CHECK(hasFrontendCode(result, "type-mismatch"));
    }
    {
        const FrontendResult result = compileSource(
            program("fn motif(root: Pitch, step: Int) -> Fragment { return note(root, 1/4, 96); }\n"
                    "let M = motif(C4);\n"),
            "<test>");
        MPL_CHECK(hasFrontendCode(result, "type-arity"));
    }
    {
        const FrontendResult result = compileSource(
            program("fn motif(root: Pitch, step: Int) -> Fragment { return note(root, 1/4, 96); }\n"
                    "let M = motif(C4, 1 st);\n"),
            "<test>");
        MPL_CHECK(hasFrontendCode(result, "type-mismatch"));
    }
    {
        const FrontendResult result = compileSource(
            program("fn motif() -> Fragment { let a = note(C4, 1/4, 96); return a; }\n"
                    "fn motif() -> Fragment { return note(D4, 1/4, 96); }\n"
                    "let M = motif();\n"),
            "<test>");
        MPL_CHECK(hasFrontendCode(result, "sema-duplicate-symbol"));
    }
}

MPL_TEST(FunctionScopeAndReturnDiagnostics) {
    {
        const FrontendResult result = compileSource(
            program("fn f() -> Fragment {\n"
                    "  let a = note(C4, 1/4, 96);\n"
                    "  let a = note(D4, 1/4, 96);\n"
                    "  return a;\n"
                    "}\n"
                    "let M = f();\n"),
            "<test>");
        MPL_CHECK(hasFrontendCode(result, "sema-duplicate-local"));
    }
    {
        const FrontendResult result = compileSource(
            program("fn f() -> Fragment {\n"
                    "  return note(C4, 1/4, 96);\n"
                    "  return note(D4, 1/4, 96);\n"
                    "}\n"
                    "let M = f();\n"),
            "<test>");
        MPL_CHECK(hasFrontendCode(result, "sema-return-not-last"));
    }
    {
        const FrontendResult result = compileSource(
            program("fn f() -> Fragment { let a = note(C4, 1/4, 96); }\n"
                    "let M = note(D4, 1/4, 96);\n"),
            "<test>");
        MPL_CHECK(hasFrontendCode(result, "sema-function-return"));
    }
    {
        const FrontendResult result = compileSource(
            program("fn f() -> Fragment { let a = PianoAsset; return a; }\n"
                    "let M = note(D4, 1/4, 96);\n"),
            "<test>");
        MPL_CHECK(hasFrontendCode(result, "sema-local-type"));
    }
}

MPL_TEST(FunctionNestedCallsAndTrace) {
    const Compilation compilation = compileText(program(
        "fn inner(root: Pitch) -> Fragment { return note(root, 1/4, 96); }\n"
        "fn outer(root: Pitch, count: Int) -> Fragment context key {\n"
        "  let parts = for i in range(0, count) { inner(root) };\n"
        "  return sequenceAll(parts);\n"
        "}\n"
        "let M = outer(C4, 3);\n"));
    MPL_CHECK(!compilation.hasErrors());
    MPL_CHECK_EQ(compilation.functionDefinitions.size(), std::size_t{2});
    MPL_CHECK(compilation.functionCalls.size() >= 4);
    bool sawOuter = false;
    bool sawInner = false;
    bool sawCacheHit = false;
    for (const FunctionCallRecord& call : compilation.functionCalls) {
        sawOuter = sawOuter || call.function == "outer";
        sawInner = sawInner || (call.function == "inner" && call.depth == 1);
        sawCacheHit = sawCacheHit || call.cacheHit;
    }
    MPL_CHECK(sawOuter);
    MPL_CHECK(sawInner);
    MPL_CHECK(sawCacheHit);
    const std::string trace = compilation.toTraceJson(false);
    MPL_CHECK(trace.find("\"functions\"") != std::string::npos);
    MPL_CHECK(trace.find("\"functionCalls\"") != std::string::npos);
    MPL_CHECK(trace.find("\"cacheHit\"") != std::string::npos);
    MPL_CHECK(trace.find("\"operators\"") != std::string::npos);
}

MPL_TEST(FunctionCallIdentityAndCacheSharing) {
    const Compilation compilation = compileText(program(
        "fn make(root: Pitch) -> Fragment { return note(root, 1/4, 96); }\n"
        "let A = make(C4);\n"
        "let B = make(C4);\n"
        "let C = make(D4);\n"
        "let M = A;\n"));
    MPL_CHECK(!compilation.hasErrors());
    MPL_CHECK_EQ(compilation.production.tracks.front().clips.size(), std::size_t{1});
    const auto& clips = compilation.production.tracks.front().clips;
    MPL_CHECK_EQ(clips.size(), std::size_t{1});
    MPL_CHECK(clips.front().content.noteCount() > 0);
    std::size_t cacheHits = 0;
    for (const FunctionCallRecord& call : compilation.functionCalls) {
        cacheHits += call.cacheHit ? 1 : 0;
    }
    MPL_CHECK(cacheHits >= 1);
}

MPL_TEST(FunctionCallIdentityAcrossClips) {
    const std::string source =
        header() +
        "fn make(root: Pitch) -> Fragment { return note(root, 1/4, 96); }\n"
        "let A = make(C4);\n"
        "let B = make(C4);\n"
        "let C = make(D4);\n"
        "track Piano { asset = PianoAsset; role = melodic; }\n"
        "section S(1 bars) {\n"
        "  clip Piano \"A\" at 0 bars { play A; }\n"
        "  clip Piano \"B\" at 1/4 bars { play B; }\n"
        "  clip Piano \"C\" at 1/2 bars { play C; }\n"
        "}\n"
        "arrangement { play S; }\n"
        "export midi(\"x.mid\");\n";
    const Compilation compilation = compileText(source);
    MPL_CHECK(!compilation.hasErrors());
    const auto& clips = compilation.production.tracks.front().clips;
    MPL_CHECK_EQ(clips.size(), std::size_t{3});
    MPL_CHECK_EQ(clips[0].sourceObject, clips[1].sourceObject);
    MPL_CHECK(clips[0].sourceObject != clips[2].sourceObject);
}

MPL_TEST(FunctionRecursionIsRejected) {
    const FrontendResult direct = compileSource(
        program("fn f(x: Int) -> Int { return f(x); }\n"
                "let M = note(C4, 1/4, 96);\n"),
        "<test>");
    MPL_CHECK(hasFrontendCode(direct, "sema-recursion-unsupported"));

    const FrontendResult mutual = compileSource(
        program("fn f(x: Int) -> Int { return g(x); }\n"
                "fn g(x: Int) -> Int { return f(x); }\n"
                "let M = note(C4, 1/4, 96);\n"),
        "<test>");
    MPL_CHECK(hasFrontendCode(mutual, "sema-recursion-unsupported"));
}

MPL_TEST(FunctionControlFlowAndRandom) {
    const std::string base =
        "fn gen(root: Pitch, seed: Int, count: Int) -> Fragment context key {\n"
        "  let parts = for i in range(0, count) {\n"
        "    if random(seed, i) > 0.5 {\n"
        "      note(root, 1/8, round(90 + random(seed, i + 100) * 30))\n"
        "    } else {\n"
        "      rest(1/8)\n"
        "    }\n"
        "  };\n"
        "  return sequenceAll(parts);\n"
        "}\n"
        "let M = gen(C4, SEED, 8);\n";
    auto source = [&](int seed) {
        std::string text = program(base);
        const std::string token = "SEED";
        const std::size_t position = text.find(token);
        text.replace(position, token.size(), std::to_string(seed));
        return text;
    };
    const Compilation first = compileText(source(7));
    const Compilation second = compileText(source(7));
    const Compilation different = compileText(source(9));
    MPL_CHECK(!first.hasErrors());
    const std::size_t notes = first.production.noteCount();
    MPL_CHECK(notes > 0);
    MPL_CHECK(notes <= 8);
    MPL_CHECK(midiOf(first) == midiOf(second));
    MPL_CHECK(midiOf(first) != midiOf(different));
}

MPL_TEST(FunctionCollectionAndCallLimits) {    {
        const FrontendResult result = compileSource(
            program("let R = range(0, 2000);\n"
                    "let M = note(C4, 1/4, 96);\n"),
            "<test>");
        MPL_CHECK(hasFrontendCode(result, "sema-collection-limit"));
    }    {
        std::string source = header();
        for (int index = 0; index < 40; ++index) {
            source += "fn f" + std::to_string(index) + "(x: Int) -> Int { return f" +
                      std::to_string(index + 1) + "(x); }\n";
        }
        source += "fn f40(x: Int) -> Int { return x; }\n";
        source += "let D = f0(1);\n";
        source += "let M = note(C4, 1/4, 96);\n";
        source += "track Piano { asset = PianoAsset; role = melodic; }\n";
        source += "section S(1 bars) { clip Piano { play M; } }\n";
        source += "arrangement { play S; }\n";
        source += "export midi(\"x.mid\");\n";
        const FrontendResult result = compileSource(source, "<test>");
        MPL_CHECK(hasFrontendCode(result, "limit-call-depth"));
    }    {
        const FrontendResult result = compileSource(
            program("fn f() -> Fragment {\n"
                    "  let parts = for i in range(0, 100) {\n"
                    "    for j in range(0, 100) { note(C4, 1/16, 90) }\n"
                    "  };\n"
                    "  return note(C4, 1/4, 96);\n"
                    "}\n"
                    "let M = f();\n"),
            "<test>");
        MPL_CHECK(hasFrontendCode(result, "limit-loop-iterations"));
    }
}

MPL_TEST(FunctionMeterContextIsRequired) {
    const FrontendResult missing = compileSource(
        program("fn f() -> Fragment {\n"
                "  let a = note(C4, 1/8, 96);\n"
                "  return syncopate(a, 1/16);\n"
                "}\n"
                "let M = f();\n"),
        "<test>");
    MPL_CHECK(hasFrontendCode(missing, "context-not-declared"));

    const FrontendResult transitive = compileSource(
        program("fn inner() -> Fragment context meter { return syncopate(note(C4, 1/8, 96), 1/16); }\n"
                "fn outer() -> Fragment { return inner(); }\n"
                "let M = outer();\n"),
        "<test>");
    MPL_CHECK(hasFrontendCode(transitive, "context-not-declared"));
}

MPL_TEST(MathRockReferenceProjectVariants) {
    const std::filesystem::path path =
        std::filesystem::path(MPL_EXAMPLES_DIR) / "math-rock.mpl";
    const std::string base = readText(path);
    FrontendResult firstFrontend = compileSource(base, path.string());
    MPL_CHECK(firstFrontend.ok());
    const Compiler compiler;
    const Compilation first = compiler.compile(std::move(firstFrontend.program->project));
    MPL_CHECK(!first.hasErrors());
    MPL_CHECK_EQ(first.production.tracks.size(), std::size_t{4});
    MPL_CHECK(first.production.meterMap.front().meter.toString() == std::string("7/8"));
    MPL_CHECK_EQ(first.production.meterMap.size(), std::size_t{3});
    MPL_CHECK(first.production.meterMap[0].grouping.toString() == std::string("2+2+3"));
    MPL_CHECK(first.production.noteCount() > 100);
    MPL_CHECK(first.functionDefinitions.size() >= 6);
    MPL_CHECK(!first.functionCalls.empty());

    std::string variant = base;
    const std::string token = "let Variant = 1;";
    const std::size_t position = variant.find(token);
    MPL_CHECK(position != std::string::npos);
    variant.replace(position, token.size(), "let Variant = 2;");
    FrontendResult secondFrontend = compileSource(variant, path.string());
    MPL_CHECK(secondFrontend.ok());
    const Compilation second = compiler.compile(std::move(secondFrontend.program->project));
    MPL_CHECK(!second.hasErrors());
    MPL_CHECK(midiOf(first) != midiOf(second));
}

MPL_TEST(FunctionTransitiveDefinitionChange) {
    auto source = [](const std::string& innerPitch) {
        return header() +
               "fn inner() -> Fragment { return note(" + innerPitch + ", 1/4, 96); }\n"
               "fn outer() -> Fragment { return inner(); }\n"
               "let M = outer();\n"
               "track Piano { asset = PianoAsset; role = melodic; }\n"
               "section S(1 bars) { clip Piano { play M; } }\n"
               "arrangement { play S; }\n"
               "export midi(\"x.mid\");\n";
    };
    const Compilation first = compileText(source("C4"));
    const Compilation second = compileText(source("D4"));
    MPL_CHECK_EQ(first.production.tracks.front().clips.front().content.notes().front().pitch.name(),
                 std::string("C4"));
    MPL_CHECK_EQ(second.production.tracks.front().clips.front().content.notes().front().pitch.name(),
                 std::string("D4"));
    MPL_CHECK(midiOf(first) != midiOf(second));
}

MPL_TEST(FunctionContextIsolationAcrossSections) {
    auto source = [](const std::string& meterB) {
        return header() +
               "fn push() -> Fragment context meter {\n"
               "  let a = note(C4, 1/8, 90);\n"
               "  return syncopate(sequence(rest(1/8), a), 1/16);\n"
               "}\n"
               "track Piano { asset = PianoAsset; role = melodic; }\n"
               "section A(1 bars) meter 7/8 grouping 2+2+3 {\n"
               "  clip Piano \"A\" { play push(); }\n"
               "}\n"
               "section B(1 bars) meter " + meterB + " {\n"
               "  clip Piano \"B\" { play push(); }\n"
               "}\n"
               "arrangement { play A; play B; }\n"
               "export midi(\"x.mid\");\n";
    };
    const Compilation first = compileText(source("4/4"));
    const Compilation second = compileText(source("5/8"));
    const auto& firstClips = first.production.tracks.front().clips;
    const auto& secondClips = second.production.tracks.front().clips;
    MPL_CHECK_EQ(firstClips.size(), std::size_t{2});
    MPL_CHECK_EQ(secondClips.size(), std::size_t{2});
    // Section A's metric context is unchanged, so its rendered offset must not
    // move when section B changes meter.
    MPL_CHECK(firstClips[0].content.notes().front().offset ==
              secondClips[0].content.notes().front().offset);
    MPL_CHECK(firstClips[1].content.notes().front().offset !=
              secondClips[1].content.notes().front().offset);
}

MPL_TEST(FunctionSharedValueIndependentPerformanceInstance) {
    const std::string source =
        header() +
        "curve V1 on relative = linear(0.5, 1.0);\n"
        "curve V2 on relative = linear(1.0, 1.5);\n"
        "let M = note(C4, 1/4, 96);\n"
        "track Piano { asset = PianoAsset; role = melodic; }\n"
        "section S(1 bars) {\n"
        "  clip Piano \"one\" at 0 bars { play M; velocity = V1; }\n"
        "  clip Piano \"two\" at 1/2 bars { play M; velocity = V2; }\n"
        "}\n"
        "arrangement { play S; }\n"
        "export midi(\"x.mid\");\n";
    const Compilation compilation = compileText(source);
    const auto& clips = compilation.production.tracks.front().clips;
    MPL_CHECK_EQ(clips.size(), std::size_t{2});
    MPL_CHECK_EQ(clips[0].sourceObject, clips[1].sourceObject);
    MPL_CHECK(clips[0].instanceId != clips[1].instanceId);
    MPL_CHECK(clips[0].expression.velocity != kInvalidTemporalId);
    MPL_CHECK(clips[1].expression.velocity != kInvalidTemporalId);
    MPL_CHECK(clips[0].expression.velocity != clips[1].expression.velocity);
}

MPL_TEST(IndependentFunctionEditStability) {
    auto source = [](const std::string& aPitch) {
        return header() +
               "fn A() -> Fragment { return note(" + aPitch + ", 1/4, 96); }\n"
               "fn B() -> Fragment { return note(G4, 1/4, 96); }\n"
               "let AOut = A();\n"
               "let BOut = B();\n"
               "track Piano { asset = PianoAsset; role = melodic; }\n"
               "section S(1 bars) {\n"
               "  clip Piano \"A\" at 0 bars { play AOut; }\n"
               "  clip Piano \"B\" at 1/2 bars { play BOut; }\n"
               "}\n"
               "arrangement { play S; }\n"
               "export midi(\"x.mid\");\n";
    };
    const Compilation first = compileText(source("C4"));
    const Compilation second = compileText(source("E4"));
    MPL_CHECK_EQ(first.production.tracks.front().clips.size(), std::size_t{2});
    const auto& firstClips = first.production.tracks.front().clips;
    const auto& secondClips = second.production.tracks.front().clips;
    MPL_CHECK_EQ(firstClips[1].sourceObject, secondClips[1].sourceObject);
    MPL_CHECK_EQ(firstClips[1].content.notes().front().pitch.name(), std::string("G4"));
    MPL_CHECK_EQ(secondClips[1].content.notes().front().pitch.name(), std::string("G4"));
    MPL_CHECK(firstClips[0].content.notes().front().pitch.name() !=
              secondClips[0].content.notes().front().pitch.name());
    const auto& firstFunctions = first.functionDefinitions;
    const auto& secondFunctions = second.functionDefinitions;
    MPL_CHECK_EQ(firstFunctions.size(), secondFunctions.size());
    if (firstFunctions.size() == 2 && secondFunctions.size() == 2) {
        MPL_CHECK(firstFunctions[0].version != secondFunctions[0].version);
        MPL_CHECK_EQ(firstFunctions[1].version, secondFunctions[1].version);
    }
}
