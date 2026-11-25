#include "mpl/Project.hpp"

#include "mpl/core/Error.hpp"

#include <utility>

namespace mpl {

void Project::setTempo(double bpm) {
    if (!(bpm > 0.0) || bpm > 1000.0) {
        throw Error("tempo must be in (0, 1000] bpm, got " + std::to_string(bpm), "tempo");
    }
    tempo_ = bpm;
}

void Project::setKey(Key key) {
    music_.setKey(std::move(key));
}

void Project::setMeter(int numerator, int denominator) {
    music_.setMeter(Meter{numerator, denominator});
    meterMap_ = MeterMap(Meter{numerator, denominator});
}

void Project::setMeterMap(MeterMap map) {
    // The Music IR's single-meter context stays the project-declared meter;
    // the full arrangement map is production data. This avoids invalidating
    // already-evaluated syncopate nodes when the first section uses another
    // meter.
    meterMap_ = std::move(map);
}

TrackSpec& Project::addTrack(std::string name, std::string instrument, ObjectId part,
                             Position start, InstrumentRole role) {
    if (!music_.contains(part)) {
        throw Error("track '" + name + "' references unknown music object #" +
                        std::to_string(part),
                    "track");
    }
    TrackSpec spec;
    spec.name = std::move(name);
    spec.instrument = std::move(instrument);
    spec.role = role;
    spec.clips.push_back(ClipSpec{{}, part, start, {}, kInvalidInstanceId});
    tracks_.push_back(std::move(spec));
    return tracks_.back();
}

TrackSpec& Project::addTrack(std::string name, std::string instrument, InstrumentRole role) {
    TrackSpec spec;
    spec.name = std::move(name);
    spec.instrument = std::move(instrument);
    spec.role = role;
    tracks_.push_back(std::move(spec));
    return tracks_.back();
}

TrackSpec& Project::addClip(std::size_t trackIndex, std::string clipName, ObjectId part,
                            Position start) {
    if (trackIndex >= tracks_.size()) {
        throw Error("addClip references unknown track index " + std::to_string(trackIndex),
                    "track");
    }
    if (!music_.contains(part)) {
        throw Error("clip '" + clipName + "' references unknown music object #" +
                        std::to_string(part),
                    "track");
    }
    tracks_[trackIndex].clips.push_back(ClipSpec{std::move(clipName), part, start});
    return tracks_[trackIndex];
}

}  // namespace mpl
