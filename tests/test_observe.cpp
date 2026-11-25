#include "TestSupport.hpp"

#include "mpl/observe/Observations.hpp"

#include <filesystem>
#include <string>

using namespace mpl;

namespace {

Compilation compileMrp() {
    const std::filesystem::path path =
        std::filesystem::path(MPL_EXAMPLES_DIR) / "math-rock.mpl";
    return mpltest::compileText(mpltest::readText(path), path.generic_string());
}

}  // namespace

MPL_TEST(ObservationRegistryDeclaresAllRequiredObservations) {
    const std::vector<ObservationSpec>& registry = observationRegistry();
    MPL_CHECK_EQ(registry.size(), std::size_t{16});
    for (const ObservationSpec& spec : registry) {
        MPL_CHECK(!spec.name.empty());
        MPL_CHECK(!spec.inputType.empty());
        MPL_CHECK(!spec.scope.empty());
        MPL_CHECK(!spec.unit.empty());
        MPL_CHECK(!spec.semantics.empty());
        MPL_CHECK(!spec.conditions.empty());
        MPL_CHECK(!spec.id.empty());
        MPL_CHECK_EQ(spec.version, 1);
        MPL_CHECK(!spec.outputType.empty());
        MPL_CHECK(!spec.preconditions.empty());
        MPL_CHECK(!spec.comparisonPolicy.empty());
        MPL_CHECK(!spec.verificationStatus.empty());
        MPL_CHECK(findObservationSpec(spec.name) != nullptr);
    }
    MPL_CHECK(observationKindName(ObservationKind::Exact) == std::string_view("exact"));
}

MPL_TEST(VoiceMovementHasIndependentReferenceVerifier) {
    const std::vector<std::vector<Pitch>> voicings = {
        {Pitch::parse("D3"), Pitch::parse("F3"), Pitch::parse("A3"), Pitch::parse("C4")},
        {Pitch::parse("D3"), Pitch::parse("F3"), Pitch::parse("G3"), Pitch::parse("B3")},
    };
    const VoiceMovementReference reference = verifyVoiceMovementReference(voicings);
    MPL_CHECK(reference.ok);
    MPL_CHECK_EQ(reference.totalMovement, 3);
    const ObservationValue observation = observeVoiceMovement(voicings);
    MPL_CHECK(observation.status == std::string("exact"));
    MPL_CHECK(observation.jsonValue.find("\"totalMovement\": 3") != std::string::npos);
    const std::vector<std::vector<Pitch>> mismatched = {
        {Pitch::parse("C4")}, {Pitch::parse("C4"), Pitch::parse("E4")}};
    MPL_CHECK(!verifyVoiceMovementReference(mismatched).ok);
    MPL_CHECK(observeVoiceMovement(mismatched).status == std::string("unverifiable"));
}

MPL_TEST(ObservationValuesFromMrpAreExactAndUnverifiableWhereRequired) {
    const Compilation compilation = compileMrp();
    const ObservationSet observations = observeCompilation(compilation);
    MPL_CHECK_EQ(observations.values.size(), std::size_t{16});
    const ObservationValue* logical = observations.find("logical_extent");
    MPL_CHECK(logical != nullptr);
    if (logical != nullptr) {
        MPL_CHECK(logical->status == std::string("exact"));
        MPL_CHECK(logical->jsonValue.find("10") != std::string::npos);
    }
    const ObservationValue* events = observations.find("event_count");
    MPL_CHECK(events != nullptr);
    if (events != nullptr) {
        MPL_CHECK(events->jsonValue.find("247") != std::string::npos);
    }
    const ObservationValue* modules = observations.find("module_dependencies");
    MPL_CHECK(modules != nullptr);
    if (modules != nullptr) {
        MPL_CHECK(modules->jsonValue.find("StdHarmony") != std::string::npos);
    }
    const ObservationValue* voice = observations.find("voice_movement");
    MPL_CHECK(voice != nullptr);
    if (voice != nullptr) {
        MPL_CHECK(voice->status == std::string("unverifiable"));
        MPL_CHECK(!voice->available);
    }
    const ObservationValue* peak = observations.find("sample_peak");
    MPL_CHECK(peak != nullptr);
    if (peak != nullptr) {
        MPL_CHECK(peak->status == std::string("unverifiable"));
    }
    const std::string json = observations.toJson();
    MPL_CHECK(json.find("\"format\": \"mpl.observations\"") != std::string::npos);
}

MPL_TEST(ObservationSetIsDeterministic) {
    const Compilation compilation = compileMrp();
    const ObservationSet first = observeCompilation(compilation);
    const ObservationSet second = observeCompilation(compilation);
    MPL_CHECK_EQ(first.toJson(), second.toJson());
}
