#include "TestHarness.hpp"

#include "mpl/core/Rational.hpp"
#include "mpl/core/Time.hpp"
#include "mpl/core/TimeTypes.hpp"
#include "mpl/music/Fragment.hpp"
#include "mpl/music/Pitch.hpp"
#include "mpl/music/Transforms.hpp"

#include <algorithm>
#include <limits>
#include <vector>

using namespace mpl;
using namespace mpl::literals;

namespace {

Fragment motif() {
    return Fragment::fromMelody({
        {"F4"_p, Rational(1, 2)},
        {"Ab4"_p, Rational(1, 2)},
        {"C5"_p, Rational(1, 1)},
    });
}

}  // namespace

MPL_TEST(RationalNormalizesAndFormats) {
    const Rational value(4, 8);
    MPL_CHECK_EQ(value.numerator(), std::int64_t{1});
    MPL_CHECK_EQ(value.denominator(), std::int64_t{2});
    MPL_CHECK_EQ(value.toString(), std::string("1/2"));
    MPL_CHECK(Rational(6, -3) == Rational(-2, 1));
    MPL_CHECK(Rational(0, 5).isZero());
}

MPL_TEST(RationalArithmeticIsExact) {
    MPL_CHECK(Rational(1, 2) + Rational(1, 3) == Rational(5, 6));
    MPL_CHECK(Rational(2, 3) * Rational(3, 4) == Rational(1, 2));
    MPL_CHECK(Rational(1, 2) / Rational(1, 4) == Rational(2, 1));
    MPL_CHECK(Rational(1, 3) < Rational(1, 2));
    MPL_CHECK(Rational(3, 4) - Rational(1, 4) == Rational(1, 2));
}

MPL_TEST(RationalRejectsZeroDenominator) {
    MPL_CHECK_THROWS(Rational(1, 0));
    MPL_CHECK_THROWS(Rational(1, 2) / Rational(0, 1));
}

MPL_TEST(RationalDetectsOverflow) {
    const std::int64_t maximum = std::numeric_limits<std::int64_t>::max();
    const std::int64_t minimum = std::numeric_limits<std::int64_t>::min();
    MPL_CHECK_THROWS(Rational(maximum, 1) + Rational(1, 1));
    MPL_CHECK_THROWS(Rational(minimum + 1, 1) - Rational(2, 1));
    MPL_CHECK_THROWS(Rational(maximum, 1) * Rational(2, 1));
    MPL_CHECK_THROWS(Rational(minimum, 1));
    try {
        (void)(Rational(maximum, 1) + Rational(1, 1));
    } catch (const Error& error) {
        MPL_CHECK_EQ(std::string(error.code()), std::string("rational"));
    }
}

MPL_TEST(RationalComparisonIsExact) {
    MPL_CHECK(Rational(1, 3) < Rational(1, 2));
    MPL_CHECK(Rational(2, 3) > Rational(1, 2));
    MPL_CHECK(Rational(-1, 3) > Rational(-1, 2));
    MPL_CHECK(!(Rational(1, 2) < Rational(1, 2)));
    MPL_CHECK(Rational(3, 2) < Rational(5, 3));
    MPL_CHECK(Rational(1, 1000000000) < Rational(1, 999999999));

    const std::int64_t maximum = std::numeric_limits<std::int64_t>::max();
    MPL_CHECK(Rational(maximum - 1, maximum) > Rational(maximum - 2, maximum - 1));
    MPL_CHECK(Rational(maximum, 1) > Rational(maximum - 1, 1));
    MPL_CHECK(Rational(-(maximum - 1), maximum) < Rational(maximum - 1, maximum));
}

