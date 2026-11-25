#include "TestSupport.hpp"

#include "mpl/core/MeterMap.hpp"

#include <string>
#include <utility>
#include <vector>

using namespace mpl;

namespace {

Rhythm simpleRhythm() {
    return Rhythm::fromEvents({{Rational(0, 1), Rational(1, 8), 2, false, kInvalidObjectId},
                               {Rational(1, 8), Rational(1, 8), 0, false, kInvalidObjectId},
                               {Rational(2, 8), Rational(1, 8), 1, true, kInvalidObjectId}},
                              Rational(3, 8));
}

std::string rhythmProgram(const std::string& body, const std::string& meter = "7/8") {
    return mpltest::header(meter) + body +
           "track Piano { asset = PianoAsset; role = melodic; }\n"
           "section S(1 bars) meter " + meter + " {\n"
           "  clip Piano \"S\" { play M; }\n"
           "}\n"
           "arrangement { play S; }\n"
           "export midi(\"x.mid\");\n";
}

Compilation compileRhythm(const std::string& body, const std::string& meter = "7/8") {
    return mpltest::compileText(rhythmProgram(body, meter));
}

}  // namespace

MPL_TEST(RhythmLiteralEventsRestsAndAccents) {
    const Compilation compilation = compileRhythm(
        "let R = rhythm {\n"
        "  hit 1/8 accent 2,\n"
        "  hit 1/8,\n"
        "  hit 1/16,\n"
        "  rest 1/16\n"
        "};\n"
        "let M = zip(R, [C4, E4, G4], 90);\n");
    const Rhythm* rhythm = mpltest::rhythmByLabel(compilation, "R");
    MPL_CHECK(rhythm != nullptr);
    if (rhythm == nullptr) {
        return;
    }
    MPL_CHECK(rhythm->logicalLength() == Rational(3, 8));
    MPL_CHECK_EQ(rhythm->eventCount(), std::size_t{4});
    MPL_CHECK_EQ(rhythm->attackCount(), std::size_t{3});
    MPL_CHECK(rhythm->events()[0].onset == Rational(0, 1));
    MPL_CHECK(rhythm->events()[2].onset == Rational(2, 8));
    MPL_CHECK(rhythm->events()[3].onset == Rational(5, 16));
    MPL_CHECK(rhythm->events()[3].rest);
    MPL_CHECK_EQ(rhythm->events()[0].accent, 2);
    MPL_CHECK_EQ(compilation.production.noteCount(), std::size_t{3});
}

MPL_TEST(RhythmLiteralSpanPreservesLogicalGap) {
    const Compilation compilation = compileRhythm(
        "let R = rhythm span 1/2 { hit 1/8, hit 1/8 };\n"
        "let M = cycle(R, [C4], 90);\n");
    const Rhythm* rhythm = mpltest::rhythmByLabel(compilation, "R");
    MPL_CHECK(rhythm != nullptr);
    if (rhythm != nullptr) {
        MPL_CHECK(rhythm->logicalLength() == Rational(1, 2));
        MPL_CHECK(rhythm->contentEnd() == Rational(1, 4));
    }
}

MPL_TEST(RhythmLiteralRejectsShortSpan) {
    const FrontendResult result = mpltest::frontend(rhythmProgram(
        "let R = rhythm span 1/8 { hit 1/8, hit 1/8 };\n"
        "let M = cycle(R, [C4], 90);\n"));
    MPL_CHECK(mpltest::hasCode(result, "type-duration"));
}

MPL_TEST(RhythmLiteralRejectsBadAccent) {
    const FrontendResult result = mpltest::frontend(rhythmProgram(
        "let R = rhythm { hit 1/8 accent 9 };\n"
        "let M = cycle(R, [C4], 90);\n"));
    MPL_CHECK(mpltest::hasCode(result, "sema-accent-range"));
}

