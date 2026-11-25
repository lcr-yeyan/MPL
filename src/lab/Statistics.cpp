#include "mpl/lab/Statistics.hpp"

#include "mpl/util/JsonValue.hpp"
#include "mpl/util/JsonWriter.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <map>
#include <numeric>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace mpl {
namespace {

std::string readText(const std::string& path) {
    std::ifstream stream(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(stream),
                       std::istreambuf_iterator<char>());
}

std::uint64_t nextRandom(std::uint64_t& state) {
    state += 0x9e3779b97f4a7c15ull;
    std::uint64_t value = state;
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ull;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebull;
    return value ^ (value >> 31);
}

double mean(const std::vector<double>& values) {
    if (values.empty()) {
        return 0.0;
    }
    return std::accumulate(values.begin(), values.end(), 0.0) /
           static_cast<double>(values.size());
}

struct Sample {
    bool finalPass = false;
    bool firstPass = false;
    int inputTokens = 0;
    int outputTokens = 0;
    double latency = 0.0;
};

}  // namespace

std::pair<double, double> pairedBootstrapConfidenceInterval(
    const std::vector<double>& pairedDifferences, int iterations, double confidence,
    std::uint64_t seed) {
    if (pairedDifferences.empty()) {
        return {0.0, 0.0};
    }
    std::vector<double> means;
    means.reserve(static_cast<std::size_t>(iterations));
    std::uint64_t state = seed;
    for (int iteration = 0; iteration < iterations; ++iteration) {
        double sum = 0.0;
        for (std::size_t index = 0; index < pairedDifferences.size(); ++index) {
            const std::size_t pick =
                static_cast<std::size_t>(nextRandom(state) % pairedDifferences.size());
            sum += pairedDifferences[pick];
        }
        means.push_back(sum / static_cast<double>(pairedDifferences.size()));
    }
    std::sort(means.begin(), means.end());
    const double alpha = (1.0 - confidence) / 2.0;
    const std::size_t low =
        static_cast<std::size_t>(alpha * static_cast<double>(means.size() - 1));
    const std::size_t high = static_cast<std::size_t>(
        (1.0 - alpha) * static_cast<double>(means.size() - 1));
    return {means[low], means[high]};
}

