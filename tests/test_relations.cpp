#include "TestSupport.hpp"

#include <string>
#include <vector>

using namespace mpl;

namespace {

std::string relationsProgram(const std::string& body) {
    return mpltest::header("4/4") + body +
           "track Piano { asset = PianoAsset; role = melodic; }\n"
           "section S(1 bars) { clip Piano \"S\" { play M; } }\n"
           "arrangement { play S; }\n"
           "export midi(\"x.mid\");\n";
}

Compilation compileRelations(const std::string& body) {
    return mpltest::compileText(relationsProgram(body));
}

Rhythm threeCycle() {
    return Rhythm::fromEvents(
        {{Rational(0, 1), Rational(1, 8), 2, false, kInvalidObjectId},
         {Rational(1, 8), Rational(1, 8), 0, false, kInvalidObjectId},
         {Rational(2, 8), Rational(1, 8), 1, false, kInvalidObjectId}},
        Rational(1, 2));
}

Rhythm twoCycle() {
    return Rhythm::fromEvents(
        {{Rational(1, 8), Rational(1, 8), 1, false, kInvalidObjectId},
         {Rational(3, 8), Rational(1, 8), 0, false, kInvalidObjectId}},
        Rational(1, 4));
}

std::vector<Rational> onsets(const Rhythm& rhythm) {
    std::vector<Rational> values;
    for (const RhythmEvent& event : rhythm.events()) {
        if (!event.rest) {
            values.push_back(event.onset);
        }
    }
    return values;
}

}  // namespace

MPL_TEST(IntersectUsesExactOnsets) {
    const Rhythm result = rhythmIntersect(threeCycle(), twoCycle());
    MPL_CHECK(result.logicalLength() == Rational(1, 2));
    const std::vector<Rational> values = onsets(result);
    MPL_CHECK_EQ(values.size(), std::size_t{1});
    MPL_CHECK(values[0] == Rational(1, 8));
    MPL_CHECK_EQ(result.events().front().accent, 0);
}

MPL_TEST(UnionUsesExactOnsets) {
    const Rhythm result = rhythmUnion(threeCycle(), twoCycle());
    MPL_CHECK(result.logicalLength() == Rational(1, 2));
    const std::vector<Rational> values = onsets(result);
    MPL_CHECK_EQ(values.size(), std::size_t{4});
    MPL_CHECK(values[3] == Rational(3, 8));
}

MPL_TEST(DifferenceUsesExactOnsets) {
    const Rhythm result = rhythmDifference(threeCycle(), twoCycle());
    const std::vector<Rational> values = onsets(result);
    MPL_CHECK_EQ(values.size(), std::size_t{2});
    MPL_CHECK(values[0] == Rational(0, 1));
    MPL_CHECK(values[1] == Rational(2, 8));
}

MPL_TEST(GridModeBucketsOnsets) {
    const Rhythm left = Rhythm::fromEvents(
        {{Rational(1, 16), Rational(1, 16), 0, false, kInvalidObjectId}}, Rational(1, 4));
    const Rhythm right = Rhythm::fromEvents(
        {{Rational(1, 8), Rational(1, 8), 2, false, kInvalidObjectId}}, Rational(1, 4));
    const Rhythm result = rhythmIntersect(left, right, Rational(1, 8));
    const std::vector<Rational> values = onsets(result);
    MPL_CHECK_EQ(values.size(), std::size_t{1});
    MPL_CHECK(values[0] == Rational(1, 8));
    MPL_CHECK(result.events().front().duration == Rational(1, 8));
    MPL_CHECK_EQ(result.events().front().accent, 0);
}

MPL_TEST(ComplementProducesGridPoints) {
    const Rhythm source = Rhythm::fromEvents(
        {{Rational(0, 1), Rational(1, 8), 0, false, kInvalidObjectId},
         {Rational(1, 4), Rational(1, 8), 0, false, kInvalidObjectId}},
        Rational(1, 2));
    const Rhythm result = rhythmComplement(source, Rational(1, 2), Rational(1, 8));
    const std::vector<Rational> values = onsets(result);
    MPL_CHECK_EQ(values.size(), std::size_t{2});
    MPL_CHECK(values[0] == Rational(1, 8));
    MPL_CHECK(values[1] == Rational(3, 8));
    MPL_CHECK(result.logicalLength() == Rational(1, 2));
}

MPL_TEST(ComplementRejectsNonMultipleSpan) {
    const Rhythm source = Rhythm::fromEvents(
        {{Rational(0, 1), Rational(1, 8), 0, false, kInvalidObjectId}}, Rational(1, 4));
    MPL_CHECK_THROWS(rhythmComplement(source, Rational(3, 8), Rational(1, 4)));
    MPL_CHECK_THROWS(rhythmComplement(source, Rational(1, 2), Rational(0, 1)));
}

MPL_TEST(AccentFollowCopiesMatchingAccents) {
    const Rhythm source = Rhythm::fromEvents(
        {{Rational(0, 1), Rational(1, 8), 3, false, kInvalidObjectId},
         {Rational(2, 8), Rational(1, 8), 1, false, kInvalidObjectId}},
        Rational(1, 2));
    const Rhythm target = Rhythm::fromEvents(
        {{Rational(0, 1), Rational(1, 8), 0, false, kInvalidObjectId},
         {Rational(1, 8), Rational(1, 8), 0, false, kInvalidObjectId},
         {Rational(2, 8), Rational(1, 8), 0, false, kInvalidObjectId}},
        Rational(1, 2));
    const Rhythm result = rhythmAccentFollow(source, target);
    MPL_CHECK(result.logicalLength() == Rational(1, 2));
    MPL_CHECK_EQ(result.events()[0].accent, 3);
    MPL_CHECK_EQ(result.events()[1].accent, 0);
    MPL_CHECK_EQ(result.events()[2].accent, 1);
}

