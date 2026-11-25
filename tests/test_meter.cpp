#include "TestSupport.hpp"

#include "mpl/core/MeterMap.hpp"

#include <string>
#include <vector>

using namespace mpl;

namespace {

MeterMap sevenEightMap() {
    return MeterMap(Meter{7, 8}, MetricGrouping{{2, 2, 3}, true});
}

MeterMap mixedMap() {
    MeterMap map(Meter{7, 8}, MetricGrouping{{2, 2, 3}, true});
    map.add(Position(Rational(7, 4)), Meter{5, 8}, MetricGrouping{{2, 3}, true});
    map.add(Position(Rational(3, 1)), Meter{4, 4}, MetricGrouping{{2, 2}, true});
    return map;
}

std::string threeSections() {
    return mpltest::header("7/8", "D minor") +
           "let R = rhythm { hit 1/8, hit 1/16, rest 1/16 };\n"
           "let M = cycle(R, [D4, F4], 90);\n"
           "track Piano { asset = PianoAsset; role = melodic; }\n"
           "section A(2 bars) meter 7/8 grouping 2+2+3 {\n"
           "  clip Piano \"A\" { play M; }\n"
           "}\n"
           "section B(2 bars) meter 5/8 grouping 2+3 {\n"
           "  clip Piano \"B\" { play M; }\n"
           "}\n"
           "section C(1 bars) meter 4/4 grouping 2+2 {\n"
           "  clip Piano \"C\" { play M; }\n"
           "}\n"
           "arrangement { play A; play B; play C; }\n"
           "export midi(\"x.mid\");\n";
}

}  // namespace

MPL_TEST(MeterMapInitialAndFourFour) {
    MeterMap map;
    MPL_CHECK_EQ(map.size(), std::size_t{1});
    MPL_CHECK(map.isInRange(Position{}));
    MPL_CHECK(map.isBarStart(Position{}));
    MPL_CHECK(map.isBarStart(Position(Rational(1, 1))));
    MPL_CHECK(!map.isBarStart(Position(Rational(1, 4))));
    MPL_CHECK(map.isBeatStart(Position(Rational(1, 4))));
    MPL_CHECK(!map.isBeatStart(Position(Rational(1, 8))));
    const MetricPosition metric = map.toMetric(Position(Rational(5, 4)));
    MPL_CHECK_EQ(metric.bar, std::int64_t{1});
    MPL_CHECK(metric.beat == Rational(1, 1));
    MPL_CHECK(map.positionOfBar(3) == Position(Rational(3, 1)));
}

MPL_TEST(MeterMapSevenEightConversions) {
    const MeterMap map = sevenEightMap();
    MPL_CHECK(map.barLengthAt(Position{}) == Rational(7, 8));
    MPL_CHECK(map.barLengthAt(Position(Rational(9, 8))) == Rational(7, 8));
    MPL_CHECK(map.toMetric(Position(Rational(7, 8))) ==
              (MetricPosition{1, Rational(0, 1)}));
    MPL_CHECK(map.toMetric(Position(Rational(9, 8))) ==
              (MetricPosition{1, Rational(2, 1)}));
    for (const Rational& offset : {Rational(0, 1), Rational(1, 8), Rational(3, 8),
                                   Rational(7, 8), Rational(9, 8), Rational(7, 4)}) {
        const Position position(offset);
        MPL_CHECK(map.toPosition(map.toMetric(position)) == position);
    }
}

MPL_TEST(MeterMapMixedMeterRoundTrip) {
    const MeterMap map = mixedMap();
    MPL_CHECK_EQ(map.size(), std::size_t{3});
    MPL_CHECK(map.barIndexAt(Position(Rational(7, 4))) == 2);
    MPL_CHECK(map.barIndexAt(Position(Rational(3, 1))) == 4);
    MPL_CHECK(map.barLengthAt(Position(Rational(7, 4))) == Rational(5, 8));
    MPL_CHECK(map.barLengthAt(Position(Rational(3, 1))) == Rational(1, 1));
    MPL_CHECK(map.positionOfBar(4) == Position(Rational(3, 1)));
    const MetricPosition metric = map.toMetric(Position(Rational(3, 1) + Rational(1, 4)));
    MPL_CHECK_EQ(metric.bar, std::int64_t{4});
    MPL_CHECK(metric.beat == Rational(1, 1));
    for (const Rational& offset : {Rational(0, 1), Rational(7, 4), Rational(15, 8),
                                   Rational(3, 1), Rational(13, 4)}) {
        const Position position(offset);
        MPL_CHECK(map.toPosition(map.toMetric(position)) == position);
    }
}

