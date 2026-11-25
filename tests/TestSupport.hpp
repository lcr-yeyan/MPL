#pragma once

#include "TestHarness.hpp"

#include "mpl/Compiler.hpp"
#include "mpl/backend/MidiBackend.hpp"
#include "mpl/frontend/Frontend.hpp"
#include "mpl/music/Rhythm.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

namespace mpltest {

using namespace mpl;

inline std::string readText(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

inline std::string header(const std::string& meter = "4/4",
                          const std::string& key = "C major") {
    return "project T { tempo = 120 bpm; meter = " + meter + "; key = " + key + "; }\n" +
           "asset PianoAsset = instrument(\"GrandPiano\");\n";
}

inline FrontendResult frontend(const std::string& source) {
    return compileSource(source, "<test>");
}

inline FrontendResult frontend(const std::string& source, const std::string& file) {
    return compileSource(source, file);
}

inline bool hasCode(const FrontendResult& result, const std::string& code) {
    for (const FrontendDiagnostic& diagnostic : result.diagnostics) {
        if (diagnostic.code == code) {
            return true;
        }
    }
    return false;
}

inline Compilation compileText(const std::string& source) {
    FrontendResult result = frontend(source);
    if (!result.ok()) {
        ::mpltest::fail(__FILE__, __LINE__, "program failed to compile");
    }
    const mpl::Compiler compiler;
    return compiler.compile(std::move(result.program->project));
}

inline Compilation compileText(const std::string& source, const std::string& file) {
    FrontendResult result = frontend(source, file);
    if (!result.ok()) {
        ::mpltest::fail(__FILE__, __LINE__, "program failed to compile");
    }
    const mpl::Compiler compiler;
    return compiler.compile(std::move(result.program->project));
}

inline std::vector<std::uint8_t> midiOf(const mpl::Compilation& compilation) {
    const mpl::MidiBackend backend(mpl::MidiOptions{480, "MPL Conductor"});
    return backend.serialize(compilation.production, compilation.temporal);
}

inline const mpl::Rhythm* rhythmByLabel(const mpl::Compilation& compilation,
                                        const std::string& label) {
    for (const mpl::MusicNode& node : compilation.music.nodes()) {
        if (node.label == label) {
            if (const mpl::Rhythm* rhythm = compilation.music.cachedRhythm(node.id)) {
                return rhythm;
            }
        }
    }
    return nullptr;
}

inline std::vector<const mpl::Rhythm*> allRhythms(const mpl::Compilation& compilation) {
    std::vector<const mpl::Rhythm*> rhythms;
    for (const mpl::MusicNode& node : compilation.music.nodes()) {
        if (const mpl::Rhythm* rhythm = compilation.music.cachedRhythm(node.id)) {
            rhythms.push_back(rhythm);
        }
    }
    return rhythms;
}

inline const mpl::Fragment* fragmentByLabel(const mpl::Compilation& compilation,
                                            const std::string& label) {
    for (const mpl::MusicNode& node : compilation.music.nodes()) {
        if (node.label == label && node.value.has_value()) {
            return &*node.value;
        }
    }
    return nullptr;
}

inline std::vector<mpl::MidiEvent> conductorEvents(const mpl::Compilation& compilation,
                                                   mpl::MidiEventKind kind) {
    const mpl::MidiBackend backend(mpl::MidiOptions{480, "MPL Conductor"});
    const std::vector<mpl::MidiTrack> tracks =
        backend.buildTracks(compilation.production, compilation.temporal);
    std::vector<mpl::MidiEvent> events;
    if (tracks.empty()) {
        return events;
    }
    for (const mpl::MidiEvent& event : tracks.front().events) {
        if (event.kind == kind) {
            events.push_back(event);
        }
    }
    return events;
}

}  // namespace mpltest
