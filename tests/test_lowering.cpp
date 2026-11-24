#include "TestHarness.hpp"

#include "mpl/Compiler.hpp"
#include "mpl/backend/MidiBackend.hpp"

#include <string>
#include <vector>

using namespace mpl;
using namespace mpl::literals;

namespace {

Project singleTrackProject() {
    Project project;
    project.setTempo(160.0);
    project.setMeter(4, 4);

    MusicIR& music = project.music();
    const ObjectId a = music.melody({
                                       {"F4"_p, Rational(1, 8)},
                                       {"Ab4"_p, Rational(1, 8)},
                                   },
                                   "A");
    const ObjectId phrase = music.sequence({a, a}, "Phrase");
    project.addTrack("Piano", "GrandPiano", phrase);
    return project;
}

bool hasDiagnostic(const Compilation& compilation, const std::string& code) {
    for (const Diagnostic& diagnostic : compilation.diagnostics) {
        if (diagnostic.code == code) {
            return true;
        }
    }
    return false;
}

std::string ledgerOf(const Compilation& compilation) {
    const MidiBackend backend(MidiOptions{480, "Conductor"});
    return backend.describe(compilation.production);
}

}  // namespace

MPL_TEST(LoweringBuildsTracksClipsAndTimeline) {
    const Compiler compiler;
    const Compilation result = compiler.compile(singleTrackProject());

    MPL_CHECK(!result.hasErrors());
    MPL_CHECK_EQ(result.production.tempoMap.size(), std::size_t{1});
    MPL_CHECK_NEAR(result.production.tempoMap.front().bpm, 160.0, 1e-9);
    MPL_CHECK_EQ(result.production.meterMap.size(), std::size_t{1});
    MPL_CHECK(result.production.meterMap.front().meter.toString() == "4/4");

    MPL_CHECK_EQ(result.production.tracks.size(), std::size_t{1});
    const TrackIR& track = result.production.tracks.front();
    MPL_CHECK_EQ(track.name, std::string("Piano"));
    MPL_CHECK_EQ(track.instrument, std::string("GrandPiano"));
    MPL_CHECK(track.role == InstrumentRole::Melodic);
    MPL_CHECK_EQ(track.clips.size(), std::size_t{1});
    MPL_CHECK(track.clips.front().start == Position{});
    MPL_CHECK(track.clips.front().content.duration() == Duration(Rational(1, 2)));
    MPL_CHECK_EQ(track.clips.front().sourceLabel, std::string("Phrase"));

    MPL_CHECK(result.production.totalDuration() == Duration(Rational(1, 2)));
    MPL_CHECK_EQ(result.production.noteCount(), std::size_t{4});
    MPL_CHECK_EQ(result.production.clipCount(), std::size_t{1});
}

MPL_TEST(LoweringBindsAssetsWithoutBackendDetails) {
    Project project = singleTrackProject();
    MusicIR& music = project.music();
    const ObjectId bass = music.transpose(ObjectId{1}, Interval::octaves(-2), "Bass");
    project.addTrack("Bass", "SubBass", bass, Position::fromBars(Rational(1, 1), Meter{4, 4}),
                     InstrumentRole::Bass);

    const Compiler compiler;
    const Compilation result = compiler.compile(std::move(project));
    MPL_CHECK(!result.hasErrors());
    MPL_CHECK_EQ(result.production.tracks.size(), std::size_t{2});

    const TrackIR& piano = result.production.tracks[0];
    const TrackIR& bassTrack = result.production.tracks[1];
    MPL_CHECK_EQ(piano.assetSlots.size(), std::size_t{1});
    MPL_CHECK_EQ(piano.assetSlots.front().name, std::string("main"));
    MPL_CHECK_EQ(piano.assetSlots.front().preferredAsset, std::string("GrandPiano"));
    MPL_CHECK_EQ(piano.assetBindings.size(), std::size_t{1});
    MPL_CHECK(piano.assetBindings.front().isResolved());
    MPL_CHECK_EQ(piano.assetBindings.front().provider, std::string("symbolic"));
    MPL_CHECK(piano.findBinding("main") != nullptr);

    MPL_CHECK(bassTrack.role == InstrumentRole::Bass);
    MPL_CHECK(bassTrack.clips.front().start == Position(Rational(1, 1)));
    MPL_CHECK(result.production.totalDuration() == Duration(Rational(3, 2)));
}

MPL_TEST(MidiExportAssignsChannelsAndPrograms) {
    Project project = singleTrackProject();
    MusicIR& music = project.music();
    const ObjectId bass = music.transpose(ObjectId{1}, Interval::octaves(-2), "Bass");
    project.addTrack("Bass", "SubBass", bass, {}, InstrumentRole::Bass);

    const Compiler compiler;
    const Compilation result = compiler.compile(std::move(project));
    const std::string ledger = ledgerOf(result);

    MPL_CHECK(ledger.find("program ch=1 program=0") != std::string::npos);
    MPL_CHECK(ledger.find("program ch=2 program=38") != std::string::npos);
    MPL_CHECK(ledger.find("note-on ch=2 F2") != std::string::npos);
}

