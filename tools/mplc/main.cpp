#include "mpl/Compiler.hpp"
#include "mpl/Version.hpp"
#include "mpl/audio/AudioAnalysis.hpp"
#include "mpl/audio/AudioExecution.hpp"
#include "mpl/audio/ReferenceAudioBackend.hpp"
#include "mpl/audio/WavFile.hpp"
#include "mpl/backend/MidiBackend.hpp"
#include "mpl/core/Error.hpp"
#include "mpl/examples/CoreDemo.hpp"
#include "mpl/frontend/Ast.hpp"
#include "mpl/frontend/Diagnostic.hpp"
#include "mpl/frontend/Frontend.hpp"
#include "mpl/frontend/Lexer.hpp"
#include "mpl/frontend/Parser.hpp"
#include "mpl/mastering/Mastering.hpp"
#include "mpl/util/JsonWriter.hpp"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

void printUsage(std::ostream& out) {
    out << "mplc 0.5.0-beta - MPL music compiler\n\n"
           "Usage:\n"
           "  mplc check <file.mpl> [--json]\n"
           "  mplc report <file.mpl>\n"
           "  mplc build <file.mpl> [options]\n"
           "  mplc ast <file.mpl>\n"
           "  mplc analyze <file.wav> [--json]\n"
           "  mplc master <file.mpl> [options]\n"
           "  mplc core-demo [options]\n"
           "  mplc suite-demo [options]\n"
           "  mplc temporal-demo [options]\n\n"
           "Options:\n"
           "  -o, --output <dir>   artifact directory (default: build)\n"
           "      --ppq <ticks>    MIDI resolution, ticks per quarter note (default: 480)\n"
           "      --sample-rate <hz>   audio sample rate (default: 48000)\n"
           "      --block-size <n>     audio block size (default: 256)\n"
           "      --channels <n>       audio channels, 1 or 2 (default: 2)\n"
           "      --tail <seconds>     rendered tail after last voice (default: 1.0)\n"
           "      --target-lufs <value>        mastering integrated loudness target\n"
           "      --loudness-tolerance <lu>    allowed loudness error (default: 0.5)\n"
           "      --peak-ceiling <dbtp>        mastering true-peak ceiling (default: -0.5)\n"
           "      --peak-tolerance <db>        independent true-peak tolerance (default: 0.1)\n"
           "      --max-iterations <n>         mastering feedback iterations (default: 4)\n"
           "      --no-limiter                 do not insert a master limiter\n"
           "      --json           machine-readable diagnostics\n"
           "      --quiet          suppress the compilation summary\n"
           "  -h, --help           show this help\n"
           "      --version        show the compiler version\n\n"
           "check runs lexing, parsing, name resolution, type/unit/domain checks and\n"
           "lowering without writing artifacts. build writes Music IR, Temporal IR,\n"
           "Production IR, Trace, MIDI and/or WAV according to export declarations.\n"
           "master renders a raw mix, inserts a master limiter, solves a loudness\n"
           "target offline and writes both WAVs plus analysis and feedback traces.\n";
}

bool writeTextFile(const std::filesystem::path& path, const std::string& content) {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) {
        std::cerr << "mplc: cannot open '" << path.string() << "' for writing\n";
        return false;
    }
    stream.write(content.data(), static_cast<std::streamsize>(content.size()));
    if (!stream) {
        std::cerr << "mplc: failed while writing '" << path.string() << "'\n";
        return false;
    }
    return true;
}

std::string formatSeconds(double seconds) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.2f", seconds);
    return buffer;
}

bool readSourceFile(const std::filesystem::path& path, std::string& content) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        std::cerr << "mplc: cannot open '" << path.string() << "'\n";
        return false;
    }
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    content = buffer.str();
    return true;
}

void printFrontendDiagnostics(const std::vector<mpl::FrontendDiagnostic>& diagnostics,
                              bool json) {
    mpl::DiagnosticSink sink;
    for (const mpl::FrontendDiagnostic& diagnostic : diagnostics) {
        sink.add(diagnostic);
    }
    if (json) {
        std::cout << sink.toJson() << "\n";
    } else {
        std::cerr << sink.toHuman();
    }
}

