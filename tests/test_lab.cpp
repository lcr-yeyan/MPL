#include "TestSupport.hpp"

#include "mpl/lab/Task.hpp"
#include "mpl/lab/Harness.hpp"
#include "mpl/lab/FlatEvents.hpp"
#include "mpl/util/JsonValue.hpp"

#include <filesystem>
#include <fstream>
#include <string>

using namespace mpl;

namespace {
const std::filesystem::path benchRoot = MPL_BENCH_DIR;
std::string taskPath(const char* name) {
    return (benchRoot / "tasks" / name).generic_string();
}
std::string baselinePath(const char* name) {
    return (benchRoot / "baselines" / name).generic_string();
}
}  // namespace

MPL_TEST(JsonValueParsesTaskShape) {
    JsonValue value;
    std::string error;
    MPL_CHECK(parseJson("{\"id\":\"x\",\"n\":3,\"a\":[1,2],\"ok\":true}", value, error));
    MPL_CHECK(value.isObject());
    MPL_CHECK(value.find("id") != nullptr && value.find("id")->isString());
    MPL_CHECK_EQ(value.find("n")->number, 3.0);
    MPL_CHECK_EQ(value.find("a")->array.size(), std::size_t{2});
    MPL_CHECK(value.find("ok")->boolean);
    MPL_CHECK(!parseJson("{", value, error));
}

MPL_TEST(FlatEventAdapterMatchesCommonObservations) {
    const std::string flat = R"({
      "format":"mpl.flat-events", "version":1,
      "logicalExtent":"1", "tempoBpm":120,
      "meterMap":[{"position":"0","numerator":4,"denominator":4,"grouping":"2+2"}],
      "tracks":[{"id":"Piano","events":[
        {"pitch":60,"onset":"0","duration":"1/4","velocity":96},
        {"pitch":62,"onset":"1/4","duration":"1/4","velocity":96},
        {"pitch":64,"onset":"1/2","duration":"1/4","velocity":96},
        {"pitch":65,"onset":"3/4","duration":"1/4","velocity":96}
      ]}]
    })";
    const std::string flatPath =
        (std::filesystem::path(MPL_TEST_OUTPUT_DIR) / "basic-flat.json").generic_string();
    std::ofstream(flatPath, std::ios::binary) << flat;
    MPL_CHECK_EQ(verifyFlatTaskFile(taskPath("01-basic-notes.json"), flatPath).status,
                 std::string("pass"));
    MPL_CHECK_EQ(verifyFlatTaskFile(taskPath("08-rhythm-replacement.json"), flatPath).status,
                 std::string("not-applicable"));
    // The harmony task cannot be evaluated from flat events without inventing
    // provenance; it is explicitly not-applicable.
    MPL_CHECK_EQ(verifyFlatTaskFile(taskPath("05-harmony-driven.json"), flatPath).status,
                 std::string("not-applicable"));
    const FlatParseResult parsed = parseFlatEvents(flat);
    MPL_CHECK(parsed.ok);
    MPL_CHECK(parsed.document.logicalExtent == Rational(1, 1));
    MPL_CHECK_EQ(parsed.document.tracks.front().events.size(), std::size_t{4});
    MPL_CHECK(flatSupportsObservation("event_count"));
    MPL_CHECK(!flatSupportsObservation("harmonic_timeline"));
}

MPL_TEST(FlatEventAdapterRejectsInexactTime) {
    const std::string flat = R"({
      "logicalExtent":"1", "tempoBpm":120,
      "tracks":[{"id":"P","events":[{"pitch":60,"onset":0.25,"duration":"1/4"}]}]
    })";
    const FlatParseResult parsed = parseFlatEvents(flat);
    MPL_CHECK(!parsed.ok);
}

MPL_TEST(TaskVerifierAcceptsBaselineAndLocalEdit) {
    const VerificationResult baseline = verifyTaskFile(
        taskPath("01-basic-notes.json"), baselinePath("basic.mpl"));
    MPL_CHECK_EQ(baseline.status, std::string("pass"));
    const VerificationResult edit = verifyTaskFile(
        taskPath("10-local-edit.json"),
        (benchRoot / "candidates" / "local-edit.mpl").generic_string());
    MPL_CHECK_EQ(edit.status, std::string("pass"));
    const std::string json = edit.toJson();
    MPL_CHECK(json.find("\"format\": \"mpl.task-verification\"") != std::string::npos);
}

