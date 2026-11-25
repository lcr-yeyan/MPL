#include "TestSupport.hpp"

#include "mpl/observe/Observations.hpp"
#include "mpl/util/JsonValue.hpp"

#include <filesystem>
#include <string>

using namespace mpl;

MPL_TEST(Sma001SourcesCompileAndObserve) {
    const std::filesystem::path root =
        std::filesystem::path(MPL_BENCH_DIR) / "sma-001";
    JsonValue manifest;
    std::string error;
    MPL_CHECK(parseJson(mpltest::readText(root / "tasks.json"), manifest, error));
    MPL_CHECK(manifest.isObject());
    const JsonValue* tasks = manifest.find("tasks");
    MPL_CHECK(tasks != nullptr && tasks->isArray());
    if (tasks == nullptr || !tasks->isArray()) {
        return;
    }
    for (const JsonValue& task : tasks->array) {
        const JsonValue* source = task.find("source");
        const JsonValue* expected = task.find("observation");
        MPL_CHECK(source != nullptr && source->isString());
        if (source == nullptr || !source->isString()) {
            continue;
        }
        const FrontendResult frontend =
            mpltest::frontend(mpltest::readText(root / source->text),
                              (root / source->text).generic_string());
        MPL_CHECK(frontend.ok());
        if (!frontend.ok()) {
            continue;
        }
        const Compilation compilation = mpltest::compileText(
            mpltest::readText(root / source->text),
            (root / source->text).generic_string());
        const ObservationSet observations = observeCompilation(compilation);
        const ObservationValue* count = observations.find("event_count");
        MPL_CHECK(count != nullptr);
        if (expected != nullptr && expected->isObject()) {
            const JsonValue* wanted = expected->find("event_count");
            if (wanted != nullptr && wanted->isNumber() && count != nullptr) {
                JsonValue actual;
                MPL_CHECK(parseJson(count->jsonValue, actual, error));
                MPL_CHECK(actual.isNumber());
                MPL_CHECK_EQ(static_cast<std::int64_t>(actual.number),
                             static_cast<std::int64_t>(wanted->number));
            }
        }
    }
}