MPL_TEST(RhythmSequenceConcatenates) {
    const Rhythm first = Rhythm::fromEvents(
        {{Rational(0, 1), Rational(1, 8), 1, false, kInvalidObjectId}}, Rational(1, 8));
    const Rhythm second = Rhythm::fromEvents(
        {{Rational(0, 1), Rational(1, 4), 2, false, kInvalidObjectId}}, Rational(1, 4));
    const Rhythm result = rhythmSequence({first, second});
    MPL_CHECK(result.logicalLength() == Rational(3, 8));
    MPL_CHECK_EQ(result.attackCount(), std::size_t{2});
    MPL_CHECK(result.events()[1].onset == Rational(1, 8));
    MPL_CHECK(result.events()[1].duration == Rational(1, 4));
}

MPL_TEST(RhythmOverlayKeepsOverlaps) {
    const Rhythm first = Rhythm::fromEvents(
        {{Rational(0, 1), Rational(1, 8), 1, false, kInvalidObjectId}}, Rational(3, 8));
    const Rhythm second = Rhythm::fromEvents(
        {{Rational(0, 1), Rational(1, 8), 2, false, kInvalidObjectId},
         {Rational(1, 8), Rational(1, 8), 0, false, kInvalidObjectId}},
        Rational(1, 4));
    const Rhythm result = rhythmOverlay({first, second});
    MPL_CHECK(result.logicalLength() == Rational(3, 8));
    MPL_CHECK_EQ(result.eventCount(), std::size_t{3});
    MPL_CHECK_EQ(result.events()[0].accent, 1);
}

MPL_TEST(RhythmRepeatAndLimit) {
    const Rhythm rhythm = simpleRhythm();
    const Rhythm repeated = rhythmRepeat(rhythm, 3);
    MPL_CHECK(repeated.logicalLength() == Rational(9, 8));
    MPL_CHECK_EQ(repeated.eventCount(), std::size_t{9});
    MPL_CHECK(repeated.events()[3].onset == Rational(3, 8));
    MPL_CHECK_THROWS(rhythmRepeat(rhythm, 0));
    MPL_CHECK_THROWS(rhythmRepeat(rhythm, kMaxRhythmRepeat + 1));
}

MPL_TEST(RhythmRotateWrapsAndSplitsCrossBoundaryEvents) {
    const Rhythm rhythm = simpleRhythm();
    const Rhythm rotated = rhythmRotate(rhythm, Rational(1, 8));
    MPL_CHECK(rotated.logicalLength() == Rational(3, 8));
    MPL_CHECK(rotated.events()[0].rest);
    MPL_CHECK(rotated.events()[0].onset == Rational(0, 1));
    MPL_CHECK(rotated.events()[1].onset == Rational(1, 8));
    MPL_CHECK(rotated.events()[2].onset == Rational(2, 8));
    const Rhythm full = rhythmRotate(rhythm, Rational(3, 8));
    MPL_CHECK(full.events()[0].onset == Rational(0, 1));
    MPL_CHECK(!full.events()[0].rest);

    const Rhythm crossing = Rhythm::fromEvents(
        {{Rational(1, 4), Rational(3, 8), 0, false, kInvalidObjectId}}, Rational(3, 8));
    const Rhythm split = rhythmRotate(crossing, Rational(0, 1));
    MPL_CHECK_EQ(split.eventCount(), std::size_t{2});
    MPL_CHECK(split.events()[0].onset == Rational(0, 1));
    MPL_CHECK(split.events()[0].duration == Rational(1, 4));
    MPL_CHECK(split.events()[0].continuation);
    MPL_CHECK(split.events()[1].onset == Rational(1, 4));
    MPL_CHECK(split.events()[1].duration == Rational(1, 8));
    MPL_CHECK(!split.events()[1].continuation);
    MPL_CHECK_EQ(split.attackCount(), std::size_t{1});
}