MPL_TEST(TaskVerifierRejectsUnchangedCandidateForEditTask) {
    const VerificationResult result = verifyTaskFile(
        taskPath("10-local-edit.json"), baselinePath("basic.mpl"));
    MPL_CHECK_EQ(result.status, std::string("fail"));
}

MPL_TEST(TaskVerifierRejectsPreserveViolation) {
    const std::string source = mpltest::readText(
        std::filesystem::path(baselinePath("basic.mpl")));
    std::string changed = source;
    const std::string token = "F4 @ 3/4";
    const std::size_t position = changed.find(token);
    MPL_CHECK(position != std::string::npos);
    changed.replace(position, token.size(), "F4 @ 5/8");
    Task task;
    task.id = "scope-test";
    task.baselinePath = baselinePath("basic.mpl");
    ObservationExpectation target;
    target.name = "event_count";
    target.expected.type = JsonValue::Type::Number;
    target.expected.number = 4;
    target.hasExpected = true;
    task.target.push_back(target);
    ObservationExpectation preserve;
    preserve.name = "onset_set";
    task.preserve.push_back(preserve);
    const std::string file =
        (std::filesystem::path(MPL_TEST_OUTPUT_DIR) / "scope-edit.mpl").generic_string();
    std::ofstream(file, std::ios::binary) << changed;
    const VerificationResult result = verifyTaskSource(task, changed, file);
    MPL_CHECK_EQ(result.status, std::string("fail"));
}

MPL_TEST(TaskVerifierReportsUnverifiableAudioWithoutWav) {
    const VerificationResult result = verifyTaskFile(
        taskPath("12-mastering-target.json"), baselinePath("mastering.mpl"));
    MPL_CHECK_EQ(result.status, std::string("unverifiable"));
}

MPL_TEST(FrameContractRejectsRename) {
    std::string source = mpltest::readText(
        std::filesystem::path(baselinePath("basic.mpl")));
    source.replace(source.find("track Piano"), std::string("track Piano").size(),
                   "track Keys");
    source.replace(source.find("clip Piano"), std::string("clip Piano").size(),
                   "clip Keys");
    Task task;
    task.id = "frame-rename";
    task.baselinePath = baselinePath("basic.mpl");
    task.frame.specified = true;
    task.frame.allow.push_back(FrameRule{"clip", "*", "symbolicEvents"});
    ObservationExpectation target;
    target.name = "event_count";
    target.expected.type = JsonValue::Type::Number;
    target.expected.number = 4;
    target.hasExpected = true;
    task.target.push_back(target);
    const std::string file =
        (std::filesystem::path(MPL_TEST_OUTPUT_DIR) / "frame-rename.mpl").generic_string();
    std::ofstream(file, std::ios::binary) << source;
    const VerificationResult result = verifyTaskSource(task, source, file);
    MPL_CHECK_EQ(result.frameStatus, std::string("failed"));
    MPL_CHECK(result.status == std::string("fail"));
}

MPL_TEST(FrameContractRejectsAddedObject) {
    std::string source = mpltest::readText(
        std::filesystem::path(baselinePath("basic.mpl")));
    source.replace(source.find("track Piano"),
                   std::string("track Piano").size(),
                   "track Extra { asset = PianoAsset; role = melodic; }\n"
                   "track Piano");
    const std::string section = "section S(1 bars) { clip Piano \"Basic\" { play M; } }";
    source.replace(source.find(section), section.size(),
                   "section S(1 bars) { clip Piano \"Basic\" { play M; } "
                   "clip Extra \"X\" { play M; } }");
    Task task;
    task.id = "frame-added";
    task.baselinePath = baselinePath("basic.mpl");
    task.frame.specified = true;
    task.frame.allow.push_back(FrameRule{"clip", "*", "symbolicEvents"});
    ObservationExpectation target;
    target.name = "event_count";
    target.expected.type = JsonValue::Type::Number;
    target.expected.number = 8;
    target.hasExpected = true;
    task.target.push_back(target);
    const std::string file =
        (std::filesystem::path(MPL_TEST_OUTPUT_DIR) / "frame-added.mpl").generic_string();
    std::ofstream(file, std::ios::binary) << source;
    const VerificationResult result = verifyTaskSource(task, source, file);
    MPL_CHECK_EQ(result.frameStatus, std::string("failed"));
    bool sawAdded = false;
    for (const ObservationComparison& comparison : result.comparisons) {
        sawAdded = sawAdded || (comparison.status == "fail" &&
                                comparison.reason.find("unauthorized added") !=
                                    std::string::npos);
    }
    MPL_CHECK(sawAdded);
}

