#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace mpl {

/// Deterministic paired bootstrap confidence interval for the mean difference.
/// Returns {low, high}; returns {0,0} for an empty sample.
std::pair<double, double> pairedBootstrapConfidenceInterval(
    const std::vector<double>& pairedDifferences, int iterations = 2000,
    double confidence = 0.95, std::uint64_t seed = 1);

/// Reads one or more experiment-result JSON files and emits per-arm summaries
/// plus pairwise paired differences. Only pass/fail pairs enter paired
/// comparisons; unverifiable/not-applicable/invalid/error runs are reported
/// separately and never counted as success.
std::string analyzeExperimentResults(const std::vector<std::string>& resultPaths);

}  // namespace mpl
