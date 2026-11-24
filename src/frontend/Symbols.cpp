#include "mpl/frontend/Symbols.hpp"

#include "mpl/core/Error.hpp"

#include <utility>

namespace mpl {

SymbolId SymbolTable::add(Symbol symbol) {
    const SymbolId id = static_cast<SymbolId>(symbols_.size());
    names_[symbol.name] = id;
    symbols_.push_back(std::move(symbol));
    return id;
}

std::optional<SymbolId> SymbolTable::find(std::string_view name) const {
    const auto iterator = names_.find(std::string(name));
    if (iterator == names_.end()) {
        return std::nullopt;
    }
    return iterator->second;
}

const Symbol& SymbolTable::get(SymbolId id) const {
    if (id >= symbols_.size()) {
        throw Error("invalid symbol id", "frontend");
    }
    return symbols_[id];
}

std::string_view symbolKindName(SymbolKind kind) {
    switch (kind) {
        case SymbolKind::Let: return "let";
        case SymbolKind::Asset: return "asset";
        case SymbolKind::Curve: return "curve";
        case SymbolKind::Track: return "track";
        case SymbolKind::Section: return "section";
        case SymbolKind::Processor: return "processor";
        case SymbolKind::Bus: return "bus";
    }
    return "symbol";
}

}  // namespace mpl
