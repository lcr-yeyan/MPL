#include "mpl/frontend/Types.hpp"

namespace mpl {

std::string_view typeKindName(TypeKind kind) {
    switch (kind) {
        case TypeKind::Error: return "error";
        case TypeKind::Number: return "number";
        case TypeKind::Scalar: return "scalar";
        case TypeKind::Duration: return "duration";
        case TypeKind::Position: return "position";
        case TypeKind::Interval: return "interval";
        case TypeKind::ScaleDegree: return "scale-degree";
        case TypeKind::Tempo: return "tempo";
        case TypeKind::Frequency: return "frequency";
        case TypeKind::Gain: return "gain";
        case TypeKind::PhysicalTime: return "physical-time";
        case TypeKind::Pitch: return "pitch";
        case TypeKind::Key: return "key";
        case TypeKind::String: return "string";
        case TypeKind::Fragment: return "fragment";
        case TypeKind::Curve: return "curve";
        case TypeKind::List: return "list";
        case TypeKind::Asset: return "asset";
        case TypeKind::Track: return "track";
        case TypeKind::Section: return "section";
        case TypeKind::Processor: return "processor";
        case TypeKind::Bus: return "bus";
    }
    return "unknown";
}

std::string typeName(const Type& type) {
    if (type.kind == TypeKind::List) {
        return "list<" + std::string(typeKindName(type.element)) + ">";
    }
    if (type.kind == TypeKind::Curve) {
        return "curve<" + std::string(timeDomainName(type.domain)) + ">";
    }
    return std::string(typeKindName(type.kind));
}

}  // namespace mpl