MPL_TEST(FrameContractRejectsDirectChangeWhenOnlyDependencyAllowed) {
    const std::string source = mpltest::readText(
        std::filesystem::path(baselinePath("basic.mpl")));
    std::string changed = source;
    changed.replace(changed.find("F4 @ 3/4"), 8, "G4 @ 3/4");
    Task task;
    task.id = "frame-cause-test";
    task.baselinePath = baselinePath("basic.mpl");
    ObservationExpectation target;
    target.name = "event_count";
    target.expected.type = JsonValue::Type::Number;
    target.expected.number = 4;
    target.hasExpected = true;
    task.target.push_back(target);
    task.frame.specified = true;
    task.frame.allow.push_back(FrameRule{"clip", "*", "symbolicEvents", "dependency"});
    const std::string file =
        (std::filesystem::path(MPL_TEST_OUTPUT_DIR) / "frame-cause.mpl").generic_string();
    std::ofstream(file, std::ios::binary) << changed;
    const VerificationResult result = verifyTaskSource(task, changed, file);
    MPL_CHECK_EQ(result.frameStatus, std::string("failed"));
}

MPL_TEST(FrameContractReportsUncoveredRule) {
    Task task;
    task.id = "frame-coverage-test";
    task.baselinePath = baselinePath("basic.mpl");
    ObservationExpectation target;
    target.name = "event_count";
    target.expected.type = JsonValue::Type::Number;
    target.expected.number = 4;
    target.hasExpected = true;
    task.target.push_back(target);
    task.frame.specified = true;
    task.frame.allow.push_back(FrameRule{"metadata", "*", "checksum"});
    const std::string source = mpltest::readText(
        std::filesystem::path(baselinePath("basic.mpl")));
    const std::string file =
        (std::filesystem::path(MPL_TEST_OUTPUT_DIR) / "frame-coverage.mpl").generic_string();
    std::ofstream(file, std::ios::binary) << source;
    const VerificationResult result = verifyTaskSource(task, source, file);
    MPL_CHECK_EQ(result.frameStatus, std::string("unverifiable"));
    MPL_CHECK_EQ(result.status, std::string("unverifiable"));
}

MPL_TEST(TaskVerifierRejectsScopeNoneChange) {
    Task task;
    task.id = "scope-none";
    task.baselinePath = baselinePath("basic.mpl");
    task.allowedScope = "none";
    ObservationExpectation target;
    target.name = "event_count";
    target.expected.type = JsonValue::Type::Number;
    target.expected.number = 4;
    target.hasExpected = true;
    task.target.push_back(target);
    const std::string source = mpltest::readText(
        std::filesystem::path(baselinePath("basic.mpl")));
    std::string changed = source;
    changed.replace(changed.find("C4 @ 0"), 6, "D4 @ 0");
    const std::string file =
        (std::filesystem::path(MPL_TEST_OUTPUT_DIR) / "scope-none.mpl").generic_string();
    std::ofstream(file, std::ios::binary) << changed;
    const VerificationResult result = verifyTaskSource(task, changed, file);
    MPL_CHECK_EQ(result.status, std::string("fail"));
}

MPL_TEST(FrameContractRejectsUnauthorizedClipEdit) {
    const std::string source = mpltest::readText(
        std::filesystem::path(baselinePath("basic.mpl")));
    std::string changed = source;
    changed.replace(changed.find("F4 @ 3/4"), 8, "G4 @ 3/4");
    Task task;
    task.id = "frame-test";
    task.baselinePath = baselinePath("basic.mpl");
    ObservationExpectation target;
    target.name = "event_count";
    target.expected.type = JsonValue::Type::Number;
    target.expected.number = 4;
    target.hasExpected = true;
    task.target.push_back(target);
    task.frame.specified = true;
    task.frame.allow.push_back(FrameRule{"track", "*", "instrument"});
    const std::string file =
        (std::filesystem::path(MPL_TEST_OUTPUT_DIR) / "frame-edit.mpl").generic_string();
    std::ofstream(file, std::ios::binary) << changed;
    const VerificationResult result = verifyTaskSource(task, changed, file);
    MPL_CHECK_EQ(result.status, std::string("fail"));
    MPL_CHECK_EQ(result.frameStatus, std::string("failed"));
    bool sawUnauthorized = false;
    for (const ObservationComparison& comparison : result.comparisons) {
        sawUnauthorized =
            sawUnauthorized ||
            (comparison.name.rfind("frame:", 0) == 0 && comparison.status == "fail" &&
             comparison.reason.find("before=") != std::string::npos);
    }
    MPL_CHECK(sawUnauthorized);
}

