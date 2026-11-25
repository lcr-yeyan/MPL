#include "mpl/knowledge/KnowledgeResolver.hpp"

#include "mpl/util/JsonWriter.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <string>

namespace mpl {
namespace {

bool contains(const std::vector<std::string>& values, const std::string& value) {
    return std::find(values.begin(), values.end(), value) != values.end();
}

int overlap(const std::vector<std::string>& lhs, const std::vector<std::string>& rhs) {
    int count = 0;
    for (const std::string& value : lhs) {
        count += contains(rhs, value) ? 1 : 0;
    }
    return count;
}

}  // namespace

std::string_view knowledgeApplicabilityName(KnowledgeApplicability value) {
    switch (value) {
        case KnowledgeApplicability::Applicable: return "applicable";
        case KnowledgeApplicability::ConditionallyApplicable: return "conditional";
        case KnowledgeApplicability::Inapplicable: return "inapplicable";
        case KnowledgeApplicability::Unknown: return "unknown";
    }
    return "unknown";
}

KnowledgeResolution resolveKnowledge(const TaskRequirementSet& requirements,
                                     const KnowledgeRegistry& registry,
                                     const std::string& domain, std::size_t maxMatches) {
    KnowledgeResolution resolution;
    resolution.taskId = requirements.taskId;
    resolution.requirementsValid = requirements.valid;
    resolution.unknown = requirements.unknown;
    resolution.unsupported = requirements.unsupported;
    for (const auto& unit : registry.units()) {
        KnowledgeMatch match;
        match.knowledgeId = unit.knowledgeId;
        match.version = unit.version;
        int score = 0;
        const int targetOverlap = overlap(unit.observationRefs, requirements.targetObservations);
        const int preserveOverlap =
            overlap(unit.observationRefs, requirements.preserveObservations);
        const int typeOverlap = overlap(unit.inputTypes, requirements.requiredTypes) +
                                overlap(unit.outputTypes, requirements.requiredTypes);
        score += targetOverlap * 4;
        score += preserveOverlap * 2;
        score += typeOverlap;
        if (!domain.empty() && unit.category == domain) {
            score += 3;
            match.reasons.push_back("category matches domain");
        }
        if (targetOverlap > 0) {
            match.reasons.push_back("target observation overlap");
        }
        if (preserveOverlap > 0) {
            match.reasons.push_back("preserve observation overlap");
        }
        if (typeOverlap > 0) {
            match.reasons.push_back("input/output type overlap");
        }
        bool effectConflict = false;
        if (!requirements.allowedEffects.empty() && !unit.allowedEffects.empty()) {
            const int effectOverlap = overlap(unit.allowedEffects, requirements.allowedEffects);
            if (effectOverlap == 0) {
                effectConflict = true;
                match.reasons.push_back("no allowed-effect overlap");
            } else {
                score += effectOverlap * 2;
                match.reasons.push_back("allowed-effect overlap");
            }
        }
        for (const std::string& dependency : unit.dependencies) {
            if (registry.resolve(dependency) == nullptr) {
                match.missingDependencies.push_back(dependency);
            }
        }
        if (!match.missingDependencies.empty()) {
            match.applicability = KnowledgeApplicability::Unknown;
            match.reasons.push_back("missing dependency");
            match.score = score;
            resolution.matches.push_back(std::move(match));
            continue;
        }
        if (unit.kind == "practice") {
            match.applicability = KnowledgeApplicability::ConditionallyApplicable;
            match.reasons.push_back("advisory practice knowledge; never a hard constraint");
        } else if (unit.verificationStatus == "unsupported" ||
                   unit.verificationStatus == "deferred") {
            match.applicability = KnowledgeApplicability::Inapplicable;
            match.reasons.push_back("unsupported/deferred knowledge");
        } else if (targetOverlap == 0 && preserveOverlap == 0 && typeOverlap == 0 &&
                   (domain.empty() || unit.category != domain)) {
            match.applicability = KnowledgeApplicability::Inapplicable;
            match.reasons.push_back("no requirement overlap");
        } else if (unit.verificationStatus != "verified") {
            match.applicability = KnowledgeApplicability::ConditionallyApplicable;
            match.reasons.push_back("knowledge is unverified");
        } else {
            match.applicability = KnowledgeApplicability::Applicable;
        }
        if (effectConflict) {
            match.applicability = KnowledgeApplicability::Inapplicable;
        }        match.score = score;
        resolution.matches.push_back(std::move(match));
    }
    std::sort(resolution.matches.begin(), resolution.matches.end(),
              [](const KnowledgeMatch& lhs, const KnowledgeMatch& rhs) {
                  if (lhs.score != rhs.score) {
                      return lhs.score > rhs.score;
                  }
                  return lhs.knowledgeId < rhs.knowledgeId;
              });
    if (resolution.matches.size() > maxMatches) {
        for (std::size_t index = maxMatches; index < resolution.matches.size(); ++index) {
            resolution.unresolved.push_back("excluded by maxMatches: " +
                                            resolution.matches[index].knowledgeId);
        }
        resolution.matches.resize(maxMatches);
    }
    return resolution;
}

std::string KnowledgeResolution::toJson() const {
    JsonWriter json;
    json.beginObject();
    json.key("format").value("mpl.knowledge-resolution");
    json.key("version").value(1);
    json.key("taskId").value(taskId);
    json.key("scoringVersion").value(scoringVersion);
    json.key("requirementsValid").value(requirementsValid);
    json.key("unknown").beginArray();
    for (const std::string& item : unknown) json.value(item);
    json.endArray();
    json.key("unsupported").beginArray();
    for (const std::string& item : unsupported) json.value(item);
    json.endArray();
    json.key("matches").beginArray();
    for (const KnowledgeMatch& match : matches) {
        json.beginObject();
        json.key("knowledgeId").value(match.knowledgeId);
        json.key("version").value(match.version);
        json.key("applicability").value(std::string(knowledgeApplicabilityName(match.applicability)));
        json.key("score").value(match.score);
        json.key("reasons").beginArray();
        for (const std::string& reason : match.reasons) {
            json.value(reason);
        }
        json.endArray();
        json.key("missingDependencies").beginArray();
        for (const std::string& dependency : match.missingDependencies) {
            json.value(dependency);
        }
        json.endArray();
        json.endObject();
    }
    json.endArray();
    json.key("unresolved").beginArray();
    for (const std::string& item : unresolved) {
        json.value(item);
    }
    json.endArray();
    json.endObject();
    return json.take();
}

}  // namespace mpl
