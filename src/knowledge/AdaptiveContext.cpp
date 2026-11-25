#include "mpl/knowledge/AdaptiveContext.hpp"

#include "mpl/util/JsonWriter.hpp"
#include "mpl/util/Sha256.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <sstream>
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

std::vector<std::string> splitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }
    return lines;
}

std::string knowledgeText(const KnowledgeUnit& unit) {
    std::ostringstream stream;
    stream << "## Knowledge " << unit.knowledgeId << " v" << unit.version << "\n";
    stream << "Category: " << unit.category << "; kind: " << unit.kind << "\n";
    stream << unit.description << "\n";
    if (!unit.inputTypes.empty()) {
        stream << "Inputs:";
        for (const std::string& type : unit.inputTypes) stream << " " << type;
        stream << "\n";
    }
    if (!unit.outputTypes.empty()) {
        stream << "Outputs:";
        for (const std::string& type : unit.outputTypes) stream << " " << type;
        stream << "\n";
    }
    stream << "Implementation: " << unit.implementationRef << "\n";
    for (const std::string& precondition : unit.preconditions) {
        stream << "Precondition: " << precondition << "\n";
    }
    for (const std::string& postcondition : unit.postconditions) {
        stream << "Postcondition: " << postcondition << "\n";
    }
    for (const std::string& invariant : unit.invariants) {
        stream << "Invariant: " << invariant << "\n";
    }
    for (const std::string& effect : unit.allowedEffects) {
        stream << "Allowed effect: " << effect << "\n";
    }
    for (const KnowledgeExample& example : unit.examples) {
        stream << "Example (" << example.role << "): " << example.program;
        if (!example.description.empty()) stream << " — " << example.description;
        stream << "\n";
    }
    return stream.str();
}

std::string taskConstraints(const Task& task, bool heldout) {
    std::ostringstream stream;
    stream << "# Task Contract\n";
    stream << "Task: " << task.id << " (" << task.category << ")\n";
    stream << "Allowed scope: " << task.allowedScope << "\n";
    stream << "Target observations:";
    for (const ObservationExpectation& expectation : task.target) {
        stream << " " << expectation.name;
    }
    stream << "\nPreserve observations:";
    for (const ObservationExpectation& expectation : task.preserve) {
        stream << " " << expectation.name;
    }
    stream << "\n";
    if (task.frame.specified) {
        stream << "Frame allow:";
        for (const FrameRule& rule : task.frame.allow) {
            stream << " " << rule.objectType << "." << rule.property;
        }
        stream << "\nFrame deny:";
        for (const FrameRule& rule : task.frame.deny) {
            stream << " " << rule.objectType << "." << rule.property;
        }
        stream << "\n";
    }
    stream << "Tolerance: " << task.tolerance << "\n";
    if (heldout) {
        stream << "Held-out: expected reference values are not included.\n";
    }
    return stream.str();
}

}  // namespace

std::string_view contextModeName(ContextMode mode) {
    switch (mode) {
        case ContextMode::Full: return "full";
        case ContextMode::Domain: return "domain";
        case ContextMode::Adaptive: return "adaptive";
    }
    return "adaptive";
}

