#include "mpl/frontend/Frontend.hpp"

#include "mpl/frontend/Lexer.hpp"
#include "mpl/frontend/Lowering.hpp"
#include "mpl/frontend/Parser.hpp"
#include "mpl/frontend/Sema.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#ifndef MPL_STDLIB_DIR
#define MPL_STDLIB_DIR ""
#endif

namespace mpl {
namespace {

using Path = std::filesystem::path;

std::uint64_t fnv1a64(std::string_view text) {
    std::uint64_t hash = 1469598103934665603ull;
    for (const char character : text) {
        hash ^= static_cast<unsigned char>(character);
        hash *= 1099511628211ull;
    }
    return hash;
}

std::string readFile(const Path& path) {
    std::ifstream stream(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(stream),
                       std::istreambuf_iterator<char>());
}

std::vector<Path> defaultImportPaths() {
    std::vector<Path> paths;
    const char* runtimeStdlib = std::getenv("MPL_STDLIB_DIR");
    const std::string stdlib = runtimeStdlib != nullptr && *runtimeStdlib != '\0'
                                   ? runtimeStdlib
                                   : MPL_STDLIB_DIR;
    if (!stdlib.empty()) {
        paths.emplace_back(stdlib);
    }
    std::error_code code;
    const Path current = std::filesystem::current_path(code);
    if (!code) {
        paths.push_back(current);
    }
    return paths;
}

std::string pathText(const Path& path) {
    return path.generic_string();
}

bool isLibraryDeclarationAllowed(const Module& module) {
    return !module.project.has_value() && module.assets.empty() && module.tracks.empty() &&
           module.sections.empty() && module.automations.empty() && module.processors.empty() &&
           module.buses.empty() && !module.master.has_value() &&
           !module.arrangement.has_value() && module.exports.empty();
}

void rewriteExpr(Module& module, ExprId id, const std::set<std::string>& locals,
                 const std::map<std::string, std::string>& renames);

void rewriteId(Module& module, ExprId& id, const std::set<std::string>& locals,
               const std::map<std::string, std::string>& renames) {
    if (id != kInvalidExprId) {
        rewriteExpr(module, id, locals, renames);
    }
}

void rewriteExpr(Module& module, ExprId id, const std::set<std::string>& locals,
                 const std::map<std::string, std::string>& renames) {
    if (id == kInvalidExprId || id >= module.expressions.size()) {
        return;
    }
    Expr& expr = module.expressions[id];
    switch (expr.kind) {
        case ExprKind::Identifier:
        case ExprKind::Call: {
            if (locals.find(expr.text) == locals.end()) {
                const auto found = renames.find(expr.text);
                if (found != renames.end()) {
                    expr.text = found->second;
                }
            }
            for (ExprId& argument : expr.args) {
                rewriteId(module, argument, locals, renames);
            }
            break;
        }
        case ExprKind::For: {
            if (!expr.args.empty()) {
                rewriteId(module, expr.args[0], locals, renames);
            }
            std::set<std::string> nested = locals;
            nested.insert(expr.text);
            if (expr.args.size() > 1) {
                rewriteId(module, expr.args[1], nested, renames);
            }
            break;
        }
        case ExprKind::Melody:
            for (MelodyEntry& entry : expr.melody) {
                rewriteId(module, entry.duration, locals, renames);
            }
            break;
        case ExprKind::Notes:
            for (NoteEntry& entry : expr.notes) {
                rewriteId(module, entry.offset, locals, renames);
                rewriteId(module, entry.duration, locals, renames);
            }
            rewriteId(module, expr.explicitSpan, locals, renames);
            break;
        case ExprKind::Rhythm:
            for (RhythmEntry& entry : expr.rhythm) {
                rewriteId(module, entry.duration, locals, renames);
            }
            rewriteId(module, expr.explicitSpan, locals, renames);
            break;
        case ExprKind::Harmony:
            for (HarmonyEntry& entry : expr.harmony) {
                rewriteId(module, entry.offset, locals, renames);
                rewriteId(module, entry.duration, locals, renames);
            }
            rewriteId(module, expr.explicitSpan, locals, renames);
            break;
        case ExprKind::Keyframes:
            for (KeyframeEntry& entry : expr.keyframes) {
                rewriteId(module, entry.time, locals, renames);
            }
            break;
        default:
            for (ExprId& argument : expr.args) {
                rewriteId(module, argument, locals, renames);
            }
            break;
    }
}

void rewriteModuleNames(Module& module, const std::map<std::string, std::string>& renames) {
    for (FunctionDecl& function : module.functions) {
        std::set<std::string> locals;
        for (const ParamDecl& param : function.params) {
            locals.insert(param.name);
        }
        for (Stmt& statement : function.body) {
            rewriteId(module, statement.value, locals, renames);
            if (statement.kind == StmtKind::Let) {
                locals.insert(statement.name);
            }
        }
    }
    for (LetDecl& let : module.lets) {
        rewriteId(module, let.value, {}, renames);
    }
    for (CurveDecl& curve : module.curves) {
        rewriteId(module, curve.extent, {}, renames);
        rewriteId(module, curve.value, {}, renames);
    }
}

void shiftExprIds(Module& module, std::size_t offset) {
    const auto shift = [offset](ExprId& id) {
        if (id != kInvalidExprId) {
            id = static_cast<ExprId>(id + offset);
        }
    };
    for (Expr& expr : module.expressions) {
        for (ExprId& argument : expr.args) {
            shift(argument);
        }
        for (MelodyEntry& entry : expr.melody) {
            shift(entry.duration);
        }
        for (NoteEntry& entry : expr.notes) {
            shift(entry.offset);
            shift(entry.duration);
        }
        for (RhythmEntry& entry : expr.rhythm) {
            shift(entry.duration);
        }
        for (HarmonyEntry& entry : expr.harmony) {
            shift(entry.offset);
            shift(entry.duration);
        }
        for (KeyframeEntry& entry : expr.keyframes) {
            shift(entry.time);
        }
        shift(expr.explicitSpan);
    }
    for (FunctionDecl& function : module.functions) {
        for (Stmt& statement : function.body) {
            shift(statement.value);
        }
    }
    for (LetDecl& let : module.lets) {
        shift(let.value);
    }
    for (CurveDecl& curve : module.curves) {
        shift(curve.extent);
        shift(curve.value);
    }
}

void collectRootNames(const Module& module, std::set<std::string>& names) {
    for (const FunctionDecl& function : module.functions) {
        names.insert(function.name);
    }
    for (const LetDecl& let : module.lets) {
        names.insert(let.name);
    }
    for (const CurveDecl& curve : module.curves) {
        names.insert(curve.name);
    }
}

struct MergeStats {
    std::vector<std::string> exports;
};

MergeStats mergeLibrary(Module& root, Module& library, std::size_t libraryIndex,
                        std::set<std::string>& usedNames, DiagnosticSink& diagnostics) {
    MergeStats stats;
    if (!isLibraryDeclarationAllowed(library)) {
        diagnostics.error(
            "module-unsupported-declaration", library.span,
            "imported modules may only declare functions, lets and curves");
    }
    std::map<std::string, std::string> renames;
    const std::string prefix = "__m" + std::to_string(libraryIndex) + "_";
    const auto assign = [&](const std::string& name, bool exported) {
        if (exported && usedNames.insert(name).second) {
            renames[name] = name;
            stats.exports.push_back(name);
            return;
        }
        if (exported && usedNames.count(name) != 0) {
            diagnostics.error("module-export-conflict", library.span,
                              "exported name '" + name +
                                  "' collides with an existing declaration");
        }
        std::string mangled = prefix + name;
        while (!usedNames.insert(mangled).second) {
            mangled += "_";
        }
        renames[name] = mangled;
    };
    for (const FunctionDecl& function : library.functions) {
        assign(function.name, function.exported);
    }
    for (const LetDecl& let : library.lets) {
        assign(let.name, let.exported);
    }
    for (const CurveDecl& curve : library.curves) {
        assign(curve.name, curve.exported);
    }

    for (FunctionDecl& function : library.functions) {
        function.name = renames.at(function.name);
    }
    for (LetDecl& let : library.lets) {
        let.name = renames.at(let.name);
    }
    for (CurveDecl& curve : library.curves) {
        curve.name = renames.at(curve.name);
    }

    rewriteModuleNames(library, renames);
    const std::size_t offset = root.expressions.size();
    shiftExprIds(library, offset);

    for (const LetDecl& let : library.lets) {
        const std::size_t index = root.lets.size();
        root.lets.push_back(let);
        root.declarationOrder.push_back(DeclRef{DeclKind::Let, index});
    }
    for (const CurveDecl& curve : library.curves) {
        const std::size_t index = root.curves.size();
        root.curves.push_back(curve);
        root.declarationOrder.push_back(DeclRef{DeclKind::Curve, index});
    }
    for (const FunctionDecl& function : library.functions) {
        root.functions.push_back(function);
    }
    root.expressions.insert(root.expressions.end(),
                            std::make_move_iterator(library.expressions.begin()),
                            std::make_move_iterator(library.expressions.end()));
    library.expressions.clear();
    library.lets.clear();
    library.curves.clear();
    library.functions.clear();
    return stats;
}

struct LoadedModule {
    Path path;
    std::string source;
    Module module;
    std::uint64_t hash = 0;
    std::vector<std::size_t> dependencies;
    std::vector<std::string> importPaths;
};

struct ModuleLoader {
    const FrontendOptions& options;
    DiagnosticSink& diagnostics;
    std::vector<LoadedModule> modules;
    std::vector<char> state;
    std::vector<char> resolvedComputed;
    std::vector<std::uint64_t> resolvedHashes;
    std::map<std::string, std::size_t> byPath;
    std::vector<Path> searchPaths;

