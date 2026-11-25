#include "mpl/backend/MidiBackend.hpp"

#include "mpl/core/Error.hpp"
#include "mpl/music/Pitch.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace mpl {
namespace {

constexpr int kMetaPriority = 0;
constexpr int kProgramPriority = 1;
constexpr int kControlPriority = 2;
constexpr int kNoteOffPriority = 3;
constexpr int kNoteOnPriority = 4;
constexpr int kEndPriority = 9;

int eventPriority(MidiEventKind kind) {
    switch (kind) {
        case MidiEventKind::MetaTrackName:
        case MidiEventKind::MetaTempo:
        case MidiEventKind::MetaTimeSignature:
            return kMetaPriority;
        case MidiEventKind::ProgramChange:
            return kProgramPriority;
        case MidiEventKind::ControlChange:
            return kControlPriority;
        case MidiEventKind::NoteOff:
            return kNoteOffPriority;
        case MidiEventKind::NoteOn:
            return kNoteOnPriority;
        case MidiEventKind::MetaEndOfTrack:
            return kEndPriority;
    }
    return kMetaPriority;
}

void sortEvents(MidiTrack& track) {
    std::stable_sort(track.events.begin(), track.events.end(),
                     [](const MidiEvent& lhs, const MidiEvent& rhs) {
                         if (lhs.tick != rhs.tick) {
                             return lhs.tick < rhs.tick;
                         }
                         return eventPriority(lhs.kind) < eventPriority(rhs.kind);
                     });
}

void appendU16(std::vector<std::uint8_t>& out, std::uint16_t value) {
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFFu));
    out.push_back(static_cast<std::uint8_t>(value & 0xFFu));
}

void appendU32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    out.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFFu));
    out.push_back(static_cast<std::uint8_t>(value & 0xFFu));
}

void appendVarLen(std::vector<std::uint8_t>& out, std::uint32_t value) {
    std::uint8_t buffer[5] = {};
    int count = 0;
    buffer[count++] = static_cast<std::uint8_t>(value & 0x7Fu);
    value >>= 7;
    while (value != 0) {
        buffer[count++] = static_cast<std::uint8_t>((value & 0x7Fu) | 0x80u);
        value >>= 7;
    }
    for (int index = count - 1; index >= 0; --index) {
        out.push_back(buffer[index]);
    }
}

void appendText(std::vector<std::uint8_t>& out, const std::string& text) {
    appendVarLen(out, static_cast<std::uint32_t>(text.size()));
    out.insert(out.end(), text.begin(), text.end());
}

int denominatorExponent(int denominator) {
    int exponent = 0;
    int value = denominator;
    while (value > 1) {
        if ((value & 1) != 0) {
            throw Error("MIDI time signature denominator must be a power of two, got " +
                            std::to_string(denominator),
                        "midi");
        }
        value >>= 1;
        ++exponent;
    }
    if (exponent > 8) {
        throw Error("MIDI time signature denominator is too small", "midi");
    }
    return exponent;
}

std::string bpmText(double bpm) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.3f", bpm);
    return buffer;
}

std::string channelText(int channel) {
    return std::to_string(channel + 1);
}

std::string lower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return text;
}

int gmProgramFor(const std::string& instrument) {
    const std::string name = lower(instrument);
    if (name.find("piano") != std::string::npos) {
        return 0;  // Acoustic Grand Piano
    }
    if (name.find("bass") != std::string::npos) {
        return 38;  // Synth Bass 1
    }
    if (name.find("string") != std::string::npos) {
        return 48;  // String Ensemble 1
    }
    if (name.find("pad") != std::string::npos) {
        return 89;  // Pad 1 (new age)
    }
    if (name.find("lead") != std::string::npos) {
        return 81;  // Lead 1 (square)
    }
    if (name.find("pluck") != std::string::npos) {
        return 84;  // Lead 8 (bass + lead)
    }
    return 0;
}

struct ChannelAssignment {
    int channel = 0;
    int program = 0;
    bool percussion = false;
};