ContextBuildReport buildTaskContext(const Task& task,
                                    const KnowledgeResolution& resolution,
                                    const KnowledgeRegistry& registry,
                                    const ContextBuildOptions& options) {
    const auto start = std::chrono::steady_clock::now();
    ContextBuildReport report;
    report.taskId = task.id;
    report.mode = options.mode;
    report.domain = options.domain;

    const std::string foundationPath =
        options.foundationPath.empty()
            ? (options.mode == ContextMode::Full
                   ? "lcp/v1.1/contexts/Core.md"
                   : "lcp/v1.1/contexts/L1.md")
            : options.foundationPath;
    const std::string fullPath =
        options.fullPath.empty()
            ? (options.includeStdlib ? "lcp/v1.1/contexts/C.md"
                                     : "lcp/v1.1/contexts/Core.md")
            : options.fullPath;
    const std::string foundation = readText(foundationPath);
    if (foundation.empty()) {
        report.diagnostics.push_back("missing foundation context '" + foundationPath + "'");
        report.budgetStatus = "insufficient-budget";
        return report;
    }

    std::vector<std::string> parts;
    std::set<std::string> seenLines;
    auto append = [&](const std::string& text, const std::string& id,
                      const std::string& kind, bool mandatory) {
        std::vector<std::string> uniqueLines;
        for (const std::string& line : splitLines(text)) {
            if (line.empty()) {
                continue;
            }
            if (!seenLines.insert(line).second) {
                ++report.duplicateLines;
                continue;
            }
            uniqueLines.push_back(line);
        }
        std::ostringstream joined;
        for (const std::string& line : uniqueLines) {
            joined << line << "\n";
        }
        const std::string value = joined.str();
        if (value.empty()) {
            return;
        }
        parts.push_back(value);
        ContextSource source;
        source.id = id;
        source.kind = kind;
        source.mandatory = mandatory;
        source.bytes = value.size();
        source.sha256 = sha256Hex(value);
        report.sources.push_back(std::move(source));
    };

    if (options.mode == ContextMode::Full) {
        const std::string full = readText(fullPath);
        if (full.empty()) {
            report.diagnostics.push_back("missing full context '" + fullPath + "'");
            report.budgetStatus = "insufficient-budget";
            return report;
        }
        append(full, "lcp.full", "foundation", true);
        std::vector<const KnowledgeUnit*> all;
        for (const KnowledgeUnit& unit : registry.units()) {
            all.push_back(&unit);
        }
        std::sort(all.begin(), all.end(),
                  [](const KnowledgeUnit* lhs, const KnowledgeUnit* rhs) {
                      return lhs->knowledgeId < rhs->knowledgeId;
                  });
        for (const KnowledgeUnit* unit : all) {
            append(knowledgeText(*unit), "knowledge:" + unit->knowledgeId,
                   "knowledge", false);
            report.selectedKnowledge.push_back(unit->knowledgeId);
        }
    } else {
        append(foundation, "lcp.foundation", "foundation", true);
        std::set<std::string> selected;
        if (options.mode == ContextMode::Domain && !options.domain.empty()) {
            for (const KnowledgeUnit* unit : registry.byCategory(options.domain)) {
                selected.insert(unit->knowledgeId);
            }
        }
        for (const KnowledgeMatch& match : resolution.matches) {
            if (match.applicability == KnowledgeApplicability::Inapplicable) {
                report.excludedKnowledge.push_back(match.knowledgeId + ": inapplicable");
                continue;
            }
            if (match.applicability == KnowledgeApplicability::Unknown) {
                report.excludedKnowledge.push_back(match.knowledgeId + ": unknown");
                continue;
            }
            bool relevant = false;
            for (const std::string& reason : match.reasons) {
                relevant = relevant || reason == "target observation overlap" ||
                           reason == "preserve observation overlap" ||
                           reason == "category matches domain";
            }
            if (options.mode == ContextMode::Adaptive && !relevant) {
                report.excludedKnowledge.push_back(match.knowledgeId +
                                                   ": not task-relevant");
                continue;
            }
            if (options.mode == ContextMode::Adaptive &&
                selected.size() >= options.maxKnowledge &&
                selected.count(match.knowledgeId) == 0) {
                report.excludedKnowledge.push_back(match.knowledgeId +
                                                   ": adaptive knowledge cap");
                continue;
            }
            if (!options.domain.empty()) {
                const KnowledgeUnit* unit = registry.resolve(match.knowledgeId, match.version);
                if (unit != nullptr && unit->category != options.domain &&
                    options.mode == ContextMode::Domain) {
                    report.excludedKnowledge.push_back(match.knowledgeId + ": other domain");
                    continue;
                }
            }
            selected.insert(match.knowledgeId);
        }
        std::set<std::string> closure;
        for (const std::string& id : selected) {
            std::vector<const KnowledgeUnit*> dependencies;
            std::string error;
            if (registry.dependencyClosure(id, dependencies, error)) {
                for (const KnowledgeUnit* unit : dependencies) {
                    closure.insert(unit->knowledgeId);
                }
            } else {
                report.diagnostics.push_back(error);
            }
        }
        std::vector<std::string> ordered(closure.begin(), closure.end());
        std::sort(ordered.begin(), ordered.end());
        for (const std::string& id : ordered) {
            const KnowledgeUnit* unit = registry.resolve(id);
            if (unit == nullptr) {
                continue;
            }
            append(knowledgeText(*unit), "knowledge:" + id, "knowledge", false);
            report.selectedKnowledge.push_back(id);
            report.dependencies.push_back(id);
        }
    }

    append(taskConstraints(task, options.heldout), "task.contract", "task", true);

    std::ostringstream content;
    for (const std::string& part : parts) {
        content << part << "\n---\n";
    }
    report.content = content.str();
    report.bytes = report.content.size();
    report.estimatedTokens = (report.bytes + 3) / 4;
    report.contextHash = sha256Hex(report.content);

    if (options.tokenBudget > 0 && report.estimatedTokens > options.tokenBudget) {
        report.overflow = true;
        if (options.mode == ContextMode::Adaptive && options.allowFullFallback) {
            ContextBuildOptions fallback = options;
            fallback.mode = ContextMode::Full;
            const ContextBuildReport full =
                buildTaskContext(task, resolution, registry, fallback);
            if (!full.content.empty()) {
                report = full;
                report.fallbackUsed = true;
                report.diagnostics.push_back("adaptive budget overflow; full context fallback");
                return report;
            }
        }
        report.budgetStatus = "insufficient-budget";
        report.diagnostics.push_back(
            "context estimated tokens exceed budget; mandatory content not removed");
    } else {
        report.budgetStatus = options.tokenBudget > 0 ? "ok" : "unlimited";
    }

    if (options.heldout) {
        for (const ObservationExpectation& expectation : task.target) {
            const std::string expected = expectation.expected.isString()
                                             ? expectation.expected.text
                                             : expectation.expected.canonical();
            if (expected.size() >= 8 &&
                report.content.find(expected) != std::string::npos) {
                report.heldoutLeakageBlocked = true;
                report.diagnostics.push_back(
                    "held-out expected value detected in context");
                report.budgetStatus = "leakage-blocked";
                break;
            }
        }
    }

    const auto end = std::chrono::steady_clock::now();
    report.elapsedMilliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    return report;
}