MPL_TEST(MeterMapRejectsMidBarChange) {
    MeterMap map;
    std::string error;
    MPL_CHECK(!map.tryAdd(Position(Rational(1, 8)), Meter{3, 4}, MetricGrouping{}, error));
    MPL_CHECK(error.find("bar boundary") != std::string::npos);
    MPL_CHECK_EQ(map.size(), std::size_t{1});
    MPL_CHECK(map.tryAdd(Position(Rational(1, 1)), Meter{3, 4}, MetricGrouping{}, error));
    MPL_CHECK_EQ(map.size(), std::size_t{2});
}

MPL_TEST(MeterMapRejectsInvalidGrouping) {
    MeterMap map;
    std::string error;
    MPL_CHECK(!map.tryAdd(Position(Rational(1, 1)), Meter{7, 8},
                          MetricGrouping{{2, 2, 2}, true}, error));
    MPL_CHECK(error.find("sums to 6") != std::string::npos);
    MPL_CHECK(!map.tryAdd(Position(Rational(1, 1)), Meter{7, 8},
                          MetricGrouping{{2, 0, 5}, true}, error));
    MPL_CHECK(error.find("non-positive") != std::string::npos);
}

MPL_TEST(MeterMapRejectsNegativeAndReverseEvents) {
    MeterMap map;
    std::string error;
    MPL_CHECK(!map.tryAdd(Position(Rational(-1, 1)), Meter{4, 4}, MetricGrouping{}, error));
    MPL_CHECK(map.tryAdd(Position(Rational(1, 1)), Meter{4, 4}, MetricGrouping{}, error));
    error.clear();
    MPL_CHECK(!map.tryAdd(Position(Rational(1, 1)), Meter{3, 4}, MetricGrouping{}, error));
    MPL_CHECK(!map.tryAdd(Position(Rational(1, 2)), Meter{3, 4}, MetricGrouping{}, error));
}

MPL_TEST(MeterMapResolvesBarsAtBarBoundary) {
    const MeterMap map = sevenEightMap();
    std::string error;
    MetricDuration twoBars;
    twoBars.amount = Rational(2, 1);
    twoBars.unit = MetricUnit::Bars;
    MPL_CHECK(map.resolveDuration(Position{}, twoBars, error) == Rational(7, 4));
    MetricDuration oneBar;
    oneBar.amount = Rational(1, 1);
    oneBar.unit = MetricUnit::Bars;
    MPL_CHECK(map.resolveDuration(Position(Rational(7, 8)), oneBar, error) == Rational(7, 8));
    error.clear();
    MPL_CHECK(!map.resolveDuration(Position(Rational(1, 8)), oneBar, error).has_value());
    MPL_CHECK(error.find("bar boundary") != std::string::npos);
}

MPL_TEST(MeterMapResolvesFractionalBarsFromAnchor) {
    const MeterMap map = sevenEightMap();
    std::string error;
    MetricDuration quarterBar;
    quarterBar.amount = Rational(1, 4);
    quarterBar.unit = MetricUnit::Bars;
    MPL_CHECK(map.resolveDuration(Position{}, quarterBar, error) == Rational(7, 32));
    MPL_CHECK(map.resolveDuration(Position(Rational(7, 8)), quarterBar, error) ==
              Rational(7, 32));
    error.clear();
    MPL_CHECK(!map.resolveDuration(Position(Rational(1, 8)), quarterBar, error).has_value());
    MPL_CHECK(error.find("bar boundary") != std::string::npos);
    MetricDuration negative;
    negative.amount = Rational(-1, 8);
    negative.unit = MetricUnit::Bars;
    MPL_CHECK(map.resolveDuration(Position{}, negative, error) == Rational(-7, 64));
}