    ModuleLoader(const FrontendOptions& optionsIn, DiagnosticSink& diagnosticsIn)
        : options(optionsIn), diagnostics(diagnosticsIn), searchPaths(defaultImportPaths()) {
        for (const Path& path : options.importPaths) {
            searchPaths.push_back(path);
        }
    }

    std::optional<Path> resolve(const std::string& importText, const Path& importer,
                                std::vector<Path>& tried) {
        const Path requested(importText);
        std::vector<Path> candidates;
        if (requested.is_absolute()) {
            candidates.push_back(requested);
        } else {
            candidates.push_back(importer.parent_path() / requested);
            if (importText.rfind("std/", 0) == 0) {
                const std::string rest = importText.substr(4);
                for (const Path& base : searchPaths) {
                    candidates.push_back(base / rest);
                }
            }
            for (const Path& base : searchPaths) {
                candidates.push_back(base / requested);
            }
            candidates.push_back(requested);
        }
        std::error_code code;
        for (const Path& candidate : candidates) {
            tried.push_back(candidate);
            if (std::filesystem::is_regular_file(candidate, code) && !code) {
                const Path canonical = std::filesystem::weakly_canonical(candidate, code);
                return code ? candidate.lexically_normal() : canonical;
            }
        }
        return std::nullopt;
    }