MPL_TEST(MidiExportReservesChannelTenForPercussion) {
    Project project;
    project.setTempo(140.0);
    project.setMeter(4, 4);
    MusicIR& music = project.music();

    const ObjectId drums = music.melody({{"C3"_p, Rational(1, 4)}}, "Drums");
    const ObjectId piano = music.melody({{"F4"_p, Rational(1, 1)}}, "Piano");
    project.addTrack("Drums", "DrumKit", drums, {}, InstrumentRole::Percussion);
    project.addTrack("Piano", "GrandPiano", piano);

    const Compiler compiler;
    const Compilation result = compiler.compile(std::move(project));
    MPL_CHECK(!result.hasErrors());

    const std::string ledger = ledgerOf(result);
    MPL_CHECK(ledger.find("note-on ch=10 C3") != std::string::npos);
    MPL_CHECK(ledger.find("program ch=10") == std::string::npos);
    MPL_CHECK(ledger.find("program ch=1 program=0") != std::string::npos);
}

MPL_TEST(LoweringSupportsMultipleClipsPerTrack) {
    Project project = singleTrackProject();
    MusicIR& music = project.music();
    const ObjectId drop = music.reverse(ObjectId{1}, "Drop");
    project.addClip(0, "Drop", drop, Position::fromBars(Rational(1, 1), Meter{4, 4}));

    const Compiler compiler;
    const Compilation result = compiler.compile(std::move(project));
    MPL_CHECK(!result.hasErrors());
    MPL_CHECK_EQ(result.production.tracks.size(), std::size_t{1});
    const TrackIR& track = result.production.tracks.front();
    MPL_CHECK_EQ(track.clips.size(), std::size_t{2});
    MPL_CHECK_EQ(track.clips[0].name, std::string("Phrase"));
    MPL_CHECK_EQ(track.clips[1].name, std::string("Drop"));
    MPL_CHECK(track.clips[1].start == Position(Rational(1, 1)));
    MPL_CHECK(result.production.totalDuration() == Duration(Rational(3, 2)));
    MPL_CHECK_EQ(result.production.clipCount(), std::size_t{2});
}

MPL_TEST(ValidationReportsStructuralProblems) {
    Project project;
    project.setTempo(120.0);
    project.setMeter(4, 4);
    MusicIR& music = project.music();
    const ObjectId part = music.melody({{"C4"_p, Rational(1, 4)}}, "A");
    project.addTrack("Piano", "GrandPiano", part);
    project.addTrack("Piano", "GrandPiano", part);

    const Compiler compiler;
    const Compilation result = compiler.compile(std::move(project));
    MPL_CHECK(result.hasErrors());
    MPL_CHECK(hasDiagnostic(result, "duplicate-track"));

    const Compilation empty = compiler.compile(Project{});
    MPL_CHECK(empty.hasErrors());
    MPL_CHECK(hasDiagnostic(empty, "no-tracks"));
}

MPL_TEST(ValidationWarnsAboutSilentClips) {
    Project project;
    MusicIR& music = project.music();
    const ObjectId silence = music.notes({}, Duration(Rational(4, 1)), "Silence");
    project.addTrack("Piano", "GrandPiano", silence);

    const Compiler compiler;
    const Compilation result = compiler.compile(std::move(project));
    MPL_CHECK(!result.hasErrors());
    MPL_CHECK_EQ(result.warningCount(), std::size_t{1});
    MPL_CHECK(hasDiagnostic(result, "silent-clip"));
}

MPL_TEST(TraceRecordsContextGraphPassesAndProvenance) {
    Project project = singleTrackProject();
    project.setKey(Key::parse("F minor"));
    const Compiler compiler;
    const Compilation result = compiler.compile(std::move(project));
    const std::string trace = result.toTraceJson();

    MPL_CHECK(trace.find("\"format\": \"mpl.trace\"") != std::string::npos);
    MPL_CHECK(trace.find("\"key\": \"F minor\"") != std::string::npos);
    MPL_CHECK(trace.find("\"objects\": 2") != std::string::npos);
    MPL_CHECK(trace.find("\"evaluated\": 2") != std::string::npos);
    MPL_CHECK(trace.find("\"validate-production\"") != std::string::npos);
    MPL_CHECK(trace.find("\"validate-assets\"") != std::string::npos);
    MPL_CHECK(trace.find("\"durationWholeNotes\": \"1/2\"") != std::string::npos);
    MPL_CHECK(trace.find("\"provenance\"") != std::string::npos);
    MPL_CHECK(trace.find("\"derivation\"") != std::string::npos);
    MPL_CHECK(trace.find("\"originLabel\": \"A\"") != std::string::npos);
}
