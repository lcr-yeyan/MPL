#pragma once

#include "mpl/core/Time.hpp"
#include "mpl/core/Provenance.hpp"
#include "mpl/core/ModuleInfo.hpp"
#include "mpl/ir/Assets.hpp"
#include "mpl/ir/Automation.hpp"
#include "mpl/ir/MixIR.hpp"
#include "mpl/ir/MusicIR.hpp"
#include "mpl/temporal/TemporalIR.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace mpl {

/// A placed reference to a Music IR object. Multiple clips per track are how
/// sections and repeated arrangements reach the production timeline.
struct ClipSpec {
    std::string name;
    ObjectId part = kInvalidObjectId;
    Position start;
    PerformanceExpression expression;

    /// Optional stable playback identity supplied by a frontend. When invalid,
    /// lowering allocates the next sequential instance id.
    InstanceId instanceId = kInvalidInstanceId;
};

/// Binds a Music IR object to an output track.
struct TrackSpec {
    std::string name;
    std::string instrument;
    InstrumentRole role = InstrumentRole::Melodic;
    std::vector<ClipSpec> clips;
};

/// A compilation unit: musical objects plus the production-level settings the
/// compiler needs in order to lower them.
struct SectionRecord {
    std::string name;
    int occurrence = 0;
    Position start;
    Duration duration;
};

class Project {
public:
    Project() = default;

    MusicIR& music() noexcept { return music_; }
    const MusicIR& music() const noexcept { return music_; }

    /// Time-varying property definitions (curves) owned by this project.
    TemporalIR& temporal() noexcept { return temporal_; }
    const TemporalIR& temporal() const noexcept { return temporal_; }

    /// Backend-neutral automation bindings.
    std::vector<AutomationBinding>& automation() noexcept { return automation_; }
    const std::vector<AutomationBinding>& automation() const noexcept { return automation_; }

    /// Mix/routing description consumed by audio backends.
    MixSpec& mix() noexcept { return mix_; }
    const MixSpec& mix() const noexcept { return mix_; }

    /// Logical arrangement length, including intentional silence.
    void setLogicalExtent(Duration extent) { logicalExtent_ = extent; }
    const std::optional<Duration>& logicalExtent() const noexcept { return logicalExtent_; }

    /// Frontend function provenance consumed by the trace writer.
    std::vector<FunctionDefinitionRecord>& functionDefinitions() noexcept {
        return functionDefinitions_;
    }
    const std::vector<FunctionDefinitionRecord>& functionDefinitions() const noexcept {
        return functionDefinitions_;
    }
    std::vector<FunctionCallRecord>& functionCalls() noexcept { return functionCalls_; }
    const std::vector<FunctionCallRecord>& functionCalls() const noexcept {
        return functionCalls_;
    }

    void setTempo(double bpm);
    double tempo() const noexcept { return tempo_; }

    /// Tonal context used by key-dependent operators.
    void setKey(Key key);
    const Key& key() const noexcept { return music_.key(); }

    /// Metric context used by meter-dependent operators and by the tempo map.
    void setMeter(int numerator, int denominator);
    const Meter& meter() const noexcept { return music_.meter(); }

    /// Full meter map, including changes and beat grouping. Setting it also
    /// updates the single-meter context to the initial event for backward
    /// compatible operators.
    void setMeterMap(MeterMap map);
    const MeterMap& meterMap() const noexcept { return meterMap_; }

    TrackSpec& addTrack(std::string name, std::string instrument, ObjectId part,
                        Position start = {},
                        InstrumentRole role = InstrumentRole::Melodic);

    /// Declares a track before its clips are known. Useful for frontends that
    /// build the arrangement in a later phase.
    TrackSpec& addTrack(std::string name, std::string instrument, InstrumentRole role);

    /// Adds another clip (usually a section) to an existing track.
    TrackSpec& addClip(std::size_t trackIndex, std::string clipName, ObjectId part,
                       Position start = {});

    const std::vector<TrackSpec>& tracks() const noexcept { return tracks_; }
    std::vector<TrackSpec>& tracks() noexcept { return tracks_; }

    std::vector<ModuleRecord>& modules() noexcept { return modules_; }
    const std::vector<ModuleRecord>& modules() const noexcept { return modules_; }

    std::vector<SectionRecord>& sections() noexcept { return sections_; }
    const std::vector<SectionRecord>& sections() const noexcept { return sections_; }

private:
    MusicIR music_;
    TemporalIR temporal_;
    std::vector<AutomationBinding> automation_;
    MixSpec mix_;
    std::optional<Duration> logicalExtent_;
    std::vector<FunctionDefinitionRecord> functionDefinitions_;
    std::vector<FunctionCallRecord> functionCalls_;
    double tempo_ = 120.0;
    MeterMap meterMap_;
    std::vector<TrackSpec> tracks_;
    std::vector<ModuleRecord> modules_;
    std::vector<SectionRecord> sections_;
};

}  // namespace mpl
