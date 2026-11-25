#include "TestSupport.hpp"

#include "mpl/lab/Statistics.hpp"

#include <fstream>
#include <string>

using namespace mpl;

MPL_TEST(BootstrapConfidenceIntervalIsDeterministic) {
    const std::vector<double> differences = {1.0, 1.0, 0.0, 1.0};
    const auto first = pairedBootstrapConfidenceInterval(differences);
    const auto second = pairedBootstrapConfidenceInterval(differences);
    MPL_CHECK_EQ(first.first, second.first);
    MPL_CHECK_EQ(first.second, second.second);
    MPL_CHECK(first.first <= 0.75 + 1e-9);
    MPL_CHECK(first.second >= 0.75 - 1e-9);
    const auto empty = pairedBootstrapConfidenceInterval({});
    MPL_CHECK_EQ(empty.first, 0.0);
    MPL_CHECK_EQ(empty.second, 0.0);
}

MPL_TEST(StatisticsDoesNotCountUnverifiableAsSuccess) {
    const std::string path =
        (std::filesystem::path(MPL_TEST_OUTPUT_DIR) / "stats-fixture.json").generic_string();
    std::ofstream(path, std::ios::binary)
        << R"({
          "format":"mpl.experiment-result","version":2,
          "experimentId":"stats","taskId":"task-1","modelGenerated":false,
          "runs":[
            {"arm":"A0","status":"pass","inputTokens":0,"outputTokens":0},
            {"arm":"B0","status":"unverifiable","inputTokens":0,"outputTokens":0},
            {"arm":"C0","status":"fail","inputTokens":0,"outputTokens":0}
          ]
        })";
    const std::string json = analyzeExperimentResults({path});
    MPL_CHECK(json.find("\"arm\": \"A0\"") != std::string::npos);
    MPL_CHECK(json.find("\"pass\": 1") != std::string::npos);
    MPL_CHECK(json.find("\"unverifiable\": 1") != std::string::npos);
    MPL_CHECK(json.find("\"finalMeanDifference\"") != std::string::npos);
}
