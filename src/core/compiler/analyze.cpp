#include "core/compiler/analyze.hpp"

#include "clpp/core/lexer/lexer.hpp"
#include "clpp/stdlib.hpp"
#include "core/binder/binder.hpp"
#include "core/parser/parser.hpp"
#include "core/compiler/module_types.hpp"
#include "core/typechecker/solver.hpp"

#include <algorithm>
#include <cctype>
#include <set>
#include <filesystem>
#include <deque>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace clpp {

namespace {

// ---------------------------------------------------------------------------------------------
// Modules. `link "./file.clp" as A;` or `link @clpp.name as A;` imports one file. There are no
// header files: the module file itself is the interface. A module may declare functions,
// data definitions, scoped enums and aliases. Imported setup runs once, in dependency
// order, and module bindings live in the execution context. Only exports form its interface.
//
// Functions are imported under an alias (`A.fn`). Imported nominal types have a
// private, module-specific identity; aliases resolve to it without sharing the
// source file's global type namespace.
// ---------------------------------------------------------------------------------------------

void forget_spans(parser::Function& function) {
  function.imported = true;
  function.name_span = {};
  for (std::string_view& span : function.param_spans) {
    span = {};
  }
}

void forget_spans(parser::StructDecl& decl) {
  decl.imported = true;
  decl.name_span = {};
  decl.base_span = {};
  for (std::string_view& span : decl.base_spans) {
    span = {};
  }
  for (parser::Field& field : decl.fields) {
    field.name_span = {};
  }
  for (parser::Function& method : decl.methods) {
    forget_spans(method);
  }
}

void forget_spans(parser::EnumDecl& decl) {
  decl.imported = true;
  decl.name_span = {};
  for (std::string_view& span : decl.variant_spans) {
    span = {};
  }
}

[[nodiscard]] bool name_taken(const parser::Program& program, const std::string& name) {
  const std::string prefix = name + ".";
  for (const parser::Function& function : program.functions) {
    if (function.name == name || function.name.compare(0, prefix.size(), prefix) == 0) {
      return true;
    }
  }
  for (const parser::StructDecl& decl : program.structs) {
    if (decl.name == name) {
      return true;
    }
  }
  for (const parser::EnumDecl& decl : program.enums) {
    if (decl.name == name) {
      return true;
    }
  }
  return false;
}

struct ImportContext {
  const ModuleLoader& modules;
  std::vector<std::string>& stack;
  std::deque<std::string>& source_buffers;
  std::vector<std::string>& source_files;
  std::vector<Diagnostic>& diagnostics;
  std::vector<ModuleInfo>* record;  // only the top-level file records its modules for the IDE
  std::deque<std::string>& owned_text;
  // Canonical module identities already installed in this program. Repeated aliases
  // reuse declarations, storage and initialization; they do not create another instance.
  std::set<std::string> typed_paths{};
};

// Repeated aliases reuse the declaration; source functions have unique qualified names.
[[nodiscard]] bool already_has(const parser::Program& program, const parser::Function& function) {
  for (const parser::Function& existing : program.functions) {
    if (existing.name == function.name) {
      return true;
    }
  }
  return false;
}

[[nodiscard]] std::string module_key(std::string path) {
  if (!path.empty() && path.front() == '@') return path;
  std::replace(path.begin(), path.end(), '\\', '/');
  path = std::filesystem::path(path).lexically_normal().generic_string();
  while (path.rfind("./", 0) == 0) path.erase(0, 2);
  return path;
}

[[nodiscard]] std::string resolve_module_path(const std::string& requested, const std::vector<std::string>& stack) {
  if (requested.empty() || requested.front() == '@') return requested;
  std::string portable = requested;
  std::replace(portable.begin(), portable.end(), '\\', '/');
  std::filesystem::path path(portable);
  if (!path.is_absolute() && !stack.empty() && !stack.back().empty() && stack.back().front() != '@') {
    path = std::filesystem::path(stack.back()).parent_path() / path;
  }
  const std::string resolved = path.lexically_normal().generic_string();
  return path.is_absolute() ? resolved : "./" + module_key(resolved);
}

AnalysisResult analyze_impl(std::string_view source, const ModuleLoader& modules, std::vector<std::string>& stack);

// A module is checked on its own first, so an error inside it is reported once, on the `link`
// line of the importing file, with the module's own line and column, instead of leaking
// module coordinates into the importing file.
[[nodiscard]] bool module_is_clean(const std::string& text, const parser::LinkDecl& link, ImportContext& context,
                                   const std::string_view span) {
  context.stack.push_back(link.path);
  const AnalysisResult checked = analyze_impl(text, context.modules, context.stack);
  context.stack.pop_back();
  if (checked.ok()) {
    return true;
  }
  const Diagnostic& first = checked.diagnostics.front();
  context.diagnostics.push_back(Diagnostic{link.path_location, span,
                                           "error in module " + link.path + " (" + std::to_string(first.location.line) +
                                               ":" + std::to_string(first.location.column) + "): " + first.formatted_message()});
  return false;
}

[[nodiscard]] bool import_links(parser::Program& program, ImportContext& context, const bool spans_live) {
  const std::vector<parser::LinkDecl> links = program.links;
  program.links.clear();
  std::set<std::string> imported_aliases;
  for (const parser::LinkDecl& source_link : links) {
    if (!imported_aliases.insert(source_link.alias).second) {
      context.diagnostics.push_back(Diagnostic{source_link.alias_location,
          spans_live ? source_link.alias_span : std::string_view{}, "already declared"});
      return false;
    }
    parser::LinkDecl link = source_link;
    link.path = resolve_module_path(source_link.path, context.stack);
    const std::string_view span = spans_live ? link.path_span : std::string_view{};
    for (const std::string& open : context.stack) {
      if (module_key(open) == module_key(link.path)) {
        context.diagnostics.push_back(Diagnostic{link.path_location, span, "circular link"});
        return false;
      }
    }
    std::optional<std::string> loaded;
    if (!link.path.empty() && link.path.front() == '@') {
      loaded = stdlib::module_source(link.path);
    } else if (context.modules) {
      loaded = context.modules(link.path);
    }
    if (!loaded) {
      context.diagnostics.push_back(Diagnostic{link.path_location, span,
                                               link.path.front() == '@' ? "unknown @clpp module"
                                                                        : "cannot open module"});
      return false;
    }
    context.source_buffers.push_back(std::move(*loaded));
    const std::size_t buffer_index = context.source_buffers.size() - 1;
    context.source_files.push_back(link.path);
    Lexer lexer(context.source_buffers.back());
    LexResult lexed = lexer.tokenize();
    for (auto& token : lexed.tokens) token.location.source = static_cast<std::uint32_t>(buffer_index);
    for (auto& diagnostic : lexed.diagnostics) diagnostic.location.source = static_cast<std::uint32_t>(buffer_index);
    parser::Parser parser(lexed.tokens, lexed.diagnostics);
    parser::ParseResult parsed = parser.parse();
    if (!module_is_clean(context.source_buffers.back(), link, context, span)) {
      return false;
    }
    module_types::Map own_types;
    const std::string identity_prefix = module_types::prefix(module_key(link.path));
    const auto collect_types = [&](const auto& declarations) {
      for (const auto& decl : declarations) own_types.push_back({decl.name, identity_prefix + decl.name});
    };
    collect_types(parsed.program.structs);
    collect_types(parsed.program.enums);
    collect_types(parsed.program.aliases);
    module_types::Map own_callables;
    const std::string function_prefix = "$f" + identity_prefix.substr(2);
    for (const auto& fn : parsed.program.functions) {
      if (fn.name.find('.') == std::string::npos) own_callables.push_back({fn.name, function_prefix + fn.name});
    }
    module_types::Map qualified_callables;
    for (const auto& fn : own_callables) {
      const auto declaration = std::find_if(parsed.program.functions.begin(), parsed.program.functions.end(), [&](const auto& d) { return d.name == fn.spelling; });
      if (declaration != parsed.program.functions.end() && declaration->exported)
        qualified_callables.push_back({link.alias + "." + fn.spelling, fn.identity});
    }
    module_types::Map qualified_types;
    const auto is_public_type = [&](const std::string& name) {
      const auto exported = [&](const auto& declarations) { return std::any_of(declarations.begin(), declarations.end(), [&](const auto& d) { return d.name == name && d.exported; }); };
      return exported(parsed.program.structs) || exported(parsed.program.enums) || exported(parsed.program.aliases);
    };
    for (const auto& type : own_types) if (is_public_type(type.spelling)) qualified_types.push_back({link.alias + "." + type.spelling, type.identity});
    module_types::Map own_globals;
    module_types::Map qualified_globals;
    std::vector<ModuleInfo::Constant> constant_info;
    for (auto& stmt : parsed.program.statements) {
      if (stmt.kind != parser::Stmt::Kind::VariableDecl) continue;
      const auto identity = "$g" + identity_prefix.substr(2) + stmt.name;
      own_globals.push_back({stmt.name, identity});
      if (stmt.exported) {
        qualified_globals.push_back({link.alias + "." + stmt.name, identity});
        constant_info.push_back({stmt.name, stmt.declared_type, std::string(stmt.expr.span), stmt.name_location});
      }
      stmt.module_global = true;
    }
    // The module's own links are imported first, while its own functions are still in place,
    // so their constants and type names are resolved inside them too. Imports only append,
    // so the module's own declarations stay at the front.
    const std::size_t own_functions = parsed.program.functions.size();
    const std::size_t own_structs = parsed.program.structs.size();
    const std::size_t own_enums = parsed.program.enums.size();
    const std::size_t own_aliases = parsed.program.aliases.size();
    context.stack.push_back(link.path);
    std::vector<ModuleInfo>* saved = context.record;
    context.record = nullptr;
    const bool nested = import_links(parsed.program, context, false);
    context.record = saved;
    context.stack.pop_back();
    if (!nested) {
      return false;
    }
    module_types::global_references(parsed.program, own_globals);
    module_types::program(parsed.program, own_types, true);
    module_types::function_references(parsed.program, own_callables, true);
    if (!parsed.program.statements.empty()) {
      parser::Function initialize; initialize.name = "$init" + identity_prefix;
      initialize.return_type = "void"; initialize.module_init = true;
      initialize.body = std::move(parsed.program.statements);
      parsed.program.functions.push_back(std::move(initialize));
    }
    const auto take_front = [](auto& all, const std::size_t count) {
      std::remove_reference_t<decltype(all)> front;
      front.reserve(count);
      for (std::size_t index = 0; index < count && index < all.size(); ++index) {
        front.push_back(std::move(all[index]));
      }
      all.erase(all.begin(), all.begin() + static_cast<std::ptrdiff_t>(std::min(count, all.size())));
      return front;
    };
    std::vector<parser::Function> own = take_front(parsed.program.functions, own_functions);
    std::vector<parser::StructDecl> structs = take_front(parsed.program.structs, own_structs);
    std::vector<parser::EnumDecl> enums = take_front(parsed.program.enums, own_enums);
    std::vector<parser::TypeAlias> aliases = take_front(parsed.program.aliases, own_aliases);
    if (name_taken(program, link.alias)) {
      context.diagnostics.push_back(
          Diagnostic{link.alias_location, spans_live ? link.alias_span : std::string_view{}, "already declared"});
      return false;
    }

    ModuleInfo info;
    info.alias = link.alias;
    info.path = link.path;
    info.buffer = buffer_index;
    info.location = link.path_location;
    for (const auto& type : qualified_types) info.types.push_back(type.spelling.substr(link.alias.size() + 1));
    for (const auto& fn : qualified_callables) info.functions.push_back(fn.spelling.substr(link.alias.size() + 1));

    const bool types_known = !context.typed_paths.insert(module_key(link.path)).second;
    if (types_known) {
      structs.clear();
      enums.clear();
      aliases.clear();
      parsed.program.structs.clear();  // nested copies are known as well
      parsed.program.enums.clear();
      parsed.program.aliases.clear();
      parsed.program.functions.clear();  // the nested modules' functions are in the program already
    }

    for (parser::StructDecl& decl : structs) {
      if (name_taken(program, decl.name)) {
        context.diagnostics.push_back(Diagnostic{link.alias_location, spans_live ? link.alias_span : std::string_view{},
                                                 "already declared"});
        return false;
      }
      forget_spans(decl);
    }
    for (parser::EnumDecl& decl : enums) {
      if (name_taken(program, decl.name)) {
        context.diagnostics.push_back(Diagnostic{link.alias_location, spans_live ? link.alias_span : std::string_view{},
                                                 "already declared"});
        return false;
      }
      forget_spans(decl);
    }
    info.constants = std::move(constant_info);
    for (parser::Function& function : own) {
      if (types_known && function.name.find('.') != std::string::npos) {
        continue;  // a method of a type that is already in the program
      }
      if (already_has(program, function)) {
        continue;  // the same module linked twice under the same alias
      }
      forget_spans(function);
      program.functions.push_back(std::move(function));
    }
    for (parser::Function& function : parsed.program.functions) {
      if (already_has(program, function)) {
        continue;
      }
      forget_spans(function);
      program.functions.push_back(std::move(function));
    }
    for (parser::StructDecl& decl : parsed.program.structs) {
      program.structs.push_back(std::move(decl));
    }
    for (parser::EnumDecl& decl : parsed.program.enums) {
      program.enums.push_back(std::move(decl));
    }
    for (parser::TypeAlias& decl : parsed.program.aliases) program.aliases.push_back(std::move(decl));
    for (parser::StructDecl& decl : structs) {
      program.structs.push_back(std::move(decl));
    }
    for (parser::EnumDecl& decl : enums) {
      program.enums.push_back(std::move(decl));
    }
    for (parser::TypeAlias& decl : aliases) {
      program.aliases.push_back(std::move(decl));
    }
    module_types::program(program, qualified_types, false);
    module_types::function_references(program, qualified_callables, false);
    for (const auto& fn : qualified_callables) program.resolved_functions.push_back(fn);
    for (const auto& type : qualified_types) {
      const auto existing = std::find_if(program.resolved_types.begin(), program.resolved_types.end(),
                                         [&](const auto& candidate) { return candidate.spelling == type.spelling; });
      if (existing == program.resolved_types.end()) program.resolved_types.push_back(type);
    }
    module_types::global_references(program, qualified_globals);
    if (context.record != nullptr) {
      context.record->push_back(std::move(info));
    }
  }
  return true;
}

AnalysisResult analyze_impl(const std::string_view source, const ModuleLoader& modules,
                            std::vector<std::string>& stack) {
  AnalysisResult result;
  result.source_buffers.emplace_back(source);
  result.source_files.emplace_back(stack.empty() ? "" : stack.back());
  Lexer lexer(result.source_buffers.front());
  const LexResult lexed = lexer.tokenize();
  result.tokens = lexed.tokens;
  parser::Parser parser(lexed.tokens, lexed.diagnostics);
  parser::ParseResult parsed = parser.parse();
  result.diagnostics = std::move(parsed.diagnostics);
  result.program = std::move(parsed.program);
  result.links = result.program.links;
  ImportContext context{modules, stack, result.source_buffers, result.source_files, result.diagnostics, &result.modules, result.owned_text};
  (void)import_links(result.program, context, true);
  // Entry files and imported files have the same file-scope binding rules.
  for (auto& stmt : result.program.statements) if (stmt.kind == parser::Stmt::Kind::VariableDecl) stmt.module_global = true;
  if (!stack.empty() && !result.program.statements.empty()) {
    parser::Function initialize; initialize.name = "$validation_init";
    initialize.return_type = "void"; initialize.module_init = true;
    for (auto& stmt : result.program.statements) if (stmt.kind == parser::Stmt::Kind::VariableDecl) stmt.module_global = true;
    initialize.body = std::move(result.program.statements);
    result.program.functions.push_back(std::move(initialize));
  }
  const std::size_t link_errors = result.diagnostics.size();
  binder::bind(result.program, result.diagnostics);
  typechecker::Solver{}.check(result.program, result.diagnostics);
  // A link that failed already has its diagnostic; uses of its alias would only repeat it.
  std::vector<std::string> failed;
  for (const parser::LinkDecl& link : result.links) {
    bool imported = false;
    for (const ModuleInfo& module : result.modules) {
      imported = imported || module.alias == link.alias;
    }
    if (!imported && !link.alias.empty()) {
      failed.push_back(link.alias);
    }
  }
  if (!failed.empty() && link_errors > 0) {
    std::vector<Diagnostic> kept;
    kept.reserve(result.diagnostics.size());
    for (std::size_t index = 0; index < result.diagnostics.size(); ++index) {
      const Diagnostic& diagnostic = result.diagnostics[index];
      bool echo = false;
      if (index >= link_errors) {
        for (const std::string& alias : failed) {
          const std::string_view span = diagnostic.span;
          echo = echo || span == alias || span.substr(0, alias.size() + 1) == alias + ".";
          // `Alias.member`: the diagnostic points at `member`; look just before it.
          const std::string_view text = result.source_buffers.front();
          std::size_t offset = 0;
          for (std::uint32_t line = 1; line < diagnostic.location.line && offset < text.size(); ++offset) {
            line += text[offset] == '\n' ? 1 : 0;
          }
          offset += diagnostic.location.column > 0 ? diagnostic.location.column - 1 : 0;
          const std::string qualified = alias + ".";
          echo = echo || (offset >= qualified.size() && offset <= text.size() &&
                          text.substr(offset - qualified.size(), qualified.size()) == qualified);
        }
      }
      if (!echo) {
        kept.push_back(diagnostic);
      }
    }
    result.diagnostics = std::move(kept);
  }
  auto display_symbols = result.program.resolved_types;
  display_symbols.insert(display_symbols.end(), result.program.resolved_functions.begin(), result.program.resolved_functions.end());
  for (auto& diagnostic : result.diagnostics) {
    diagnostic.message = module_types::display(std::move(diagnostic.message), display_symbols);
    diagnostic.expected_type = module_types::display(std::move(diagnostic.expected_type), display_symbols);
    diagnostic.actual_type = module_types::display(std::move(diagnostic.actual_type), display_symbols);
  }
  return result;
}

}  // namespace

AnalysisResult analyze_program(const std::string_view source, const ModuleLoader& modules) {
  std::vector<std::string> stack;
  return analyze_impl(source, modules, stack);
}

}  // namespace clpp