std::vector<ChannelAssignment> assignChannels(const ProductionIR& production) {
    std::vector<ChannelAssignment> assignments;
    assignments.reserve(production.tracks.size());
    int nextChannel = 0;

    for (const TrackIR& track : production.tracks) {
        ChannelAssignment assignment;
        if (track.role == InstrumentRole::Percussion) {
            assignment.channel = 9;
            assignment.percussion = true;
            assignments.push_back(assignment);
            continue;
        }
        while (nextChannel == 9) {
            ++nextChannel;
        }
        if (nextChannel > 15) {
            throw Error("track '" + track.name + "' cannot be assigned a MIDI channel", "midi");
        }
        assignment.channel = nextChannel++;
        assignment.program = gmProgramFor(track.instrument);
        assignments.push_back(assignment);
    }
    return assignments;
}

std::optional<int> ccNumberFor(const std::string& parameter) {
    const std::string name = lower(parameter);
    if (name == "volume") {
        return 7;
    }
    if (name == "pan") {
        return 10;
    }
    if (name == "expression") {
        return 11;
    }
    if (name == "modulation") {
        return 1;
    }
    if (name == "cutoff") {
        return 74;
    }
    if (name == "resonance") {
        return 71;
    }
    if (name == "reverb") {
        return 91;
    }
    if (name == "chorus") {
        return 93;
    }
    if (name.size() > 2 && name[0] == 'c' && name[1] == 'c') {
        int number = 0;
        for (std::size_t index = 2; index < name.size(); ++index) {
            if (name[index] < '0' || name[index] > '9') {
                return std::nullopt;
            }
            number = number * 10 + (name[index] - '0');
            if (number > 127) {
                return std::nullopt;
            }
        }
        if (name.size() > 2) {
            return number;
        }
    }
    return std::nullopt;
}

int midiControlValue(const ParameterTarget& target, double value) {
    double normalized = 0.0;
    if (target.unit == ParameterUnit::Pan) {
        normalized = (value + 1.0) / 2.0;
    } else {
        const double span = target.maximum - target.minimum;
        normalized = span > 0.0 ? (value - target.minimum) / span : 0.5;
    }
    const long rounded = std::lround(normalized * 127.0);
    return static_cast<int>(std::clamp<long>(rounded, 0L, 127L));
}

void report(std::vector<Diagnostic>* diagnostics, Diagnostic diagnostic) {
    if (diagnostics != nullptr) {
        diagnostics->push_back(std::move(diagnostic));
    }
}

std::vector<Rational> automationSampleTimes(const Rational& extent) {
    std::vector<Rational> times;
    Rational step(1, 16);
    const Rational cap = extent / Rational(2048, 1);
    if (step < cap) {
        step = cap;
    }
    Rational time;
    while (time < extent) {
        times.push_back(time);
        time += step;
    }
    times.push_back(extent);
    return times;
}

}  // namespace

MidiBackend::MidiBackend(MidiOptions options) : options_(std::move(options)) {
    if (options_.ppq < 1 || options_.ppq > 32767) {
        throw Error("MIDI PPQ must be in 1..32767, got " + std::to_string(options_.ppq), "midi");
    }
}

std::int64_t MidiBackend::tickAt(const Position& position) const {
    const Rational& value = position.inWholeNotes();
    constexpr int kQuarterNotesPerWhole = 4;
    const long double ticks = static_cast<long double>(value.numerator()) *
                              static_cast<long double>(kQuarterNotesPerWhole * options_.ppq) /
                              static_cast<long double>(value.denominator());
    return static_cast<std::int64_t>(std::llround(ticks));
}

bool MidiBackend::isExactAtPpq(const Position& position) const {
    const Rational scaled =
        position.inWholeNotes() * Rational(static_cast<std::int64_t>(4) * options_.ppq, 1);
    return scaled.isInteger();
}