int runAst(const std::filesystem::path& input) {
    std::string source;
    if (!readSourceFile(input, source)) {
        return 1;
    }
    mpl::DiagnosticSink diagnostics;
    mpl::Lexer lexer(source, input.string(), diagnostics);
    mpl::Parser parser(lexer.lex(), diagnostics);
    const mpl::Module module = parser.parse();
    printFrontendDiagnostics(diagnostics.diagnostics(), false);
    std::cout << mpl::astToJson(module) << "\n";
    return diagnostics.hasErrors() ? 1 : 0;
}

int runReport(const std::filesystem::path& input) {
    std::ifstream stream(input, std::ios::binary);
    if (!stream) {
        std::cerr << "mplc: cannot read '" << input.string() << "'\n";
        return 1;
    }
    const std::string source{std::istreambuf_iterator<char>(stream),
                             std::istreambuf_iterator<char>()};
    const mpl::FrontendResult result = mpl::compileSource(source, input.string());
    if (!result.ok()) {
        mpl::DiagnosticSink sink;
        for (const mpl::FrontendDiagnostic& diagnostic : result.diagnostics) {
            sink.add(diagnostic);
        }
        std::cout << "{\"format\":\"mpl.compiler-report\",\"version\":1,"
                     "\"compilerVersion\":\""
                  << mpl::kCompilerVersion << "\",\"languageVersion\":\""
                  << mpl::kLanguageVersion
                  << "\",\"phases\":{\"compile\":\"failed\",\"validate\":\"failed\","
                     "\"execute\":\"not-run\"},\"diagnostics\":"
                  << sink.toJson() << "}\n";
        return 1;
    }
    const mpl::Compiler compiler;
    mpl::Compilation compilation =
        compiler.compile(std::move(result.program->project));
    std::cout << compilation.toReportJson() << "\n";
    return compilation.hasErrors() ? 1 : 0;
}

int runCheck(const std::filesystem::path& input, bool json) {    std::string source;
    if (!readSourceFile(input, source)) {
        return 1;
    }
    const mpl::FrontendResult result = mpl::compileSource(source, input.string());
    printFrontendDiagnostics(result.diagnostics, json);
    if (!json) {
        if (result.ok()) {
            std::cout << "ok: " << input.string() << "\n";
        } else {
            std::cerr << "mplc: check failed\n";
        }
    }
    return result.ok() ? 0 : 1;
}

struct SourceBuildOptions {
    int ppq = 480;
    int sampleRate = 48000;
    int blockSize = 256;
    int channels = 2;
    double tailSeconds = 1.0;
    bool quiet = false;
    bool json = false;
};

std::string combinedDiagnosticsJson(const std::vector<mpl::FrontendDiagnostic>& frontend,
                                    const std::vector<mpl::Diagnostic>& core) {
    mpl::JsonWriter json;
    json.beginArray();
    for (const mpl::FrontendDiagnostic& diagnostic : frontend) {
        json.beginObject();
        json.key("code").value(diagnostic.code);
        json.key("severity").value(mpl::diagnosticSeverityName(diagnostic.severity));
        json.key("file").value(diagnostic.span.file);
        json.key("line").value(diagnostic.span.line);
        json.key("column").value(diagnostic.span.column);
        json.key("message").value(diagnostic.message);
        json.key("expected").value(diagnostic.expected);
        json.key("actual").value(diagnostic.actual);
        json.key("related").value(diagnostic.related);
        json.endObject();
    }
    for (const mpl::Diagnostic& diagnostic : core) {
        json.beginObject();
        json.key("code").value(diagnostic.code);
        json.key("severity").value(diagnostic.severityName());
        json.key("file").value("");
        json.key("line").value(0);
        json.key("column").value(0);
        json.key("message").value(diagnostic.message);
        json.key("expected").value("");
        json.key("actual").value("");
        json.key("related").value("");
        json.endObject();
    }
    json.endArray();
    return json.take();
}

