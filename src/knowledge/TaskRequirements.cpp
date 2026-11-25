#include "mpl/knowledge/TaskRequirements.hpp"

#include "mpl/observe/Observations.hpp"
#include "mpl/util/JsonWriter.hpp"

#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <string>

namespace mpl {
namespace {

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::vector<std::string> typesForObservation(const std::string& name) {
    static const std::map<std::string, std::vector<std::string>> types = {
        {"logical_extent", {"Duration"}}, {"content_extent", {"Duration"}},
        {"event_count", {"Int"}},          {"onset_set", {"Position"}},
        {"onset_density", {"Number"}},     {"pitch_sequence", {"Pitch"}},
        {"interval_sequence", {"Int"}},    {"normalized_rhythm", {"Rhythm"}},
        {"harmonic_timeline", {"Harmony"}}, {"fragment_range", {"Fragment"}},
        {"voice_movement", {"Voicing"}},   {"module_dependencies", {"Module"}},
        {"track_symbolic_events", {"Note"}}, {"sample_peak", {"Audio"}},
        {"true_peak", {"Audio"}},          {"integrated_loudness", {"Audio"}},
    };
    const auto found = types.find(name);
    return found == types.end() ? std::vector<std::string>{} : found->second;
}

}  // namespace

TaskRequirementSet analyzeTaskRequirements(const Task& task,
                                           const std::string& baselineSource) {    TaskRequirementSet requirements;
    requirements.taskId = task.id;
    requirements.category = task.category;
    requirements.budget = task.budget;
    for (const ObservationExpectation& expectation : task.target) {
        requirements.targetObservations.push_back(expectation.name);
        if (findObservationSpec(expectation.name) == nullptr) {
            requirements.unknown.push_back("target observation '" + expectation.name + "'");
        }
        for (const std::string& type : typesForObservation(expectation.name)) {
            requirements.requiredTypes.push_back(type);
        }
    }
    for (const ObservationExpectation& expectation : task.preserve) {
        requirements.preserveObservations.push_back(expectation.name);
        if (findObservationSpec(expectation.name) == nullptr) {
            requirements.unknown.push_back("preserve observation '" + expectation.name + "'");
        }
        for (const std::string& type : typesForObservation(expectation.name)) {
            requirements.requiredTypes.push_back(type);
        }
    }
    for (const FrameRule& rule : task.frame.allow) {
        requirements.frameObjects.push_back(rule.objectType);
        requirements.frameProperties.push_back(rule.property);
        requirements.allowedEffects.push_back(rule.objectType + "." + rule.property);
    }
    for (const FrameRule& rule : task.frame.deny) {
        requirements.frameObjects.push_back(rule.objectType);
        requirements.frameProperties.push_back(rule.property);
    }
    const FrameCoverage coverage = frameCoverage();
    for (const std::string& object : requirements.frameObjects) {
        if (object != "*" &&
            std::find(coverage.objectTypes.begin(), coverage.objectTypes.end(), object) ==
                coverage.objectTypes.end()) {
            requirements.unsupported.push_back("frame object '" + object + "'");
        }
    }
    for (std::size_t index = 0; index < requirements.frameObjects.size(); ++index) {
        const std::string property =
            requirements.frameObjects[index] + "." + requirements.frameProperties[index];
        if (requirements.frameProperties[index] != "*" &&
            requirements.frameObjects[index] != "*" &&
            std::find(coverage.properties.begin(), coverage.properties.end(), property) ==
                coverage.properties.end()) {
            requirements.unsupported.push_back("frame property '" + property + "'");
        }
    }

    const std::string source = lower(baselineSource);
    const std::vector<std::pair<std::string, std::string>> capabilities = {
        {"rhythm", "core:rhythm"}, {"voicing", "core:voicing"},
        {"voicelead", "core:voiceLead"}, {"chord", "core:chord"},
        {"harmony", "core:harmony"}, {"automation", "core:automation"},
        {"processor", "core:processor"}, {"section", "core:section"},
        {"track", "core:track"}, {"import", "modules"},
    };
    for (const auto& [needle, capability] : capabilities) {
        if (source.find(needle) != std::string::npos) {
            requirements.requiredCapabilities.push_back(capability);
        }
    }
    if (source.find("import \"std/") != std::string::npos) {
        requirements.requiredCapabilities.push_back("stdlib");
    }

    auto unique = [](std::vector<std::string>& values) {
        std::sort(values.begin(), values.end());
        values.erase(std::unique(values.begin(), values.end()), values.end());
    };
    unique(requirements.targetObservations);
    unique(requirements.preserveObservations);
    unique(requirements.frameObjects);
    unique(requirements.frameProperties);
    unique(requirements.allowedEffects);
    unique(requirements.requiredTypes);
    unique(requirements.requiredCapabilities);
    unique(requirements.unknown);
    unique(requirements.unsupported);
    requirements.valid = requirements.unknown.empty() && requirements.unsupported.empty();
    return requirements;
}

std::string TaskRequirementSet::toJson() const {
    JsonWriter json;
    json.beginObject();
    json.key("format").value("mpl.task-requirements");
    json.key("version").value(1);
    json.key("taskId").value(taskId);
    json.key("category").value(category);
    json.key("valid").value(valid);
    const auto array = [&](const char* key, const std::vector<std::string>& values) {
        json.key(key).beginArray();
        for (const std::string& value : values) json.value(value);
        json.endArray();
    };
    array("targetObservations", targetObservations);
    array("preserveObservations", preserveObservations);
    array("allowedEffects", allowedEffects);
    array("requiredTypes", requiredTypes);
    array("requiredCapabilities", requiredCapabilities);
    array("unknown", unknown);
    array("unsupported", unsupported);
    json.key("budget").beginObject();
    json.key("maxRepairs").value(budget.maxRepairs);
    json.key("maxSeconds").value(budget.maxSeconds);
    json.key("maxTokens").value(budget.maxTokens);
    json.endObject();
    json.endObject();
    return json.take();
}

}  // namespace mpl
