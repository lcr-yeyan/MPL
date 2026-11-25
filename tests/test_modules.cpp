#include "TestSupport.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

using namespace mpl;

namespace {

std::filesystem::path outputDirectory() {
    const std::filesystem::path directory = MPL_TEST_OUTPUT_DIR;
    std::filesystem::create_directories(directory);
    return directory;
}

std::string writeModule(const std::string& name, const std::string& source) {
    const std::filesystem::path path = outputDirectory() / name;
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << source;
    stream.close();
    return path.generic_string();
}

std::string rootProject(const std::string& imports, const std::string& body) {
    return imports +
           "project T { tempo = 120 bpm; meter = 4/4; key = C major; }\n"
           "asset PianoAsset = instrument(\"GrandPiano\");\n" +
           body +
           "track Piano { asset = PianoAsset; role = melodic; }\n"
           "section S(1 bars) { clip Piano { play M; } }\n"
           "arrangement { play S; }\n"
           "export midi(\"x.mid\");\n";
}

}  // namespace

MPL_TEST(ModuleImportsExportsAndPrivateHelpers) {
    writeModule("rc_basic_lib.mpl",
                "module BasicLib;\n"
                "fn privateCell() -> Fragment { return note(C4, 1/8, 90); }\n"
                "export fn makeCell() -> Fragment { return sequence(privateCell(), privateCell()); }\n");
    const std::string root = rootProject(
        "import \"rc_basic_lib.mpl\";\n",
        "let M = makeCell();\n");
    const std::string rootPath = writeModule("rc_basic_root.mpl", root);
    const Compilation compilation = mpltest::compileText(root, rootPath);
    MPL_CHECK(!compilation.hasErrors());
    MPL_CHECK_EQ(compilation.production.noteCount(), std::size_t{2});
    MPL_CHECK_EQ(compilation.modules.size(), std::size_t{2});
}

MPL_TEST(ModulePrivateSymbolsDoNotCollide) {
    writeModule("rc_priv_a.mpl",
                "module PrivA;\n"
                "fn helper() -> Fragment { return note(C4, 1/8, 90); }\n"
                "export fn runA() -> Fragment { return helper(); }\n");
    writeModule("rc_priv_b.mpl",
                "module PrivB;\n"
                "fn helper() -> Fragment { return note(E4, 1/8, 90); }\n"
                "export fn runB() -> Fragment { return helper(); }\n");
    const std::string root = rootProject(
        "import \"rc_priv_a.mpl\";\nimport \"rc_priv_b.mpl\";\n",
        "let M = sequence(runA(), runB());\n");
    const std::string rootPath = writeModule("rc_priv_root.mpl", root);
    const Compilation compilation = mpltest::compileText(root, rootPath);
    const auto& notes = compilation.production.tracks.front().clips.front().content.notes();
    MPL_CHECK_EQ(notes.size(), std::size_t{2});
    MPL_CHECK(notes[0].pitch.name() == std::string("C4"));
    MPL_CHECK(notes[1].pitch.name() == std::string("E4"));
}

MPL_TEST(ModuleExportCollisionIsDiagnosed) {
    writeModule("rc_collide_a.mpl",
                "module CollideA;\n"
                "export fn make() -> Fragment { return note(C4, 1/8, 90); }\n");
    writeModule("rc_collide_b.mpl",
                "module CollideB;\n"
                "export fn make() -> Fragment { return note(E4, 1/8, 90); }\n");
    const std::string root = rootProject(
        "import \"rc_collide_a.mpl\";\nimport \"rc_collide_b.mpl\";\n",
        "let M = make();\n");
    const std::string rootPath = writeModule("rc_collide_root.mpl", root);
    const FrontendResult result = mpltest::frontend(root, rootPath);
    MPL_CHECK(mpltest::hasCode(result, "module-export-conflict"));
}

