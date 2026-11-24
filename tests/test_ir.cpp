#include "TestHarness.hpp"

#include "mpl/ir/MusicIR.hpp"

#include <string>
#include <vector>

using namespace mpl;
using namespace mpl::literals;

namespace {

ObjectId buildMotif(MusicIR& music) {
    return music.melody({
                            {"F4"_p, Rational(1, 2)},
                            {"Ab4"_p, Rational(1, 2)},
                            {"C5"_p, Rational(1, 1)},
                            {"Eb5"_p, Rational(1, 1)},
                            {"C5"_p, Rational(1, 1)},
                        },
                        "A");
}

}  // namespace

MPL_TEST(MusicIrEvaluatesDependencyChain) {
    MusicIR music;
    const ObjectId a = buildMotif(music);
    const ObjectId b = music.transpose(a, Interval::octaves(1), "B");
    const ObjectId c = music.reverse(a, "C");
    const ObjectId main = music.sequence({a, b, c, a}, "Main");

    MPL_CHECK_EQ(music.size(), std::size_t{4});
    MPL_CHECK_EQ(music.evaluatedCount(), std::size_t{0});

    music.evaluateAll();
    MPL_CHECK_EQ(music.evaluatedCount(), std::size_t{4});
    MPL_CHECK(music.node(main).value.has_value());
    MPL_CHECK(music.node(main).value->duration() == Duration(Rational(16, 1)));
    MPL_CHECK_EQ(music.node(b).value->notes().front().pitch.name(), std::string("F5"));
    MPL_CHECK_EQ(music.node(c).value->notes().front().pitch.name(), std::string("C5"));
}

MPL_TEST(MusicIrTracksDependentsAndInvalidates) {
    MusicIR music;
    const ObjectId a = buildMotif(music);
    const ObjectId b = music.transpose(a, Interval::octaves(1), "B");
    const ObjectId c = music.reverse(b, "C");
    const ObjectId independent = music.melody({{"C3"_p, Rational(1, 1)}}, "D");

    music.evaluateAll();
    MPL_CHECK_EQ(music.evaluatedCount(), std::size_t{4});

    const std::vector<ObjectId> dependents = music.dependents(a);
    MPL_CHECK_EQ(dependents.size(), std::size_t{2});
    MPL_CHECK_EQ(dependents[0], b);
    MPL_CHECK_EQ(dependents[1], c);

    music.invalidate(a);
    MPL_CHECK_EQ(music.evaluatedCount(), std::size_t{1});
    MPL_CHECK(!music.node(a).value.has_value());
    MPL_CHECK(!music.node(b).value.has_value());
    MPL_CHECK(!music.node(c).value.has_value());
    MPL_CHECK(music.node(independent).value.has_value());

    music.evaluateAll();
    MPL_CHECK_EQ(music.evaluatedCount(), std::size_t{4});
}

MPL_TEST(MusicIrEvaluationOrderIsTopological) {
    MusicIR music;
    const ObjectId a = buildMotif(music);
    const ObjectId b = music.transpose(a, Interval::octaves(1));
    const ObjectId c = music.sequence({a, b});

    const std::vector<ObjectId> order = music.evaluationOrder();
    MPL_CHECK_EQ(order.size(), std::size_t{3});
    MPL_CHECK_EQ(order[0], ObjectId{0});
    MPL_CHECK_EQ(order[1], ObjectId{1});
    MPL_CHECK_EQ(order[2], ObjectId{2});
    MPL_CHECK_EQ(c, ObjectId{2});
}

MPL_TEST(MusicIrRejectsUnknownInputsAndBadParameters) {
    MusicIR music;
    const ObjectId a = buildMotif(music);

    MPL_CHECK_THROWS(music.transpose(99, Interval::octaves(1)));
    MPL_CHECK_THROWS(music.reverse(kInvalidObjectId));
    MPL_CHECK_THROWS(music.stretch(a, Rational(0, 1)));
    MPL_CHECK_THROWS(music.repeat(a, 0));
    MPL_CHECK_THROWS(music.sequence({}));
    MPL_CHECK_THROWS(music.parallel({}));
}

MPL_TEST(MusicIrRecomputesDependentsAfterEdit) {
    MusicIR music;
    const ObjectId a = music.melody({{"F4"_p, Rational(1, 1)}}, "A");
    const ObjectId b = music.transpose(a, Interval::octaves(1), "B");
    music.evaluateAll();
    MPL_CHECK_EQ(music.node(b).value->notes().front().pitch.name(), std::string("F5"));

    music.updateLiteral(a, {Note{"A4"_p, Duration{}, Duration(Rational(1, 1)), 96}});
    MPL_CHECK(!music.node(a).value.has_value());
    MPL_CHECK(!music.node(b).value.has_value());

    music.evaluateAll();
    MPL_CHECK_EQ(music.node(a).value->notes().front().pitch.name(), std::string("A4"));
    MPL_CHECK_EQ(music.node(b).value->notes().front().pitch.name(), std::string("A5"));

    MPL_CHECK_THROWS(music.updateLiteral(b, {}));
}

MPL_TEST(MusicIrSerializesGraphStructure) {
    MusicIR music;
    const ObjectId a = buildMotif(music);
    const ObjectId b = music.transpose(a, Interval::octaves(1), "B");
    const ObjectId main = music.sequence({a, b}, "Main");
    music.evaluateAll();

    const std::string json = music.toJson();
    MPL_CHECK(json.find("\"format\": \"mpl.music-ir\"") != std::string::npos);
    MPL_CHECK(json.find("\"kind\": \"literal\"") != std::string::npos);
    MPL_CHECK(json.find("\"kind\": \"transpose\"") != std::string::npos);
    MPL_CHECK(json.find("\"kind\": \"sequence\"") != std::string::npos);
    MPL_CHECK(json.find("\"label\": \"Main\"") != std::string::npos);
    MPL_CHECK(json.find("\"intervalSemitones\": 12") != std::string::npos);
    MPL_CHECK(json.find("\"duration\": \"8\"") != std::string::npos);

    const ObjectId mainId = main;
    MPL_CHECK(json.find("\"id\": " + std::to_string(mainId)) != std::string::npos);
}
