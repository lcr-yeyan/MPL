#pragma once

#include "mpl/frontend/Source.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace mpl {

using SymbolId = std::uint32_t;

inline constexpr SymbolId kInvalidSymbolId = 0xFFFFFFFFu;

enum class SymbolKind { Let, Asset, Curve, Track, Section, Processor, Bus, Function };

struct Symbol {
    std::string name;
    SymbolKind kind = SymbolKind::Let;
    SourceSpan span;
    std::size_t declarationIndex = 0;
};

class SymbolTable {
public:
    SymbolId add(Symbol symbol);
    std::optional<SymbolId> find(std::string_view name) const;
    const Symbol& get(SymbolId id) const;
    std::size_t size() const noexcept { return symbols_.size(); }
    const std::vector<Symbol>& symbols() const noexcept { return symbols_; }

private:
    std::vector<Symbol> symbols_;
    std::unordered_map<std::string, SymbolId> names_;
};

std::string_view symbolKindName(SymbolKind kind);

}  // namespace mpl
