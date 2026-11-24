#pragma once

#include <algorithm>
#include <cstddef>
#include <vector>

namespace mpl {

/// Interleaved float audio buffer. The reference backend uses stereo by
/// default, but mono is supported for tests and sample assets.
struct AudioBuffer {
    int sampleRate = 48000;
    int channels = 2;
    std::vector<float> samples;

    std::size_t frameCount() const {
        return channels > 0 ? samples.size() / static_cast<std::size_t>(channels) : 0;
    }

    double durationSeconds() const {
        return sampleRate > 0 ? static_cast<double>(frameCount()) / sampleRate : 0.0;
    }

    void resizeFrames(std::size_t frames) {
        samples.assign(frames * static_cast<std::size_t>(std::max(channels, 1)), 0.0f);
    }
};

}  // namespace mpl