MPL_TEST(RationalSortingIsConsistent) {
    std::vector<Rational> values{
        Rational(2, 3), Rational(1, 2), Rational(-1, 3), Rational(3, 4),
        Rational(1, 2), Rational(-2, 3), Rational(5, 6), Rational(0, 1),
    };
    std::sort(values.begin(), values.end());
    for (std::size_t index = 0; index + 1 < values.size(); ++index) {
        MPL_CHECK(values[index] <= values[index + 1]);
        MPL_CHECK(!(values[index + 1] < values[index]));
    }
    MPL_CHECK(values.front() == Rational(-2, 3));
    MPL_CHECK(values.back() == Rational(5, 6));

    const std::vector<Rational> forward{Rational(1, 3), Rational(1, 2), Rational(2, 3)};
    const std::vector<Rational> backward{Rational(2, 3), Rational(1, 2), Rational(1, 3)};
    MPL_CHECK(std::is_sorted(forward.begin(), forward.end()));
    MPL_CHECK(!std::is_sorted(backward.begin(), backward.end()));
    MPL_CHECK(std::is_sorted(backward.rbegin(), backward.rend()));
}

MPL_TEST(TimeTypesEnforceDomainBoundaries) {
    MPL_CHECK_THROWS(PhysicalTime(-1.0));
    MPL_CHECK_THROWS(RelativeTime(Rational(3, 2)));
    MPL_CHECK_THROWS(RelativeTime(Rational(-1, 2)));
    MPL_CHECK_THROWS(SampleTime(-1));

    const RelativeTime relative(Rational(1, 2));
    MPL_CHECK(relative.fraction() == Rational(1, 2));
    const MusicalTime musical(Position(Rational(3, 2)));
    MPL_CHECK(musical.wholeNotes() == Rational(3, 2));
    MPL_CHECK_EQ(std::string(timeDomainName(TimeDomain::Musical)), std::string("musical"));
    MPL_CHECK_EQ(std::string(timeDomainName(TimeDomain::Relative)), std::string("relative"));
    MPL_CHECK_EQ(std::string(timeDomainName(TimeDomain::Physical)), std::string("physical"));
    MPL_CHECK_EQ(std::string(timeDomainName(TimeDomain::Sample)), std::string("sample"));
}

MPL_TEST(MeterComputesBarAndBeatLengths) {
    MPL_CHECK((Meter{4, 4}.barLength() == Rational(1, 1)));
    MPL_CHECK((Meter{3, 4}.barLength() == Rational(3, 4)));
    MPL_CHECK((Meter{6, 8}.barLength() == Rational(3, 4)));
    MPL_CHECK((Meter{6, 8}.beatLength() == Rational(1, 8)));
    MPL_CHECK_EQ((Meter{7, 8}.toString()), std::string("7/8"));
}

MPL_TEST(DurationConvertsBarsAndBeats) {
    MPL_CHECK(Duration::fromBars(Rational(2, 1), Meter{4, 4}) == Duration(Rational(2, 1)));
    MPL_CHECK(Duration::fromBars(Rational(1, 1), Meter{3, 4}) == Duration(Rational(3, 4)));
    MPL_CHECK(Duration::fromBeats(Rational(4, 1), Meter{6, 8}) == Duration(Rational(1, 2)));
    MPL_CHECK(Duration::fromQuarterNotes(Rational(3, 1)) == Duration(Rational(3, 4)));
}

MPL_TEST(DurationConvertsToSeconds) {
    MPL_CHECK_NEAR(Duration(Rational(1, 1)).toSeconds(120.0), 2.0, 1e-9);
    MPL_CHECK_NEAR(Duration(Rational(1, 1)).toSeconds(160.0), 1.5, 1e-9);
    MPL_CHECK_NEAR(Duration(Rational(1, 4)).toSeconds(120.0), 0.5, 1e-9);
}

MPL_TEST(PositionArithmetic) {
    const Position start = Position::fromBars(Rational(1, 1), Meter{4, 4});
    const Position later = start + Duration(Rational(1, 2));
    MPL_CHECK(later == Position(Rational(3, 2)));
    MPL_CHECK(later - start == Duration(Rational(1, 2)));
    MPL_CHECK(start > Position{});
}

MPL_TEST(PitchParsesScientificNotation) {
    MPL_CHECK_EQ(("F4"_p).midi(), 65);
    MPL_CHECK_EQ(("Ab4"_p).midi(), 68);
    MPL_CHECK_EQ(("C#5"_p).midi(), 73);
    MPL_CHECK_EQ(("Bb3"_p).midi(), 58);
    MPL_CHECK_EQ(Pitch::parse("C-1").midi(), 0);
    MPL_CHECK_EQ(Pitch::parse("G9").midi(), 127);
    MPL_CHECK_THROWS(Pitch::parse("H4"));
    MPL_CHECK_THROWS(Pitch::parse(""));
}

