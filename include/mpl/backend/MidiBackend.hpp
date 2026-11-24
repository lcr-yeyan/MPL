#pragma once

#include "mpl/core/Diagnostic.hpp"
#include "mpl/ir/Automation.hpp"
#include "mpl/ir/ProductionIR.hpp"
#include "mpl/temporal/TemporalIR.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace mpl {

struct MidiOptions {
    /// Ticks per quarter note. This is an export detail only: the Music IR and
    /// Production IR are always expressed in exact musical time.
    int ppq = 480;
    std::string conductorTrackName = "MPL Conductor";
};

enum class MidiEventKind {
    MetaTrackName,
    MetaTempo,
    MetaTimeSignature,
    MetaEndOfTrack,
    ProgramChange,
    ControlChange,
    NoteOn,
    NoteOff,
};

struct MidiEvent {
    MidiEventKind kind = MidiEventKind::MetaEndOfTrack;
    std::int64_t tick = 0;
    int channel = 0;
    int a = 0;   ///< note number, program, controller or time-signature numerator
    int b = 0;   ///< velocity, controller value or time-signature denominator
    std::string text;
    double value = 0.0;  ///< bpm for tempo events
};

struct MidiTrack {
    std::string name;
    std::vector<MidiEvent> events;
};

/// Serializes Production IR to a Standard MIDI File (format 1). The backend is
/// deliberately dumb: channel and program assignment already happened in the
/// production passes.
class MidiBackend {
public:
    explicit MidiBackend(MidiOptions options = {});

    const MidiOptions& options() const noexcept { return options_; }

    /// Absolute tick of a musical position at the configured PPQ.
    std::int64_t tickAt(const Position& position) const;

    /// True when the position maps to an integer tick without rounding. The
    /// Production IR stays exact, so this is an export-time precision check.
    bool isExactAtPpq(const Position& position) const;

    std::vector<MidiTrack> buildTracks(const ProductionIR& production,
                                       const TemporalIR& temporal = TemporalIR{},
                                       std::vector<Diagnostic>* diagnostics = nullptr) const;

    std::vector<std::uint8_t> serialize(const ProductionIR& production,
                                        const TemporalIR& temporal = TemporalIR{},
                                        std::vector<Diagnostic>* diagnostics = nullptr) const;

    /// Human-readable event ledger, used by golden tests.
    std::string describe(const ProductionIR& production,
                         const TemporalIR& temporal = TemporalIR{},
                         std::vector<Diagnostic>* diagnostics = nullptr) const;

    void emit(const ProductionIR& production, const TemporalIR& temporal,
              const std::filesystem::path& path,
              std::vector<Diagnostic>* diagnostics = nullptr) const;

    /// Convenience overload for productions without temporal data.
    void emit(const ProductionIR& production, const std::filesystem::path& path,
              std::vector<Diagnostic>* diagnostics = nullptr) const;

private:
    MidiOptions options_;
};

std::string hexDump(const std::vector<std::uint8_t>& bytes);

}  // namespace mpl