MPL_TEST(AccentFollowGridBuckets) {
    const Rhythm source = Rhythm::fromEvents(
        {{Rational(1, 16), Rational(1, 8), 2, false, kInvalidObjectId}}, Rational(1, 4));
    const Rhythm target = Rhythm::fromEvents(
        {{Rational(1, 8), Rational(1, 8), 0, false, kInvalidObjectId}}, Rational(1, 4));
    const Rhythm result = rhythmAccentFollow(source, target, Rational(1, 8));
    MPL_CHECK_EQ(result.events()[0].accent, 2);
}

MPL_TEST(OnsetRelationsIgnoreDurations) {
    const Rhythm longNote = Rhythm::fromEvents(
        {{Rational(0, 1), Rational(1, 2), 0, false, kInvalidObjectId}}, Rational(1, 2));
    const Rhythm inner = Rhythm::fromEvents(
        {{Rational(1, 4), Rational(1, 8), 0, false, kInvalidObjectId}}, Rational(1, 2));
    const Rhythm result = rhythmDifference(longNote, inner);
    MPL_CHECK_EQ(onsets(result).size(), std::size_t{1});
    MPL_CHECK(onsets(result)[0] == Rational(0, 1));
}

MPL_TEST(RelationUsesCommonPeriod) {
    const Rhythm three = Rhythm::fromEvents(
        {{Rational(0, 1), Rational(1, 8), 0, false, kInvalidObjectId}}, Rational(3, 8));
    const Rhythm half = Rhythm::fromEvents(
        {{Rational(0, 1), Rational(1, 8), 0, false, kInvalidObjectId}}, Rational(1, 2));
    const Rhythm result = rhythmUnion(three, half);
    MPL_CHECK(result.logicalLength() == Rational(3, 2));
}

MPL_TEST(RelationPeriodOverflowIsRejected) {
    const Rhythm huge = Rhythm::fromEvents(
        {{Rational(0, 1), Rational(1, 8), 0, false, kInvalidObjectId}},
        Rational(kMusicalRationalLimit, 1));
    const Rhythm prime = Rhythm::fromEvents(
        {{Rational(0, 1), Rational(1, 8), 0, false, kInvalidObjectId}},
        Rational(999999937, 1));
    MPL_CHECK(!rhythmCommonPeriod(huge, prime).has_value());
    MPL_CHECK_THROWS(rhythmUnion(huge, prime));
}

MPL_TEST(RelationsAreAvailableInTheLanguage) {
    const Compilation compilation = compileRelations(
        "let A = rhythm { hit 1/8, hit 1/8, hit 1/8 };\n"
        "let B = rhythm { hit 1/8, rest 1/8, hit 1/8 };\n"
        "let I = intersect(A, B, 1/8);\n"
        "let U = union(A, B, 1/8);\n"
        "let D = difference(A, B, 1/8);\n"
        "let C = complement(I, 1/4, 1/8);\n"
        "let F = accentFollow(accent(A, [3, 1, 2]), B, 1/8);\n"
        "let M = cycle(U, [C4], 90);\n");
    MPL_CHECK(mpltest::rhythmByLabel(compilation, "I") != nullptr);
    MPL_CHECK(mpltest::rhythmByLabel(compilation, "U") != nullptr);
    MPL_CHECK(mpltest::rhythmByLabel(compilation, "D") != nullptr);
    MPL_CHECK(mpltest::rhythmByLabel(compilation, "C") != nullptr);
    MPL_CHECK(mpltest::rhythmByLabel(compilation, "F") != nullptr);
}

MPL_TEST(CrossVoiceRhythmRelationInProject) {
    const std::string source =
        mpltest::header("4/4") +
        "let GuitarRhythm = rhythm { hit 1/8, hit 1/8, rest 1/8, hit 1/8 };\n"
        "let BassRhythm = rhythm { hit 1/4, rest 1/4 };\n"
        "let Shared = intersect(GuitarRhythm, BassRhythm, 1/8);\n"
        "let GuitarPart = cycle(GuitarRhythm, [D4, F4, A4], 96);\n"
        "let BassPart = cycle(BassRhythm, [D2], 100);\n"
        "let SharedPart = cycle(Shared, [A4], 110);\n"
        "track Guitar { asset = PianoAsset; role = melodic; }\n"
        "track Bass { asset = PianoAsset; role = bass; }\n"
        "track Lead { asset = PianoAsset; role = melodic; }\n"
        "section S(1 bars) {\n"
        "  clip Guitar \"G\" { play GuitarPart; }\n"
        "  clip Bass \"B\" { play BassPart; }\n"
        "  clip Lead \"L\" { play SharedPart; }\n"
        "}\n"
        "arrangement { play S; }\n"
        "export midi(\"x.mid\");\n";
    const Compilation compilation = mpltest::compileText(source);
    const Rhythm* shared = mpltest::rhythmByLabel(compilation, "Shared");
    MPL_CHECK(shared != nullptr);
    if (shared != nullptr) {
        MPL_CHECK_EQ(shared->attackCount(), std::size_t{1});
        MPL_CHECK(shared->events().front().onset == Rational(0, 1));
    }
    const auto& tracks = compilation.production.tracks;
    MPL_CHECK_EQ(tracks.size(), std::size_t{3});
    MPL_CHECK_EQ(tracks[2].clips.front().content.noteCount(), std::size_t{1});
}
