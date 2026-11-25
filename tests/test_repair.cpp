#include "TestSupport.hpp"

#include "mpl/lab/ModelAdapter.hpp"
#include "mpl/lab/ModelAdapters.hpp"
#include "mpl/lab/Repair.hpp"

#include <filesystem>
#include <string>
#include <vector>

using namespace mpl;

namespace {

std::string basicSource() {
    return mpltest::readText(
        std::filesystem::path(MPL_BENCH_DIR) / "baselines" / "basic.mpl");
}
std::string editedSource() {
    std::string source = basicSource();
    source.replace(source.find("F4 @ 3/4"), 8, "G4 @ 3/4");
    return source;
}

class ScriptedAdapter : public ModelAdapter {
public:
    explicit ScriptedAdapter(std::vector<std::string> responses)
        : responses_(std::move(responses)) {}
    std::string name() const override { return "scripted"; }
    ModelResponse generate(const ModelRequest& request) override {
        lastFeedback = request.semanticFeedback;
        ModelResponse response;
        if (index_ >= responses_.size()) {
            response.ok = false;
            response.error = "script exhausted";
            return response;
        }
        response.ok = true;
        response.candidateSource = responses_[index_++];
        response.model = "scripted";
        response.modelVersion = "1";
        response.inputTokens = 10;
        response.outputTokens = 20;
        response.modelCalls = 1;
        return response;
    }
    std::string lastFeedback;

private:
    std::vector<std::string> responses_;
    std::size_t index_ = 0;
};

std::string taskPath() {
    return (std::filesystem::path(MPL_BENCH_DIR) / "tasks" / "10-local-edit.json")
        .generic_string();
}

RepairBudget budget(int attempts) {
    RepairBudget value;
    value.maxAttempts = attempts;
    return value;
}

}  // namespace

MPL_TEST(StaticModelAdapterSupportsOfflineImports) {
    StaticModelAdapter adapter(editedSource(), "offline-fixture");
    const ModelResponse response = adapter.generate(ModelRequest{});
    MPL_CHECK(response.ok);
    MPL_CHECK_EQ(response.model, std::string("offline-fixture"));
    MPL_CHECK_EQ(response.modelCalls, 0);
    MPL_CHECK(response.candidateSource.find("G4 @ 3/4") != std::string::npos);
}

MPL_TEST(BoundedRepairUsesFeedbackAndStopsOnSuccess) {
    ScriptedAdapter adapter({editedSource()});
    ModelResponse initial;
    initial.ok = true;
    initial.candidateSource = basicSource();
    initial.modelCalls = 1;
    initial.inputTokens = 100;
    initial.outputTokens = 50;
    const RepairResult result =
        runRepairLoop(adapter, Task{}, taskPath(), initial, budget(2), false);
    MPL_CHECK(result.success);
    MPL_CHECK_EQ(result.attempts.size(), std::size_t{2});
    MPL_CHECK_EQ(result.totalModelCalls, 2);
    MPL_CHECK(!adapter.lastFeedback.empty());
    MPL_CHECK(adapter.lastFeedback.find("targetStatus") != std::string::npos);
    MPL_CHECK(result.toJson().find("\"format\": \"mpl.repair-result\"") !=
              std::string::npos);
}

MPL_TEST(BoundedRepairRedactsExpectedValues) {
    ScriptedAdapter adapter({editedSource()});
    ModelResponse initial;
    initial.ok = true;
    initial.candidateSource = basicSource();
    const RepairResult result =
        runRepairLoop(adapter, Task{}, taskPath(), initial, budget(2), true);
    MPL_CHECK(result.success);
    MPL_CHECK(adapter.lastFeedback.find("<redacted>") != std::string::npos);
    MPL_CHECK(adapter.lastFeedback.find("[[60, 62, 64, 67]]") == std::string::npos);
}

MPL_TEST(BoundedRepairReportsBudgetExhaustion) {
    ScriptedAdapter adapter({basicSource()});
    ModelResponse initial;
    initial.ok = true;
    initial.candidateSource = basicSource();
    const RepairResult result =
        runRepairLoop(adapter, Task{}, taskPath(), initial, budget(2), false);
    MPL_CHECK(!result.success);
    MPL_CHECK(result.failure == RepairFailure::BudgetExhausted);
    MPL_CHECK_EQ(result.attempts.size(), std::size_t{2});
}

MPL_TEST(BoundedRepairClassifiesModelFailure) {
    ScriptedAdapter adapter({});
    ModelResponse initial;
    initial.ok = false;
    initial.error = "provider unavailable";
    const RepairResult result =
        runRepairLoop(adapter, Task{}, taskPath(), initial, budget(2), false);
    MPL_CHECK(result.failure == RepairFailure::ModelFailure);
    MPL_CHECK(result.toJson().find("model-failure") != std::string::npos);
}
