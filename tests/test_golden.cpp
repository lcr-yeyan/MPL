#include "TestHarness.hpp"

#include "mpl/Compiler.hpp"
#include "mpl/backend/MidiBackend.hpp"
#include "mpl/examples/CoreDemo.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {

std::string readText(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

void checkGolden(const std::string& actual, const std::string& name) {
    const std::filesystem::path golden = std::filesystem::path(MPL_GOLDEN_DIR) / name;
    if (::mpltest::updateGoldenMode()) {
        std::filesystem::create_directories(golden.parent_path());
        std::ofstream stream(golden, std::ios::binary | std::ios::trunc);
        stream << actual;
        return;
    }

    const std::string expected = readText(golden);
    if (actual == expected) {
        return;
    }

    std::filesystem::path outputDirectory(MPL_TEST_OUTPUT_DIR);
    std::filesystem::create_directories(outputDirectory);
    const std::filesystem::path actualPath = outputDirectory / (name + ".actual");
    std::ofstream stream(actualPath, std::ios::binary | std::ios::trunc);
    stream << actual;

    throw ::mpltest::AssertionFailure("golden mismatch: " + golden.string() +
                                      " (actual written to " + actualPath.string() + ")");
}

mpl::Compilation compileCoreDemo() {
    const mpl::Compiler compiler;
    return compiler.compile(mpl::examples::buildCoreDemoProject());
}

mpl::Compilation compileTemporalDemo() {
    const mpl::Compiler compiler;
    mpl::Compilation compilation =
        compiler.compile(mpl::examples::buildTemporalDemoProject());
    const mpl::MidiBackend backend(mpl::MidiOptions{480, "MPL Conductor"});
    std::vector<mpl::Diagnostic> diagnostics;
    (void)backend.serialize(compilation.production, compilation.temporal, &diagnostics);
    compilation.diagnostics.insert(compilation.diagnostics.end(), diagnostics.begin(),
                                   diagnostics.end());
    return compilation;
}

}  // namespace

MPL_TEST(CoreDemoGoldenMusicIr) {
    const mpl::Compilation compilation = compileCoreDemo();
    checkGolden(compilation.music.toJson(), "core-demo.music.ir");
}

MPL_TEST(CoreDemoGoldenProductionIr) {
    const mpl::Compilation compilation = compileCoreDemo();
    checkGolden(compilation.production.toJson(), "core-demo.production.ir");
}

MPL_TEST(CoreDemoGoldenMidi) {
    const mpl::Compilation compilation = compileCoreDemo();
    const mpl::MidiBackend backend(mpl::MidiOptions{480, "MPL Conductor"});
    checkGolden(mpl::hexDump(backend.serialize(compilation.production)), "core-demo.mid.hex");
}

MPL_TEST(CoreDemoHasCompleteArrangement) {
    const mpl::Compilation compilation = compileCoreDemo();
    MPL_CHECK(!compilation.hasErrors());
    MPL_CHECK_EQ(compilation.production.tracks.size(), std::size_t{2});
    MPL_CHECK_EQ(compilation.production.noteCount(), std::size_t{40});
    MPL_CHECK(compilation.production.totalDuration() == mpl::Duration(mpl::Rational(4, 1)));
    MPL_CHECK_NEAR(compilation.production.totalDuration().toSeconds(160.0), 6.0, 1e-9);
    MPL_CHECK_EQ(compilation.music.size(), std::size_t{5});

    const mpl::MidiBackend backend(mpl::MidiOptions{480, "MPL Conductor"});
    MPL_CHECK_EQ(backend.tickAt(mpl::Position(mpl::Rational(4, 1))), std::int64_t{7680});
    const std::string ledger = backend.describe(compilation.production);
    MPL_CHECK(ledger.find("@7680 note-off") != std::string::npos);
}

MPL_TEST(SuiteDemoGoldenMidi) {
    const mpl::Compiler compiler;
    const mpl::Compilation compilation =
        compiler.compile(mpl::examples::buildSuiteDemoProject());
    const mpl::MidiBackend backend(mpl::MidiOptions{480, "MPL Conductor"});
    checkGolden(mpl::hexDump(backend.serialize(compilation.production)), "suite-demo.mid.hex");
}

MPL_TEST(TemporalDemoGoldenArtifacts) {
    const mpl::Compilation compilation = compileTemporalDemo();

    checkGolden(compilation.music.toJson(), "temporal-demo.music.ir");
    checkGolden(compilation.temporal.toJson(), "temporal-demo.temporal.ir");
    checkGolden(compilation.production.toJson(), "temporal-demo.production.ir");
    checkGolden(compilation.toTraceJson(false), "temporal-demo.trace.json");

    const mpl::MidiBackend backend(mpl::MidiOptions{480, "MPL Conductor"});
    checkGolden(mpl::hexDump(backend.serialize(compilation.production, compilation.temporal)),
                "temporal-demo.mid.hex");
}
