#include "TestHarness.hpp"

#include "mpl/Compiler.hpp"
#include "mpl/backend/MidiBackend.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using namespace mpl;
using namespace mpl::literals;

namespace {

Project oneNoteProject() {
    Project project;
    project.setTempo(160.0);
    project.setMeter(4, 4);

    Note note;
    note.pitch = "F4"_p;
    note.duration = Duration(Rational(1, 8));
    const ObjectId part = project.music().notes({note}, std::nullopt, "A");
    project.addTrack("Piano", "GrandPiano", part);
    return project;
}

ClipIR oneNoteClip(Pitch pitch = "F4"_p, Duration length = Duration(Rational(1, 1))) {
    Note note;
    note.pitch = pitch;
    note.duration = length;

    ClipIR clip;
    clip.name = "A";
    clip.content = Fragment::fromNotes({note});
    return clip;
}

ProductionIR twoNoteProduction() {
    Note first;
    first.pitch = "F4"_p;
    first.duration = Duration(Rational(1, 1));

    Note second;
    second.pitch = "G4"_p;
    second.offset = Duration(Rational(1, 1));
    second.duration = Duration(Rational(1, 1));

    ClipIR clip;
    clip.name = "A";
    clip.content = Fragment::fromNotes({first, second});

    TrackIR track;
    track.name = "Piano";
    track.instrument = "GrandPiano";
    track.clips.push_back(clip);

    ProductionIR production;
    production.tempoMap.push_back(TempoEvent{Position{}, 120.0});
    production.meterMap.push_back(MeterEvent{Position{}, Meter{4, 4}});
    production.tracks.push_back(track);
    return production;
}

std::vector<std::uint8_t> readBytes(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(stream),
                                     std::istreambuf_iterator<char>());
}

}  // namespace

MPL_TEST(MidiTicksFollowPpqAndMusicalTime) {
    const MidiBackend at480(MidiOptions{480, "Conductor"});
    MPL_CHECK_EQ(at480.tickAt(Position{}), std::int64_t{0});
    MPL_CHECK_EQ(at480.tickAt(Position(Rational(1, 1))), std::int64_t{1920});
    MPL_CHECK_EQ(at480.tickAt(Position(Rational(1, 2))), std::int64_t{960});
    MPL_CHECK_EQ(at480.tickAt(Position(Rational(1, 8))), std::int64_t{240});
    MPL_CHECK_EQ(at480.tickAt(Position(Rational(1, 12))), std::int64_t{160});
    MPL_CHECK_EQ(at480.tickAt(Position::fromBars(Rational(2, 1), Meter{4, 4})),
                 std::int64_t{3840});

    const MidiBackend at960(MidiOptions{960, "Conductor"});
    MPL_CHECK_EQ(at960.tickAt(Position(Rational(1, 1))), std::int64_t{3840});
    MPL_CHECK_EQ(at960.tickAt(Position(Rational(1, 12))), std::int64_t{320});
}

MPL_TEST(MidiReportsPpqQuantization) {
    const MidiBackend backend(MidiOptions{480, "Conductor"});
    MPL_CHECK(backend.isExactAtPpq(Position(Rational(1, 12))));
    MPL_CHECK(backend.isExactAtPpq(Position(Rational(3, 8))));
    MPL_CHECK(!backend.isExactAtPpq(Position(Rational(1, 7))));
    MPL_CHECK(!backend.isExactAtPpq(Position(Rational(1, 9))));
}

MPL_TEST(MidiHeaderDescribesTheExportedProject) {
    const Compiler compiler;
    const Compilation result = compiler.compile(oneNoteProject());
    MPL_CHECK(!result.hasErrors());

    const MidiBackend backend(MidiOptions{480, "Conductor"});
    const std::vector<std::uint8_t> bytes = backend.serialize(result.production);

    MPL_CHECK(bytes.size() > std::size_t{22});
    MPL_CHECK_EQ(static_cast<char>(bytes[0]), 'M');
    MPL_CHECK_EQ(static_cast<char>(bytes[1]), 'T');
    MPL_CHECK_EQ(static_cast<char>(bytes[2]), 'h');
    MPL_CHECK_EQ(static_cast<char>(bytes[3]), 'd');
    MPL_CHECK_EQ(bytes[8], std::uint8_t{0});
    MPL_CHECK_EQ(bytes[9], std::uint8_t{1});
    MPL_CHECK_EQ(bytes[10], std::uint8_t{0});
    MPL_CHECK_EQ(bytes[11], std::uint8_t{2});
    MPL_CHECK_EQ(bytes[12], std::uint8_t{0x01});
    MPL_CHECK_EQ(bytes[13], std::uint8_t{0xE0});
}

