#include "mpl/knowledge/Knowledge.hpp"

#include "mpl/util/JsonWriter.hpp"
#include "mpl/util/Sha256.hpp"

#include <algorithm>
#include <fstream>
#include <functional>
#include <iterator>
#include <set>
#include <string>
#include <utility>

namespace mpl {
namespace {

std::string readText(const std::string& path) {
    std::ifstream stream(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(stream),
                       std::istreambuf_iterator<char>());
}

bool stringArray(const JsonValue* value, std::vector<std::string>& out,
                 std::string& error, const char* field) {
    if (value == nullptr) {
        return true;
    }
    if (!value->isArray()) {
        error = std::string(field) + " must be an array";
        return false;
    }
    for (const JsonValue& entry : value->array) {
        if (!entry.isString()) {
            error = std::string(field) + " entries must be strings";
            return false;
        }
        out.push_back(entry.text);
    }
    return true;
}

std::string stringField(const JsonValue& object, const char* name,
                        const std::string& fallback = {}) {
    const JsonValue* value = object.find(name);
    return value != nullptr && value->isString() ? value->text : fallback;
}

std::vector<std::string> sortedStrings(std::vector<std::string> values) {
    std::sort(values.begin(), values.end());
    return values;
}

std::string keyOf(const KnowledgeUnit& unit) {
    return unit.knowledgeId + "@" + unit.version;
}

bool validStatus(const std::string& status) {
    return status == "verified" || status == "unverified" || status == "unsupported" ||
           status == "deferred" || status == "failed";
}

void writeStringArray(JsonWriter& json, const char* key,
                      const std::vector<std::string>& values) {
    json.key(key).beginArray();
    for (const std::string& value : values) {
        json.value(value);
    }
    json.endArray();
}

}  // namespace

std::string knowledgeKindName(const std::string& kind) {
    return kind;
}

bool knowledgeKindIsHardConstraint(const std::string& kind) {
    return kind != "practice";
}

std::string knowledgeCanonicalJson(const KnowledgeUnit& unit, bool includeHashes) {
    JsonWriter json;
    json.beginObject();
    json.key("knowledgeId").value(unit.knowledgeId);
    json.key("version").value(unit.version);
    json.key("category").value(unit.category);
    json.key("kind").value(unit.kind);
    json.key("referenceKind").value(unit.referenceKind);
    json.key("description").value(unit.description);
    writeStringArray(json, "inputTypes", unit.inputTypes);
    writeStringArray(json, "outputTypes", unit.outputTypes);
    writeStringArray(json, "parameters", unit.parameters);
    writeStringArray(json, "preconditions", unit.preconditions);
    writeStringArray(json, "postconditions", unit.postconditions);
    writeStringArray(json, "invariants", unit.invariants);
    writeStringArray(json, "allowedEffects", unit.allowedEffects);
    writeStringArray(json, "requiredContext", unit.requiredContext);
    writeStringArray(json, "dependencies", unit.dependencies);
    json.key("implementationRef").value(unit.implementationRef);
    writeStringArray(json, "observationRefs", unit.observationRefs);
    json.key("examples").beginArray();
    for (const KnowledgeExample& example : unit.examples) {
        json.beginObject();
        json.key("role").value(example.role);
        json.key("program").value(example.program);
        json.key("description").value(example.description);
        json.endObject();
    }
    json.endArray();
    json.key("checks").beginArray();
    for (const KnowledgeCheck& check : unit.checks) {
        json.beginObject();
        json.key("phase").value(check.phase);
        json.key("program").value(check.program);
        json.key("observation").value(check.observation);
        json.key("comparator").value(check.comparator);
        json.key("expected").value(check.expected.canonical());
        json.key("tolerance").value(check.tolerance);
        json.key("note").value(check.note);
        json.endObject();
    }
    json.endArray();
    json.key("source").value(unit.source);
    json.key("license").value(unit.license);
    json.key("verificationStatus").value(unit.verificationStatus);
    json.key("unsupportedReason").value(unit.unsupportedReason);
    writeStringArray(json, "tags", unit.tags);
    if (includeHashes) {
        json.key("contentHash").value(unit.contentHash);
        json.key("resolvedHash").value(unit.resolvedHash);
    }
    json.endObject();
    return json.take();
}

bool KnowledgeRegistry::loadCatalog(const std::string& path, std::string& error) {
    const std::string text = readText(path);
    if (text.empty()) {
        error = "cannot read knowledge catalog '" + path + "'";
        return false;
    }
    return loadCatalogText(text, error);
}

bool KnowledgeRegistry::loadCatalogText(std::string_view text, std::string& error) {
    JsonValue root;
    std::string parseError;
    if (!parseJson(text, root, parseError) || !root.isObject()) {
        error = "invalid knowledge catalog JSON: " + parseError;
        return false;
    }
    if (stringField(root, "format") != "mpl.knowledge-catalog") {
        error = "knowledge catalog format must be mpl.knowledge-catalog";
        return false;
    }
    const JsonValue* units = root.find("units");
    if (units == nullptr || !units->isArray()) {
        error = "knowledge catalog requires units array";
        return false;
    }
    units_.clear();
    byKey_.clear();
    categoryIndex_.clear();
    typeIndex_.clear();
    observationIndex_.clear();
    tagIndex_.clear();
    errors_.clear();
    for (const JsonValue& entry : units->array) {
        if (!entry.isObject()) {
            error = "knowledge unit must be an object";
            return false;
        }
        KnowledgeUnit unit;
        unit.knowledgeId = stringField(entry, "knowledgeId");
        unit.version = stringField(entry, "version", "1");
        unit.category = stringField(entry, "category");
        unit.kind = stringField(entry, "kind");
        unit.referenceKind = stringField(entry, "referenceKind");
        if (unit.referenceKind.empty()) {
            const std::size_t colon = unit.implementationRef.find(':');
            const std::string prefix = colon == std::string::npos
                                           ? "callable"
                                           : unit.implementationRef.substr(0, colon);
            unit.referenceKind = prefix == "core"     ? "callable"
                                 : prefix == "stdlib" ? "stdlib"
                                 : prefix == "syntax" ? "syntax"
                                 : prefix == "observation" ? "observation"
                                 : prefix == "task" ? "task"
                                 : prefix == "benchmark" ? "benchmark"
                                 : prefix == "deferred" ? "deferred"
                                                        : "callable";
        }
        unit.description = stringField(entry, "description");
        unit.implementationRef = stringField(entry, "implementationRef");
        unit.source = stringField(entry, "source");
        unit.license = stringField(entry, "license");
        unit.verificationStatus = stringField(entry, "verificationStatus", "unverified");
        unit.unsupportedReason = stringField(entry, "unsupportedReason");
        if (unit.knowledgeId.empty() || unit.category.empty() || unit.kind.empty()) {
            error = "knowledge unit requires knowledgeId, category and kind";
            return false;
        }
        if (!validStatus(unit.verificationStatus)) {
            error = "invalid verificationStatus for '" + unit.knowledgeId + "'";
            return false;
        }
        if (!stringArray(entry.find("inputTypes"), unit.inputTypes, error, "inputTypes") ||
            !stringArray(entry.find("outputTypes"), unit.outputTypes, error, "outputTypes") ||
            !stringArray(entry.find("parameters"), unit.parameters, error, "parameters") ||
            !stringArray(entry.find("preconditions"), unit.preconditions, error,
                         "preconditions") ||
            !stringArray(entry.find("postconditions"), unit.postconditions, error,
                         "postconditions") ||
            !stringArray(entry.find("invariants"), unit.invariants, error, "invariants") ||
            !stringArray(entry.find("allowedEffects"), unit.allowedEffects, error,
                         "allowedEffects") ||
            !stringArray(entry.find("requiredContext"), unit.requiredContext, error,
                         "requiredContext") ||
            !stringArray(entry.find("dependencies"), unit.dependencies, error,
                         "dependencies") ||
            !stringArray(entry.find("observationRefs"), unit.observationRefs, error,
                         "observationRefs") ||
            !stringArray(entry.find("tags"), unit.tags, error, "tags")) {
            return false;
        }
        if (const JsonValue* examples = entry.find("examples");
            examples != nullptr && examples->isArray()) {
            for (const JsonValue& exampleValue : examples->array) {
                if (!exampleValue.isObject()) {
                    error = "knowledge example must be an object";
                    return false;
                }
                KnowledgeExample example;
                example.role = stringField(exampleValue, "role", "legal");
                example.program = stringField(exampleValue, "program");
                example.description = stringField(exampleValue, "description");
                if (example.program.empty()) {
                    error = "knowledge example requires program";
                    return false;
                }
                unit.examples.push_back(std::move(example));
            }
        }
        if (const JsonValue* checks = entry.find("checks");
            checks != nullptr && checks->isArray()) {
            for (const JsonValue& checkValue : checks->array) {
                if (!checkValue.isObject()) {
                    error = "knowledge check must be an object";
                    return false;
                }
                KnowledgeCheck check;
                check.phase = stringField(checkValue, "phase", "postcondition");
                check.program = stringField(checkValue, "program");
                check.observation = stringField(checkValue, "observation");
                check.comparator = stringField(checkValue, "comparator", "equals");
                check.note = stringField(checkValue, "note");
                check.tolerance = 0.0;
                if (const JsonValue* tolerance = checkValue.find("tolerance");
                    tolerance != nullptr && tolerance->isNumber()) {
                    check.tolerance = tolerance->number;
                }
                if (const JsonValue* expected = checkValue.find("expected")) {
                    check.expected = *expected;
                }
                if (check.program.empty() || check.observation.empty()) {
                    error = "knowledge check requires program and observation";
                    return false;
                }
                unit.checks.push_back(std::move(check));
            }
        }
        if (!registerUnit(std::move(unit), error)) {
            return false;
        }
    }
    if (!recomputeHashes(error)) {
        return false;
    }
    return true;
}

bool KnowledgeRegistry::registerUnit(KnowledgeUnit unit, std::string& error) {
    const std::string key = keyOf(unit);
    if (byKey_.find(key) != byKey_.end()) {
        error = "duplicate knowledge id/version '" + key + "'";
        errors_.push_back(KnowledgeLoadError{"knowledge-duplicate", error, unit.knowledgeId});
        return false;
    }
    unit.contentHash = sha256Hex(knowledgeCanonicalJson(unit, false));
    byKey_[key] = units_.size();
    const std::size_t index = units_.size();
    categoryIndex_[unit.category].push_back(index);
    typeIndex_[unit.kind].push_back(index);
    for (const std::string& type : unit.inputTypes) {
        typeIndex_[type].push_back(index);
    }
    for (const std::string& type : unit.outputTypes) {
        typeIndex_[type].push_back(index);
    }
    for (const std::string& observation : unit.observationRefs) {
        observationIndex_[observation].push_back(index);
    }
    for (const std::string& tag : unit.tags) {
        tagIndex_[tag].push_back(index);
    }
    units_.push_back(std::move(unit));
    return true;
}

const KnowledgeUnit* KnowledgeRegistry::resolve(const std::string& id,
                                               const std::string& version) const {
    if (!version.empty()) {
        const auto found = byKey_.find(id + "@" + version);
        return found == byKey_.end() ? nullptr : &units_[found->second];
    }
    for (const KnowledgeUnit& unit : units_) {
        if (unit.knowledgeId == id) {
            return &unit;
        }
    }
    return nullptr;
}

std::vector<const KnowledgeUnit*> KnowledgeRegistry::byCategory(
    const std::string& category) const {
    std::vector<const KnowledgeUnit*> result;
    const auto found = categoryIndex_.find(category);
    if (found == categoryIndex_.end()) {
        return result;
    }
    for (const std::size_t index : found->second) {
        result.push_back(&units_[index]);
    }
    std::sort(result.begin(), result.end(),
              [](const KnowledgeUnit* lhs, const KnowledgeUnit* rhs) {
                  return keyOf(*lhs) < keyOf(*rhs);
              });
    return result;
}

std::vector<const KnowledgeUnit*> KnowledgeRegistry::byType(const std::string& type) const {
    std::vector<const KnowledgeUnit*> result;
    const auto found = typeIndex_.find(type);
    if (found == typeIndex_.end()) {
        return result;
    }
    for (const std::size_t index : found->second) {
        result.push_back(&units_[index]);
    }
    std::sort(result.begin(), result.end(),
              [](const KnowledgeUnit* lhs, const KnowledgeUnit* rhs) {
                  return keyOf(*lhs) < keyOf(*rhs);
              });
    return result;
}

std::vector<const KnowledgeUnit*> KnowledgeRegistry::byObservation(
    const std::string& observation) const {
    std::vector<const KnowledgeUnit*> result;
    const auto found = observationIndex_.find(observation);
    if (found == observationIndex_.end()) {
        return result;
    }
    for (const std::size_t index : found->second) {
        result.push_back(&units_[index]);
    }
    std::sort(result.begin(), result.end(),
              [](const KnowledgeUnit* lhs, const KnowledgeUnit* rhs) {
                  return keyOf(*lhs) < keyOf(*rhs);
              });
    return result;
}

std::vector<const KnowledgeUnit*> KnowledgeRegistry::byTag(const std::string& tag) const {
    std::vector<const KnowledgeUnit*> result;
    const auto found = tagIndex_.find(tag);
    if (found == tagIndex_.end()) {
        return result;
    }
    for (const std::size_t index : found->second) {
        result.push_back(&units_[index]);
    }
    std::sort(result.begin(), result.end(),
              [](const KnowledgeUnit* lhs, const KnowledgeUnit* rhs) {
                  return keyOf(*lhs) < keyOf(*rhs);
              });
    return result;
}

bool KnowledgeRegistry::dependencyClosure(const std::string& id,
                                          std::vector<const KnowledgeUnit*>& out,
                                          std::string& error) const {
    const KnowledgeUnit* root = resolve(id);
    if (root == nullptr) {
        error = "unknown knowledge id '" + id + "'";
        return false;
    }
    std::map<std::string, int> state;
    std::function<bool(const KnowledgeUnit&)> visit = [&](const KnowledgeUnit& unit) {
        const std::string key = keyOf(unit);
        if (state[key] == 2) {
            return true;
        }
        if (state[key] == 1) {
            error = "cyclic knowledge dependency at '" + key + "'";
            return false;
        }
        state[key] = 1;
        for (const std::string& dependency : unit.dependencies) {
            const KnowledgeUnit* child = resolve(dependency);
            if (child == nullptr) {
                error = "missing knowledge dependency '" + dependency + "' for '" + key + "'";
                return false;
            }
            if (!visit(*child)) {
                return false;
            }
        }
        state[key] = 2;
        out.push_back(&unit);
        return true;
    };
    return visit(*root);
}

std::vector<std::string> KnowledgeRegistry::missingDependencies(
    const std::string& id) const {
    std::vector<std::string> missing;
    std::set<std::string> visited;
    std::function<void(const KnowledgeUnit&)> visit = [&](const KnowledgeUnit& unit) {
        if (!visited.insert(keyOf(unit)).second) {
            return;
        }
        for (const std::string& dependency : unit.dependencies) {
            const KnowledgeUnit* child = resolve(dependency);
            if (child == nullptr) {
                missing.push_back(dependency);
            } else {
                visit(*child);
            }
        }
    };
    if (const KnowledgeUnit* root = resolve(id)) {
        visit(*root);
    }
    std::sort(missing.begin(), missing.end());
    missing.erase(std::unique(missing.begin(), missing.end()), missing.end());
    return missing;
}

bool KnowledgeRegistry::recomputeHashes(std::string& error) {
    std::map<std::string, int> state;
    for (KnowledgeUnit& unit : units_) {
        unit.resolvedHash.clear();
    }
    std::function<bool(KnowledgeUnit&)> visit = [&](KnowledgeUnit& unit) -> bool {
        const std::string key = keyOf(unit);
        if (state[key] == 2) {
            return true;
        }
        if (state[key] == 1) {
            error = "cyclic knowledge dependency at '" + key + "'";
            errors_.push_back(KnowledgeLoadError{"knowledge-cycle", error, unit.knowledgeId});
            return false;
        }
        state[key] = 1;
        std::string combined = unit.contentHash;
        for (const std::string& dependency : unit.dependencies) {
            const KnowledgeUnit* child = resolve(dependency);
            if (child == nullptr) {
                error = "missing knowledge dependency '" + dependency + "' for '" + key + "'";
                errors_.push_back(
                    KnowledgeLoadError{"knowledge-missing-dependency", error, unit.knowledgeId});
                return false;
            }
            auto& mutableChild = const_cast<KnowledgeUnit&>(*child);
            if (!visit(mutableChild)) {
                return false;
            }
            combined += "|" + mutableChild.resolvedHash;
        }
        unit.resolvedHash = sha256Hex(combined);
        state[key] = 2;
        return true;
    };
    for (KnowledgeUnit& unit : units_) {
        if (!visit(unit)) {
            return false;
        }
    }
    return true;
}

std::string KnowledgeRegistry::toJson() const {
    std::vector<const KnowledgeUnit*> sorted;
    for (const KnowledgeUnit& unit : units_) {
        sorted.push_back(&unit);
    }
    std::sort(sorted.begin(), sorted.end(),
              [](const KnowledgeUnit* lhs, const KnowledgeUnit* rhs) {
                  return keyOf(*lhs) < keyOf(*rhs);
              });
    JsonWriter json;
    json.beginObject();
    json.key("format").value("mpl.knowledge-registry");
    json.key("version").value(1);
    json.key("units").beginArray();
    for (const KnowledgeUnit* unit : sorted) {
        json.value(knowledgeCanonicalJson(*unit, true));
    }
    json.endArray();
    json.endObject();
    return json.take();
}

}  // namespace mpl
