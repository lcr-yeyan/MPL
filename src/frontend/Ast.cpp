#include "mpl/frontend/Ast.hpp"

#include "mpl/util/JsonWriter.hpp"

namespace mpl {
namespace {

void writeSpan(JsonWriter& json, const SourceSpan& span) {
    json.key("span").beginObject();
    json.key("line").value(span.line);
    json.key("column").value(span.column);
    json.key("endLine").value(span.endLine);
    json.key("endColumn").value(span.endColumn);
    json.key("offset").value(static_cast<std::int64_t>(span.begin));
    json.key("length").value(static_cast<std::int64_t>(span.end - span.begin));
    json.endObject();
}

void writeExpr(JsonWriter& json, const Module& module, ExprId id);

void writeExprBody(JsonWriter& json, const Module& module, const Expr& expr) {
    json.key("kind").value(exprKindName(expr.kind));
    writeSpan(json, expr.span);
    if (!expr.text.empty()) {
        json.key("text").value(expr.text);
    }
    if (!expr.unit.empty()) {
        json.key("unit").value(expr.unit);
    }
    switch (expr.kind) {
        case ExprKind::Integer:
            json.key("value").value(static_cast<std::int64_t>(expr.integer));
            break;
        case ExprKind::Rational:
            json.key("numerator").value(static_cast<std::int64_t>(expr.numerator));
            json.key("denominator").value(static_cast<std::int64_t>(expr.denominator));
            break;
        case ExprKind::Number:
            json.key("value").value(expr.number);
            break;
        case ExprKind::Call:
            json.key("arguments").beginArray();
            for (const ExprId argument : expr.args) {
                writeExpr(json, module, argument);
            }
            json.endArray();
            break;
        case ExprKind::UnaryMinus:
            json.key("operand");
            if (!expr.args.empty()) {
                writeExpr(json, module, expr.args.front());
            }
            break;
        case ExprKind::List:
            json.key("items").beginArray();
            for (const ExprId item : expr.args) {
                writeExpr(json, module, item);
            }
            json.endArray();
            break;
        case ExprKind::Melody:
            json.key("entries").beginArray();
            for (const MelodyEntry& entry : expr.melody) {
                json.beginObject();
                json.key("pitch").value(entry.pitch);
                json.key("duration");
                writeExpr(json, module, entry.duration);
                json.endObject();
            }
            json.endArray();
            break;
        case ExprKind::Notes:
            if (expr.explicitSpan != kInvalidExprId) {
                json.key("spanDuration");
                writeExpr(json, module, expr.explicitSpan);
            }
            json.key("entries").beginArray();
            for (const NoteEntry& entry : expr.notes) {
                json.beginObject();
                json.key("pitch").value(entry.pitch);
                json.key("offset");
                writeExpr(json, module, entry.offset);
                json.key("duration");
                writeExpr(json, module, entry.duration);
                json.key("velocity").value(entry.velocity);
                json.endObject();
            }
            json.endArray();
            break;
        case ExprKind::Keyframes:
            json.key("entries").beginArray();
            for (const KeyframeEntry& entry : expr.keyframes) {
                json.beginObject();
                json.key("time");
                writeExpr(json, module, entry.time);
                json.key("value").value(entry.value);
                json.key("interpolation").value(entry.interpolation);
                json.endObject();
            }
            json.endArray();
            break;
        default:
            break;
    }
}

void writeExpr(JsonWriter& json, const Module& module, ExprId id) {
    if (id == kInvalidExprId) {
        json.beginObject();
        json.key("kind").value("invalid");
        json.endObject();
        return;
    }
    json.beginObject();
    writeExprBody(json, module, module.expr(id));
    json.endObject();
}

}  // namespace

std::string_view exprKindName(ExprKind kind) {
    switch (kind) {
        case ExprKind::Invalid: return "invalid";
        case ExprKind::Integer: return "integer";
        case ExprKind::Rational: return "rational";
        case ExprKind::Number: return "number";
        case ExprKind::String: return "string";
        case ExprKind::Identifier: return "identifier";
        case ExprKind::Key: return "key";
        case ExprKind::Call: return "call";
        case ExprKind::UnaryMinus: return "unary-minus";
        case ExprKind::List: return "list";
        case ExprKind::Melody: return "melody";
        case ExprKind::Notes: return "notes";
        case ExprKind::Keyframes: return "keyframes";
    }
    return "unknown";
}

std::string astToJson(const Module& module) {
    JsonWriter json;
    json.beginObject();
    json.key("format").value("mpl.ast");
    json.key("version").value(1);
    json.key("file").value(module.file);
    writeSpan(json, module.span);

    if (module.project.has_value()) {
        json.key("project").beginObject();
        json.key("name").value(module.project->name);
        writeSpan(json, module.project->span);
        json.key("tempo");
        writeExpr(json, module, module.project->tempo);
        json.key("meter");
        writeExpr(json, module, module.project->meter);
        json.key("key");
        writeExpr(json, module, module.project->key);
        json.endObject();
    }

    json.key("declarations").beginArray();
    for (const DeclRef& reference : module.declarationOrder) {
        json.beginObject();
        switch (reference.kind) {
            case DeclKind::Asset: {
                const AssetDecl& decl = module.assets[reference.index];
                json.key("kind").value("asset");
                json.key("name").value(decl.name);
                json.key("assetKind").value(decl.kind);
                json.key("provider").value(decl.provider);
                writeSpan(json, decl.span);
                break;
            }
            case DeclKind::Let: {
                const LetDecl& decl = module.lets[reference.index];
                json.key("kind").value("let");
                json.key("name").value(decl.name);
                writeSpan(json, decl.span);
                json.key("value");
                writeExpr(json, module, decl.value);
                break;
            }
            case DeclKind::Curve: {
                const CurveDecl& decl = module.curves[reference.index];
                json.key("kind").value("curve");
                json.key("name").value(decl.name);
                json.key("domain").value(decl.domain);
                writeSpan(json, decl.span);
                if (decl.extent != kInvalidExprId) {
                    json.key("extent");
                    writeExpr(json, module, decl.extent);
                }
                json.key("value");
                writeExpr(json, module, decl.value);
                break;
            }
            case DeclKind::Track: {
                const TrackDecl& decl = module.tracks[reference.index];
                json.key("kind").value("track");
                json.key("name").value(decl.name);
                json.key("asset").value(decl.asset);
                json.key("role").value(decl.role);
                writeSpan(json, decl.span);
                if (decl.gain != kInvalidExprId) {
                    json.key("gain");
                    writeExpr(json, module, decl.gain);
                }
                if (decl.pan != kInvalidExprId) {
                    json.key("pan");
                    writeExpr(json, module, decl.pan);
                }
                if (!decl.inserts.empty()) {
                    json.key("inserts").beginArray();
                    for (const std::string& insert : decl.inserts) {
                        json.value(insert);
                    }
                    json.endArray();
                }
                if (!decl.sends.empty()) {
                    json.key("sends").beginArray();
                    for (const SendDecl& send : decl.sends) {
                        json.beginObject();
                        json.key("bus").value(send.bus);
                        json.key("level");
                        writeExpr(json, module, send.level);
                        writeSpan(json, send.span);
                        json.endObject();
                    }
                    json.endArray();
                }
                if (!decl.sidechains.empty()) {
                    json.key("sidechains").beginArray();
                    for (const SidechainDecl& sidechain : decl.sidechains) {
                        json.beginObject();
                        json.key("processor").value(sidechain.processor);
                        json.key("source").value(sidechain.source);
                        writeSpan(json, sidechain.span);
                        json.endObject();
                    }
                    json.endArray();
                }
                break;
            }
            case DeclKind::Processor: {
                const ProcessorDecl& decl = module.processors[reference.index];
                json.key("kind").value("processor");
                json.key("name").value(decl.name);
                json.key("processorKind").value(decl.kind);
                if (!decl.mode.empty()) {
                    json.key("mode").value(decl.mode);
                }
                writeSpan(json, decl.span);
                json.key("fields").beginArray();
                for (const ProcessorField& field : decl.fields) {
                    json.beginObject();
                    json.key("name").value(field.name);
                    writeSpan(json, field.span);
                    json.key("value");
                    writeExpr(json, module, field.value);
                    json.endObject();
                }
                json.endArray();
                break;
            }
            case DeclKind::Bus: {
                const BusDecl& decl = module.buses[reference.index];
                json.key("kind").value("bus");
                json.key("name").value(decl.name);
                writeSpan(json, decl.span);
                if (decl.gain != kInvalidExprId) {
                    json.key("gain");
                    writeExpr(json, module, decl.gain);
                }
                if (decl.pan != kInvalidExprId) {
                    json.key("pan");
                    writeExpr(json, module, decl.pan);
                }
                if (!decl.inserts.empty()) {
                    json.key("inserts").beginArray();
                    for (const std::string& insert : decl.inserts) {
                        json.value(insert);
                    }
                    json.endArray();
                }
                break;
            }
            case DeclKind::Section: {
                const SectionDecl& decl = module.sections[reference.index];
                json.key("kind").value("section");
                json.key("name").value(decl.name);
                writeSpan(json, decl.span);
                json.key("duration");
                writeExpr(json, module, decl.duration);
                json.key("clips").beginArray();
                for (const ClipDecl& clip : decl.clips) {
                    json.beginObject();
                    json.key("track").value(clip.track);
                    json.key("name").value(clip.name);
                    writeSpan(json, clip.span);
                    if (clip.at != kInvalidExprId) {
                        json.key("at");
                        writeExpr(json, module, clip.at);
                    }
                    json.key("play");
                    writeExpr(json, module, clip.play);
                    json.key("velocity");
                    writeExpr(json, module, clip.velocity);
                    json.key("timing");
                    writeExpr(json, module, clip.timing);
                    json.key("gate");
                    writeExpr(json, module, clip.gate);
                    json.key("pan");
                    writeExpr(json, module, clip.pan);
                    json.endObject();
                }
                json.endArray();
                break;
            }
            case DeclKind::Automation: {
                const AutomationDecl& decl = module.automations[reference.index];
                json.key("kind").value("automation");
                json.key("scope").value(decl.scope);
                json.key("track").value(decl.track);
                if (!decl.processor.empty()) {
                    json.key("processor").value(decl.processor);
                }
                json.key("parameter").value(decl.parameter);
                json.key("mode").value(decl.mode);
                writeSpan(json, decl.span);
                json.key("curve");
                writeExpr(json, module, decl.curve);
                break;
            }
        }
        json.endObject();
    }
    json.endArray();

    if (module.master.has_value()) {
        const MasterDecl& decl = *module.master;
        json.key("master").beginObject();
        writeSpan(json, decl.span);
        if (decl.gain != kInvalidExprId) {
            json.key("gain");
            writeExpr(json, module, decl.gain);
        }
        if (decl.pan != kInvalidExprId) {
            json.key("pan");
            writeExpr(json, module, decl.pan);
        }
        if (!decl.inserts.empty()) {
            json.key("inserts").beginArray();
            for (const std::string& insert : decl.inserts) {
                json.value(insert);
            }
            json.endArray();
        }
        json.endObject();
    }

    if (module.arrangement.has_value()) {
        json.key("arrangement").beginArray();
        for (const auto& play : module.arrangement->plays) {
            json.beginObject();
            json.key("section").value(play.first);
            writeSpan(json, play.second);
            json.endObject();
        }
        json.endArray();
    }

    if (!module.exports.empty()) {
        json.key("exports").beginArray();
        for (const ExportDecl& exportDecl : module.exports) {
            json.beginObject();
            json.key("format").value(exportDecl.format);
            json.key("path").value(exportDecl.path);
            writeSpan(json, exportDecl.span);
            json.endObject();
        }
        json.endArray();
    }

    json.endObject();
    return json.take();
}

}  // namespace mpl