MPL_TEST(MeterMapResolvesBeatsFromAnchorMeter) {
    const MeterMap map = mixedMap();
    std::string error;
    MetricDuration twoBeats;
    twoBeats.amount = Rational(2, 1);
    twoBeats.unit = MetricUnit::Beats;
    MPL_CHECK(map.resolveDuration(Position{}, twoBeats, error) == Rational(2, 8));
    MPL_CHECK(map.resolveDuration(Position(Rational(7, 4)), twoBeats, error) == Rational(2, 8));
    MPL_CHECK(map.resolveDuration(Position(Rational(3, 1)), twoBeats, error) == Rational(1, 2));
}

MPL_TEST(MeterMapMetricHierarchySevenEight) {
    const MeterMap map = sevenEightMap();
    MPL_CHECK_EQ(map.metricWeight(Position{}).level, 3);
    MPL_CHECK_EQ(map.metricWeight(Position(Rational(1, 8))).level, 1);
    MPL_CHECK_EQ(map.metricWeight(Position(Rational(2, 8))).level, 2);
    MPL_CHECK_EQ(map.metricWeight(Position(Rational(3, 8))).level, 1);
    MPL_CHECK_EQ(map.metricWeight(Position(Rational(4, 8))).level, 2);
    MPL_CHECK_EQ(map.metricWeight(Position(Rational(1, 16))).level, 0);
    const MetricWeight group = map.metricWeight(Position(Rational(2, 8)));
    MPL_CHECK(group.groupStart);
    MPL_CHECK_EQ(group.groupIndex, 1);
    MPL_CHECK_EQ(group.beatIndex, 2);
}

MPL_TEST(MeterMapMetricHierarchyAlternateGrouping) {
    MeterMap map(Meter{7, 8}, MetricGrouping{{3, 2, 2}, true});
    MPL_CHECK_EQ(map.metricWeight(Position(Rational(3, 8))).level, 2);
    MPL_CHECK_EQ(map.metricWeight(Position(Rational(5, 8))).level, 2);
    MPL_CHECK_EQ(map.metricWeight(Position(Rational(2, 8))).level, 1);
    MPL_CHECK_EQ(map.metricWeight(Position(Rational(4, 8))).level, 1);
    MPL_CHECK_EQ(map.metricWeight(Position(Rational(6, 8))).level, 1);
}

MPL_TEST(MeterMapDescribeAndValidate) {
    const MeterMap map = mixedMap();
    std::string error;
    MPL_CHECK(map.validate(error));
    MPL_CHECK(map.describe().find("7/8 (2+2+3)") != std::string::npos);
    MPL_CHECK(map.describe().find("5/8 (2+3)") != std::string::npos);
}

MPL_TEST(MeterMapNegativeQueriesThrow) {
    const MeterMap map = sevenEightMap();
    MPL_CHECK_THROWS(map.toMetric(Position(Rational(-1, 8))));
    MPL_CHECK_THROWS(map.positionOfBar(-1));
    MPL_CHECK_THROWS(map.toPosition(MetricPosition{-1, Rational(0, 1)}));
    MPL_CHECK_THROWS(map.toPosition(MetricPosition{0, Rational(8, 1)}));
}

MPL_TEST(RationalPeriodLcm) {
    MPL_CHECK(rationalLcm(Rational(3, 8), Rational(1, 2)) == Rational(3, 2));
    MPL_CHECK(rationalLcm(Rational(1, 2), Rational(3, 4)) == Rational(3, 2));
    MPL_CHECK(rationalLcm(Rational(2, 4), Rational(1, 2)) == Rational(1, 2));
    MPL_CHECK(!rationalLcm(Rational(0, 1), Rational(1, 2)).has_value());
    MPL_CHECK(!rationalLcm(Rational(1000000000, 1), Rational(999999937, 1)).has_value());
}

MPL_TEST(MeterMapNegativeBarsAreLinearAndAsymmetric) {
    const MeterMap map = mixedMap();
    std::string error;
    MetricDuration positive;
    positive.amount = Rational(3, 1);
    positive.unit = MetricUnit::Bars;
    MPL_CHECK(map.resolveDuration(Position{}, positive, error) == Rational(19, 8));
    MetricDuration negative;
    negative.amount = Rational(-3, 1);
    negative.unit = MetricUnit::Bars;
    // Negative bars use the anchor's bar length (4/4 at bar 4 = 1), not a
    // backwards walk. Positive and negative are intentionally asymmetric.
    MPL_CHECK(map.resolveDuration(Position(Rational(3, 1)), negative, error) ==
              Rational(-3, 1));
    MPL_CHECK(map.resolveDuration(Position{}, negative, error) == Rational(-21, 8));
}