MPL_TEST(PitchNamesRoundTrip) {
    MPL_CHECK_EQ(("F4"_p).name(), std::string("F4"));
    MPL_CHECK_EQ(("Ab4"_p).name(), std::string("Ab4"));
    MPL_CHECK_EQ(Pitch::fromMidi(61).name(), std::string("C#4"));
    MPL_CHECK_EQ(Pitch::fromMidi(61, true).name(), std::string("Db4"));
    MPL_CHECK_EQ(Pitch::fromMidi(("F4"_p).midi()).name(), std::string("F4"));
}

MPL_TEST(PitchTransposesWithinMidiRange) {
    const Pitch f4 = "F4"_p;
    MPL_CHECK_EQ(f4.transposed(Interval::octaves(1)).name(), std::string("F5"));
    MPL_CHECK_EQ(f4.transposed(Interval::fromSemitones(-2)).midi(), 63);

    const Pitch g9 = Pitch::parse("G9");
    MPL_CHECK_THROWS(g9.transposed(Interval::fromSemitones(1)));
}

MPL_TEST(MelodyLaysOutNotesSequentially) {
    const Fragment fragment = motif();
    MPL_CHECK_EQ(fragment.noteCount(), std::size_t{3});
    MPL_CHECK(fragment.duration() == Duration(Rational(2, 1)));
    MPL_CHECK(fragment.notes()[0].offset == Duration(Rational(0, 1)));
    MPL_CHECK(fragment.notes()[1].offset == Duration(Rational(1, 2)));
    MPL_CHECK(fragment.notes()[2].offset == Duration(Rational(1, 1)));
    MPL_CHECK(fragment.notes()[2].duration == Duration(Rational(1, 1)));
}

MPL_TEST(FragmentRejectsInvalidNotes) {
    Note negativeOffset;
    negativeOffset.pitch = "C4"_p;
    negativeOffset.offset = Duration(Rational(-1, 4));
    negativeOffset.duration = Duration(Rational(1, 4));
    MPL_CHECK_THROWS(Fragment::fromNotes({negativeOffset}));

    Note zeroLength;
    zeroLength.pitch = "C4"_p;
    zeroLength.duration = Duration(Rational(0, 1));
    MPL_CHECK_THROWS(Fragment::fromNotes({zeroLength}));

    Note badVelocity;
    badVelocity.pitch = "C4"_p;
    badVelocity.duration = Duration(Rational(1, 4));
    badVelocity.velocity = 0;
    MPL_CHECK_THROWS(Fragment::fromNotes({badVelocity}));

    MPL_CHECK_THROWS(Fragment::fromNotes({Note{"C4"_p, Duration{}, Duration(Rational(1, 1)), 96}},
                                          Duration(Rational(1, 2))));
}

MPL_TEST(TransposeShiftsEveryNote) {
    const Fragment transposed = transpose(motif(), Interval::octaves(1));
    MPL_CHECK_EQ(transposed.noteCount(), std::size_t{3});
    MPL_CHECK_EQ(transposed.notes()[0].pitch.name(), std::string("F5"));
    MPL_CHECK_EQ(transposed.notes()[1].pitch.name(), std::string("Ab5"));
    MPL_CHECK(transposed.duration() == Duration(Rational(2, 1)));
}

MPL_TEST(ReverseMirrorsInsideSpan) {
    const Fragment reversed = reverse(motif());
    MPL_CHECK(reversed.duration() == Duration(Rational(2, 1)));
    MPL_CHECK_EQ(reversed.notes()[0].pitch.name(), std::string("C5"));
    MPL_CHECK(reversed.notes()[0].offset == Duration(Rational(0, 1)));
    MPL_CHECK(reversed.notes()[0].duration == Duration(Rational(1, 1)));
    MPL_CHECK(reversed.notes()[2].offset == Duration(Rational(3, 2)));
    MPL_CHECK(reversed.notes()[2].duration == Duration(Rational(1, 2)));
}