std::string ContextBuildReport::toJson() const {
    JsonWriter json;
    json.beginObject();
    json.key("format").value("mpl.context-build");
    json.key("version").value(1);
    json.key("taskId").value(taskId);
    json.key("mode").value(std::string(contextModeName(mode)));
    json.key("domain").value(domain);
    json.key("contextHash").value(contextHash);
    json.key("bytes").value(static_cast<std::int64_t>(bytes));
    json.key("estimatedTokens").value(static_cast<std::int64_t>(estimatedTokens));
    json.key("budgetStatus").value(budgetStatus);
    json.key("fallbackUsed").value(fallbackUsed);
    json.key("overflow").value(overflow);
    json.key("heldoutLeakageBlocked").value(heldoutLeakageBlocked);
    json.key("duplicateLines").value(static_cast<std::int64_t>(duplicateLines));
    json.key("elapsedMilliseconds").value(elapsedMilliseconds);
    json.key("selectedKnowledge").beginArray();
    for (const std::string& id : selectedKnowledge) json.value(id);
    json.endArray();
    json.key("excludedKnowledge").beginArray();
    for (const std::string& id : excludedKnowledge) json.value(id);
    json.endArray();
    json.key("dependencies").beginArray();
    for (const std::string& id : dependencies) json.value(id);
    json.endArray();
    json.key("sources").beginArray();
    for (const ContextSource& source : sources) {
        json.beginObject();
        json.key("id").value(source.id);
        json.key("kind").value(source.kind);
        json.key("mandatory").value(source.mandatory);
        json.key("bytes").value(static_cast<std::int64_t>(source.bytes));
        json.key("sha256").value(source.sha256);
        json.endObject();
    }
    json.endArray();
    json.key("diagnostics").beginArray();
    for (const std::string& diagnostic : diagnostics) json.value(diagnostic);
    json.endArray();
    json.endObject();
    return json.take();
}

}  // namespace mpl
