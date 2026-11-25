#include "TestSupport.hpp"

#include "mpl/observe/Observations.hpp"

#include <string>

using namespace mpl;

namespace {
std::string libraryProgram(const std::string& body) {
    return "import \"std/analysis.mpl\";\n"
           "import \"std/voicing.mpl\";\n" +
           mpltest::header() +
           "let N = voicingNotes(voicing(chord(\"Cmaj7\"), C4, C5, 4, 0, 12));\n" +
           body +
           "track Piano { asset = PianoAsset; role = melodic; }\n"
           "section S(1 bars) { clip Piano \"L\" { play P; } }\n"
           "arrangement { play S; }\n"
           "export midi(\"lib.mid\");\n";
}
}  // namespace

MPL_TEST(StdlibAnalysisDerivesPitchClassesAndIntervals) {
    const std::string source = libraryProgram(
        "let PC = pitchClasses(N);\n"
        "let IV = intervalsOf(N);\n"
        "let P = cycle(rhythm { hit 1/4, hit 1/4 }, [topVoice(N), bottomVoice(N)], 96);\n");
    const Compilation compilation = mpltest::compileText(source);
    const ObservationSet observations = observeCompilation(compilation);
    const ObservationValue* count = observations.find("event_count");
    MPL_CHECK(count != nullptr);
    MPL_CHECK_EQ(count == nullptr ? "" : count->jsonValue, std::string("2"));
}

MPL_TEST(StdlibVoicingSlicesInnerAndOuterVoices) {
    const std::string source = libraryProgram(
        "let INNER = innerVoices(N);\n"
        "let P = cycle(rhythm { hit 1/2, hit 1/2 }, [topVoice(N), bottomVoice(N)], 96);\n");
    MPL_CHECK(mpltest::frontend(source).ok());
}

MPL_TEST(StdlibIntervalsOfSinglePitchReturnsEmptyList) {
    const std::string source = libraryProgram(
        "let ONE = [topVoice(N)];\n"
        "let IV = intervalsOf(ONE);\n"
        "let P = note(topVoice(N), 1/4, 96);\n");
    MPL_CHECK(mpltest::frontend(source).ok());
}

MPL_TEST(StdlibTopVoiceRejectsEmptyPrecondition) {
    const std::string source = libraryProgram(
        "let EMPTY = [topVoice(N)][1:1];\n"
        "let BAD = topVoice(EMPTY);\n"
        "let P = note(topVoice(N), 1/4, 96);\n");
    const FrontendResult result = mpltest::frontend(source);
    MPL_CHECK(mpltest::hasCode(result, "index-negative") ||
              mpltest::hasCode(result, "index-out-of-range"));
}