std::vector<MidiTrack> MidiBackend::buildTracks(const ProductionIR& production,
                                                const TemporalIR& temporal,
                                                std::vector<Diagnostic>* diagnostics) const {
    if (production.tempoMap.empty()) {
        throw Error("cannot export MIDI without a tempo event", "midi");
    }
    if (production.meterMap.empty()) {
        throw Error("cannot export MIDI without a meter event", "midi");
    }

    std::vector<MidiTrack> tracks;

    MidiTrack conductor;
    conductor.name = options_.conductorTrackName;
    conductor.events.push_back(
        MidiEvent{MidiEventKind::MetaTrackName, 0, 0, 0, 0, conductor.name, 0.0});
    std::int64_t conductorLastTick = 0;

    for (const TempoEvent& tempo : production.tempoMap) {
        const std::int64_t tick = tickAt(tempo.position);
        if (tick < 0) {
            throw Error("tempo event starts before tick zero", "midi");
        }
        conductor.events.push_back(
            MidiEvent{MidiEventKind::MetaTempo, tick, 0, 0, 0, {}, tempo.bpm});
        conductorLastTick = std::max(conductorLastTick, tick);
    }

    for (const MeterEvent& meter : production.meterMap) {
        const std::int64_t tick = tickAt(meter.position);
        if (tick < 0) {
            throw Error("meter event starts before tick zero", "midi");
        }
        if (!isExactAtPpq(meter.position)) {
            throw Error("meter event at " + meter.position.toString() +
                            " is not representable at PPQ " + std::to_string(options_.ppq),
                        "midi");
        }
        denominatorExponent(meter.meter.denominator);
        const MetricGrouping grouping = effectiveGrouping(meter);
        conductor.events.push_back(MidiEvent{MidiEventKind::MetaTimeSignature, tick, 0,
                                             meter.meter.numerator, meter.meter.denominator,
                                             grouping.toString(), 0.0});
        conductorLastTick = std::max(conductorLastTick, tick);
    }

    conductor.events.push_back(
        MidiEvent{MidiEventKind::MetaEndOfTrack, conductorLastTick, 0, 0, 0, {}, 0.0});
    sortEvents(conductor);
    tracks.push_back(std::move(conductor));

    const std::vector<ChannelAssignment> assignments = assignChannels(production);
    std::vector<std::int64_t> trackEnds;
    trackEnds.reserve(production.tracks.size());

    for (std::size_t trackIndex = 0; trackIndex < production.tracks.size(); ++trackIndex) {
        const TrackIR& productionTrack = production.tracks[trackIndex];
        const ChannelAssignment& assignment = assignments[trackIndex];
        if (productionTrack.clips.empty()) {
            throw Error("track '" + productionTrack.name + "' has no clips", "midi");
        }

        MidiTrack track;
        track.name = productionTrack.name;
        track.events.push_back(
            MidiEvent{MidiEventKind::MetaTrackName, 0, 0, 0, 0, track.name, 0.0});
        if (!assignment.percussion) {
            track.events.push_back(MidiEvent{MidiEventKind::ProgramChange, 0,
                                             assignment.channel, assignment.program, 0, {}, 0.0});
        }

        std::int64_t lastTick = 0;
        for (const ClipIR& clip : productionTrack.clips) {
            const Rational clipStart = clip.start.inWholeNotes();
            const Rational clipDuration = clip.content.duration().inWholeNotes();
            bool panReported = false;
            bool quantizationReported = false;
            bool velocityClamped = false;

            for (const Note& note : clip.content.notes()) {
                const Position onset(clipStart + note.offset.inWholeNotes());
                const Position end(onset.inWholeNotes() + note.duration.inWholeNotes());

                double velocityScale = 1.0;
                double timingOffset = 0.0;
                double gateScale = 1.0;

                if (clip.expression.velocity != kInvalidTemporalId) {
                    const Rational relative =
                        clipDuration.isZero() ? Rational(0, 1) : note.offset.inWholeNotes() / clipDuration;
                    velocityScale =
                        temporal.sampleAt(clip.expression.velocity, RelativeTime(relative));
                }
                if (clip.expression.timing != kInvalidTemporalId) {
                    const Rational relative =
                        clipDuration.isZero() ? Rational(0, 1) : note.offset.inWholeNotes() / clipDuration;
                    timingOffset =
                        temporal.sampleAt(clip.expression.timing, RelativeTime(relative));
                }
                if (clip.expression.gate != kInvalidTemporalId) {
                    const Rational relative =
                        clipDuration.isZero() ? Rational(0, 1) : note.offset.inWholeNotes() / clipDuration;
                    gateScale = temporal.sampleAt(clip.expression.gate, RelativeTime(relative));
                }
                if (clip.expression.pan != kInvalidTemporalId && !panReported) {
                    panReported = true;
                    report(diagnostics, Diagnostic::warning(
                                            "midi-unsupported-expression",
                                            "track '" + productionTrack.name + "' clip '" +
                                                clip.name +
                                                "' has per-note pan automation, which MIDI "
                                                "cannot express; the lane is ignored"));
                }

                const double rawTicks = timingOffset * 4.0 * options_.ppq;
                const std::int64_t timingTicks = static_cast<std::int64_t>(std::llround(rawTicks));
                if (!quantizationReported &&
                    std::fabs(rawTicks - static_cast<double>(timingTicks)) > 1e-6) {
                    quantizationReported = true;
                    report(diagnostics, Diagnostic::warning(
                                            "midi-quantization",
                                            "track '" + productionTrack.name + "' clip '" +
                                                clip.name +
                                                "' expression timing was quantized to MIDI ticks"));
                }

                const double rawVelocity = static_cast<double>(note.velocity) * velocityScale;
                long velocity = std::lround(rawVelocity);
                if (velocity < 1 || velocity > 127) {
                    if (!velocityClamped) {
                        velocityClamped = true;
                        report(diagnostics, Diagnostic::warning(
                                                "midi-velocity-clamped",
                                                "track '" + productionTrack.name + "' clip '" +
                                                    clip.name +
                                                    "' expression velocity was clamped to 1..127"));
                    }
                    velocity = std::clamp<long>(velocity, 1L, 127L);
                }

                const std::int64_t onTick = std::max<std::int64_t>(
                    0, tickAt(onset) + timingTicks);
                std::int64_t offTick = std::max<std::int64_t>(
                    0, tickAt(end) + timingTicks);
                if (offTick <= onTick) {
                    offTick = onTick + 1;
                }
                if (clip.expression.gate != kInvalidTemporalId) {
                    const std::int64_t span = offTick - onTick;
                    const long gated = std::lround(static_cast<double>(span) * gateScale);
                    offTick = onTick + std::max<long>(1L, gated);
                }

                track.events.push_back(MidiEvent{MidiEventKind::NoteOn, onTick,
                                                 assignment.channel, note.pitch.midi(),
                                                 static_cast<int>(velocity), {}, 0.0});
                track.events.push_back(MidiEvent{MidiEventKind::NoteOff, offTick,
                                                 assignment.channel, note.pitch.midi(), 0, {},
                                                 0.0});
                lastTick = std::max(lastTick, offTick);
            }
        }
        trackEnds.push_back(lastTick);
        tracks.push_back(std::move(track));
    }

    // Automation is sampled into CC events. Bindings that share a target and
    // controller are combined with Replace/Add/Multiply semantics; unsupported
    // targets are reported, never silently dropped.
    struct AutomationGroup {
        std::size_t trackIndex = 0;
        int controller = 0;
        std::string parameter;
        ParameterTarget target;
        std::vector<const AutomationBinding*> bindings;
    };

    std::vector<AutomationGroup> groups;
    for (const AutomationBinding& binding : production.automation) {
        if (!binding.enabled) {
            continue;
        }
        if (binding.target.id.scope != ParameterScope::Track) {
            report(diagnostics, Diagnostic::warning(
                                    "midi-unsupported-automation",
                                    "automation '" + binding.target.id.str() +
                                        "' targets a non-track scope, which the MIDI backend "
                                        "cannot express; skipped"));
            continue;
        }
        if (binding.domain != TimeDomain::Musical) {
            report(diagnostics, Diagnostic::warning(
                                    "midi-unsupported-automation",
                                    "automation '" + binding.target.id.str() +
                                        "' uses a non-musical time domain; MIDI CC export only "
                                        "supports absolute musical time"));
            continue;
        }

        std::size_t trackIndex = 0;
        bool found = false;
        for (; trackIndex < production.tracks.size(); ++trackIndex) {
            if (production.tracks[trackIndex].name == binding.target.id.track) {
                found = true;
                break;
            }
        }
        if (!found) {
            report(diagnostics, Diagnostic::error(
                                    "midi-unknown-track",
                                    "automation '" + binding.target.id.str() +
                                        "' references an unknown track"));
            continue;
        }

        const std::optional<int> controller = ccNumberFor(binding.target.id.parameter);
        if (!controller.has_value()) {
            report(diagnostics, Diagnostic::warning(
                                    "midi-unsupported-parameter",
                                    "automation parameter '" + binding.target.id.parameter +
                                        "' has no MIDI CC mapping; skipped"));
            continue;
        }
        if (!temporal.contains(binding.curve)) {
            report(diagnostics, Diagnostic::error(
                                    "midi-unknown-curve",
                                    "automation '" + binding.target.id.str() +
                                        "' references an unknown curve"));
            continue;
        }
        const Curve& curve = temporal.evaluate(binding.curve);
        if (curve.domain != TimeDomain::Musical) {
            report(diagnostics, Diagnostic::warning(
                                    "midi-unsupported-automation",
                                    "automation '" + binding.target.id.str() +
                                        "' curve is not in the musical domain; skipped"));
            continue;
        }

        auto group = std::find_if(groups.begin(), groups.end(),
                                  [&](const AutomationGroup& candidate) {
                                      return candidate.trackIndex == trackIndex &&
                                             candidate.controller == controller.value() &&
                                             candidate.parameter == binding.target.id.parameter;
                                  });
        if (group == groups.end()) {
            AutomationGroup created;
            created.trackIndex = trackIndex;
            created.controller = controller.value();
            created.parameter = binding.target.id.parameter;
            created.target = binding.target;
            groups.push_back(std::move(created));
            group = std::prev(groups.end());
        }
        group->bindings.push_back(&binding);
    }

    for (const AutomationGroup& group : groups) {
        Rational extent;
        for (const AutomationBinding* binding : group.bindings) {
            extent = std::max(extent, temporal.extent(binding->curve));
        }

        MidiTrack& midiTrack = tracks[group.trackIndex + 1];
        std::int64_t lastTick = trackEnds[group.trackIndex];
        bool rangeReported = false;

        for (const Rational& time : automationSampleTimes(extent)) {
            bool hasReplace = false;
            bool hasAdd = false;
            bool hasMultiply = false;
            double replaceValue = 0.0;
            double addValue = 0.0;
            double multiplyValue = 1.0;

            for (const AutomationBinding* binding : group.bindings) {
                const Curve& curve = temporal.evaluate(binding->curve);
                const double value = curve.sample(time);
                switch (binding->mode) {
                    case AutomationMode::Replace:
                        hasReplace = true;
                        replaceValue = value;
                        break;
                    case AutomationMode::Add:
                        hasAdd = true;
                        addValue += value;
                        break;
                    case AutomationMode::Multiply:
                        hasMultiply = true;
                        multiplyValue *= value;
                        break;
                }
            }

            // Replace supplies the base; Add is relative to zero and Multiply
            // is relative to one when no Replace binding exists.
            double combined = hasReplace ? replaceValue : (hasAdd ? 0.0 : 1.0);
            combined += addValue;
            if (hasMultiply) {
                combined *= multiplyValue;
            }
            if (combined < group.target.minimum || combined > group.target.maximum) {
                if (!rangeReported) {
                    rangeReported = true;
                    report(diagnostics, Diagnostic::warning(
                                            "midi-automation-range",
                                            "combined automation for '" +
                                                group.target.id.str() +
                                                "' left the parameter range and was clamped"));
                }
                combined = std::clamp(combined, group.target.minimum, group.target.maximum);
            }

            const std::int64_t tick = tickAt(Position(time));
            if (tick < 0) {
                continue;
            }
            midiTrack.events.push_back(
                MidiEvent{MidiEventKind::ControlChange, tick,
                          assignments[group.trackIndex].channel, group.controller,
                          midiControlValue(group.bindings.front()->target, combined), {}, 0.0});
            lastTick = std::max(lastTick, tick);
        }
        trackEnds[group.trackIndex] = lastTick;
    }

    for (std::size_t trackIndex = 0; trackIndex < production.tracks.size(); ++trackIndex) {
        MidiTrack& track = tracks[trackIndex + 1];
        track.events.push_back(MidiEvent{MidiEventKind::MetaEndOfTrack, trackEnds[trackIndex],
                                         0, 0, 0, {}, 0.0});
        sortEvents(track);
    }

    return tracks;
}