MPL_TEST(ModuleCyclicImportIsDiagnosed) {
    writeModule("rc_cycle_a.mpl",
                "module CycleA;\n"
                "import \"rc_cycle_b.mpl\";\n"
                "export fn a() -> Fragment { return note(C4, 1/8, 90); }\n");
    writeModule("rc_cycle_b.mpl",
                "module CycleB;\n"
                "import \"rc_cycle_a.mpl\";\n"
                "export fn b() -> Fragment { return note(E4, 1/8, 90); }\n");
    const std::string root = rootProject(
        "import \"rc_cycle_a.mpl\";\n",
        "let M = a();\n");
    const std::string rootPath = writeModule("rc_cycle_root.mpl", root);
    const FrontendResult result = mpltest::frontend(root, rootPath);
    MPL_CHECK(mpltest::hasCode(result, "module-import-cycle"));
}

MPL_TEST(ModuleMissingImportIsDiagnosed) {
    const std::string root = rootProject(
        "import \"rc_missing_lib.mpl\";\n",
        "let M = note(C4, 1/8, 90);\n");
    const FrontendResult result =
        mpltest::frontend(root, (outputDirectory() / "rc_missing_root.mpl").generic_string());
    MPL_CHECK(mpltest::hasCode(result, "module-import-missing"));
}

MPL_TEST(ModuleRejectsUnsupportedDeclarations) {
    writeModule("rc_bad_lib.mpl",
                "module BadLib;\n"
                "asset Hidden = instrument(\"X\");\n"
                "export fn f() -> Fragment { return note(C4, 1/8, 90); }\n");
    const std::string root = rootProject(
        "import \"rc_bad_lib.mpl\";\n",
        "let M = f();\n");
    const std::string rootPath = writeModule("rc_bad_root.mpl", root);
    const FrontendResult result = mpltest::frontend(root, rootPath);
    MPL_CHECK(mpltest::hasCode(result, "module-unsupported-declaration"));
}

MPL_TEST(ModuleContentIdentityAndDependencyRecords) {
    writeModule("rc_hash_lib.mpl",
                "module HashLib;\n"
                "export fn f() -> Fragment { return note(C4, 1/8, 90); }\n");
    const std::string root = rootProject(
        "import \"rc_hash_lib.mpl\";\n",
        "let M = f();\n");
    const std::string rootPath = writeModule("rc_hash_root.mpl", root);
    const Compilation first = mpltest::compileText(root, rootPath);
    const Compilation second = mpltest::compileText(root, rootPath);
    MPL_CHECK_EQ(first.modules.size(), std::size_t{2});
    MPL_CHECK_EQ(second.modules.size(), std::size_t{2});
    std::uint64_t firstHash = 0;
    std::uint64_t secondHash = 0;
    for (const ModuleRecord& module : first.modules) {
        if (module.name == "HashLib") {
            firstHash = module.contentHash;
            MPL_CHECK_EQ(module.imports.size(), std::size_t{0});
            MPL_CHECK_EQ(module.exports.size(), std::size_t{1});
        }
    }
    for (const ModuleRecord& module : second.modules) {
        if (module.name == "HashLib") {
            secondHash = module.contentHash;
        }
    }
    MPL_CHECK(firstHash != 0);
    MPL_CHECK_EQ(firstHash, secondHash);
    writeModule("rc_hash_lib.mpl",
                "module HashLib;\n"
                "export fn f() -> Fragment { return note(D4, 1/8, 90); }\n");
    const Compilation third = mpltest::compileText(root, rootPath);
    std::uint64_t thirdHash = 0;
    for (const ModuleRecord& module : third.modules) {
        if (module.name == "HashLib") {
            thirdHash = module.contentHash;
        }
    }
    MPL_CHECK(thirdHash != firstHash);
    const std::string trace = first.toTraceJson(false);
    MPL_CHECK(trace.find("\"modules\"") != std::string::npos);
    MPL_CHECK(trace.find("\"contentHash\"") != std::string::npos);
}