int runSourceBuild(const std::filesystem::path& input,
                   const std::filesystem::path& outputDirectory,
                   const SourceBuildOptions& options) {
    std::string source;
    if (!readSourceFile(input, source)) {
        return 1;
    }

    mpl::FrontendResult frontend = mpl::compileSource(source, input.string());
    if (!options.json) {
        printFrontendDiagnostics(frontend.diagnostics, false);
    }
    if (!frontend.ok()) {
        if (options.json) {
            std::cout << combinedDiagnosticsJson(frontend.diagnostics, {}) << "\n";
        } else {
            std::cerr << "mplc: build failed with frontend errors\n";
        }
        return 1;
    }

    mpl::FrontendProgram program = std::move(*frontend.program);
    mpl::Compiler compiler;
    mpl::Compilation compilation = compiler.compile(std::move(program.project));

    if (compilation.hasErrors()) {
        if (options.json) {
            std::cout << combinedDiagnosticsJson(frontend.diagnostics,
                                                 compilation.diagnostics)
                      << "\n";
        } else {
            for (const mpl::Diagnostic& diagnostic : compilation.diagnostics) {
                std::cerr << diagnostic.severityName() << "[" << diagnostic.code
                          << "]: " << diagnostic.message << "\n";
            }
            std::cerr << "mplc: build failed with core errors\n";
        }
        return 1;
    }

    std::error_code directoryError;
    std::filesystem::create_directories(outputDirectory, directoryError);
    if (directoryError) {
        std::cerr << "mplc: cannot create '" << outputDirectory.string()
                  << "': " << directoryError.message() << "\n";
        return 1;
    }

    const std::string prefix = input.stem().string().empty() ? "program" : input.stem().string();
    const std::filesystem::path musicPath = outputDirectory / (prefix + ".music.ir");
    const std::filesystem::path temporalPath = outputDirectory / (prefix + ".temporal.ir");
    const std::filesystem::path productionPath = outputDirectory / (prefix + ".production.ir");
    const std::filesystem::path tracePath = outputDirectory / (prefix + ".trace.json");

    std::vector<std::string> artifacts;
    std::vector<mpl::Diagnostic> exportDiagnostics;

    for (const mpl::ExportRequest& request : program.exports) {
        std::filesystem::path path(request.path);
        if (path.is_relative()) {
            path = outputDirectory / path;
        }
        if (request.format == "midi") {
            mpl::MidiOptions midiOptions;
            midiOptions.ppq = options.ppq;
            mpl::MidiBackend midi(midiOptions);
            midi.emit(compilation.production, compilation.temporal, path, &exportDiagnostics);
            artifacts.push_back(path.string());
        } else if (request.format == "wav") {
            mpl::AudioRenderConfig config;
            config.sampleRate = options.sampleRate;
            config.blockSize = options.blockSize;
            config.channels = options.channels;
            config.tailSeconds = options.tailSeconds;
            mpl::ReferenceAssetLibrary resolver(input.parent_path());
            mpl::ReferenceAudioBackend audio(config);
            mpl::AudioBuffer buffer =
                audio.render(compilation.production, compilation.temporal, resolver,
                             exportDiagnostics);
            audio.writeWav(buffer, path, exportDiagnostics);
            artifacts.push_back(path.string());
        }
    }

    compilation.diagnostics.insert(compilation.diagnostics.end(), exportDiagnostics.begin(),
                                   exportDiagnostics.end());
    if (!options.json) {
        for (const mpl::Diagnostic& diagnostic : compilation.diagnostics) {
            std::cerr << diagnostic.severityName() << "[" << diagnostic.code
                      << "]: " << diagnostic.message << "\n";
        }
    }
    if (compilation.hasErrors()) {
        if (options.json) {
            std::cout << combinedDiagnosticsJson(frontend.diagnostics,
                                                 compilation.diagnostics)
                      << "\n";
        } else {
            std::cerr << "mplc: export failed with errors\n";
        }
        return 1;
    }

    if (!writeTextFile(musicPath, compilation.music.toJson()) ||
        !writeTextFile(temporalPath, compilation.temporal.toJson()) ||
        !writeTextFile(productionPath, compilation.production.toJson()) ||
        !writeTextFile(tracePath, compilation.toTraceJson())) {
        return 1;
    }
    artifacts.push_back(musicPath.string());
    artifacts.push_back(temporalPath.string());
    artifacts.push_back(productionPath.string());
    artifacts.push_back(tracePath.string());

    if (options.json) {
        std::cout << combinedDiagnosticsJson(frontend.diagnostics, compilation.diagnostics)
                  << "\n";
    }

    if (!options.quiet && !options.json) {
        const mpl::ProductionIR& production = compilation.production;
        const double tempo = production.tempoMap.front().bpm;
        std::cout << "MPL V0.5.0-beta build\n";
        std::cout << "  source     " << input.string() << "\n";
        std::cout << "  tempo      " << tempo << " bpm, meter "
                  << production.meterMap.front().meter.toString() << ", key "
                  << compilation.music.key().name() << "\n";
        std::cout << "  music IR   " << compilation.music.size() << " objects\n";
        std::cout << "  temporal   " << compilation.temporal.size() << " curves, "
                  << production.automation.size() << " automation bindings\n";
        std::cout << "  production " << production.tracks.size() << " tracks, "
                  << production.clipCount() << " clips, " << production.noteCount()
                  << " notes\n";
        std::cout << "  duration   " << production.totalDuration().toString()
                  << " whole notes (" << formatSeconds(production.totalDuration().toSeconds(tempo))
                  << " s)\n";
        std::cout << "  artifacts\n";
        for (const std::string& artifact : artifacts) {
            std::cout << "             " << artifact << "\n";
        }
    }
    return 0;
}