std::vector<std::uint8_t> MidiBackend::serialize(const ProductionIR& production,
                                                 const TemporalIR& temporal,
                                                 std::vector<Diagnostic>* diagnostics) const {
    const std::vector<MidiTrack> tracks = buildTracks(production, temporal, diagnostics);

    std::vector<std::uint8_t> bytes;
    bytes.insert(bytes.end(), {'M', 'T', 'h', 'd'});
    appendU32(bytes, 6);
    appendU16(bytes, 1);  // format 1: one conductor track plus one track per instrument
    appendU16(bytes, static_cast<std::uint16_t>(tracks.size()));
    appendU16(bytes, static_cast<std::uint16_t>(options_.ppq));

    for (const MidiTrack& track : tracks) {
        std::vector<std::uint8_t> data;
        std::int64_t previousTick = 0;
        for (const MidiEvent& event : track.events) {
            if (event.tick < previousTick) {
                throw Error("MIDI events are not sorted in track '" + track.name + "'", "midi");
            }
            const std::int64_t delta = event.tick - previousTick;
            if (delta > 0x0FFFFFFF) {
                throw Error("MIDI delta time exceeds the variable-length limit", "midi");
            }
            appendVarLen(data, static_cast<std::uint32_t>(delta));
            previousTick = event.tick;

            switch (event.kind) {
                case MidiEventKind::MetaTrackName:
                    data.push_back(0xFF);
                    data.push_back(0x03);
                    appendText(data, event.text);
                    break;
                case MidiEventKind::MetaTempo: {
                    const double microsecondsPerQuarter = 60000000.0 / event.value;
                    const auto microseconds = static_cast<std::uint32_t>(
                        std::llround(microsecondsPerQuarter));
                    if (microseconds == 0 || microseconds > 0xFFFFFFu) {
                        throw Error("tempo cannot be represented in a MIDI file", "midi");
                    }
                    data.push_back(0xFF);
                    data.push_back(0x51);
                    data.push_back(0x03);
                    data.push_back(static_cast<std::uint8_t>((microseconds >> 16) & 0xFFu));
                    data.push_back(static_cast<std::uint8_t>((microseconds >> 8) & 0xFFu));
                    data.push_back(static_cast<std::uint8_t>(microseconds & 0xFFu));
                    break;
                }
                case MidiEventKind::MetaTimeSignature: {
                    data.push_back(0xFF);
                    data.push_back(0x58);
                    data.push_back(0x04);
                    data.push_back(static_cast<std::uint8_t>(event.a));
                    data.push_back(static_cast<std::uint8_t>(denominatorExponent(event.b)));
                    data.push_back(24);
                    data.push_back(8);
                    break;
                }
                case MidiEventKind::MetaEndOfTrack:
                    data.push_back(0xFF);
                    data.push_back(0x2F);
                    data.push_back(0x00);
                    break;
                case MidiEventKind::ProgramChange:
                    data.push_back(static_cast<std::uint8_t>(0xC0 | (event.channel & 0x0F)));
                    data.push_back(static_cast<std::uint8_t>(event.a & 0x7F));
                    break;
                case MidiEventKind::ControlChange:
                    data.push_back(static_cast<std::uint8_t>(0xB0 | (event.channel & 0x0F)));
                    data.push_back(static_cast<std::uint8_t>(event.a & 0x7F));
                    data.push_back(static_cast<std::uint8_t>(event.b & 0x7F));
                    break;
                case MidiEventKind::NoteOn:
                    data.push_back(static_cast<std::uint8_t>(0x90 | (event.channel & 0x0F)));
                    data.push_back(static_cast<std::uint8_t>(event.a & 0x7F));
                    data.push_back(static_cast<std::uint8_t>(event.b & 0x7F));
                    break;
                case MidiEventKind::NoteOff:
                    data.push_back(static_cast<std::uint8_t>(0x80 | (event.channel & 0x0F)));
                    data.push_back(static_cast<std::uint8_t>(event.a & 0x7F));
                    data.push_back(static_cast<std::uint8_t>(event.b & 0x7F));
                    break;
            }
        }

        bytes.insert(bytes.end(), {'M', 'T', 'r', 'k'});
        appendU32(bytes, static_cast<std::uint32_t>(data.size()));
        bytes.insert(bytes.end(), data.begin(), data.end());
    }

    return bytes;
}