MPL_TEST(RhythmShiftAllowsNegativeOnsets) {
    const Rhythm shifted = rhythmShift(simpleRhythm(), Rational(-1, 16));
    MPL_CHECK(shifted.logicalLength() == Rational(3, 8));
    MPL_CHECK(shifted.events()[0].onset == Rational(-1, 16));
    MPL_CHECK(shifted.events()[2].onset == Rational(3, 16));
}

MPL_TEST(RhythmStretchScalesEverything) {
    const Rhythm stretched = rhythmStretch(simpleRhythm(), Rational(3, 2));
    MPL_CHECK(stretched.logicalLength() == Rational(9, 16));
    MPL_CHECK(stretched.events()[0].duration == Rational(3, 16));
    MPL_CHECK(stretched.events()[2].onset == Rational(3, 8));
    MPL_CHECK_THROWS(rhythmStretch(simpleRhythm(), Rational(0, 1)));
}

MPL_TEST(RhythmSubdivideSplitsEvents) {
    const Rhythm result = rhythmSubdivide(simpleRhythm(), 2);
    MPL_CHECK(result.logicalLength() == Rational(3, 8));
    MPL_CHECK_EQ(result.eventCount(), std::size_t{6});
    MPL_CHECK(result.events()[0].onset == Rational(0, 1));
    MPL_CHECK(result.events()[0].duration == Rational(1, 16));
    MPL_CHECK(result.events()[1].onset == Rational(1, 16));
    MPL_CHECK_THROWS(rhythmSubdivide(simpleRhythm(), kMaxRhythmSubdivision + 1));
}

MPL_TEST(RhythmAccentCyclesLevels) {
    const Rhythm result = rhythmAccent(simpleRhythm(), {3, 1});
    MPL_CHECK_EQ(result.events()[0].accent, 3);
    MPL_CHECK_EQ(result.events()[1].accent, 1);
    MPL_CHECK_EQ(result.events()[2].accent, 1);
    MPL_CHECK(result.events()[2].rest);
    MPL_CHECK_THROWS(rhythmAccent(simpleRhythm(), {9}));
    MPL_CHECK_THROWS(rhythmAccent(simpleRhythm(), {}));
}

MPL_TEST(RhythmRealignRepeatsToTargetPeriod) {
    const Rhythm aligned = rhythmRealign(simpleRhythm(), Rational(3, 2));
    MPL_CHECK(aligned.logicalLength() == Rational(3, 2));
    MPL_CHECK_EQ(aligned.eventCount(), std::size_t{12});
    MPL_CHECK_THROWS(rhythmRealign(simpleRhythm(), Rational(1, 2)));
    MPL_CHECK_THROWS(rhythmRealign(simpleRhythm(), Rational(0, 1)));
}

MPL_TEST(RhythmCommonPeriodExact) {
    const Rhythm three = Rhythm::fromEvents(
        {{Rational(0, 1), Rational(1, 8), 0, false, kInvalidObjectId}}, Rational(3, 8));
    const Rhythm half = Rhythm::fromEvents(
        {{Rational(0, 1), Rational(1, 8), 0, false, kInvalidObjectId}}, Rational(1, 2));
    const std::optional<Rational> common = rhythmCommonPeriod(three, half);
    MPL_CHECK(common.has_value());
    MPL_CHECK(*common == Rational(3, 2));
    MPL_CHECK(rhythmRealign(three, *common).logicalLength() == Rational(3, 2));
    MPL_CHECK(rhythmRealign(half, *common).logicalLength() == Rational(3, 2));
}

MPL_TEST(RhythmQuantizeSnapsOnsets) {
    const Rhythm rhythm = Rhythm::fromEvents(
        {{Rational(1, 32), Rational(1, 32), 0, false, kInvalidObjectId},
         {Rational(3, 32), Rational(1, 32), 0, false, kInvalidObjectId}},
        Rational(3, 8));
    const Rhythm quantized = rhythmQuantize(rhythm, Rational(1, 16));
    MPL_CHECK(quantized.events()[0].onset == Rational(1, 16));
    MPL_CHECK(quantized.events()[1].onset == Rational(1, 8));
    MPL_CHECK_THROWS(rhythmQuantize(rhythm, Rational(0, 1)));
}