MPL_TEST(SectionMetersProduceGlobalMeterMap) {
    const Compilation compilation = mpltest::compileText(threeSections());
    MPL_CHECK(!compilation.hasErrors());
    const auto& meters = compilation.production.meterMap;
    MPL_CHECK_EQ(meters.size(), std::size_t{3});
    MPL_CHECK(meters[0].position == Position{});
    MPL_CHECK(meters[0].meter.toString() == std::string("7/8"));
    MPL_CHECK_EQ(meters[0].barIndex, std::int64_t{0});
    MPL_CHECK(meters[1].position == Position(Rational(7, 4)));
    MPL_CHECK(meters[1].meter.toString() == std::string("5/8"));
    MPL_CHECK_EQ(meters[1].barIndex, std::int64_t{2});
    MPL_CHECK(meters[2].position == Position(Rational(3, 1)));
    MPL_CHECK(meters[2].meter.toString() == std::string("4/4"));
    MPL_CHECK_EQ(meters[2].barIndex, std::int64_t{4});
    MPL_CHECK(meters[0].grouping.toString() == std::string("2+2+3"));
    MPL_CHECK(meters[1].grouping.toString() == std::string("2+3"));
    MPL_CHECK(compilation.production.logicalExtent == Duration(Rational(4, 1)));
}

MPL_TEST(SectionMeterChangeWithinSection) {
    const std::string source =
        mpltest::header("4/4") +
        "let R = rhythm { hit 1/8, hit 1/8, rest 1/8 };\n"
        "let M = cycle(R, [C4, E4], 90);\n"
        "track Piano { asset = PianoAsset; role = melodic; }\n"
        "section A(4 bars) meter 4/4 {\n"
        "  meter 7/8 at 2 bars grouping 2+2+3;\n"
        "  clip Piano \"A\" { play M; }\n"
        "}\n"
        "arrangement { play A; }\n"
        "export midi(\"x.mid\");\n";
    const Compilation compilation = mpltest::compileText(source);
    MPL_CHECK(!compilation.hasErrors());
    const auto& meters = compilation.production.meterMap;
    MPL_CHECK_EQ(meters.size(), std::size_t{2});
    MPL_CHECK(meters[0].meter.toString() == std::string("4/4"));
    MPL_CHECK(meters[1].position == Position(Rational(2, 1)));
    MPL_CHECK(meters[1].meter.toString() == std::string("7/8"));
    MPL_CHECK(meters[1].grouping.toString() == std::string("2+2+3"));
    MPL_CHECK(compilation.production.logicalExtent == Duration(Rational(15, 4)));
}

MPL_TEST(SectionDurationMustBeBarAligned) {
    const std::string source =
        mpltest::header("4/4") +
        "let M = note(C4, 1/8, 90);\n"
        "track Piano { asset = PianoAsset; role = melodic; }\n"
        "section A(3/8) { clip Piano { play M; } }\n"
        "arrangement { play A; }\n"
        "export midi(\"x.mid\");\n";
    const FrontendResult result = mpltest::frontend(source);
    MPL_CHECK(mpltest::hasCode(result, "section-not-bar-aligned"));
}

MPL_TEST(ClipBarsResolveAgainstSectionMeter) {
    const std::string source =
        mpltest::header("7/8") +
        "let M = note(C4, 1/8, 90);\n"
        "track Piano { asset = PianoAsset; role = melodic; }\n"
        "section A(2 bars) meter 7/8 grouping 2+2+3 {\n"
        "  clip Piano \"two\" at 2 bars { play M; }\n"
        "  clip Piano \"quarter\" at 1/4 bars { play M; }\n"
        "}\n"
        "arrangement { play A; }\n"
        "export midi(\"x.mid\");\n";
    const Compilation compilation = mpltest::compileText(source);
    const auto& clips = compilation.production.tracks.front().clips;
    MPL_CHECK_EQ(clips.size(), std::size_t{2});
    MPL_CHECK(clips[0].start == Position(Rational(7, 4)));
    MPL_CHECK(clips[1].start == Position(Rational(7, 32)));
}