std::string MidiBackend::describe(const ProductionIR& production, const TemporalIR& temporal,
                                  std::vector<Diagnostic>* diagnostics) const {
    const std::vector<MidiTrack> tracks = buildTracks(production, temporal, diagnostics);
    std::string out = "# MPL MIDI ledger (ppq=" + std::to_string(options_.ppq) + ")\n";

    for (std::size_t index = 0; index < tracks.size(); ++index) {
        const MidiTrack& track = tracks[index];
        out += "\ntrack[" + std::to_string(index) + "] \"" + track.name + "\"\n";
        for (const MidiEvent& event : track.events) {
            out += "  @" + std::to_string(event.tick) + " ";
            switch (event.kind) {
                case MidiEventKind::MetaTrackName:
                    out += "name \"" + event.text + "\"\n";
                    break;
                case MidiEventKind::MetaTempo:
                    out += "tempo " + bpmText(event.value) + " bpm\n";
                    break;
                case MidiEventKind::MetaTimeSignature:
                    out += "meter " + std::to_string(event.a) + "/" + std::to_string(event.b);
                    if (!event.text.empty()) {
                        out += " (" + event.text + ")";
                    }
                    out += "\n";
                    break;
                case MidiEventKind::MetaEndOfTrack:
                    out += "end-of-track\n";
                    break;
                case MidiEventKind::ProgramChange:
                    out += "program ch=" + channelText(event.channel) +
                           " program=" + std::to_string(event.a) + "\n";
                    break;
                case MidiEventKind::ControlChange:
                    out += "cc ch=" + channelText(event.channel) +
                           " controller=" + std::to_string(event.a) +
                           " value=" + std::to_string(event.b) + "\n";
                    break;
                case MidiEventKind::NoteOn:
                    out += "note-on ch=" + channelText(event.channel) + " " +
                           Pitch::fromMidi(event.a).name() + " vel=" +
                           std::to_string(event.b) + "\n";
                    break;
                case MidiEventKind::NoteOff:
                    out += "note-off ch=" + channelText(event.channel) + " " +
                           Pitch::fromMidi(event.a).name() + "\n";
                    break;
            }
        }
    }
    return out;
}