MPL_TEST(StretchScalesOffsetsAndLengths) {
    const Fragment stretched = stretch(motif(), Rational(2, 1));
    MPL_CHECK(stretched.duration() == Duration(Rational(4, 1)));
    MPL_CHECK(stretched.notes()[1].offset == Duration(Rational(1, 1)));
    MPL_CHECK(stretched.notes()[1].duration == Duration(Rational(1, 1)));
    MPL_CHECK_THROWS(stretch(motif(), Rational(0, 1)));
    MPL_CHECK_THROWS(stretch(motif(), Rational(-1, 2)));
}

MPL_TEST(RepeatConcatenatesCopies) {
    const Fragment repeated = repeat(motif(), 3);
    MPL_CHECK(repeated.duration() == Duration(Rational(6, 1)));
    MPL_CHECK_EQ(repeated.noteCount(), std::size_t{9});
    MPL_CHECK(repeated.notes()[3].offset == Duration(Rational(2, 1)));
    MPL_CHECK_THROWS(repeat(motif(), 0));
}

MPL_TEST(SequenceShiftsPartsInTime) {
    const Fragment first = motif();
    const Fragment second = Fragment::fromMelody({{"C3"_p, Rational(1, 1)}});
    const Fragment joined = sequence({first, second});
    MPL_CHECK(joined.duration() == Duration(Rational(3, 1)));
    MPL_CHECK(joined.notes()[3].offset == Duration(Rational(2, 1)));
    MPL_CHECK_EQ(joined.notes()[3].pitch.name(), std::string("C3"));
    MPL_CHECK_THROWS(sequence({}));
}

MPL_TEST(ParallelOverlaysParts) {
    const Fragment high = motif();
    const Fragment low = Fragment::fromMelody({{"C2"_p, Rational(1, 2)}});
    const Fragment layered = parallel({high, low});
    MPL_CHECK(layered.duration() == Duration(Rational(2, 1)));
    MPL_CHECK_EQ(layered.noteCount(), std::size_t{4});
    MPL_CHECK(layered.notes()[0].offset == Duration(Rational(0, 1)));
    MPL_CHECK_THROWS(parallel({}));
}

MPL_TEST(TripletsUseExactRationalTime) {
    const Fragment triplet = Fragment::fromMelody({
        {"C4"_p, Rational(1, 12)},
        {"D4"_p, Rational(1, 12)},
        {"E4"_p, Rational(1, 12)},
    });
    MPL_CHECK(triplet.duration() == Duration(Rational(1, 4)));
    MPL_CHECK(triplet.notes()[1].offset == Duration(Rational(1, 12)));
    MPL_CHECK(triplet.notes()[2].offset == Duration(Rational(1, 6)));
}

MPL_TEST(CompoundMeterKeepsExactBarPositions) {
    const Meter sevenEight{7, 8};
    MPL_CHECK(sevenEight.barLength() == Rational(7, 8));
    MPL_CHECK(Position::fromBars(Rational(2, 1), sevenEight) == Position(Rational(7, 4)));
    MPL_CHECK(Duration::fromBeats(Rational(3, 1), Meter{6, 8}) == Duration(Rational(3, 8)));
    MPL_CHECK(Duration::fromBars(Rational(1, 1), sevenEight).toSeconds(120.0) == 1.75);
}

MPL_TEST(CrossBarNotesAndTrailingRestsAreRepresentable) {
    Note early;
    early.pitch = "C4"_p;
    early.duration = Duration(Rational(1, 4));

    Note crossing;
    crossing.pitch = "G4"_p;
    crossing.offset = Duration(Rational(7, 8));
    crossing.duration = Duration(Rational(1, 4));

    const Fragment fragment =
        Fragment::fromNotes({early, crossing}, Duration(Rational(2, 1)));
    MPL_CHECK(fragment.duration() == Duration(Rational(2, 1)));
    MPL_CHECK(fragment.contentEnd() == Duration(Rational(9, 8)));

    const Fragment mirrored = reverse(fragment);
    MPL_CHECK(mirrored.duration() == Duration(Rational(2, 1)));
    MPL_CHECK(mirrored.notes()[0].offset == Duration(Rational(7, 8)));
    MPL_CHECK(mirrored.notes()[1].offset == Duration(Rational(7, 4)));
}
