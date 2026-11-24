#include "TestHarness.hpp"

#include "mpl/Compiler.hpp"
#include "mpl/backend/MidiBackend.hpp"
#include "mpl/examples/CoreDemo.hpp"
#include "mpl/frontend/Ast.hpp"
#include "mpl/frontend/Diagnostic.hpp"
#include "mpl/frontend/Frontend.hpp"
#include "mpl/frontend/Lexer.hpp"
#include "mpl/frontend/Parser.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using namespace mpl;

namespace {

std::string readText(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

std::filesystem::path examplePath(const std::string& name) {
    return std::filesystem::path(MPL_EXAMPLES_DIR) / name;
}

FrontendResult compileExample(const std::string& name) {
    const std::filesystem::path path = examplePath(name);
    return compileSource(readText(path), path.string());
}

FrontendResult compileText(const std::string& source) {
    return compileSource(source, "<test>");
}

std::string diagnosticText(const FrontendResult& result) {
    DiagnosticSink sink;
    for (const FrontendDiagnostic& diagnostic : result.diagnostics) {
        sink.add(diagnostic);
    }
    return sink.toHuman();
}

bool hasCode(const FrontendResult& result, const std::string& code) {
    for (const FrontendDiagnostic& diagnostic : result.diagnostics) {
        if (diagnostic.code == code) {
            return true;
        }
    }
    return false;
}

bool hasWarning(const FrontendResult& result, const std::string& code) {
    for (const FrontendDiagnostic& diagnostic : result.diagnostics) {
        if (diagnostic.code == code && diagnostic.severity == DiagnosticSeverity::Warning) {
            return true;
        }
    }
    return false;
}

std::vector<std::uint8_t> midiOf(const Project& project) {
    const Compiler compiler;
    const Compilation compilation = compiler.compile(project);
    const MidiBackend backend(MidiOptions{480, "MPL Conductor"});
    return backend.serialize(compilation.production, compilation.temporal);
}

Compilation compileReference() {
    FrontendResult result = compileExample("reference.mpl");
    if (!result.ok()) {
        throw ::mpltest::AssertionFailure("reference.mpl failed:\n" + diagnosticText(result));
    }
    const Compiler compiler;
    Compilation compilation = compiler.compile(std::move(result.program->project));
    const MidiBackend backend(MidiOptions{480, "MPL Conductor"});
    std::vector<Diagnostic> backendDiagnostics;
    (void)backend.serialize(compilation.production, compilation.temporal, &backendDiagnostics);
    compilation.diagnostics.insert(compilation.diagnostics.end(), backendDiagnostics.begin(),
                                   backendDiagnostics.end());
    return compilation;
}

void checkGolden(const std::string& actual, const std::string& name) {
    const std::filesystem::path golden = std::filesystem::path(MPL_GOLDEN_DIR) / name;
    if (::mpltest::updateGoldenMode()) {
        std::filesystem::create_directories(golden.parent_path());
        std::ofstream stream(golden, std::ios::binary | std::ios::trunc);
        stream << actual;
        return;
    }
    const std::string expected = readText(golden);
    if (actual == expected) {
        return;
    }
    std::filesystem::path outputDirectory(MPL_TEST_OUTPUT_DIR);
    std::filesystem::create_directories(outputDirectory);
    const std::filesystem::path actualPath = outputDirectory / (name + ".actual");
    std::ofstream(actualPath, std::ios::binary) << actual;
    throw ::mpltest::AssertionFailure("golden mismatch: " + golden.string() +
                                      " (actual written to " + actualPath.string() + ")");
}

std::string baseProgram(const std::string& declarations) {
    return "project T {\n tempo = 120 bpm; meter = 4/4; key = C major;\n}\n" + declarations;
}

}  // namespace

MPL_TEST(LexerProducesStableTokens) {
    DiagnosticSink diagnostics;
    const std::string source =
        "// comment\nproject P { tempo = 160 bpm; meter = 4/4; }\n"
        "let X = melody { F4: 1/8, Ab4: 1/4 };\n";
    Lexer lexer(source, "tokens.mpl", diagnostics);
    const std::vector<Token> tokens = lexer.lex();
    MPL_CHECK(!diagnostics.hasErrors());
    MPL_CHECK(tokens.front().kind == TokenKind::Project);
    MPL_CHECK(tokens[1].kind == TokenKind::Identifier);
    MPL_CHECK(tokens[2].kind == TokenKind::LBrace);
    bool sawRational = false;
    bool sawPitch = false;
    for (const Token& token : tokens) {
        if (token.kind == TokenKind::Rational && token.numerator == 4 && token.denominator == 4) {
            sawRational = true;
        }
        if (token.kind == TokenKind::Identifier && token.text == "Ab4") {
            sawPitch = true;
        }
    }
    MPL_CHECK(sawRational);
    MPL_CHECK(sawPitch);
}

MPL_TEST(LexerReportsUnterminatedStringAndCharacter) {
    DiagnosticSink diagnostics;
    Lexer lexer("project P { asset A = instrument(\"unterminated); } @", "bad.mpl", diagnostics);
    (void)lexer.lex();
    MPL_CHECK(diagnostics.hasErrors());
    MPL_CHECK_EQ(std::string(diagnostics.diagnostics().front().code),
                 std::string("lex-unterminated-string"));
}

MPL_TEST(ParserBuildsDeclarationsAndSpans) {
    DiagnosticSink diagnostics;
    const std::string source = baseProgram(
        "asset PianoAsset = instrument(\"GrandPiano\");\n"
        "let A = melody { C4: 1/4 };\n"
        "track Piano { asset = PianoAsset; role = melodic; }\n"
        "section S(1 bars) { clip Piano \"C\" { play A; } }\n"
        "arrangement { play S; }\n"
        "export midi(\"x.mid\");\n");
    Lexer lexer(source, "parse.mpl", diagnostics);
    Parser parser(lexer.lex(), diagnostics);
    const Module module = parser.parse();
    MPL_CHECK(!diagnostics.hasErrors());
    MPL_CHECK(module.project.has_value());
    MPL_CHECK_EQ(module.assets.size(), std::size_t{1});
    MPL_CHECK_EQ(module.lets.size(), std::size_t{1});
    MPL_CHECK_EQ(module.tracks.size(), std::size_t{1});
    MPL_CHECK_EQ(module.sections.size(), std::size_t{1});
    MPL_CHECK_EQ(module.sections.front().clips.size(), std::size_t{1});
    MPL_CHECK(module.arrangement.has_value());
    MPL_CHECK(!module.exports.empty());
    MPL_CHECK(module.sections.front().span.line > 0);
    MPL_CHECK(module.sections.front().span.endColumn > 0);
}

MPL_TEST(ParserRecoversFromMultipleErrors) {
    DiagnosticSink diagnostics;
    const std::string source =
        "project P { tempo = ; key = F minor; }\n"
        "let A = ;\n"
        "let B = melody { C4: 1/4 };\n";
    Lexer lexer(source, "recover.mpl", diagnostics);
    Parser parser(lexer.lex(), diagnostics);
    const Module module = parser.parse();
    MPL_CHECK(diagnostics.errorCount() >= 2);
    MPL_CHECK_EQ(module.lets.size(), std::size_t{2});
}

MPL_TEST(CoreExampleMatchesCoreApiMidi) {
    FrontendResult result = compileExample("core.mpl");
    MPL_CHECK_EQ(diagnosticText(result), std::string());
    MPL_CHECK(result.ok());
    const std::vector<std::uint8_t> source = midiOf(result.program->project);
    const std::vector<std::uint8_t> reference = midiOf(mpl::examples::buildCoreDemoProject());
    MPL_CHECK(source == reference);
}

MPL_TEST(SuiteExampleMatchesCoreApiMidi) {
    FrontendResult result = compileExample("suite.mpl");
    MPL_CHECK(result.ok());
    MPL_CHECK(hasWarning(result, "sema-section-overflow"));
    const std::vector<std::uint8_t> source = midiOf(result.program->project);
    const std::vector<std::uint8_t> reference = midiOf(mpl::examples::buildSuiteDemoProject());
    MPL_CHECK(source == reference);
}

MPL_TEST(TemporalExampleMatchesCoreApiMidi) {
    FrontendResult result = compileExample("temporal.mpl");
    MPL_CHECK_EQ(diagnosticText(result), std::string());
    MPL_CHECK(result.ok());
    const std::vector<std::uint8_t> source = midiOf(result.program->project);
    const std::vector<std::uint8_t> reference =
        midiOf(mpl::examples::buildTemporalDemoProject());
    MPL_CHECK(source == reference);
}

MPL_TEST(ReferenceExampleGoldenArtifacts) {
    const Compilation compilation = compileReference();
    MPL_CHECK(!compilation.hasErrors());
    checkGolden(compilation.music.toJson(), "reference.music.ir");
    checkGolden(compilation.temporal.toJson(), "reference.temporal.ir");
    checkGolden(compilation.production.toJson(), "reference.production.ir");
    checkGolden(compilation.toTraceJson(false), "reference.trace.json");
    const MidiBackend backend(MidiOptions{480, "MPL Conductor"});
    checkGolden(hexDump(backend.serialize(compilation.production, compilation.temporal)),
                "reference.mid.hex");
}

MPL_TEST(CoreAstGolden) {
    const std::filesystem::path path = examplePath("core.mpl");
    DiagnosticSink diagnostics;
    const std::string source = readText(path);
    Lexer lexer(source, path.string(), diagnostics);
    Parser parser(lexer.lex(), diagnostics);
    const Module module = parser.parse();
    MPL_CHECK_EQ(diagnostics.toHuman(), std::string());
    checkGolden(astToJson(module), "core.ast.json");
}

MPL_TEST(NameResolutionDiagnostics) {
    FrontendResult duplicate = compileText(baseProgram(
        "let A = melody { C4: 1/4 };\n"
        "let A = melody { D4: 1/4 };\n"));
    MPL_CHECK(hasCode(duplicate, "sema-duplicate-symbol"));

    FrontendResult unknown = compileText(baseProgram(
        "let A = transpose(Missing, 12 st);\n"));
    MPL_CHECK(hasCode(unknown, "sema-unknown-name"));
}

MPL_TEST(UnitAndTypeDiagnostics) {
    FrontendResult tempo = compileText(
        "project T { tempo = 120; meter = 4/4; key = C major; }\n");
    MPL_CHECK(hasCode(tempo, "type-unit"));

    FrontendResult interval = compileText(baseProgram(
        "let A = melody { C4: 1/4 };\n"
        "let B = transpose(A, 200 Hz);\n"));
    MPL_CHECK(hasCode(interval, "type-mismatch"));

    FrontendResult gain = compileText(baseProgram(
        "let A = melody { C4: 1/4 };\n"
        "let B = stretch(A, -6 dB);\n"));
    MPL_CHECK(hasCode(gain, "type-mismatch"));

    FrontendResult physical = compileText(baseProgram(
        "let A = melody { C4: 1/4 };\n"
        "let B = syncopate(A, 100 ms);\n"));
    MPL_CHECK(hasCode(physical, "type-mismatch"));

    FrontendResult unknownOperator = compileText(baseProgram(
        "let A = melody { C4: 1/4 };\n"
        "let B = compress(A, 2);\n"));
    MPL_CHECK(hasCode(unknownOperator, "type-unknown-operator"));
}

MPL_TEST(TimeDomainDiagnostics) {
    FrontendResult physicalCurve = compileText(baseProgram(
        "curve C on physical = constant(1.0);\n"));
    MPL_CHECK(hasCode(physicalCurve, "type-domain-unsupported"));

    FrontendResult automation = compileText(baseProgram(
        "curve C on relative = constant(0.5);\n"
        "asset PianoAsset = instrument(\"GrandPiano\");\n"
        "let A = melody { C4: 1/4 };\n"
        "track Piano { asset = PianoAsset; role = melodic; }\n"
        "section S(1 bars) { clip Piano { play A; } }\n"
        "arrangement { play S; }\n"
        "export midi(\"x.mid\");\n"
        "automation Piano.volume = C;\n"));
    MPL_CHECK(hasCode(automation, "type-domain"));

    FrontendResult expression = compileText(baseProgram(
        "curve C on musical for 1 bars = constant(0.5);\n"
        "asset PianoAsset = instrument(\"GrandPiano\");\n"
        "let A = melody { C4: 1/4 };\n"
        "track Piano { asset = PianoAsset; role = melodic; }\n"
        "section S(1 bars) { clip Piano { play A; velocity = C; } }\n"
        "arrangement { play S; }\n"
        "export midi(\"x.mid\");\n"));
    MPL_CHECK(hasCode(expression, "type-domain"));
}

MPL_TEST(AutomationRangeConflictAndExtentDiagnostics) {
    const std::string common =
        "asset PianoAsset = instrument(\"GrandPiano\");\n"
        "let A = melody { C4: 1/4 };\n"
        "track Piano { asset = PianoAsset; role = melodic; }\n"
        "section S(1 bars) { clip Piano { play A; } }\n"
        "arrangement { play S; }\n"
        "export midi(\"x.mid\");\n";

    FrontendResult conflict = compileText(baseProgram(
        "curve V on musical for 1 bars = constant(0.5);\n" + common +
        "automation Piano.volume = V;\n"
        "automation Piano.volume = V;\n"));
    MPL_CHECK(hasCode(conflict, "sema-automation-conflict"));

    FrontendResult range = compileText(baseProgram(
        "curve V on musical for 1 bars = constant(2.0);\n" + common +
        "automation Piano.volume = V;\n"));
    MPL_CHECK(hasCode(range, "sema-parameter-range"));

    FrontendResult extent = compileText(baseProgram(
        "curve V on relative = linear(0.5, 1.0);\n"
        "let V2 = stretch(V, 2);\n" + common +
        "section S2(1 bars) { clip Piano \"X\" { play A; velocity = V2; } }\n"));
    MPL_CHECK(hasCode(extent, "type-expression-extent"));
}

MPL_TEST(StableInstanceIdsAcrossCompiles) {
    FrontendResult first = compileExample("temporal.mpl");
    FrontendResult second = compileExample("temporal.mpl");
    MPL_CHECK(first.ok());
    MPL_CHECK(second.ok());
    std::vector<InstanceId> firstIds;
    std::vector<InstanceId> secondIds;
    for (const TrackSpec& track : first.program->project.tracks()) {
        for (const ClipSpec& clip : track.clips) {
            firstIds.push_back(clip.instanceId);
        }
    }
    for (const TrackSpec& track : second.program->project.tracks()) {
        for (const ClipSpec& clip : track.clips) {
            secondIds.push_back(clip.instanceId);
        }
    }
    MPL_CHECK(firstIds == secondIds);
    std::sort(firstIds.begin(), firstIds.end());
    MPL_CHECK(std::adjacent_find(firstIds.begin(), firstIds.end()) == firstIds.end());
    MPL_CHECK(std::all_of(firstIds.begin(), firstIds.end(),
                          [](InstanceId id) { return id != kInvalidInstanceId; }));
}

MPL_TEST(DiagnosticsHaveMachineReadableForm) {
    DiagnosticSink sink;
    sink.error("type-unit", SourceSpan{"x.mpl", 0, 3, 1, 1, 1, 4}, "expected bpm",
               "tempo", "number", "tempo");
    const std::string json = sink.toJson();
    MPL_CHECK(json.find("\"code\": \"type-unit\"") != std::string::npos);
    MPL_CHECK(json.find("\"severity\": \"error\"") != std::string::npos);
    MPL_CHECK(json.find("\"line\": 1") != std::string::npos);
    MPL_CHECK(json.find("\"expected\": \"tempo\"") != std::string::npos);
    MPL_CHECK(json.find("\"actual\": \"number\"") != std::string::npos);
    const std::string human = sink.toHuman();
    MPL_CHECK(human.find("x.mpl:1:1") != std::string::npos);
    MPL_CHECK(human.find("error[type-unit]") != std::string::npos);
}
