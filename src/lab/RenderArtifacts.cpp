#include "mpl/lab/RenderArtifacts.hpp"

#include "mpl/util/JsonValue.hpp"
#include "mpl/util/JsonWriter.hpp"
#include "mpl/util/Sha256.hpp"

#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <string>

namespace mpl {
namespace {

std::string hexHash(std::uint64_t value) {
    std::ostringstream stream;
    stream << std::hex << std::setw(16) << std::setfill('0') << value;
    return stream.str();
}

std::string fingerprint(const std::string& text) {
    return hexHash(hashBytes(text));
}

std::string moduleFingerprint(const Compilation& compilation) {
    std::uint64_t hash = 1469598103934665603ull;
    for (const ModuleRecord& module : compilation.modules) {
        const std::string entry = module.name + ":" + std::to_string(module.resolvedHash);
        hash ^= hashBytes(entry);
        hash *= 1099511628211ull;
    }
    return hexHash(hash);
}

std::string assetFingerprint(const ProductionIR& production) {
    JsonWriter json;
    json.beginArray();
    for (const TrackIR& track : production.tracks) {
        json.beginObject();
        json.key("track").value(track.name);
        json.key("instrument").value(track.instrument);
        json.key("assets").beginArray();
        for (const AssetBinding& binding : track.assetBindings) {
            json.beginObject();
            json.key("slot").value(binding.slot);
            json.key("assetId").value(binding.assetId);
            json.key("provider").value(binding.provider);
            json.endObject();
        }
        json.endArray();
        json.endObject();
    }
    json.endArray();
    return fingerprint(json.take());
}

std::string configurationFingerprint(const RenderConfiguration& configuration) {
    JsonWriter json;
    json.beginObject();
    json.key("sampleRate").value(configuration.sampleRate);
    json.key("blockSize").value(configuration.blockSize);
    json.key("channels").value(configuration.channels);
    json.key("tailSeconds").value(configuration.tailSeconds);
    json.endObject();
    return json.take();
}

}  // namespace

std::uint64_t hashBytes(std::string_view bytes) {
    std::uint64_t hash = 1469598103934665603ull;
    for (const char character : bytes) {
        hash ^= static_cast<unsigned char>(character);
        hash *= 1099511628211ull;
    }
    return hash;
}

std::string hashFileHex(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        return {};
    }
    std::string bytes{std::istreambuf_iterator<char>(stream),
                      std::istreambuf_iterator<char>()};
    return hexHash(hashBytes(bytes));
}

RenderArtifactManifest buildRenderManifest(std::string_view source,
                                           const Compilation& compilation,
                                           const RenderConfiguration& configuration,
                                           const std::filesystem::path& audioPath) {
    RenderArtifactManifest manifest;
    manifest.sourceHash = fingerprint(std::string(source));
    manifest.moduleHash = moduleFingerprint(compilation);
    manifest.productionHash = fingerprint(compilation.production.toJson());
    manifest.assetManifestHash = assetFingerprint(compilation.production);
    manifest.rendererVersion = "mpl-reference-audio-1";
    manifest.configuration = configuration;
    manifest.audioHash = audioPath.empty() ? std::string{} : hashFileHex(audioPath);
    manifest.sourceSha256 = sha256Hex(source);
    manifest.moduleSha256 = sha256Hex(manifest.moduleHash);
    manifest.productionSha256 = sha256Hex(compilation.production.toJson());
    manifest.assetSha256 = sha256Hex(manifest.assetManifestHash);
    manifest.audioSha256 = audioPath.empty() ? std::string{} : sha256FileHex(audioPath.generic_string());
    return manifest;
}

bool verifyRenderManifest(const RenderArtifactManifest& manifest,
                          std::string_view source, const Compilation& compilation,
                          const RenderConfiguration& configuration,
                          const std::filesystem::path& audioPath, std::string& error) {
    const RenderArtifactManifest expected =
        buildRenderManifest(source, compilation, configuration, audioPath);
    if (manifest.sourceHash != expected.sourceHash) {
        error = "source hash does not match the candidate program";
        return false;
    }
    if (manifest.moduleHash != expected.moduleHash) {
        error = "module hash does not match the candidate dependency closure";
        return false;
    }
    if (manifest.productionHash != expected.productionHash) {
        error = "production hash does not match the candidate production";
        return false;
    }
    if (manifest.assetManifestHash != expected.assetManifestHash) {
        error = "asset manifest hash does not match the candidate";
        return false;
    }
    if (manifest.rendererVersion != expected.rendererVersion) {
        error = "renderer version does not match";
        return false;
    }
    if (manifest.configuration.sampleRate != expected.configuration.sampleRate ||
        manifest.configuration.blockSize != expected.configuration.blockSize ||
        manifest.configuration.channels != expected.configuration.channels ||
        manifest.configuration.tailSeconds != expected.configuration.tailSeconds) {
        error = "render configuration does not match";
        return false;
    }
    if (manifest.audioHash != expected.audioHash) {
        error = "audio hash does not match the rendered WAV";
        return false;
    }
    if (manifest.sourceSha256 != expected.sourceSha256 ||
        manifest.moduleSha256 != expected.moduleSha256 ||
        manifest.productionSha256 != expected.productionSha256 ||
        manifest.assetSha256 != expected.assetSha256 ||
        manifest.audioSha256 != expected.audioSha256) {
        error = "SHA-256 content identity does not match";
        return false;
    }
    return true;
}

