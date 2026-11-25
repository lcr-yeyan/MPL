#include "TestSupport.hpp"

#include "mpl/observe/Observations.hpp"

#include <string>

using namespace mpl;

namespace {

std::string tieProgram(const std::string& expression, const std::string& bars = "2") {
    return mpltest::header() +
           "let Tied = " + expression + ";\n"
           "track Piano { asset = PianoAsset; role = melodic; }\n"
           "section S(" + bars + " bars) { clip Piano \"C\" { play Tied; } }\n"
           "arrangement { play S; }\n"
           "export midi(\"tie.mid\");\n";
}

ObservationSet observationsOf(const std::string& source) {
    return observeCompilation(mpltest::compileText(source));
}

std::string observation(const ObservationSet& set, const std::string& name) {
    const ObservationValue* value = set.find(name);
    return value == nullptr ? "" : value->jsonValue;
}

std::size_t noteOns(const std::string& source) {
    const std::vector<std::uint8_t> midi = mpltest::midiOf(mpltest::compileText(source));
    std::size_t count = 0;
    for (std::size_t index = 0; index + 1 < midi.size(); ++index) {
        if ((midi[index] & 0xF0) == 0x90 && midi[index + 1] != 0) {
            ++count;
        }
    }
    return count;
}

}  // namespace

MPL_TEST(TieSustainsAcrossBoundaryAsOneLogicalNote) {
    const std::string source = tieProgram(
        "tie(note(C4, 1 whole, 96), note(C4, 1 whole, 96))");
    const ObservationSet set = observationsOf(source);
    MPL_CHECK_EQ(observation(set, "event_count"), std::string("1"));
    MPL_CHECK_EQ(observation(set, "logical_extent"), std::string("\"2\""));
    MPL_CHECK_EQ(noteOns(source), std::size_t{1});
}

MPL_TEST(AdjacentSamePitchWithoutTieRemainsTwoNotes) {
    const std::string source = tieProgram(
        "sequence(note(C4, 1 whole, 96), note(C4, 1 whole, 96))");
    const ObservationSet set = observationsOf(source);
    MPL_CHECK_EQ(observation(set, "event_count"), std::string("2"));
    MPL_CHECK_EQ(noteOns(source), std::size_t{2});
}

MPL_TEST(TieRejectsPitchMismatchAndAmbiguousBoundary) {
    MPL_CHECK(mpltest::hasCode(
        mpltest::frontend(tieProgram("tie(note(C4, 1 whole, 96), note(D4, 1 whole, 96))")),
        "tie-pitch"));
    MPL_CHECK(mpltest::hasCode(
        mpltest::frontend(tieProgram(
            "tie(parallel(note(C4, 1 whole, 96), note(E4, 1 whole, 96)), "
            "note(C4, 1 whole, 96))")),
        "tie-ambiguous"));
    MPL_CHECK(mpltest::hasCode(
        mpltest::frontend(tieProgram("tie(rest(1 whole), note(C4, 1 whole, 96))")),
        "tie-empty"));
}

MPL_TEST(TiePreservesSpanAcrossMeterBoundary) {
    const std::string source =
        "project T { tempo = 120 bpm; meter = 4/4; key = C major; }\n"
        "asset PianoAsset = instrument(\"GrandPiano\");\n"
        "let Tied = tie(note(C4, 1 whole, 96), note(C4, 1 whole, 96));\n"
        "track Piano { asset = PianoAsset; role = melodic; }\n"
        "section A(1 bars) { clip Piano \"A\" { play note(C4, 1 whole, 96); } }\n"
        "section B(1 bars) meter 3/4 { clip Piano \"B\" { play Tied; } }\n"
        "arrangement { play A; play B; }\n"
        "export midi(\"tie-meter.mid\");\n";
    const ObservationSet set = observationsOf(source);
    MPL_CHECK_EQ(observation(set, "event_count"), std::string("2"));
    MPL_CHECK_EQ(observation(set, "logical_extent"), std::string("\"7/4\""));
}