    std::size_t addModule(Path path, std::string source, Module module) {
        LoadedModule loaded;
        loaded.path = std::move(path);
        loaded.source = std::move(source);
        loaded.hash = fnv1a64(loaded.source);
        loaded.module = std::move(module);
        const std::string key = loaded.path.generic_string();
        byPath[key] = modules.size();
        modules.push_back(std::move(loaded));
        state.push_back(0);
        resolvedComputed.push_back(0);
        resolvedHashes.push_back(0);
        return modules.size() - 1;
    }

    std::uint64_t resolvedHash(std::size_t index) {
        if (resolvedComputed[index] != 0) {
            return resolvedHashes[index];
        }
        std::uint64_t hash = modules[index].hash;
        for (const std::size_t dependency : modules[index].dependencies) {
            const std::uint64_t dependencyHash = resolvedHash(dependency);
            hash ^= dependencyHash + 0x9e3779b97f4a7c15ull + (hash << 6) + (hash >> 2);
        }
        resolvedHashes[index] = hash;
        resolvedComputed[index] = 1;
        return hash;
    }

    void load(std::size_t index) {
        if (state[index] == 2) {
            return;
        }
        if (state[index] == 1) {
            diagnostics.error("module-import-cycle", modules[index].module.span,
                              "cyclic module import involving '" +
                                  modules[index].module.moduleName + "'");
            return;
        }
        state[index] = 1;
        Module& module = modules[index].module;
        for (const ImportDecl& import : module.imports) {
            std::vector<Path> tried;
            const std::optional<Path> resolved =
                resolve(import.path, modules[index].path, tried);
            if (!resolved.has_value()) {
                std::string message = "cannot resolve import '" + import.path + "'";
                diagnostics.error("module-import-missing", import.span, message);
                continue;
            }
            modules[index].importPaths.push_back(pathText(*resolved));
            const std::string key = resolved->generic_string();
            const auto found = byPath.find(key);
            std::size_t dependency = 0;
            if (found != byPath.end()) {
                dependency = found->second;
            } else {
                std::error_code code;
                const std::string source = readFile(*resolved);
                if (!std::filesystem::is_regular_file(*resolved, code)) {
                    diagnostics.error("module-import-missing", import.span,
                                      "cannot read import '" + import.path + "'");
                    continue;
                }
                DiagnosticSink parseDiagnostics;
                Lexer lexer(source, pathText(*resolved), parseDiagnostics);
                Parser parser(lexer.lex(), parseDiagnostics, false);
                Module loaded = parser.parse();
                if (parseDiagnostics.hasErrors()) {
                    for (FrontendDiagnostic& diagnostic : parseDiagnostics.diagnostics()) {
                        diagnostics.add(std::move(diagnostic));
                    }
                    continue;
                }
                dependency = addModule(*resolved, source, std::move(loaded));
            }
            if (state[dependency] == 1) {
                diagnostics.error("module-import-cycle", import.span,
                                  "cyclic import '" + import.path + "'");
                continue;
            }
            load(dependency);
            modules[index].dependencies.push_back(dependency);
        }
        state[index] = 2;
    }