MPL_TEST(FrameContractAllowsDeclaredClipEdit) {
    const std::string source = mpltest::readText(
        std::filesystem::path(baselinePath("basic.mpl")));
    std::string changed = source;
    changed.replace(changed.find("F4 @ 3/4"), 8, "G4 @ 3/4");
    Task task;
    task.id = "frame-allow-test";
    task.baselinePath = baselinePath("basic.mpl");
    ObservationExpectation target;
    target.name = "event_count";
    target.expected.type = JsonValue::Type::Number;
    target.expected.number = 4;
    target.hasExpected = true;
    task.target.push_back(target);
    task.frame.specified = true;
    task.frame.allow.push_back(FrameRule{"clip", "*", "symbolicEvents"});
    const std::string file =
        (std::filesystem::path(MPL_TEST_OUTPUT_DIR) / "frame-allow.mpl").generic_string();
    std::ofstream(file, std::ios::binary) << changed;
    const VerificationResult result = verifyTaskSource(task, changed, file);
    MPL_CHECK_EQ(result.frameStatus, std::string("pass"));
    MPL_CHECK_EQ(result.status, std::string("pass"));
}

MPL_TEST(TaskParserRejectsUnknownObservation) {
    const std::string text = R"({
      "id":"bad", "baseline":"x.mpl",
      "target":[{"name":"not_an_observation","value":1}]
    })";
    const TaskParseResult parsed = parseTaskJson(text);
    MPL_CHECK(!parsed.ok);
    MPL_CHECK(parsed.error.find("unknown observation") != std::string::npos);
}

MPL_TEST(HarnessRejectsStdlibInMplCoreArm) {
    const std::string experimentPath =
        (std::filesystem::path(MPL_TEST_OUTPUT_DIR) / "core-arm.json").generic_string();
    const std::string task = taskPath("05-harmony-driven.json");
    const std::string candidate =
        (benchRoot / "candidates" / "harmony-replacement.mpl").generic_string();
    std::ofstream(experimentPath, std::ios::binary)
        << "{\"id\":\"core-arm\",\"task\":\"" << task << "\",\"submissions\":["
        << "{\"arm\":\"B0\",\"candidate\":\"" << candidate << "\",\"modelCalls\":0}]}";
    const ExperimentResult result = runExperiment(experimentPath);
    MPL_CHECK_EQ(result.runs.size(), std::size_t{1});
    if (!result.runs.empty()) {
        MPL_CHECK_EQ(result.runs.front().status, std::string("invalid-arm"));
    }
}

MPL_TEST(OfflineHarnessDoesNotClaimModelSuccess) {
    const std::string experiment =
        (benchRoot / "experiments" / "factorial-basic.json").generic_string();
    const ExperimentResult result = runExperiment(experiment);
    MPL_CHECK_EQ(result.experimentId, std::string("offline-factorial-basic"));
    MPL_CHECK_EQ(result.runs.size(), std::size_t{6});
    MPL_CHECK(!result.modelGenerated);
    const std::string json = result.toJson();
    MPL_CHECK(json.find("\"observedSuccessRate\": null") != std::string::npos);
    bool sawFlat = false;
    bool sawMpl = false;
    bool sawFeedback = false;
    for (const ExperimentRun& run : result.runs) {
        if (run.arm == "A0") {
            sawFlat = run.status == "pass";
        }
        if (run.arm == "B0") {
            sawMpl = run.status == "pass";
        }
        if (run.arm == "C1") {
            sawFeedback = run.feedback && run.status == "pass";
        }
        MPL_CHECK_EQ(run.modelCalls, 0);
    }
    MPL_CHECK(sawFlat);
    MPL_CHECK(sawMpl);
    MPL_CHECK(sawFeedback);
}