MPL_TEST(RhythmEventLimitIsEnforced) {
    std::vector<RhythmEvent> events;
    events.reserve(kMaxRhythmEvents + 1);
    for (std::size_t index = 0; index <= kMaxRhythmEvents; ++index) {
        events.push_back(RhythmEvent{Rational(static_cast<std::int64_t>(index), 4096),
                                     Rational(1, 4096), 0, false, kInvalidObjectId});
    }
    MPL_CHECK_THROWS(Rhythm::fromEvents(std::move(events), Rational(2, 1)));
}

MPL_TEST(ZipRequiresMatchingCounts) {
    const FrontendResult result = mpltest::frontend(rhythmProgram(
        "let R = rhythm { hit 1/8, hit 1/8, rest 1/8 };\n"
        "let M = zip(R, [C4], 90);\n"));
    MPL_CHECK(mpltest::hasCode(result, "rhythm-pitch-count"));
}

MPL_TEST(CycleFillsPitchesAndRestsConsumeNone) {
    const Compilation compilation = compileRhythm(
        "let R = rhythm { hit 1/8, rest 1/8, hit 1/8 };\n"
        "let M = cycle(R, [C4], 90);\n");
    const auto& notes = compilation.production.tracks.front().clips.front().content.notes();
    MPL_CHECK_EQ(notes.size(), std::size_t{2});
    MPL_CHECK(notes[0].offset == Duration(Rational(0, 1)));
    MPL_CHECK(notes[1].offset == Duration(Rational(1, 4)));
    MPL_CHECK(notes[0].pitch.name() == std::string("C4"));
    MPL_CHECK(notes[1].pitch.name() == std::string("C4"));
}

MPL_TEST(ZipMapsAccentsToVelocity) {
    const Compilation compilation = compileRhythm(
        "let R = rhythm { hit 1/8 accent 2, hit 1/8, hit 1/8 };\n"
        "let M = zip(R, [C4, E4, G4], 80);\n");
    const auto& notes = compilation.production.tracks.front().clips.front().content.notes();
    MPL_CHECK_EQ(notes.size(), std::size_t{3});
    MPL_CHECK_EQ(notes[0].velocity, 96);
    MPL_CHECK_EQ(notes[1].velocity, 80);
}

MPL_TEST(RhythmCombineRecordsPerEventProvenance) {
    const Compilation compilation = compileRhythm(
        "let R = rhythm { hit 1/8 accent 1, rest 1/8, hit 1/8 };\n"
        "let M = cycle(R, [C4, E4], 90);\n");
    const Rhythm* rhythm = mpltest::rhythmByLabel(compilation, "R");
    MPL_CHECK(rhythm != nullptr);
    const auto& notes = compilation.production.tracks.front().clips.front().content.notes();
    MPL_CHECK_EQ(notes.size(), std::size_t{2});
    MPL_CHECK(notes[0].rhythmOrigin != kInvalidObjectId);
    MPL_CHECK(notes[1].rhythmOrigin != kInvalidObjectId);
    MPL_CHECK(notes[0].rhythmOrigin == notes[1].rhythmOrigin);
    MPL_CHECK(notes[0].rhythmEventIndex != notes[1].rhythmEventIndex);
    if (rhythm != nullptr) {
        MPL_CHECK(notes[0].rhythmOrigin == rhythm->events()[0].origin);
        MPL_CHECK_EQ(notes[0].rhythmEventIndex, std::uint32_t{0});
    }
}

MPL_TEST(RhythmObjectsDoNotEvaluateAsFragments) {
    Compilation compilation = compileRhythm(
        "let R = rhythm { hit 1/8, hit 1/8 };\n"
        "let M = cycle(R, [C4], 90);\n");
    ObjectId rhythmId = kInvalidObjectId;
    for (const MusicNode& node : compilation.music.nodes()) {
        if (compilation.music.isRhythmObject(node.id)) {
            rhythmId = node.id;
            break;
        }
    }
    MPL_CHECK(rhythmId != kInvalidObjectId);
    MPL_CHECK_THROWS(compilation.music.evaluate(rhythmId));
}

