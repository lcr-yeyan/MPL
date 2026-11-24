#pragma once

#include "mpl/core/Ids.hpp"
#include "mpl/core/TimeTypes.hpp"
#include "mpl/temporal/TemporalIR.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace mpl {

using InstanceId = std::uint32_t;

inline constexpr InstanceId kInvalidInstanceId = 0xFFFFFFFFu;

/// Stable identity of one note playback inside a clip instance.
struct NoteInstanceId {
    InstanceId clipInstance = kInvalidInstanceId;
    std::uint32_t noteIndex = 0;

    std::string str() const;
};

enum class ParameterScope { Track, Clip, Master };

std::string_view parameterScopeName(ParameterScope scope);

enum class ParameterUnit { Generic, Decibel, Ratio, Pan, Milliseconds, Hertz, Seconds };

std::string_view parameterUnitName(ParameterUnit unit);

/// Default value range for a unit. Generic ranges are expressed in the
/// parameter's own units; MIDI mapping is a backend concern.
std::pair<double, double> defaultParameterRange(ParameterUnit unit);

/// Stable, human-readable parameter identity. Names rather than indices are
/// used so that adding/reordering tracks does not change the id.
struct ParameterId {
    ParameterScope scope = ParameterScope::Track;
    std::string track;
    std::string clip;
    std::string parameter;

    std::string str() const;

    friend bool operator==(const ParameterId& lhs, const ParameterId& rhs) {
        return lhs.scope == rhs.scope && lhs.track == rhs.track && lhs.clip == rhs.clip &&
               lhs.parameter == rhs.parameter;
    }
};

struct ParameterTarget {
    ParameterId id;
    ParameterUnit unit = ParameterUnit::Generic;
    double minimum = 0.0;
    double maximum = 1.0;
};

ParameterTarget makeParameterTarget(ParameterScope scope, std::string track, std::string clip,
                                    std::string parameter, ParameterUnit unit);

enum class AutomationMode { Replace, Add, Multiply };

std::string_view automationModeName(AutomationMode mode);

/// Backend-neutral automation binding. `domain` must match the referenced
/// curve's domain; Musical-domain automation is absolute project time,
/// Relative-domain automation is normalized inside a clip instance.
struct AutomationBinding {
    std::string label;
    ParameterTarget target;
    TemporalId curve = kInvalidTemporalId;
    TimeDomain domain = TimeDomain::Musical;
    AutomationMode mode = AutomationMode::Replace;
    bool enabled = true;

    const std::string& parameterKey() const { return target.id.parameter; }
};

/// Offline-evaluable performance expression for one clip instance. All lanes
/// are Relative-domain curves normalized over the instance duration.
struct PerformanceExpression {
    TemporalId velocity = kInvalidTemporalId;  ///< velocity multiplier
    TemporalId timing = kInvalidTemporalId;    ///< onset offset in whole notes
    TemporalId gate = kInvalidTemporalId;      ///< duration multiplier
    TemporalId pan = kInvalidTemporalId;       ///< -1..1; MIDI cannot express per-note pan

    bool empty() const noexcept {
        return velocity == kInvalidTemporalId && timing == kInvalidTemporalId &&
               gate == kInvalidTemporalId && pan == kInvalidTemporalId;
    }
};

}  // namespace mpl
