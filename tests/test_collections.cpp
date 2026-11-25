#include "TestSupport.hpp"

#include "mpl/observe/Observations.hpp"

#include <string>

using namespace mpl;

namespace {

std::string programWith(const std::string& play, const std::string& lets = "") {
    return mpltest::header() +
           "let A = note(C4, 1/4, 96);\n"
           "let B = note(E4, 1/4, 96);\n"
           "let L = [A, B];\n" +
           lets +
           "track Piano { asset = PianoAsset; role = melodic; }\n"
           "section S(1 bars) { clip Piano \"C\" { play " + play + "; } }\n"
           "arrangement { play S; }\n"
           "export midi(\"collections.mid\");\n";
}

std::string eventCount(const std::string& play, const std::string& lets = "") {
    const Compilation compilation = mpltest::compileText(programWith(play, lets));
    const ObservationSet observations = observeCompilation(compilation);
    const ObservationValue* value = observations.find("event_count");
    return value == nullptr ? "" : value->jsonValue;
}

}  // namespace

MPL_TEST(CollectionIndexSelectsElement) {
    MPL_CHECK_EQ(eventCount("L[0]"), std::string("1"));
    MPL_CHECK_EQ(eventCount("L[1]"), std::string("1"));
}

MPL_TEST(CollectionSliceIsHalfOpenAndClamped) {
    MPL_CHECK_EQ(eventCount("sequenceAll(L[0:2])"), std::string("2"));
    MPL_CHECK_EQ(eventCount("sequenceAll(L[0:1])"), std::string("1"));
    MPL_CHECK_EQ(eventCount("sequenceAll(L[1:9])"), std::string("1"));
    MPL_CHECK(mpltest::frontend(programWith("note(C4, 1/4, 96)",
                                             "let Empty = L[1:1];\n"))
                  .ok());
}

MPL_TEST(CollectionSliceOpenBounds) {
    MPL_CHECK_EQ(eventCount("sequenceAll(L[:1])"), std::string("1"));
    MPL_CHECK_EQ(eventCount("sequenceAll(L[1:])"), std::string("1"));
    MPL_CHECK_EQ(eventCount("sequenceAll(L[:])"), std::string("2"));
}

MPL_TEST(CollectionIndexDiagnostics) {
    MPL_CHECK(mpltest::hasCode(mpltest::frontend(programWith("L[-1]")), "index-negative"));
    MPL_CHECK(mpltest::hasCode(mpltest::frontend(programWith("L[9]")), "index-out-of-range"));
    MPL_CHECK(mpltest::hasCode(mpltest::frontend(programWith("sequenceAll(L[2:1])")),
                               "index-slice-bounds"));
    MPL_CHECK(mpltest::hasCode(mpltest::frontend(programWith("L[\"x\"]")), "type-index"));
    MPL_CHECK(mpltest::hasCode(mpltest::frontend(programWith("A[0]")), "type-index-not-list"));
}

MPL_TEST(CollectionEmptyListIndexFails) {
    const std::string source = programWith("note(C4, 1/4, 96)",
                                           "let E = [];\nlet X = E[0];\n");
    MPL_CHECK(mpltest::hasCode(mpltest::frontend(source), "index-out-of-range"));
}

MPL_TEST(CollectionSliceValueSemantics) {
    // Indexing returns an independent value; mutating operations do not exist,
    // so repeated reads must remain stable and compile independently.
    const std::string source = programWith(
        "sequenceAll([L[0], L[1]][1:])");
    const Compilation compilation = mpltest::compileText(source);
    const ObservationSet observations = observeCompilation(compilation);
    const ObservationValue* value = observations.find("event_count");
    MPL_CHECK(value != nullptr);
    MPL_CHECK_EQ(value == nullptr ? "" : value->jsonValue, std::string("1"));
}
