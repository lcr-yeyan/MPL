#include "TestSupport.hpp"

#include "mpl/audio/WavFile.hpp"
#include "mpl/lab/RenderArtifacts.hpp"
#include "mpl/lab/Task.hpp"
#include "mpl/lab/TrustedExecution.hpp"
#include "mpl/observe/Observations.hpp"
#include "mpl/util/Sha256.hpp"

#include <filesystem>
#include <fstream>
#include <string>

using namespace mpl;

namespace {

Compilation compileBasic() {
    return mpltest::compileText(
        "project T { tempo = 120 bpm; meter = 4/4; key = C major; }\n"
        "asset Piano = instrument(\"GrandPiano\");\n"
        "let M = notes span 1 whole { C4 @ 0 for 1/2 vel 96, E4 @ 1/2 for 1/2 vel 96 };\n"
        "track Keys { asset = Piano; role = melodic; }\n"
        "section S(1 bars) { clip Keys { play M; } }\n"
        "arrangement { play S; }\n"
        "export midi(\"x.mid\");\n");
}

}  // namespace

MPL_TEST(Sha256MatchesKnownVectors) {
    MPL_CHECK_EQ(sha256Hex(""),
                 std::string("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
    MPL_CHECK_EQ(sha256Hex("abc"),
                 std::string("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
    MPL_CHECK_EQ(
        sha256Hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"),
        std::string("248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"));
}

MPL_TEST(TrustedRenderBindsActualRenderedAudio) {
    const std::string source =
        "project T { tempo = 120 bpm; meter = 4/4; key = C major; }\n"
        "asset Piano = instrument(\"GrandPiano\");\n"
        "let M = notes span 1 whole {\n"
        "  C4 @ 0 for 1/4 vel 96, D4 @ 1/4 for 1/4 vel 96,\n"
        "  E4 @ 1/2 for 1/4 vel 96, F4 @ 3/4 for 1/4 vel 96\n"
        "};\n"
        "track Keys { asset = Piano; role = melodic; }\n"
        "section S(1 bars) { clip Keys { play M; } }\n"
        "arrangement { play S; }\n"
        "export midi(\"x.mid\");\n";
    const std::filesystem::path directory = MPL_TEST_OUTPUT_DIR;
    std::filesystem::create_directories(directory);
    const std::filesystem::path sourcePath = directory / "trusted-render.mpl";
    const std::filesystem::path wavPath = directory / "trusted-render.wav";
    const std::filesystem::path manifestPath = directory / "trusted-render.manifest.json";
    std::ofstream(sourcePath, std::ios::binary) << source;
    const RenderConfiguration configuration{8000, 256, 1, 0.1};
    const TrustedRenderResult rendered =
        renderTrusted(source, sourcePath, configuration, wavPath, manifestPath);
    MPL_CHECK(rendered.ok);
    MPL_CHECK(std::filesystem::exists(wavPath));
    MPL_CHECK(std::filesystem::exists(manifestPath));
    MPL_CHECK(!rendered.manifest.audioSha256.empty());
    std::string error;
    MPL_CHECK(verifyRenderManifest(rendered.manifest, source, mpltest::compileText(
        source, sourcePath.generic_string()), configuration, wavPath, error));
    const std::string task =
        (std::filesystem::path(MPL_BENCH_DIR) / "tasks" / "01-basic-notes.json")
            .generic_string();
    const VerificationResult verified = verifyTaskWithRenderManifest(
        task, sourcePath.generic_string(), manifestPath.generic_string(),
        wavPath.generic_string());
    MPL_CHECK_EQ(verified.status, std::string("pass"));
}

MPL_TEST(RenderManifestBindsSourceProductionAndAudio) {
    const Compilation compilation = compileBasic();
    const std::filesystem::path directory = MPL_TEST_OUTPUT_DIR;
    std::filesystem::create_directories(directory);
    const std::filesystem::path audioPath = directory / "manifest-test.wav";
    AudioBuffer buffer;
    buffer.sampleRate = 8000;
    buffer.channels = 2;
    buffer.resizeFrames(8000);
    std::vector<Diagnostic> diagnostics;
    writeWavFile(audioPath, buffer, diagnostics);
    const RenderConfiguration configuration{8000, 256, 2, 1.0};
    const std::string source = "source";
    const RenderArtifactManifest manifest =
        buildRenderManifest(source, compilation, configuration, audioPath);
    std::string error;
    MPL_CHECK(verifyRenderManifest(manifest, source, compilation, configuration,
                                   audioPath, error));
    MPL_CHECK(manifest.audioHash.size() > 8);
    const std::string json = manifest.toJson();
    RenderArtifactManifest parsed;
    MPL_CHECK(parseRenderManifest(json, parsed, error));
    MPL_CHECK(verifyRenderManifest(parsed, source, compilation, configuration,
                                   audioPath, error));

    // Different audio bytes cannot be treated as this candidate's render.
    std::ofstream(audioPath, std::ios::binary | std::ios::trunc) << "not the render";
    MPL_CHECK(!verifyRenderManifest(manifest, source, compilation, configuration,
                                    audioPath, error));
    MPL_CHECK(error.find("audio hash") != std::string::npos);
}

MPL_TEST(TaskVerifierRejectsMismatchedRenderManifest) {
    const std::string source =
        "project T { tempo = 120 bpm; meter = 4/4; key = C major; }\n"
        "asset Piano = instrument(\"GrandPiano\");\n"
        "let M = notes span 1 whole {\n"
        "  C4 @ 0 for 1/4 vel 96, D4 @ 1/4 for 1/4 vel 96,\n"
        "  E4 @ 1/2 for 1/4 vel 96, F4 @ 3/4 for 1/4 vel 96\n"
        "};\n"
        "track Keys { asset = Piano; role = melodic; }\n"
        "section S(1 bars) { clip Keys { play M; } }\n"
        "arrangement { play S; }\n"
        "export midi(\"x.mid\");\n";
    const std::filesystem::path directory = MPL_TEST_OUTPUT_DIR;
    std::filesystem::create_directories(directory);
    const std::filesystem::path candidatePath = directory / "render-candidate.mpl";
    std::ofstream(candidatePath, std::ios::binary) << source;
    const std::filesystem::path audioPath = directory / "render-candidate.wav";
    AudioBuffer buffer;
    buffer.sampleRate = 8000;
    buffer.channels = 1;
    buffer.resizeFrames(800);
    std::vector<Diagnostic> diagnostics;
    writeWavFile(audioPath, buffer, diagnostics);
    const Compilation compilation =
        mpltest::compileText(source, candidatePath.generic_string());
    const RenderConfiguration configuration{8000, 256, 1, 1.0};
    const RenderArtifactManifest manifest =
        buildRenderManifest(source, compilation, configuration, audioPath);
    const std::filesystem::path manifestPath = directory / "render-candidate.manifest.json";
    std::ofstream(manifestPath, std::ios::binary) << manifest.toJson();
    const std::string task =
        (std::filesystem::path(MPL_BENCH_DIR) / "tasks" / "01-basic-notes.json")
            .generic_string();
    const VerificationResult pass = verifyTaskWithRenderManifest(
        task, candidatePath.generic_string(), manifestPath.generic_string(),
        audioPath.generic_string());
    MPL_CHECK_EQ(pass.status, std::string("pass"));

    std::ofstream(audioPath, std::ios::binary | std::ios::trunc) << "tampered";
    const VerificationResult tampered = verifyTaskWithRenderManifest(
        task, candidatePath.generic_string(), manifestPath.generic_string(),
        audioPath.generic_string());
    MPL_CHECK_EQ(tampered.status, std::string("error"));
    MPL_CHECK(!tampered.diagnostics.empty());
}

MPL_TEST(ObservationRejectsUntrustedAudioHash) {    const Compilation compilation = compileBasic();
    const std::filesystem::path audioPath = std::filesystem::path(MPL_TEST_OUTPUT_DIR) /
                                            "manifest-test.wav";
    ObservationOptions options;
    options.audioPath = audioPath;
    options.expectedAudioHash = "0000000000000000";
    const ObservationSet observations = observeCompilation(compilation, options);
    const ObservationValue* peak = observations.find("sample_peak");
    MPL_CHECK(peak != nullptr);
    if (peak != nullptr) {
        MPL_CHECK(peak->status == std::string("unverifiable"));
        MPL_CHECK(peak->reason.find("trusted") != std::string::npos);
    }
}