void printCoreDiagnostics(const std::vector<mpl::Diagnostic>& diagnostics) {
    for (const mpl::Diagnostic& diagnostic : diagnostics) {
        std::cerr << diagnostic.severityName() << "[" << diagnostic.code
                  << "]: " << diagnostic.message << "\n";
    }
}

bool hasCoreErrors(const std::vector<mpl::Diagnostic>& diagnostics) {
    for (const mpl::Diagnostic& diagnostic : diagnostics) {
        if (diagnostic.severity == mpl::Severity::Error) {
            return true;
        }
    }
    return false;
}

std::string formatOptionalDb(double value) {
    if (!std::isfinite(value)) {
        return "-inf";
    }
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.2f", value);
    return buffer;
}

int runAnalyze(const std::filesystem::path& input, bool json) {
    std::vector<mpl::Diagnostic> diagnostics;
    const std::optional<mpl::AudioBuffer> buffer = mpl::loadWavFile(input, diagnostics);
    if (!buffer.has_value()) {
        printCoreDiagnostics(diagnostics);
        std::cerr << "mplc: cannot analyse '" << input.string() << "'\n";
        return 1;
    }
    mpl::AudioAnalysisConfig config;
    const mpl::AudioAnalysis analysis = mpl::analyzeAudio(*buffer, config, diagnostics);
    printCoreDiagnostics(diagnostics);

    if (json) {
        std::cout << mpl::audioAnalysisToJson(analysis, config) << "\n";
    } else {
        std::cout << "MPL audio analysis\n";
        std::cout << "  file         " << input.string() << "\n";
        std::cout << "  format       " << analysis.sampleRate << " Hz, "
                  << analysis.channels << " channels, " << analysis.frames << " frames\n";
        std::cout << "  sample peak  " << formatOptionalDb(analysis.samplePeakDb)
                  << " dBFS\n";
        std::cout << "  rms          " << formatOptionalDb(analysis.rmsDb) << " dBFS\n";
        std::cout << "  crest        " << formatOptionalDb(analysis.crestFactorDb)
                  << " dB\n";
        std::cout << "  true peak    " << formatOptionalDb(analysis.truePeakDb)
                  << " dBTP\n";
        std::cout << "  integrated   " << formatOptionalDb(analysis.integratedLoudness)
                  << " LUFS\n";
        std::cout << "  short-term   " << formatOptionalDb(analysis.maxShortTermLoudness)
                  << " LUFS\n";
        std::cout << "  clipped      " << (analysis.clipped ? "yes" : "no") << "\n";
    }
    return hasCoreErrors(diagnostics) ? 1 : 0;
}