MPL_TEST(MidiEncodesTempoAsMicrosecondsPerQuarter) {
    const Compiler compiler;
    const Compilation result = compiler.compile(oneNoteProject());
    const MidiBackend backend(MidiOptions{480, "Conductor"});
    const std::vector<std::uint8_t> bytes = backend.serialize(result.production);

    // 60,000,000 / 160 = 375,000 = 0x05B8D8
    const std::vector<std::uint8_t> tempoMeta = {0xFF, 0x51, 0x03, 0x05, 0xB8, 0xD8};
    MPL_CHECK(std::search(bytes.begin(), bytes.end(), tempoMeta.begin(), tempoMeta.end()) !=
              bytes.end());

    const std::vector<std::uint8_t> meterMeta = {0xFF, 0x58, 0x04, 0x04, 0x02, 0x18, 0x08};
    MPL_CHECK(std::search(bytes.begin(), bytes.end(), meterMeta.begin(), meterMeta.end()) !=
              bytes.end());
}

MPL_TEST(MidiLedgerOrdersNoteOffsBeforeNoteOns) {
    const MidiBackend backend(MidiOptions{480, "Conductor"});
    const std::string ledger = backend.describe(twoNoteProduction());

    MPL_CHECK(ledger.find("tempo 120.000 bpm") != std::string::npos);
    MPL_CHECK(ledger.find("@1920 note-off ch=1 F4") != std::string::npos);
    MPL_CHECK(ledger.find("@1920 note-on ch=1 G4") != std::string::npos);
    const std::size_t noteOff = ledger.find("@1920 note-off");
    const std::size_t noteOn = ledger.find("@1920 note-on");
    MPL_CHECK(noteOff < noteOn);
}

MPL_TEST(MidiLedgerContainsMusicalEvents) {
    const Compiler compiler;
    const Compilation result = compiler.compile(oneNoteProject());
    const MidiBackend backend(MidiOptions{480, "Conductor"});
    const std::string ledger = backend.describe(result.production);

    MPL_CHECK(ledger.find("track[0] \"Conductor\"") != std::string::npos);
    MPL_CHECK(ledger.find("track[1] \"Piano\"") != std::string::npos);
    MPL_CHECK(ledger.find("meter 4/4") != std::string::npos);
    MPL_CHECK(ledger.find("program ch=1 program=0") != std::string::npos);
    MPL_CHECK(ledger.find("note-on ch=1 F4 vel=96") != std::string::npos);
    MPL_CHECK(ledger.find("note-off ch=1 F4") != std::string::npos);
    MPL_CHECK(ledger.find("@240 note-off ch=1 F4") != std::string::npos);
}

MPL_TEST(MidiChannelPolicyHandlesPercussion) {
    ProductionIR production = twoNoteProduction();
    TrackIR drums;
    drums.name = "Drums";
    drums.instrument = "DrumKit";
    drums.role = InstrumentRole::Percussion;
    drums.clips.push_back(oneNoteClip("C2"_p, Duration(Rational(1, 4))));
    production.tracks.push_back(drums);

    const MidiBackend backend(MidiOptions{480, "Conductor"});
    const std::string ledger = backend.describe(production);
    MPL_CHECK(ledger.find("track[2] \"Drums\"") != std::string::npos);
    MPL_CHECK(ledger.find("note-on ch=10 C2") != std::string::npos);
    MPL_CHECK(ledger.find("program ch=10") == std::string::npos);
}

MPL_TEST(MidiBackendWritesAFile) {
    const Compiler compiler;
    const Compilation result = compiler.compile(oneNoteProject());
    const MidiBackend backend(MidiOptions{480, "Conductor"});

    const std::filesystem::path output =
        std::filesystem::path(MPL_TEST_OUTPUT_DIR) / "backend-write-test.mid";
    std::filesystem::create_directories(output.parent_path());
    backend.emit(result.production, output);

    const std::vector<std::uint8_t> bytes = readBytes(output);
    MPL_CHECK(bytes.size() > std::size_t{22});
    MPL_CHECK_EQ(static_cast<char>(bytes[0]), 'M');
    MPL_CHECK_EQ(hexDump({bytes[0], bytes[1], bytes[2], bytes[3]}), std::string("4d546864"));
}

MPL_TEST(MidiBackendRejectsInvalidOptionsAndChannelExhaustion) {
    MPL_CHECK_THROWS(MidiBackend(MidiOptions{0, "Conductor"}));

    ProductionIR production = twoNoteProduction();
    const TrackIR base = production.tracks.front();
    production.tracks.clear();
    for (int index = 0; index < 16; ++index) {
        TrackIR track = base;
        track.name = "Track " + std::to_string(index);
        production.tracks.push_back(track);
    }

    const MidiBackend backend(MidiOptions{480, "Conductor"});
    MPL_CHECK_THROWS(backend.buildTracks(production));
}
