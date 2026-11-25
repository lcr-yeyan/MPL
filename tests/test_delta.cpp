#include "TestSupport.hpp"

#include "mpl/analysis/SemanticDelta.hpp"

#include <filesystem>
#include <fstream>
#include <string>

using namespace mpl;

namespace {

std::string basicProgram(const std::string& pitch) {
    return "project T { tempo = 120 bpm; meter = 4/4; key = C major; }\n"
           "asset Piano = instrument(\"GrandPiano\");\n"
           "let M = notes span 1 whole {\n"
           "  C4 @ 0 for 1/4 vel 96,\n"
           "  D4 @ 1/4 for 1/4 vel 96,\n"
           "  E4 @ 1/2 for 1/4 vel 96,\n" +
           pitch +
           " @ 3/4 for 1/4 vel 96\n"
           "};\n"
           "track Keys { asset = Piano; role = melodic; }\n"
           "section S(1 bars) { clip Keys { play M; } }\n"
           "arrangement { play S; }\n"
           "export midi(\"x.mid\");\n";
}

}  // namespace

MPL_TEST(SemanticDeltaDetectsSymbolicAndObservationChanges) {
    const Compilation before = mpltest::compileText(basicProgram("F4"));
    const Compilation after = mpltest::compileText(basicProgram("G4"));
    const SemanticDelta delta = computeSemanticDelta(before, after);
    MPL_CHECK(!delta.changes.empty());
    bool sawNotes = false;
    bool sawPitchObservation = false;
    for (const SemanticChange& change : delta.changes) {
        sawNotes = sawNotes || (change.objectType == "clip" &&
                                change.property == "symbolicEvents");
    }
    for (const ChangedObservation& observation : delta.changedObservations) {
        sawPitchObservation =
            sawPitchObservation || observation.name == "pitch_sequence";
    }
    MPL_CHECK(sawNotes);
    MPL_CHECK(sawPitchObservation);
    MPL_CHECK(!delta.unobserved.empty());
    MPL_CHECK(delta.toJson().find("\"unobserved\"") != std::string::npos);
}

MPL_TEST(SemanticDeltaIgnoresIdenticalCompilations) {
    const Compilation first = mpltest::compileText(basicProgram("F4"));
    const Compilation second = mpltest::compileText(basicProgram("F4"));
    const SemanticDelta delta = computeSemanticDelta(first, second);
    MPL_CHECK(delta.changes.empty());
    MPL_CHECK(delta.changedObservations.empty());
}

MPL_TEST(SemanticDeltaClassifiesTransitiveDependencyChange) {
    const std::filesystem::path directory = MPL_TEST_OUTPUT_DIR;
    std::filesystem::create_directories(directory);
    const std::filesystem::path bPath = directory / "delta_b.mpl";
    const std::filesystem::path aPath = directory / "delta_a.mpl";
    const std::filesystem::path rootPath = directory / "delta_root.mpl";
    std::ofstream(bPath, std::ios::binary)
        << "module DeltaB;\nexport fn b() -> Fragment { return note(C4, 1/4, 90); }\n";
    std::ofstream(aPath, std::ios::binary)
        << "module DeltaA;\nimport \"delta_b.mpl\";\n"
           "export fn a() -> Fragment { return b(); }\n";
    const std::string root =
        "import \"delta_a.mpl\";\n"
        "project T { tempo = 120 bpm; meter = 4/4; key = C major; }\n"
        "asset Piano = instrument(\"GrandPiano\");\n"
        "let M = a();\n"
        "track Keys { asset = Piano; role = melodic; }\n"
        "section S(1 bars) { clip Keys { play M; } }\n"
        "arrangement { play S; }\n"
        "export midi(\"x.mid\");\n";
    std::ofstream(rootPath, std::ios::binary) << root;
    const Compilation before =
        mpltest::compileText(mpltest::readText(rootPath), rootPath.generic_string());
    std::ofstream(bPath, std::ios::binary | std::ios::trunc)
        << "module DeltaB;\nexport fn b() -> Fragment { return note(D4, 1/4, 90); }\n";
    const Compilation after =
        mpltest::compileText(mpltest::readText(rootPath), rootPath.generic_string());
    const SemanticDelta delta = computeSemanticDelta(before, after);
    MPL_CHECK_EQ(delta.causeSummary, std::string("dependency"));
    MPL_CHECK(!delta.changes.empty());
    for (const SemanticChange& change : delta.changes) {
        MPL_CHECK(change.cause == DeltaCause::Dependency);
    }
}

MPL_TEST(SemanticDeltaReportsSectionsAndProductionExtent) {
    const Compilation before = mpltest::compileText(basicProgram("F4"));
    std::string text = basicProgram("F4");
    const std::string token = "project T { tempo = 120 bpm; meter = 4/4; key = C major; }";
    text.replace(text.find(token), token.size(),
                 "project T { tempo = 100 bpm; meter = 3/4; key = C major; }");
    // Keep the note positions; this deliberately changes meter/tempo only.
    const Compilation after = mpltest::compileText(text);
    const SemanticDelta delta = computeSemanticDelta(before, after);
    bool sawTempo = false;
    bool sawMeter = false;
    for (const SemanticChange& change : delta.changes) {
        sawTempo = sawTempo || (change.objectType == "production" &&
                                change.property == "tempo");
        sawMeter = sawMeter || (change.objectType == "production" &&
                                change.property == "meter");
    }
    MPL_CHECK(sawTempo);
    MPL_CHECK(sawMeter);
    MPL_CHECK_EQ(before.sections.size(), std::size_t{1});
    MPL_CHECK_EQ(before.sections.front().name, std::string("S"));
}
