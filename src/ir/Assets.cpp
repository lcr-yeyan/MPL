#include "mpl/ir/Assets.hpp"

#include "mpl/core/Error.hpp"

#include <string>

namespace mpl {

std::string_view instrumentRoleName(InstrumentRole role) {
    switch (role) {
        case InstrumentRole::Melodic: return "melodic";
        case InstrumentRole::Bass: return "bass";
        case InstrumentRole::Harmony: return "harmony";
        case InstrumentRole::Percussion: return "percussion";
        case InstrumentRole::Texture: return "texture";
    }
    return "unknown";
}

InstrumentRole instrumentRoleFromName(std::string_view name) {
    if (name == "melodic") {
        return InstrumentRole::Melodic;
    }
    if (name == "bass") {
        return InstrumentRole::Bass;
    }
    if (name == "harmony") {
        return InstrumentRole::Harmony;
    }
    if (name == "percussion") {
        return InstrumentRole::Percussion;
    }
    if (name == "texture") {
        return InstrumentRole::Texture;
    }
    throw Error("unknown instrument role '" + std::string(name) + "'", "asset");
}

std::string_view assetKindName(AssetKind kind) {
    return kind == AssetKind::Instrument ? "instrument" : "sample";
}

}  // namespace mpl