MPL_TEST(NegativeRhythmCombineIsDiagnosed) {
    const FrontendResult result = mpltest::frontend(rhythmProgram(
        "let R = shift(rhythm { hit 1/8, hit 1/8 }, -1/16);\n"
        "let M = zip(R, [C4, E4], 90);\n"));
    MPL_CHECK(mpltest::hasCode(result, "rhythm-pitch"));
}

MPL_TEST(OnsetSetReducesDurationsAndDropsRests) {
    const Compilation compilation = compileRhythm(
        "let R = rhythm { hit 1/4 accent 2, rest 1/4, hit 1/4 };\n"
        "let O = onsetSet(R, 1/16);\n"
        "let M = cycle(O, [C4], 90);\n");
    const Rhythm* onsetSet = mpltest::rhythmByLabel(compilation, "O");
    MPL_CHECK(onsetSet != nullptr);
    if (onsetSet != nullptr) {
        MPL_CHECK(onsetSet->logicalLength() == Rational(3, 4));
        MPL_CHECK_EQ(onsetSet->eventCount(), std::size_t{2});
        MPL_CHECK_EQ(onsetSet->attackCount(), std::size_t{2});
        MPL_CHECK(!onsetSet->hasRests());
        MPL_CHECK(onsetSet->events()[0].duration == Rational(1, 16));
        MPL_CHECK_EQ(onsetSet->events()[0].accent, 2);
        MPL_CHECK(onsetSet->events()[1].onset == Rational(1, 2));
    }
}

MPL_TEST(SplitContinuationDoesNotRetrigger) {
    const std::string source =
        mpltest::header("4/4") +
        "let R = shift(rhythm { hit 1/2 }, 1/4);\n"
        "let X = rotate(R, 0);\n"
        "let Y = retrigger(X);\n"
        "let M1 = cycle(X, [C4], 90);\n"
        "let M2 = cycle(Y, [C4], 90);\n"
        "track Piano { asset = PianoAsset; role = melodic; }\n"
        "section S(1 bars) {\n"
        "  clip Piano \"split\" { play M1; }\n"
        "  clip Piano \"retrigger\" { play M2; }\n"
        "}\n"
        "arrangement { play S; }\n"
        "export midi(\"x.mid\");\n";
    const Compilation compilation = mpltest::compileText(source);
    const auto& clips = compilation.production.tracks.front().clips;
    MPL_CHECK_EQ(clips.size(), std::size_t{2});
    // The split continuation is not retriggered; retrigger() opts in.
    MPL_CHECK_EQ(clips[0].content.noteCount(), std::size_t{1});
    MPL_CHECK_EQ(clips[1].content.noteCount(), std::size_t{2});
}

MPL_TEST(ContinuationWithoutPredecessorIsRejected) {
    std::vector<RhythmEvent> orphan;
    RhythmEvent continuation;
    continuation.onset = Rational(1, 4);
    continuation.duration = Rational(1, 8);
    continuation.continuation = true;
    orphan.push_back(continuation);
    MPL_CHECK_THROWS(Rhythm::fromEvents(std::move(orphan), Rational(1, 2)));

    // A valid split can be repeated: the wrapped predecessor exists in the
    // repeated cycle as well.
    const Rhythm crossing = Rhythm::fromEvents(
        {{Rational(1, 4), Rational(3, 8), 0, false, kInvalidObjectId}}, Rational(3, 8));
    const Rhythm split = rhythmRotate(crossing, Rational(0, 1));
    const Rhythm repeated = rhythmRepeat(split, 2);
    MPL_CHECK_EQ(repeated.attackCount(), std::size_t{2});
    MPL_CHECK(repeated.isValid());
}