std::string RenderArtifactManifest::toJson() const {
    JsonWriter json;
    json.beginObject();
    json.key("format").value("mpl.render-manifest");
    json.key("version").value(1);
    json.key("sourceHash").value(sourceHash);
    json.key("moduleHash").value(moduleHash);
    json.key("productionHash").value(productionHash);
    json.key("assetManifestHash").value(assetManifestHash);
    json.key("rendererVersion").value(rendererVersion);
    json.key("configuration").value(configurationFingerprint(configuration));
    json.key("audioHash").value(audioHash);
    json.key("sourceSha256").value(sourceSha256);
    json.key("moduleSha256").value(moduleSha256);
    json.key("productionSha256").value(productionSha256);
    json.key("assetSha256").value(assetSha256);
    json.key("audioSha256").value(audioSha256);
    json.endObject();
    return json.take();
}

bool parseRenderManifest(std::string_view text, RenderArtifactManifest& manifest,
                         std::string& error) {
    JsonValue root;
    if (!parseJson(text, root, error) || !root.isObject()) {
        error = "invalid render manifest JSON";
        return false;
    }
    const auto stringField = [&](const char* name, std::string& out) {
        const JsonValue* value = root.find(name);
        if (value == nullptr || !value->isString() || value->text.empty()) {
            error = std::string("render manifest requires ") + name;
            return false;
        }
        out = value->text;
        return true;
    };
    if (!stringField("sourceHash", manifest.sourceHash) ||
        !stringField("moduleHash", manifest.moduleHash) ||
        !stringField("productionHash", manifest.productionHash) ||
        !stringField("assetManifestHash", manifest.assetManifestHash) ||
        !stringField("rendererVersion", manifest.rendererVersion) ||
        !stringField("audioHash", manifest.audioHash)) {
        return false;
    }
    const auto optionalStringField = [&](const char* name, std::string& out) {
        if (const JsonValue* value = root.find(name);
            value != nullptr && value->isString()) {
            out = value->text;
        }
        return true;
    };
    optionalStringField("sourceSha256", manifest.sourceSha256);
    optionalStringField("moduleSha256", manifest.moduleSha256);
    optionalStringField("productionSha256", manifest.productionSha256);
    optionalStringField("assetSha256", manifest.assetSha256);
    optionalStringField("audioSha256", manifest.audioSha256);
    const JsonValue* configuration = root.find("configuration");
    if (configuration == nullptr) {
        error = "render manifest requires configuration";
        return false;
    }
    if (configuration->isString()) {
        JsonValue parsed;
        if (!parseJson(configuration->text, parsed, error) || !parsed.isObject()) {
            error = "invalid configuration in render manifest";
            return false;
        }
        const auto number = [&](const char* name, double& out) {
            const JsonValue* value = parsed.find(name);
            if (value == nullptr || !value->isNumber()) {
                return false;
            }
            out = value->number;
            return true;
        };
        double sampleRate = 0.0;
        double blockSize = 0.0;
        double channels = 0.0;
        double tail = 0.0;
        if (!number("sampleRate", sampleRate) || !number("blockSize", blockSize) ||
            !number("channels", channels) || !number("tailSeconds", tail)) {
            error = "render configuration requires numeric fields";
            return false;
        }
        manifest.configuration.sampleRate = static_cast<int>(sampleRate);
        manifest.configuration.blockSize = static_cast<int>(blockSize);
        manifest.configuration.channels = static_cast<int>(channels);
        manifest.configuration.tailSeconds = tail;
    } else if (configuration->isObject()) {
        const auto number = [&](const char* name, double& out) {
            const JsonValue* value = configuration->find(name);
            if (value == nullptr || !value->isNumber()) {
                return false;
            }
            out = value->number;
            return true;
        };
        double sampleRate = 0.0;
        double blockSize = 0.0;
        double channels = 0.0;
        double tail = 0.0;
        if (!number("sampleRate", sampleRate) || !number("blockSize", blockSize) ||
            !number("channels", channels) || !number("tailSeconds", tail)) {
            error = "render configuration requires numeric fields";
            return false;
        }
        manifest.configuration.sampleRate = static_cast<int>(sampleRate);
        manifest.configuration.blockSize = static_cast<int>(blockSize);
        manifest.configuration.channels = static_cast<int>(channels);
        manifest.configuration.tailSeconds = tail;
    }
    return true;
}

}  // namespace mpl
