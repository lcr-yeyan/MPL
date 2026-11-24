#pragma once

#include <string>
#include <string_view>

namespace mpl {

/// Semantic role of a track. Roles let the production layer reason about
/// orchestration without committing to a concrete sound source.
enum class InstrumentRole { Melodic, Bass, Harmony, Percussion, Texture };

std::string_view instrumentRoleName(InstrumentRole role);

InstrumentRole instrumentRoleFromName(std::string_view name);

enum class AssetKind { Instrument, Sample };

std::string_view assetKindName(AssetKind kind);

/// A named placeholder for a sound source inside a track. `preferredAsset` is
/// only a hint; binding it to a concrete provider is a separate step so that
/// humans or the SoundBoxer backend can make the final choice.
struct AssetSlot {
    std::string name;
    AssetKind kind = AssetKind::Instrument;
    std::string preferredAsset;
};

/// Binds a slot to a concrete asset. An unresolved binding (empty assetId or
/// provider) is valid and means "a human or backend must still choose".
struct AssetBinding {
    std::string slot;
    std::string assetId;
    std::string provider;

    bool isResolved() const noexcept { return !assetId.empty() && !provider.empty(); }
};

}  // namespace mpl
