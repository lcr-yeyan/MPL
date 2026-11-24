#include "mpl/examples/CoreDemo.hpp"

#include "mpl/core/Rational.hpp"
#include "mpl/music/Harmony.hpp"
#include "mpl/music/Key.hpp"
#include "mpl/music/Pitch.hpp"

#include <utility>
#include <vector>

namespace mpl::examples {

using namespace mpl::literals;

Project buildCoreDemoProject() {
    Project project;
    project.setTempo(160.0);
    project.setMeter(4, 4);
    project.setKey(Key::parse("F minor"));

    MusicIR& music = project.music();

    // One bar of 4/4: eighth, eighth, quarter, quarter, quarter.
    const ObjectId motif = music.melody(
        {
            {"F4"_p, Rational(1, 8)},
            {"Ab4"_p, Rational(1, 8)},
            {"C5"_p, Rational(1, 4)},
            {"Eb5"_p, Rational(1, 4)},
            {"C5"_p, Rational(1, 4)},
        },
        "A");

    const ObjectId transposed = music.transpose(motif, Interval::octaves(1), "B = A + 12st");
    const ObjectId reversed = music.reverse(motif, "C = reverse(A)");
    const ObjectId phrase = music.sequence({motif, transposed, reversed, motif}, "Main");
    const ObjectId bassLine = music.transpose(phrase, Interval::octaves(-2), "Bass");

    project.addTrack("Piano", "GrandPiano", phrase);
    project.addTrack("Bass", "SubBass", bassLine, {}, InstrumentRole::Bass);
    return project;
}

Project buildSuiteDemoProject() {
    Project project;
    project.setTempo(160.0);
    project.setMeter(4, 4);
    project.setKey(Key::parse("F minor"));

    MusicIR& music = project.music();

    const ObjectId a = music.melody(
        {
            {"F4"_p, Rational(1, 8)},
            {"Ab4"_p, Rational(1, 8)},
            {"C5"_p, Rational(1, 4)},
            {"Eb5"_p, Rational(1, 4)},
            {"C5"_p, Rational(1, 4)},
        },
        "A");
    const ObjectId a1 = music.diatonicTranspose(a, 1, "A degree+1");
    const ObjectId b = music.diatonicTranspose(a, 2, "B degree+2");
    const ObjectId reversedA = music.reverse(a, "rev(A)");
    const ObjectId reversedB = music.reverse(b, "rev(B)");

    const ObjectId introPiano = music.sequence({a, a1, reversedA, a}, "Intro Piano");
    const ObjectId dropAPiano =
        music.sequence({a, b, a1, reversedB, a, b, reversedA, a1}, "Drop A Piano");
    const ObjectId dropBPiano =
        music.sequence({b, a1, a, b, reversedB, a1, a, reversedA}, "Drop B Piano");
    const ObjectId outroPiano = music.sequence({a, reversedA, a1, a}, "Outro Piano");

    // Harmonic skeleton: i - VI - III - VII in F minor, one chord per bar.
    const ObjectId rootF = music.melody({{"F2"_p, Rational(1, 1)}}, "i");
    const ObjectId rootDb = music.melody({{"Db3"_p, Rational(1, 1)}}, "VI");
    const ObjectId rootAb = music.melody({{"Ab2"_p, Rational(1, 1)}}, "III");
    const ObjectId rootEb = music.melody({{"Eb3"_p, Rational(1, 1)}}, "VII");
    const ObjectId progression =
        music.sequence({rootF, rootDb, rootAb, rootEb}, "i-VI-III-VII");
    const ObjectId progressionEight = music.repeat(progression, 2, "Progression x2");

    const ObjectId padVoicing = music.harmonize(progressionEight, {2, 4}, "Pad Voicing");
    const ObjectId bassRoots =
        music.transpose(progressionEight, Interval::octaves(-1), "Bass Roots");

    const ObjectId syncopatedA = music.syncopate(a, Rational(1, 8), "Syncopated A");
    const ObjectId lead = music.repeat(syncopatedA, 8, "Lead 8 bars");

    std::vector<Note> drumNotes;
    const auto addDrum = [&drumNotes](int midi, const Rational& offset, const Rational& length,
                                      int velocity) {
        Note note;
        note.pitch = Pitch::fromMidi(midi);
        note.offset = Duration(offset);
        note.duration = Duration(length);
        note.velocity = velocity;
        drumNotes.push_back(note);
    };
    addDrum(36, Rational(0, 1), Rational(1, 8), 110);    // kick
    addDrum(38, Rational(1, 4), Rational(1, 8), 100);    // snare
    addDrum(36, Rational(1, 2), Rational(1, 8), 110);    // kick
    addDrum(38, Rational(3, 4), Rational(1, 8), 100);    // snare
    for (int eighth = 0; eighth < 8; ++eighth) {
        addDrum(42, Rational(eighth, 8), Rational(1, 16), 72);  // hi-hat
    }
    const ObjectId drumPattern = music.notes(drumNotes, Duration(Rational(1, 1)), "Drum Pattern");
    const ObjectId drums = music.repeat(drumPattern, 8, "Drums 8 bars");

    const Meter meter{4, 4};
    const Position dropA = Position::fromBars(Rational(4, 1), meter);
    const Position dropB = Position::fromBars(Rational(12, 1), meter);
    const Position outro = Position::fromBars(Rational(20, 1), meter);

    project.addTrack("Piano", "GrandPiano", introPiano, {}, InstrumentRole::Melodic);
    project.addClip(0, "Drop A", dropAPiano, dropA);
    project.addClip(0, "Drop B", dropBPiano, dropB);
    project.addClip(0, "Outro", outroPiano, outro);

    project.addTrack("Pad", "WarmPad", padVoicing, dropA, InstrumentRole::Harmony)
        .clips.front()
        .name = "Drop A";
    project.addClip(1, "Drop B", padVoicing, dropB);

    project.addTrack("Bass", "SubBass", bassRoots, dropA, InstrumentRole::Bass)
        .clips.front()
        .name = "Drop A";
    project.addClip(2, "Drop B", bassRoots, dropB);

    project.addTrack("Lead", "LeadSynth", lead, dropB, InstrumentRole::Melodic)
        .clips.front()
        .name = "Drop B";

    project.addTrack("Drums", "DrumKit", drums, dropA, InstrumentRole::Percussion)
        .clips.front()
        .name = "Drop A";
    project.addClip(4, "Drop B", drums, dropB);

    return project;
}

Project buildTemporalDemoProject() {
    Project project;
    project.setTempo(160.0);
    project.setMeter(4, 4);
    project.setKey(Key::parse("F minor"));

    MusicIR& music = project.music();
    TemporalIR& temporal = project.temporal();

    const ObjectId a = music.melody(
        {
            {"F4"_p, Rational(1, 8)},
            {"Ab4"_p, Rational(1, 8)},
            {"C5"_p, Rational(1, 4)},
            {"Eb5"_p, Rational(1, 4)},
            {"C5"_p, Rational(1, 4)},
        },
        "A");

    const ObjectId rootF = music.melody({{"F2"_p, Rational(1, 1)}}, "i");
    const ObjectId rootDb = music.melody({{"Db3"_p, Rational(1, 1)}}, "VI");
    const ObjectId rootAb = music.melody({{"Ab2"_p, Rational(1, 1)}}, "III");
    const ObjectId rootEb = music.melody({{"Eb3"_p, Rational(1, 1)}}, "VII");
    const ObjectId progression =
        music.sequence({rootF, rootDb, rootAb, rootEb}, "i-VI-III-VII");
    const ObjectId progressionEight = music.repeat(progression, 2, "Progression x2");
    const ObjectId padVoicing = music.harmonize(progressionEight, {2, 4}, "Pad Voicing");
    const ObjectId bassRoots =
        music.transpose(progressionEight, Interval::octaves(-1), "Bass Roots");

    const TemporalId crescendo = temporal.linear(0.55, 1.15, TimeDomain::Relative,
                                                 Rational(1, 1), "Crescendo");
    const TemporalId decrescendo = temporal.smoothstep(1.15, 0.5, TimeDomain::Relative,
                                                       Rational(1, 1), "Decrescendo");
    const TemporalId humanize = temporal.keyframes(
        {
            CurvePoint{Rational(0, 1), 0.0, Interpolation::Linear},
            CurvePoint{Rational(1, 4), 0.04, Interpolation::Step},
            CurvePoint{Rational(3, 4), -0.03, Interpolation::Linear},
            CurvePoint{Rational(1, 1), 0.0, Interpolation::Linear},
        },
        TimeDomain::Relative, Rational(1, 1), "Humanize");
    const TemporalId steady = temporal.constant(0.9, TimeDomain::Relative, Rational(1, 1),
                                                "Velocity 0.9");
    const TemporalId gate = temporal.constant(1.0, TimeDomain::Relative, Rational(1, 1), "Gate");
    const TemporalId perNotePan = temporal.constant(0.0, TimeDomain::Relative, Rational(1, 1),
                                                    "Per-note pan");

    const TemporalId volume = temporal.linear(0.55, 1.0, TimeDomain::Musical,
                                              Rational(4, 1), "Volume 4 bars");
    const TemporalId stretchedVolume = temporal.stretch(volume, Rational(2, 1), "Volume x2");
    const TemporalId reversedVolume = temporal.reverse(volume, "Volume reversed");

    auto& piano = project.addTrack("Piano", "GrandPiano", a, {}, InstrumentRole::Melodic);
    piano.clips.front().name = "A Linear Crescendo";
    piano.clips.front().expression.velocity = crescendo;

    auto& second = project.addClip(0, "A Smooth Decrescendo", a, Position(Rational(1, 1)));
    second.clips.back().expression.velocity = decrescendo;

    auto& third = project.addClip(0, "A Humanized", a, Position(Rational(2, 1)));
    third.clips.back().expression = PerformanceExpression{steady, humanize, gate, perNotePan};

    project.addTrack("Pad", "WarmPad", padVoicing, {}, InstrumentRole::Harmony)
        .clips.front()
        .name = "Pad";
    project.addTrack("Bass", "SubBass", bassRoots, {}, InstrumentRole::Bass)
        .clips.front()
        .name = "Bass";

    project.automation().push_back(AutomationBinding{
        "Piano volume", makeParameterTarget(ParameterScope::Track, "Piano", "", "volume",
                                            ParameterUnit::Ratio),
        volume, TimeDomain::Musical, AutomationMode::Replace, true});
    project.automation().push_back(AutomationBinding{
        "Piano bloom", makeParameterTarget(ParameterScope::Track, "Piano", "", "modulation",
                                           ParameterUnit::Ratio),
        stretchedVolume, TimeDomain::Musical, AutomationMode::Add, true});
    project.automation().push_back(AutomationBinding{
        "Piano expression", makeParameterTarget(ParameterScope::Track, "Piano", "", "expression",
                                                ParameterUnit::Ratio),
        reversedVolume, TimeDomain::Musical, AutomationMode::Replace, true});
    project.automation().push_back(AutomationBinding{
        "Master fade", makeParameterTarget(ParameterScope::Master, "", "", "volume",
                                           ParameterUnit::Ratio),
        reversedVolume, TimeDomain::Musical, AutomationMode::Replace, true});

    return project;
}

}  // namespace mpl::examples