MPL_TEST(ModuleTransitiveImportsResolve) {
    writeModule("rc_trans_b.mpl",
                "module TransB;\n"
                "export fn bNote() -> Fragment { return note(E4, 1/8, 90); }\n");
    writeModule("rc_trans_a.mpl",
                "module TransA;\n"
                "import \"rc_trans_b.mpl\";\n"
                "export fn aNote() -> Fragment { return sequence(bNote(), bNote()); }\n");
    const std::string root = rootProject(
        "import \"rc_trans_a.mpl\";\n",
        "let M = aNote();\n");
    const std::string rootPath = writeModule("rc_trans_root.mpl", root);
    const Compilation compilation = mpltest::compileText(root, rootPath);
    MPL_CHECK_EQ(compilation.production.noteCount(), std::size_t{2});
    MPL_CHECK_EQ(compilation.modules.size(), std::size_t{3});
    bool sawImports = false;
    for (const ModuleRecord& module : compilation.modules) {
        if (module.name == "TransA" && !module.imports.empty()) {
            sawImports = true;
        }
    }
    MPL_CHECK(sawImports);
}

MPL_TEST(ModuleTransitiveHashChangesWithDependency) {
    writeModule("rc_dep_b.mpl",
                "module DepB;\n"
                "export fn b() -> Fragment { return note(C4, 1/8, 90); }\n");
    writeModule("rc_dep_a.mpl",
                "module DepA;\n"
                "import \"rc_dep_b.mpl\";\n"
                "export fn a() -> Fragment { return b(); }\n");
    const std::string root = rootProject(
        "import \"rc_dep_a.mpl\";\n",
        "let M = a();\n");
    const std::string rootPath = writeModule("rc_dep_root.mpl", root);
    const Compilation first = mpltest::compileText(root, rootPath);
    std::uint64_t firstAContent = 0;
    std::uint64_t firstAResolved = 0;
    std::uint64_t firstRootResolved = 0;
    for (const ModuleRecord& module : first.modules) {
        if (module.name == "DepA") {
            firstAContent = module.contentHash;
            firstAResolved = module.resolvedHash;
        }
    }
    firstRootResolved = first.modules.front().resolvedHash;
    MPL_CHECK(firstAResolved != 0);
    MPL_CHECK(firstRootResolved != 0);

    writeModule("rc_dep_b.mpl",
                "module DepB;\n"
                "export fn b() -> Fragment { return note(D4, 1/8, 90); }\n");
    const Compilation second = mpltest::compileText(root, rootPath);
    std::uint64_t secondAContent = 0;
    std::uint64_t secondAResolved = 0;
    std::uint64_t secondRootResolved = 0;
    for (const ModuleRecord& module : second.modules) {
        if (module.name == "DepA") {
            secondAContent = module.contentHash;
            secondAResolved = module.resolvedHash;
        }
    }
    secondRootResolved = second.modules.front().resolvedHash;
    // A's own source is unchanged; its resolved identity changes because B did.
    MPL_CHECK_EQ(firstAContent, secondAContent);
    MPL_CHECK(firstAResolved != secondAResolved);
    MPL_CHECK(firstRootResolved != secondRootResolved);
}

MPL_TEST(ModuleFunctionContextsArePreserved) {
    writeModule("rc_ctx_lib.mpl",
                "module CtxLib;\n"
                "export fn barLengthSound() -> Fragment context meter {\n"
                "  return note(C4, 1 bars, 90);\n"
                "}\n");
    const std::string root =
        "import \"rc_ctx_lib.mpl\";\n"
        "project T { tempo = 120 bpm; meter = 7/8; key = C major; }\n"
        "asset PianoAsset = instrument(\"GrandPiano\");\n"
        "let M = barLengthSound();\n"
        "track Piano { asset = PianoAsset; role = melodic; }\n"
        "section S(1 bars) meter 7/8 grouping 2+2+3 { clip Piano { play M; } }\n"
        "arrangement { play S; }\n"
        "export midi(\"x.mid\");\n";
    const std::string rootPath = writeModule("rc_ctx_root.mpl", root);
    const Compilation compilation = mpltest::compileText(root, rootPath);
    const auto& notes = compilation.production.tracks.front().clips.front().content.notes();
    MPL_CHECK_EQ(notes.size(), std::size_t{1});
    MPL_CHECK(notes.front().duration == Duration(Rational(7, 8)));
}