    void postorder(std::size_t index, std::vector<std::size_t>& order,
                   std::vector<char>& visited) const {
        if (visited[index] != 0) {
            return;
        }
        visited[index] = 1;
        for (const std::size_t dependency : modules[index].dependencies) {
            postorder(dependency, order, visited);
        }
        order.push_back(index);
    }
};

std::string moduleNameOf(const Module& module, const Path& path) {
    if (!module.moduleName.empty()) {
        return module.moduleName;
    }
    if (path.has_stem() && !path.stem().empty()) {
        return path.stem().string();
    }
    return "main";
}

}  // namespace

bool FrontendResult::hasErrors() const noexcept {
    for (const FrontendDiagnostic& diagnostic : diagnostics) {
        if (diagnostic.severity == DiagnosticSeverity::Error) {
            return true;
        }
    }
    return false;
}

FrontendResult compileSource(std::string_view source, std::string file) {
    return compileSource(source, std::move(file), FrontendOptions{});
}

FrontendResult compileSource(std::string_view source, std::string file,
                             const FrontendOptions& options) {
    FrontendResult result;
    DiagnosticSink diagnostics;

    Lexer lexer(source, file, diagnostics);
    Parser parser(lexer.lex(), diagnostics, true);
    Module module = parser.parse();
    if (diagnostics.hasErrors()) {
        result.diagnostics = diagnostics.diagnostics();
        return result;
    }

    std::vector<ModuleRecord> records;
    if (!module.imports.empty()) {
        if (!options.allowFileImports) {
            diagnostics.error("module-import-disabled", module.span,
                              "file imports are disabled for this compilation");
            result.diagnostics = diagnostics.diagnostics();
            return result;
        }
        ModuleLoader loader(options, diagnostics);
        const Path rootPath = file.empty() ? Path("<source>") : Path(file);
        const std::size_t rootIndex = loader.addModule(rootPath, std::string(source),
                                                       std::move(module));
        loader.load(rootIndex);
        std::vector<std::size_t> order;
        std::vector<char> visited(loader.modules.size(), 0);
        loader.postorder(rootIndex, order, visited);
        if (diagnostics.hasErrors()) {
            result.diagnostics = diagnostics.diagnostics();
            return result;
        }
        Module& root = loader.modules[rootIndex].module;
        std::set<std::string> usedNames;
        collectRootNames(root, usedNames);
        std::map<std::size_t, MergeStats> merged;
        for (const std::size_t index : order) {
            if (index == rootIndex) {
                continue;
            }
            merged[index] = mergeLibrary(root, loader.modules[index].module, index,
                                         usedNames, diagnostics);
        }
        for (std::size_t index = 0; index < loader.modules.size(); ++index) {
            const LoadedModule& loaded = loader.modules[index];
            ModuleRecord record;
            record.name = moduleNameOf(loaded.module, loaded.path);
            record.path = pathText(loaded.path);
            record.contentHash = loaded.hash;
            record.resolvedHash = loader.resolvedHash(index);
            record.imports = loaded.importPaths;
            if (index == rootIndex) {
                for (const FunctionDecl& function : loaded.module.functions) {
                    if (function.exported) {
                        record.exports.push_back(function.name);
                    }
                }
            } else {
                const auto found = merged.find(index);
                if (found != merged.end()) {
                    record.exports = found->second.exports;
                }
            }
            records.push_back(std::move(record));
        }
        root.modules = records;
        module = std::move(root);
    }
    if (diagnostics.hasErrors()) {
        result.diagnostics = diagnostics.diagnostics();
        return result;
    }

    if (!module.project.has_value()) {
        diagnostics.error("sema-missing-project", module.span,
                          "program has no project declaration");
        result.diagnostics = diagnostics.diagnostics();
        return result;
    }

    Sema sema(module, diagnostics);
    HirModule hir = sema.analyze();
    if (diagnostics.hasErrors()) {
        result.diagnostics = diagnostics.diagnostics();
        return result;
    }
    hir.modules = module.modules;

    LoweringResult lowered = lowerToProject(module, hir, diagnostics);
    if (diagnostics.hasErrors()) {
        result.diagnostics = diagnostics.diagnostics();
        return result;
    }

    FrontendProgram program;
    program.module = std::move(module);
    program.hir = std::move(hir);
    program.project = std::move(lowered.project);
    program.exports = std::move(lowered.exports);
    result.program = std::move(program);
    result.diagnostics = diagnostics.diagnostics();
    return result;
}

}  // namespace mpl
