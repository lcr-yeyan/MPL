#include "mpl/audio/WavFile.hpp"

#include "mpl/core/Error.hpp"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace mpl {
namespace {

std::uint16_t readU16(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    return static_cast<std::uint16_t>(bytes[offset] | (bytes[offset + 1] << 8));
}

std::uint32_t readU32(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    return static_cast<std::uint32_t>(bytes[offset]) |
           (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
           (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
}

std::int16_t readS16(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    return static_cast<std::int16_t>(readU16(bytes, offset));
}

float readF32(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    const std::uint32_t bits = readU32(bytes, offset);
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(float));
    return value;
}

void appendU16(std::vector<std::uint8_t>& out, std::uint16_t value) {
    out.push_back(static_cast<std::uint8_t>(value & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFFu));
}

void appendU32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    out.push_back(static_cast<std::uint8_t>(value & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFFu));
}

void appendFourCC(std::vector<std::uint8_t>& out, const char* text) {
    out.insert(out.end(), text, text + 4);
}

}  // namespace

std::optional<AudioBuffer> loadWavFile(const std::filesystem::path& path,
                                       std::vector<Diagnostic>& diagnostics) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        diagnostics.push_back(Diagnostic::error(
            "audio-missing-asset", "cannot open WAV asset '" + path.string() + "'"));
        return std::nullopt;
    }
    std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(stream),
                                    std::istreambuf_iterator<char>()};
    if (bytes.size() < 44 || std::memcmp(bytes.data(), "RIFF", 4) != 0 ||
        std::memcmp(bytes.data() + 8, "WAVE", 4) != 0) {
        diagnostics.push_back(Diagnostic::error(
            "wav-read", "invalid WAV header in '" + path.string() + "'"));
        return std::nullopt;
    }

    std::uint16_t format = 0;
    std::uint16_t channels = 0;
    std::uint32_t sampleRate = 0;
    std::uint16_t bitsPerSample = 0;
    std::size_t dataOffset = 0;
    std::size_t dataSize = 0;

    std::size_t offset = 12;
    while (offset + 8 <= bytes.size()) {
        const std::string id(reinterpret_cast<const char*>(bytes.data() + offset), 4);
        const std::uint32_t chunkSize = readU32(bytes, offset + 4);
        const std::size_t payload = offset + 8;
        if (payload + chunkSize > bytes.size()) {
            diagnostics.push_back(Diagnostic::error(
                "wav-read", "truncated WAV chunk in '" + path.string() + "'"));
            return std::nullopt;
        }
        if (id == "fmt ") {
            if (chunkSize < 16) {
                diagnostics.push_back(Diagnostic::error(
                    "wav-read", "short WAV fmt chunk in '" + path.string() + "'"));
                return std::nullopt;
            }
            format = readU16(bytes, payload);
            channels = readU16(bytes, payload + 2);
            sampleRate = readU32(bytes, payload + 4);
            bitsPerSample = readU16(bytes, payload + 14);
        } else if (id == "data") {
            dataOffset = payload;
            dataSize = chunkSize;
        }
        offset = payload + chunkSize + (chunkSize % 2);
    }

    if (format != 1 && format != 3) {
        diagnostics.push_back(Diagnostic::error(
            "wav-read", "unsupported WAV encoding in '" + path.string() +
                            "' (only PCM16 and float32 are supported)"));
        return std::nullopt;
    }
    if (channels < 1 || channels > 2 || sampleRate == 0 || dataSize == 0) {
        diagnostics.push_back(Diagnostic::error(
            "wav-read", "invalid WAV format parameters in '" + path.string() + "'"));
        return std::nullopt;
    }

    const int bytesPerSample = bitsPerSample / 8;
    if ((format == 1 && bitsPerSample != 16) || (format == 3 && bitsPerSample != 32)) {
        diagnostics.push_back(Diagnostic::error(
            "wav-read", "unsupported WAV bit depth in '" + path.string() + "'"));
        return std::nullopt;
    }

    AudioBuffer buffer;
    buffer.sampleRate = static_cast<int>(sampleRate);
    buffer.channels = static_cast<int>(channels);
    const std::size_t frames = dataSize / (static_cast<std::size_t>(bytesPerSample) * channels);
    buffer.samples.reserve(frames * channels);
    for (std::size_t frame = 0; frame < frames; ++frame) {
        for (int channel = 0; channel < channels; ++channel) {
            const std::size_t sampleOffset =
                dataOffset + (frame * static_cast<std::size_t>(channels) + channel) *
                                 static_cast<std::size_t>(bytesPerSample);
            if (format == 1) {
                buffer.samples.push_back(static_cast<float>(readS16(bytes, sampleOffset)) /
                                         32768.0f);
            } else {
                buffer.samples.push_back(readF32(bytes, sampleOffset));
            }
        }
    }
    return buffer;
}

void writeWavFile(const std::filesystem::path& path, const AudioBuffer& buffer,
                  std::vector<Diagnostic>& diagnostics) {
    if (buffer.sampleRate <= 0 || buffer.channels < 1 || buffer.channels > 2) {
        diagnostics.push_back(Diagnostic::error(
            "wav-write", "cannot write WAV with invalid sample rate or channel count"));
        return;
    }

    const std::uint32_t dataBytes =
        static_cast<std::uint32_t>(buffer.samples.size() * sizeof(float));
    std::vector<std::uint8_t> bytes;
    bytes.reserve(44 + dataBytes);
    appendFourCC(bytes, "RIFF");
    appendU32(bytes, 36 + dataBytes);
    appendFourCC(bytes, "WAVE");

    appendFourCC(bytes, "fmt ");
    appendU32(bytes, 16);
    appendU16(bytes, 3);  // IEEE float
    appendU16(bytes, static_cast<std::uint16_t>(buffer.channels));
    appendU32(bytes, static_cast<std::uint32_t>(buffer.sampleRate));
    appendU32(bytes, static_cast<std::uint32_t>(buffer.sampleRate * buffer.channels * 4));
    appendU16(bytes, static_cast<std::uint16_t>(buffer.channels * 4));
    appendU16(bytes, 32);

    appendFourCC(bytes, "data");
    appendU32(bytes, dataBytes);
    const std::size_t offset = bytes.size();
    bytes.resize(offset + dataBytes);
    std::memcpy(bytes.data() + offset, buffer.samples.data(), dataBytes);

    const std::filesystem::path parent = path.parent_path();
    if (!parent.empty()) {
        std::error_code error;
        std::filesystem::create_directories(parent, error);
        if (error) {
            diagnostics.push_back(Diagnostic::error(
                "wav-write", "cannot create output directory '" + parent.string() + "'"));
            return;
        }
    }

    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) {
        diagnostics.push_back(
            Diagnostic::error("wav-write", "cannot open '" + path.string() + "' for writing"));
        return;
    }
    stream.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    if (!stream) {
        diagnostics.push_back(
            Diagnostic::error("wav-write", "failed while writing '" + path.string() + "'"));
    }
}

}  // namespace mpl