MPL_TEST(MidiExportsEveryMeterEvent) {
    const Compilation compilation = mpltest::compileText(threeSections());
    const std::vector<MidiEvent> meters =
        mpltest::conductorEvents(compilation, MidiEventKind::MetaTimeSignature);
    MPL_CHECK_EQ(meters.size(), std::size_t{3});
    MPL_CHECK_EQ(meters[0].tick, std::int64_t{0});
    MPL_CHECK_EQ(meters[0].a, 7);
    MPL_CHECK_EQ(meters[0].b, 8);
    MPL_CHECK_EQ(meters[1].tick, std::int64_t{3360});
    MPL_CHECK_EQ(meters[1].a, 5);
    MPL_CHECK_EQ(meters[1].b, 8);
    MPL_CHECK_EQ(meters[2].tick, std::int64_t{5760});
    MPL_CHECK_EQ(meters[2].a, 4);
    MPL_CHECK_EQ(meters[2].b, 4);
    MPL_CHECK(meters[0].text == std::string("2+2+3"));
    MPL_CHECK(meters[1].text == std::string("2+3"));
}

MPL_TEST(TraceRecordsMeterMapAndGrouping) {
    const Compilation compilation = mpltest::compileText(threeSections());
    const std::string trace = compilation.toTraceJson(false);
    MPL_CHECK(trace.find("\"meterMap\"") != std::string::npos);
    MPL_CHECK(trace.find("\"grouping\": \"2+2+3\"") != std::string::npos);
    MPL_CHECK(trace.find("\"barIndex\": 2") != std::string::npos);
}

MPL_TEST(SectionMeterSeparatesFunctionCacheContexts) {
    const std::string source =
        mpltest::header("4/4") +
        "fn cell() -> Fragment context meter { return note(C4, 2 beats, 90); }\n"
        "track Piano { asset = PianoAsset; role = melodic; }\n"
        "section A(1 bars) meter 4/4 { clip Piano \"A\" { play cell(); } }\n"
        "section B(1 bars) meter 7/8 grouping 2+2+3 {\n"
        "  clip Piano \"B\" { play cell(); }\n"
        "}\n"
        "arrangement { play A; play B; }\n"
        "export midi(\"x.mid\");\n";
    const Compilation compilation = mpltest::compileText(source);
    MPL_CHECK(!compilation.hasErrors());
    const auto& clips = compilation.production.tracks.front().clips;
    MPL_CHECK_EQ(clips.size(), std::size_t{2});
    MPL_CHECK(clips[0].content.notes().front().duration == Duration(Rational(1, 2)));
    MPL_CHECK(clips[1].content.notes().front().duration == Duration(Rational(2, 8)));
    bool sawA = false;
    bool sawB = false;
    for (const FunctionCallRecord& call : compilation.functionCalls) {
        sawA = sawA || call.metricContext.find("section:0") != std::string::npos;
        sawB = sawB || call.metricContext.find("section:1") != std::string::npos;
    }
    MPL_CHECK(sawA);
    MPL_CHECK(sawB);
}

MPL_TEST(SyncopateUsesExplicitSectionMeter) {
    const std::string source =
        mpltest::header("4/4") +
        "fn push() -> Fragment context meter {\n"
        "  let a = note(C4, 1/8, 90);\n"
        "  return syncopate(sequence(rest(1/8), a), 1/16);\n"
        "}\n"
        "track Piano { asset = PianoAsset; role = melodic; }\n"
        "section A(1 bars) meter 7/8 grouping 2+2+3 { clip Piano \"A\" { play push(); } }\n"
        "section B(1 bars) meter 4/4 { clip Piano \"B\" { play push(); } }\n"
        "arrangement { play A; play B; }\n"
        "export midi(\"x.mid\");\n";
    const Compilation compilation = mpltest::compileText(source);
    const auto& clips = compilation.production.tracks.front().clips;
    MPL_CHECK_EQ(clips.size(), std::size_t{2});
    // 1/8 is a beat in 7/8 but not in 4/4.
    MPL_CHECK(clips[0].content.notes().front().offset == Duration(Rational(3, 16)));
    MPL_CHECK(clips[1].content.notes().front().offset == Duration(Rational(1, 8)));
}