int runMastering(const std::filesystem::path& input,
                 const std::filesystem::path& outputDirectory,
                 const SourceBuildOptions& options, const mpl::MasteringTarget& target,
                 bool json) {
    std::string source;
    if (!readSourceFile(input, source)) {
        return 1;
    }
    mpl::FrontendResult frontend = mpl::compileSource(source, input.string());
    printFrontendDiagnostics(frontend.diagnostics, false);
    if (!frontend.ok()) {
        std::cerr << "mplc: mastering failed with frontend errors\n";
        return 1;
    }

    mpl::Compiler compiler;
    mpl::Compilation compilation = compiler.compile(std::move(frontend.program->project));
    if (compilation.hasErrors()) {
        printCoreDiagnostics(compilation.diagnostics);
        std::cerr << "mplc: mastering failed with core errors\n";
        return 1;
    }

    std::error_code directoryError;
    std::filesystem::create_directories(outputDirectory, directoryError);
    if (directoryError) {
        std::cerr << "mplc: cannot create '" << outputDirectory.string()
                  << "': " << directoryError.message() << "\n";
        return 1;
    }

    mpl::AudioRenderConfig config;
    config.sampleRate = options.sampleRate;
    config.blockSize = options.blockSize;
    config.channels = options.channels;
    config.tailSeconds = options.tailSeconds;

    std::vector<mpl::Diagnostic> diagnostics;
    const mpl::ReferenceAssetLibrary resolver(input.parent_path());
    const mpl::MasteringResult result = mpl::runMasteringFeedback(
        compilation.production, compilation.temporal, resolver, config, target, diagnostics);

    const std::string prefix = input.stem().string().empty() ? "program" : input.stem().string();
    const std::filesystem::path rawPath = outputDirectory / (prefix + ".raw.wav");
    const std::filesystem::path masteredPath =
        outputDirectory / (prefix + ".mastered.wav");
    const std::filesystem::path beforePath =
        outputDirectory / (prefix + ".analysis-before.json");
    const std::filesystem::path afterPath =
        outputDirectory / (prefix + ".analysis-after.json");
    const std::filesystem::path tracePath =
        outputDirectory / (prefix + ".mastering-trace.json");

    mpl::ReferenceAudioBackend backend(config);
    backend.writeWav(result.raw, rawPath, diagnostics);
    backend.writeWav(result.mastered, masteredPath, diagnostics);

    mpl::AudioAnalysisConfig analysisConfig;
    const std::string beforeJson = mpl::audioAnalysisToJson(result.before, analysisConfig);
    const std::string afterJson = mpl::audioAnalysisToJson(result.after, analysisConfig);
    const std::string reportJson = mpl::masteringResultToJson(result, analysisConfig);
    if (!writeTextFile(beforePath, beforeJson) || !writeTextFile(afterPath, afterJson) ||
        !writeTextFile(tracePath, reportJson)) {
        return 1;
    }

    printCoreDiagnostics(diagnostics);
    if (json) {
        std::cout << reportJson << "\n";
    } else {
        std::cout << "MPL V0.5.0-beta mastering\n";
        std::cout << "  source       " << input.string() << "\n";
        std::cout << "  target       " << target.targetLufs << " LUFS +/- "
                  << target.loudnessToleranceLu << " LU, true peak <= "
                  << target.peakCeilingDb << " dBTP (+/- " << target.peakToleranceDb
                  << " dB)\n";
        if (!result.limiterName.empty()) {
            std::cout << "  limiter      " << result.limiterName << ", ceiling "
                      << result.limiterCeilingDb << " dB, attack "
                      << result.limiterAttackSeconds * 1000.0 << " ms, release "
                      << result.limiterReleaseSeconds * 1000.0 << " ms\n";
        }
        std::cout << "  before       "
                  << formatOptionalDb(result.before.integratedLoudness) << " LUFS, true peak "
                  << formatOptionalDb(result.before.truePeakDb) << " dBTP, sample peak "
                  << formatOptionalDb(result.before.samplePeakDb) << " dBFS\n";
        std::cout << "  after        "
                  << formatOptionalDb(result.after.integratedLoudness) << " LUFS, true peak "
                  << formatOptionalDb(result.after.truePeakDb) << " dBTP, sample peak "
                  << formatOptionalDb(result.after.samplePeakDb) << " dBFS\n";
        std::cout << "  gain         " << result.finalMasterGainDb << " dB\n";
        std::cout << "  limiter ceil " << result.limiterCeilingDb
                  << " dB pre-fader, " << result.finalPostFaderSampleCeilingDb
                  << " dB post-fader (sample)\n";
        std::cout << "  status       " << mpl::masteringStatusName(result.status) << "\n";
        std::cout << "  iterations   " << result.trace.size() - 1 << ", satisfied "
                  << (result.satisfied ? "yes" : "no") << "\n";
        std::cout << "  artifacts\n";
        std::cout << "             " << rawPath.string() << "\n";
        std::cout << "             " << masteredPath.string() << "\n";
        std::cout << "             " << beforePath.string() << "\n";
        std::cout << "             " << afterPath.string() << "\n";
        std::cout << "             " << tracePath.string() << "\n";
    }
    return hasCoreErrors(diagnostics) ? 1 : 0;
}