void MidiBackend::emit(const ProductionIR& production, const TemporalIR& temporal,
                       const std::filesystem::path& path,
                       std::vector<Diagnostic>* diagnostics) const {
    const std::vector<std::uint8_t> bytes = serialize(production, temporal, diagnostics);
    const std::filesystem::path parent = path.parent_path();
    if (!parent.empty()) {
        std::error_code error;
        std::filesystem::create_directories(parent, error);
        if (error) {
            throw Error("cannot create output directory '" + parent.string() + "': " +
                            error.message(),
                        "io");
        }
    }

    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) {
        throw Error("cannot open '" + path.string() + "' for writing", "io");
    }
    stream.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    if (!stream) {
        throw Error("failed while writing '" + path.string() + "'", "io");
    }
}

void MidiBackend::emit(const ProductionIR& production, const std::filesystem::path& path,
                       std::vector<Diagnostic>* diagnostics) const {
    emit(production, TemporalIR{}, path, diagnostics);
}

std::string hexDump(const std::vector<std::uint8_t>& bytes) {
    static constexpr char kDigits[] = "0123456789abcdef";
    std::string out;
    out.reserve(bytes.size() * 2);
    for (const std::uint8_t byte : bytes) {
        out.push_back(kDigits[(byte >> 4) & 0x0F]);
        out.push_back(kDigits[byte & 0x0F]);
    }
    return out;
}

}  // namespace mpl