MPL_TEST(RhythmCombineDropsContinuation) {
    const Compilation compilation = mpltest::compileText(
        "project T { tempo = 120 bpm; meter = 4/4; key = C major; }\n"
        "asset Piano = instrument(\"GrandPiano\");\n"
        "let R = rhythm span 3/8 { hit 3/8 };\n"
        "let M = cycle(rotate(R, 0), [C4], 90);\n"
        "track Keys { asset = Piano; role = melodic; }\n"
        "section S(1 bars) { clip Keys { play M; } }\n"
        "arrangement { play S; }\n"
        "export midi(\"x.mid\");\n");
    const auto& notes = compilation.production.tracks.front().clips.front().content.notes();
    // The continuation is a split tail, not a second attack.
    MPL_CHECK_EQ(notes.size(), std::size_t{1});
}

MPL_TEST(MetricAccentUsesSevenEightGrouping) {
    const std::string source =
        mpltest::header("7/8") +
        "let R = rhythm { hit 1/8, hit 1/8, hit 1/8, hit 1/8, hit 1/8, hit 1/8, hit 1/8 };\n"
        "fn drums() -> Fragment context meter { return cycle(metricAccent(R), [C2], 100); }\n"
        "track Piano { asset = PianoAsset; role = percussion; }\n"
        "section S(1 bars) meter 7/8 grouping 2+2+3 {\n"
        "  clip Piano \"S\" { play drums(); }\n"
        "}\n"
        "arrangement { play S; }\n"
        "export midi(\"x.mid\");\n";
    const Compilation compilation = mpltest::compileText(source);
    const auto& notes = compilation.production.tracks.front().clips.front().content.notes();
    MPL_CHECK_EQ(notes.size(), std::size_t{7});
    const std::vector<int> expected = {124, 108, 116, 108, 116, 108, 108};
    for (std::size_t index = 0; index < notes.size(); ++index) {
        MPL_CHECK_EQ(notes[index].velocity, expected[index]);
    }
}

MPL_TEST(MetricAccentAlternateGroupingChangesAccents) {
    const std::string source =
        mpltest::header("7/8") +
        "let R = rhythm { hit 1/8, hit 1/8, hit 1/8, hit 1/8, hit 1/8, hit 1/8, hit 1/8 };\n"
        "fn drums() -> Fragment context meter { return cycle(metricAccent(R), [C2], 100); }\n"
        "track Piano { asset = PianoAsset; role = percussion; }\n"
        "section S(1 bars) meter 7/8 grouping 3+2+2 {\n"
        "  clip Piano \"S\" { play drums(); }\n"
        "}\n"
        "arrangement { play S; }\n"
        "export midi(\"x.mid\");\n";
    const Compilation compilation = mpltest::compileText(source);
    const auto& notes = compilation.production.tracks.front().clips.front().content.notes();
    MPL_CHECK_EQ(notes.size(), std::size_t{7});
    const std::vector<int> expected = {124, 108, 108, 116, 108, 116, 108};
    for (std::size_t index = 0; index < notes.size(); ++index) {
        MPL_CHECK_EQ(notes[index].velocity, expected[index]);
    }
}

MPL_TEST(MetricWeightQueryIsExposedToLanguage) {
    const std::string source =
        mpltest::header("7/8") +
        "let R = rhythm { hit 1/8, hit 1/8, hit 1/8 };\n"
        "fn weight() -> Int context meter { return metricWeight(2/8); }\n"
        "track Piano { asset = PianoAsset; role = melodic; }\n"
        "section S(1 bars) meter 7/8 grouping 2+2+3 {\n"
        "  clip Piano \"S\" { play note(C4, 1/8, 60 + weight() * 10); }\n"
        "}\n"
        "arrangement { play S; }\n"
        "export midi(\"x.mid\");\n";
    const Compilation compilation = mpltest::compileText(source);
    const auto& notes = compilation.production.tracks.front().clips.front().content.notes();
    MPL_CHECK_EQ(notes.front().velocity, 80);
}