int runDemo(const std::string& label, mpl::Project project, const std::string& prefix,
            const std::filesystem::path& outputDirectory, int ppq, bool quiet) {
    mpl::Compiler compiler;
    mpl::Compilation compilation = compiler.compile(std::move(project));

    const auto printDiagnostics = [&compilation]() {
        for (const mpl::Diagnostic& diagnostic : compilation.diagnostics) {
            std::cerr << diagnostic.severityName() << "[" << diagnostic.code
                      << "]: " << diagnostic.message << "\n";
        }
    };

    if (compilation.hasErrors()) {
        printDiagnostics();
        std::cerr << "mplc: compilation failed with errors\n";
        return 1;
    }

    std::error_code directoryError;
    std::filesystem::create_directories(outputDirectory, directoryError);
    if (directoryError) {
        std::cerr << "mplc: cannot create '" << outputDirectory.string()
                  << "': " << directoryError.message() << "\n";
        return 1;
    }

    const std::filesystem::path midiPath = outputDirectory / (prefix + ".mid");
    const std::filesystem::path musicPath = outputDirectory / (prefix + ".music.ir");
    const std::filesystem::path temporalPath = outputDirectory / (prefix + ".temporal.ir");
    const std::filesystem::path productionPath = outputDirectory / (prefix + ".production.ir");
    const std::filesystem::path tracePath = outputDirectory / (prefix + ".trace.json");

    mpl::MidiOptions options;
    options.ppq = ppq;
    mpl::MidiBackend backend(options);
    std::vector<mpl::Diagnostic> backendDiagnostics;
    backend.emit(compilation.production, compilation.temporal, midiPath, &backendDiagnostics);
    compilation.diagnostics.insert(compilation.diagnostics.end(), backendDiagnostics.begin(),
                                   backendDiagnostics.end());
    printDiagnostics();
    if (compilation.hasErrors()) {
        std::cerr << "mplc: MIDI export failed with errors\n";
        return 1;
    }

    if (!writeTextFile(musicPath, compilation.music.toJson()) ||
        !writeTextFile(temporalPath, compilation.temporal.toJson()) ||
        !writeTextFile(productionPath, compilation.production.toJson()) ||
        !writeTextFile(tracePath, compilation.toTraceJson())) {
        return 1;
    }

    if (!quiet) {
        const mpl::ProductionIR& production = compilation.production;
        const double tempo = production.tempoMap.front().bpm;
        std::cout << label << "\n";
        std::cout << "  tempo      " << tempo << " bpm, meter "
                  << production.meterMap.front().meter.toString() << ", key "
                  << compilation.music.key().name() << "\n";
        std::cout << "  music IR   " << compilation.music.size() << " objects, "
                  << compilation.music.evaluatedCount() << " evaluated\n";
        std::cout << "  temporal   " << compilation.temporal.size() << " curves, "
                  << production.automation.size() << " automation bindings\n";
        std::cout << "  production " << production.tracks.size() << " tracks, "
                  << production.clipCount() << " clips, " << production.noteCount()
                  << " notes\n";
        std::cout << "  duration   " << production.totalDuration().toString()
                  << " whole notes (" << formatSeconds(production.totalDuration().toSeconds(tempo))
                  << " s)\n";
        std::cout << "  artifacts  " << midiPath.string() << "\n"
                  << "             " << musicPath.string() << "\n"
                  << "             " << temporalPath.string() << "\n"
                  << "             " << productionPath.string() << "\n"
                  << "             " << tracePath.string() << "\n";
        std::cout << "  note       render WAV with 'mplc build <file.mpl>' using the "
                     "reference audio backend.\n";
    }
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    std::string command;
    std::string inputFile;
    std::filesystem::path outputDirectory = "build";
    int ppq = 480;
    int sampleRate = 48000;
    int blockSize = 256;
    int channels = 2;
    double tailSeconds = 1.0;
    double targetLufs = -14.0;
    double loudnessTolerance = 0.5;
    double peakCeiling = -0.5;
    double peakTolerance = 0.1;
    int maxIterations = 4;
    bool addLimiter = true;
    bool quiet = false;
    bool json = false;

    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "-h" || argument == "--help") {
            printUsage(std::cout);
            return 0;
        }
        if (argument == "--version") {
            std::cout << "mplc 0.5.0-beta\n";
            return 0;
        }
        if (argument == "-o" || argument == "--output") {
            if (index + 1 >= argc) {
                std::cerr << "mplc: missing value for " << argument << "\n";
                return 1;
            }
            outputDirectory = argv[++index];
            continue;
        }
        if (argument == "--ppq") {
            if (index + 1 >= argc) {
                std::cerr << "mplc: missing value for --ppq\n";
                return 1;
            }
            try {
                ppq = std::stoi(argv[++index]);
            } catch (const std::exception&) {
                std::cerr << "mplc: --ppq expects an integer\n";
                return 1;
            }
            continue;
        }
        if (argument == "--quiet") {
            quiet = true;
            continue;
        }
        if (argument == "--no-limiter") {
            addLimiter = false;
            continue;
        }
        if (argument == "--target-lufs" || argument == "--loudness-tolerance" ||
            argument == "--peak-ceiling" || argument == "--peak-tolerance" ||
            argument == "--max-iterations") {
            if (index + 1 >= argc) {
                std::cerr << "mplc: missing value for " << argument << "\n";
                return 1;
            }
            try {
                if (argument == "--target-lufs") {
                    targetLufs = std::stod(argv[++index]);
                } else if (argument == "--loudness-tolerance") {
                    loudnessTolerance = std::stod(argv[++index]);
                } else if (argument == "--peak-ceiling") {
                    peakCeiling = std::stod(argv[++index]);
                } else if (argument == "--peak-tolerance") {
                    peakTolerance = std::stod(argv[++index]);
                } else {
                    maxIterations = std::stoi(argv[++index]);
                }
            } catch (const std::exception&) {
                std::cerr << "mplc: invalid value for " << argument << "\n";
                return 1;
            }
            continue;
        }
        if (argument == "--sample-rate" || argument == "--block-size" ||
            argument == "--channels" || argument == "--tail") {
            if (index + 1 >= argc) {
                std::cerr << "mplc: missing value for " << argument << "\n";
                return 1;
            }
            try {
                if (argument == "--sample-rate") {
                    sampleRate = std::stoi(argv[++index]);
                } else if (argument == "--block-size") {
                    blockSize = std::stoi(argv[++index]);
                } else if (argument == "--channels") {
                    channels = std::stoi(argv[++index]);
                } else {
                    tailSeconds = std::stod(argv[++index]);
                }
            } catch (const std::exception&) {
                std::cerr << "mplc: invalid value for " << argument << "\n";
                return 1;
            }
            continue;
        }
        if (argument == "--json") {
            json = true;
            continue;
        }
        if (!argument.empty() && argument.front() == '-') {
            std::cerr << "mplc: unknown option '" << argument << "'\n";
            printUsage(std::cerr);
            return 1;
        }
        if (command.empty()) {
            command = argument;
        } else if (inputFile.empty()) {
            inputFile = argument;
        } else {
            std::cerr << "mplc: unexpected argument '" << argument << "'\n";
            return 1;
        }
    }

    if (command.empty()) {
        printUsage(std::cerr);
        return 1;
    }

    try {
        if (command == "check") {
            if (inputFile.empty()) {
                std::cerr << "mplc: check requires a .mpl file\n";
                return 1;
            }
            return runCheck(inputFile, json);
        }
        if (command == "report") {
            if (inputFile.empty()) {
                std::cerr << "mplc: report requires a .mpl file\n";
                return 1;
            }
            return runReport(inputFile);
        }
        if (command == "ast") {
            if (inputFile.empty()) {
                std::cerr << "mplc: ast requires a .mpl file\n";
                return 1;
            }
            return runAst(inputFile);
        }
        if (command == "build") {
            if (inputFile.empty()) {
                std::cerr << "mplc: build requires a .mpl file\n";
                return 1;
            }
            SourceBuildOptions buildOptions;
            buildOptions.ppq = ppq;
            buildOptions.sampleRate = sampleRate;
            buildOptions.blockSize = blockSize;
            buildOptions.channels = channels;
            buildOptions.tailSeconds = tailSeconds;
            buildOptions.quiet = quiet;
            buildOptions.json = json;
            return runSourceBuild(inputFile, outputDirectory, buildOptions);
        }
        if (command == "analyze") {
            if (inputFile.empty()) {
                std::cerr << "mplc: analyze requires a .wav file\n";
                return 1;
            }
            return runAnalyze(inputFile, json);
        }
        if (command == "master") {
            if (inputFile.empty()) {
                std::cerr << "mplc: master requires a .mpl file\n";
                return 1;
            }
            SourceBuildOptions buildOptions;
            buildOptions.sampleRate = sampleRate;
            buildOptions.blockSize = blockSize;
            buildOptions.channels = channels;
            buildOptions.tailSeconds = tailSeconds;
            buildOptions.quiet = quiet;
            buildOptions.json = json;
            mpl::MasteringTarget target;
            target.targetLufs = targetLufs;
            target.loudnessToleranceLu = loudnessTolerance;
            target.peakCeilingDb = peakCeiling;
            target.peakToleranceDb = peakTolerance;
            target.maxIterations = maxIterations;
            target.addLimiter = addLimiter;
            return runMastering(inputFile, outputDirectory, buildOptions, target, json);
        }
        if (!inputFile.empty()) {
            std::cerr << "mplc: unexpected argument '" << inputFile << "'\n";
            return 1;
        }
        if (command == "core-demo") {
            return runDemo("MPL Core V0.0.3 (core demo)", mpl::examples::buildCoreDemoProject(),
                           "demo", outputDirectory, ppq, quiet);
        }
        if (command == "suite-demo") {
            return runDemo("MPL Core V0.0.3 (suite demo)",
                           mpl::examples::buildSuiteDemoProject(), "suite", outputDirectory, ppq,
                           quiet);
        }
        if (command == "temporal-demo") {
            return runDemo("MPL Core V0.0.3 (temporal demo)",
                           mpl::examples::buildTemporalDemoProject(), "temporal", outputDirectory,
                           ppq, quiet);
        }
        std::cerr << "mplc: unknown command '" << command << "'\n";
        printUsage(std::cerr);
        return 1;
    } catch (const mpl::Error& error) {
        std::cerr << "mplc: " << error.what() << "\n";
        return 1;
    } catch (const std::exception& error) {
        std::cerr << "mplc: unexpected failure: " << error.what() << "\n";
        return 1;
    }
}