std::string analyzeExperimentResults(const std::vector<std::string>& resultPaths) {
    std::map<std::string, std::map<std::string, Sample>> samples;
    std::map<std::string, std::map<std::string, int>> statusCounts;
    std::map<std::string, std::map<std::string, int>> tokenSums;
    std::map<std::string, std::map<std::string, int>> latencyCounts;
    std::map<std::string, std::map<std::string, double>> latencySums;

    for (const std::string& path : resultPaths) {
        JsonValue root;
        std::string error;
        if (!parseJson(readText(path), root, error) || !root.isObject()) {
            continue;
        }
        const JsonValue* experiment = root.find("experimentId");
        const JsonValue* task = root.find("taskId");
        const std::string experimentId =
            experiment != nullptr && experiment->isString() ? experiment->text : path;
        const std::string taskId = task != nullptr && task->isString() ? task->text : "";
        const JsonValue* runs = root.find("runs");
        if (runs == nullptr || !runs->isArray()) {
            continue;
        }
        for (const JsonValue& run : runs->array) {
            if (!run.isObject()) {
                continue;
            }
            const JsonValue* arm = run.find("arm");
            const JsonValue* status = run.find("status");
            if (arm == nullptr || !arm->isString() || status == nullptr ||
                !status->isString()) {
                continue;
            }
            const std::string armName = arm->text;
            statusCounts[armName][status->text]++;
            const std::string key = experimentId + "|" + taskId;
            const bool finalPass = status->text == "pass";
            if (status->text != "pass" && status->text != "fail") {
                continue;
            }
            Sample sample;
            sample.finalPass = finalPass;
            sample.firstPass = finalPass;
            if (const JsonValue* initial = run.find("initialStatus");
                initial != nullptr && initial->isString() &&
                (initial->text == "pass" || initial->text == "fail")) {
                sample.firstPass = initial->text == "pass";
            }
            if (const JsonValue* tokens = run.find("inputTokens");
                tokens != nullptr && tokens->isNumber()) {
                sample.inputTokens = static_cast<int>(tokens->number);
            }
            if (const JsonValue* tokens = run.find("outputTokens");
                tokens != nullptr && tokens->isNumber()) {
                sample.outputTokens = static_cast<int>(tokens->number);
            }
            if (const JsonValue* latency = run.find("latencyMilliseconds");
                latency != nullptr && latency->isNumber()) {
                sample.latency = latency->number;
            }
            tokenSums[armName]["input"] += sample.inputTokens;
            tokenSums[armName]["output"] += sample.outputTokens;
            latencySums[armName]["total"] += sample.latency;
            latencyCounts[armName]["count"]++;
            // Multiple repetitions of the same task instance are keyed
            // separately; they remain paired across arms when repetitions
            // share the same experiment/task/arm sequence.
            std::string uniqueKey = key;
            int suffix = 1;
            while (samples[uniqueKey].count(armName) != 0) {
                uniqueKey = key + "#" + std::to_string(suffix++);
            }
            samples[uniqueKey][armName] = sample;
        }
    }

    JsonWriter json;
    json.beginObject();
    json.key("format").value("mpl.experiment-statistics");
    json.key("version").value(1);
    json.key("note").value(
        "Descriptive statistics only; pilot data are not confirmatory and no model "
        "success is claimed when no model calls were made.");
    json.key("arms").beginArray();
    const auto countOf = [](const std::map<std::string, int>& counts,
                            const std::string& key) {
        const auto found = counts.find(key);
        return found == counts.end() ? 0 : found->second;
    };
    for (const auto& [arm, counts] : statusCounts) {
        json.beginObject();
        json.key("arm").value(arm);
        json.key("runs").value(countOf(counts, "pass") + countOf(counts, "fail") +
                                countOf(counts, "unverifiable") +
                                countOf(counts, "not-applicable") +
                                countOf(counts, "invalid-arm") + countOf(counts, "error") +
                                countOf(counts, "not-run") + countOf(counts, "unsupported"));
        json.key("pass").value(countOf(counts, "pass"));
        json.key("fail").value(countOf(counts, "fail"));
        json.key("unverifiable").value(countOf(counts, "unverifiable"));
        json.key("notApplicable").value(countOf(counts, "not-applicable"));
        json.key("invalidArm").value(countOf(counts, "invalid-arm"));
        json.key("error").value(countOf(counts, "error"));
        json.key("inputTokens").value(tokenSums[arm]["input"]);
        json.key("outputTokens").value(tokenSums[arm]["output"]);
        const int latencyCount = latencyCounts[arm]["count"];
        json.key("meanLatencyMilliseconds")
            .value(latencyCount == 0 ? 0.0 : latencySums[arm]["total"] / latencyCount);
        json.endObject();
    }
    json.endArray();

    std::vector<std::string> arms;
    for (const auto& [arm, counts] : statusCounts) {
        (void)counts;
        arms.push_back(arm);
    }
    std::sort(arms.begin(), arms.end());
    json.key("paired").beginArray();
    for (std::size_t left = 0; left < arms.size(); ++left) {
        for (std::size_t right = left + 1; right < arms.size(); ++right) {
            std::vector<double> finalDifferences;
            std::vector<double> firstDifferences;
            for (const auto& [key, entry] : samples) {
                (void)key;
                const auto a = entry.find(arms[left]);
                const auto b = entry.find(arms[right]);
                if (a == entry.end() || b == entry.end()) {
                    continue;
                }
                finalDifferences.push_back((a->second.finalPass ? 1.0 : 0.0) -
                                           (b->second.finalPass ? 1.0 : 0.0));
                firstDifferences.push_back((a->second.firstPass ? 1.0 : 0.0) -
                                           (b->second.firstPass ? 1.0 : 0.0));
            }
            if (finalDifferences.empty()) {
                continue;
            }
            const auto finalCi = pairedBootstrapConfidenceInterval(finalDifferences);
            const auto firstCi = pairedBootstrapConfidenceInterval(firstDifferences);
            json.beginObject();
            json.key("armA").value(arms[left]);
            json.key("armB").value(arms[right]);
            json.key("n").value(static_cast<std::int64_t>(finalDifferences.size()));
            json.key("finalMeanDifference").value(mean(finalDifferences));
            json.key("finalCiLow").value(finalCi.first);
            json.key("finalCiHigh").value(finalCi.second);
            json.key("firstMeanDifference").value(mean(firstDifferences));
            json.key("firstCiLow").value(firstCi.first);
            json.key("firstCiHigh").value(firstCi.second);
            json.endObject();
        }
    }
    json.endArray();
    json.endObject();
    return json.take();
}

}  // namespace mpl
