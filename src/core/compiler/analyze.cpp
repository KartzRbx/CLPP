#include "core/compiler/analyze.hpp"

#include "clpp/core/lexer/lexer.hpp"
#include "clpp/stdlib.hpp"
#include "core/binder/binder.hpp"
#include "core/parser/parser.hpp"
#include "core/typechecker/solver.hpp"

#include <algorithm>
#include <set>
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
// structs, enums, variants and type aliases; top-level statements are rejected so that
// importing a module never runs code.
//
// Functions are imported under the alias (`A.fn`). Types keep their own name so that they can
// be used in declarations (`Point p = A.Point(1, 2);` or `Point p = Point(1, 2);`); `A.Type`
// in the importing file is rewritten to `Type`.
// ---------------------------------------------------------------------------------------------

void qualify_stmts(std::vector<parser::Stmt>& statements, const std::vector<std::string>& names,
                   const std::string& alias);

void qualify_expr(parser::Expr& expr, const std::vector<std::string>& names, const std::string& alias) {
  if (expr.kind == parser::Expr::Kind::Call) {
    for (const std::string& name : names) {
      if (expr.value == name) {
        expr.value = alias + "." + name;
        break;
      }
    }
  }
  for (parser::Expr& arg : expr.args) {
    qualify_expr(arg, names, alias);
  }
  if (expr.left != nullptr) {
    qualify_expr(*expr.left, names, alias);
  }
  if (expr.right != nullptr) {
    qualify_expr(*expr.right, names, alias);
  }
}

void qualify_stmts(std::vector<parser::Stmt>& statements, const std::vector<std::string>& names,
                   const std::string& alias) {
  for (parser::Stmt& stmt : statements) {
    qualify_expr(stmt.expr, names, alias);
    qualify_expr(stmt.type_expr, names, alias);
    qualify_stmts(stmt.then_body, names, alias);
    qualify_stmts(stmt.else_body, names, alias);
    qualify_stmts(stmt.init, names, alias);
    qualify_stmts(stmt.step, names, alias);
    for (parser::Stmt::MatchArm& arm : stmt.arms) {
      qualify_expr(arm.pattern, names, alias);
      qualify_expr(arm.guard, names, alias);
      qualify_stmts(arm.body, names, alias);
    }
  }
}

// `A.Type` in the importing file becomes `Type` once `Type` was imported from `A`.
void unqualify_types(parser::Expr& expr, const std::string& alias, const std::vector<std::string>& types) {
  const auto imported = [&](const std::string& name) {
    for (const std::string& type : types) {
      if (type == name) {
        return true;
      }
    }
    return false;
  };
  const std::string prefix = alias + ".";
  if ((expr.kind == parser::Expr::Kind::Call || expr.kind == parser::Expr::Kind::Construct) &&
      expr.value.compare(0, prefix.size(), prefix) == 0 && imported(expr.value.substr(prefix.size()))) {
    expr.value = expr.value.substr(prefix.size());
  }
  if (expr.kind == parser::Expr::Kind::Member && expr.left != nullptr && expr.left->kind == parser::Expr::Kind::Name &&
      expr.left->value == alias && imported(expr.value)) {
    parser::Expr name;
    name.kind = parser::Expr::Kind::Name;
    name.value = expr.value;
    name.span = expr.span;
    name.location = expr.location;
    expr = std::move(name);
    return;
  }
  for (parser::Expr& arg : expr.args) {
    unqualify_types(arg, alias, types);
  }
  if (expr.left != nullptr) {
    unqualify_types(*expr.left, alias, types);
  }
  if (expr.right != nullptr) {
    unqualify_types(*expr.right, alias, types);
  }
}

void unqualify_stmts(std::vector<parser::Stmt>& statements, const std::string& alias,
                     const std::vector<std::string>& types) {
  for (parser::Stmt& stmt : statements) {
    unqualify_types(stmt.expr, alias, types);
    unqualify_types(stmt.type_expr, alias, types);
    unqualify_stmts(stmt.then_body, alias, types);
    unqualify_stmts(stmt.else_body, alias, types);
    unqualify_stmts(stmt.init, alias, types);
    unqualify_stmts(stmt.step, alias, types);
    for (parser::Stmt::MatchArm& arm : stmt.arms) {
      unqualify_types(arm.pattern, alias, types);
      unqualify_types(arm.guard, alias, types);
      unqualify_stmts(arm.body, alias, types);
    }
  }
}

void unqualify_program(parser::Program& program, const std::string& alias, const std::vector<std::string>& types) {
  if (types.empty()) {
    return;
  }
  unqualify_stmts(program.statements, alias, types);
  for (parser::Function& function : program.functions) {
    unqualify_stmts(function.body, alias, types);
  }
  for (parser::StructDecl& decl : program.structs) {
    for (parser::Function& method : decl.methods) {
      unqualify_stmts(method.body, alias, types);
    }
  }
}

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
  for (const parser::VariantDecl& decl : program.variants) {
    if (decl.name == name) {
      return true;
    }
  }
  return false;
}

// A module-level constant: `const NAME = literal;` (or an immutable `let`). Its value is inlined
// wherever the importer writes `Alias.NAME`, so importing a module still never runs code.
struct ConstantValue {
  std::string name;
  parser::Expr::Kind kind{parser::Expr::Kind::Number};
  std::string value;
  double number{0};
  bool float_literal{false};
  bool bool_literal{false};
  bool null_literal{false};
};

[[nodiscard]] bool literal_constant(const parser::Stmt& stmt) {
  if (stmt.kind != parser::Stmt::Kind::Let || !stmt.immutable) {
    return false;
  }
  const parser::Expr& value = stmt.expr;
  return (value.kind == parser::Expr::Kind::String || value.kind == parser::Expr::Kind::Number) &&
         value.left == nullptr && value.args.empty() && !value.enum_literal;
}

void inline_constants(parser::Expr& expr, const std::string& alias, const std::vector<ConstantValue>& constants) {
  if (expr.kind == parser::Expr::Kind::Member && expr.left != nullptr && expr.left->kind == parser::Expr::Kind::Name &&
      expr.left->value == alias) {
    for (const ConstantValue& constant : constants) {
      if (constant.name != expr.value) {
        continue;
      }
      parser::Expr literal;
      literal.kind = constant.kind;
      literal.value = constant.value;
      literal.number = constant.number;
      literal.float_literal = constant.float_literal;
      literal.bool_literal = constant.bool_literal;
      literal.null_literal = constant.null_literal;
      literal.location = expr.location;
      literal.span = expr.span;
      expr = std::move(literal);
      return;
    }
  }
  for (parser::Expr& arg : expr.args) {
    inline_constants(arg, alias, constants);
  }
  if (expr.left != nullptr) {
    inline_constants(*expr.left, alias, constants);
  }
  if (expr.right != nullptr) {
    inline_constants(*expr.right, alias, constants);
  }
}

void inline_constants(std::vector<parser::Stmt>& statements, const std::string& alias,
                      const std::vector<ConstantValue>& constants) {
  for (parser::Stmt& stmt : statements) {
    inline_constants(stmt.expr, alias, constants);
    inline_constants(stmt.type_expr, alias, constants);
    inline_constants(stmt.then_body, alias, constants);
    inline_constants(stmt.else_body, alias, constants);
    inline_constants(stmt.init, alias, constants);
    inline_constants(stmt.step, alias, constants);
    for (parser::Stmt::MatchArm& arm : stmt.arms) {
      inline_constants(arm.pattern, alias, constants);
      inline_constants(arm.guard, alias, constants);
      inline_constants(arm.body, alias, constants);
    }
  }
}

// Inside the module itself, a bare `NAME` refers to its own constant.
void inline_own_constants(parser::Expr& expr, const std::vector<ConstantValue>& constants) {
  if (expr.kind == parser::Expr::Kind::Name) {
    for (const ConstantValue& constant : constants) {
      if (constant.name == expr.value) {
        parser::Expr literal;
        literal.kind = constant.kind;
        literal.value = constant.value;
        literal.number = constant.number;
        literal.float_literal = constant.float_literal;
        literal.bool_literal = constant.bool_literal;
        literal.null_literal = constant.null_literal;
        literal.location = expr.location;
        expr = std::move(literal);
        return;
      }
    }
  }
  for (parser::Expr& arg : expr.args) {
    inline_own_constants(arg, constants);
  }
  if (expr.left != nullptr) {
    inline_own_constants(*expr.left, constants);
  }
  if (expr.right != nullptr) {
    inline_own_constants(*expr.right, constants);
  }
}

void collect_declared(const std::vector<parser::Stmt>& statements, std::vector<std::string>& names) {
  for (const parser::Stmt& stmt : statements) {
    if (!stmt.name.empty() && (stmt.kind == parser::Stmt::Kind::Let || stmt.kind == parser::Stmt::Kind::ForIn)) {
      names.push_back(stmt.name);
    }
    for (const std::string& name : stmt.unpack) {
      names.push_back(name);
    }
    collect_declared(stmt.then_body, names);
    collect_declared(stmt.else_body, names);
    collect_declared(stmt.init, names);
    collect_declared(stmt.step, names);
    for (const parser::Stmt::MatchArm& arm : stmt.arms) {
      if (!arm.bind_name.empty()) {
        names.push_back(arm.bind_name);
      }
      collect_declared(arm.body, names);
    }
  }
}

void inline_own_constants(std::vector<parser::Stmt>& statements, const std::vector<ConstantValue>& constants);

// Functions see the file's top-level constants (`const NAME = literal;`), unless a parameter or a
// local of the function uses the same name.
void inline_into_function(parser::Function& function, const std::vector<ConstantValue>& constants) {
  std::vector<std::string> hidden = function.params;
  collect_declared(function.body, hidden);
  std::vector<ConstantValue> visible;
  for (const ConstantValue& constant : constants) {
    if (std::find(hidden.begin(), hidden.end(), constant.name) == hidden.end()) {
      visible.push_back(constant);
    }
  }
  if (!visible.empty()) {
    inline_own_constants(function.body, visible);
  }
}

void inline_own_constants(std::vector<parser::Stmt>& statements, const std::vector<ConstantValue>& constants) {
  for (parser::Stmt& stmt : statements) {
    inline_own_constants(stmt.expr, constants);
    inline_own_constants(stmt.then_body, constants);
    inline_own_constants(stmt.else_body, constants);
    inline_own_constants(stmt.init, constants);
    inline_own_constants(stmt.step, constants);
    for (parser::Stmt::MatchArm& arm : stmt.arms) {
      inline_own_constants(arm.guard, constants);
      inline_own_constants(arm.body, constants);
    }
  }
}

[[nodiscard]] std::vector<ConstantValue> file_constants(const std::vector<parser::Stmt>& statements) {
  std::vector<ConstantValue> constants;
  for (const parser::Stmt& stmt : statements) {
    if (!literal_constant(stmt)) {
      continue;
    }
    ConstantValue constant;
    constant.name = stmt.name;
    constant.kind = stmt.expr.kind;
    constant.value = stmt.expr.value;
    constant.number = stmt.expr.number;
    constant.float_literal = stmt.expr.float_literal;
    constant.bool_literal = stmt.expr.bool_literal;
    constant.null_literal = stmt.expr.null_literal;
    constants.push_back(std::move(constant));
  }
  return constants;
}

struct ImportContext {
  const ModuleLoader& modules;
  std::vector<std::string>& stack;
  std::deque<std::string>& source_buffers;
  std::vector<Diagnostic>& diagnostics;
  std::vector<ModuleInfo>* record;  // only the top-level file records its modules for the IDE
  std::deque<std::string>& owned_text;
  // Modules whose types are already in the program: a second path to the same module
  // (A links C, B links C) adds its functions under the new alias but not its types again.
  std::set<std::string> typed_paths{};
};

// Same name and arity: overloads such as Round(x) / Round(x, digits) are different functions.
[[nodiscard]] bool already_has(const parser::Program& program, const parser::Function& function) {
  for (const parser::Function& existing : program.functions) {
    if (existing.name == function.name && existing.params.size() == function.params.size()) {
      return true;
    }
  }
  return false;
}

[[nodiscard]] std::string module_key(std::string path) {
  while (path.rfind("./", 0) == 0) {
    path.erase(0, 2);
  }
  return path;
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
                                               ":" + std::to_string(first.location.column) + "): " + first.message});
  return false;
}

[[nodiscard]] bool import_links(parser::Program& program, ImportContext& context, const bool spans_live) {
  const std::vector<parser::LinkDecl> links = program.links;
  program.links.clear();
  for (const parser::LinkDecl& link : links) {
    const std::string_view span = spans_live ? link.path_span : std::string_view{};
    for (const std::string& open : context.stack) {
      if (open == link.path) {
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
    Lexer lexer(context.source_buffers.back());
    const LexResult lexed = lexer.tokenize();
    parser::Parser parser(lexed.tokens, lexed.diagnostics);
    parser::ParseResult parsed = parser.parse();
    if (!module_is_clean(context.source_buffers.back(), link, context, span)) {
      return false;
    }
    std::vector<ConstantValue> constants;
    std::vector<ModuleInfo::Constant> constant_info;
    for (const parser::Stmt& stmt : parsed.program.statements) {
      if (!literal_constant(stmt)) {
        context.diagnostics.push_back(Diagnostic{
            link.path_location, span,
            "a module can only declare func, struct, enum, variant, type and constants (const NAME = literal;); "
            "move other top-level code into a function"});
        return false;
      }
      ConstantValue constant;
      constant.name = stmt.name;
      constant.kind = stmt.expr.kind;
      constant.value = stmt.expr.value;
      constant.number = stmt.expr.number;
      constant.float_literal = stmt.expr.float_literal;
      constant.bool_literal = stmt.expr.bool_literal;
      constant.null_literal = stmt.expr.null_literal;
      std::string type = stmt.expr.kind == parser::Expr::Kind::String ? "string"
                         : stmt.expr.bool_literal                     ? "bool"
                         : stmt.expr.float_literal                    ? "float"
                                                                      : "int";
      if (!stmt.declared_type.empty()) {
        type = stmt.declared_type;
      }
      std::string display = stmt.expr.kind == parser::Expr::Kind::String ? "\"" + stmt.expr.value + "\""
                                                                         : std::string(stmt.expr.span);
      if (display.empty()) {
        display = stmt.expr.bool_literal ? (stmt.expr.number != 0 ? "true" : "false") : std::to_string(stmt.expr.number);
      }
      constant_info.push_back(ModuleInfo::Constant{stmt.name, type, display, stmt.name_location});
      constants.push_back(std::move(constant));
    }
    // The module's own links are imported first, while its own functions are still in place,
    // so their constants and type names are resolved inside them too. Imports only append,
    // so the module's own declarations stay at the front.
    const std::size_t own_functions = parsed.program.functions.size();
    const std::size_t own_structs = parsed.program.structs.size();
    const std::size_t own_enums = parsed.program.enums.size();
    const std::size_t own_variants = parsed.program.variants.size();
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
    std::vector<parser::VariantDecl> variants = take_front(parsed.program.variants, own_variants);
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

    const bool types_known = !context.typed_paths.insert(module_key(link.path)).second;
    if (types_known) {
      for (const parser::StructDecl& decl : structs) {
        info.types.push_back(decl.name);
      }
      for (const parser::EnumDecl& decl : enums) {
        info.types.push_back(decl.name);
      }
      for (const parser::VariantDecl& decl : variants) {
        info.types.push_back(decl.name);
      }
      for (const parser::TypeAlias& decl : aliases) {
        info.types.push_back(decl.name);
      }
      structs.clear();
      enums.clear();
      variants.clear();
      aliases.clear();
      parsed.program.structs.clear();  // nested copies are known as well
      parsed.program.enums.clear();
      parsed.program.variants.clear();
      parsed.program.aliases.clear();
      parsed.program.functions.clear();  // the nested modules' functions are in the program already
    }

    std::vector<std::string> local_names;
    local_names.reserve(own.size());
    for (const parser::Function& function : own) {
      if (function.name.find('.') == std::string::npos) {  // `Type.method` are struct methods
        local_names.push_back(function.name);
      }
    }
    for (parser::StructDecl& decl : structs) {
      if (name_taken(program, decl.name)) {
        context.diagnostics.push_back(Diagnostic{link.alias_location, spans_live ? link.alias_span : std::string_view{},
                                                 "already declared"});
        return false;
      }
      for (parser::Function& method : decl.methods) {
        qualify_stmts(method.body, local_names, link.alias);
      }
      info.types.push_back(decl.name);
      forget_spans(decl);
    }
    for (parser::EnumDecl& decl : enums) {
      if (name_taken(program, decl.name)) {
        context.diagnostics.push_back(Diagnostic{link.alias_location, spans_live ? link.alias_span : std::string_view{},
                                                 "already declared"});
        return false;
      }
      info.types.push_back(decl.name);
      forget_spans(decl);
    }
    for (const parser::VariantDecl& decl : variants) {
      info.types.push_back(decl.name);
    }
    for (const parser::TypeAlias& decl : aliases) {
      info.types.push_back(decl.name);
    }
    info.constants = std::move(constant_info);
    for (parser::Function& function : own) {
      if (types_known && function.name.find('.') != std::string::npos) {
        continue;  // a method of a type that is already in the program
      }
      inline_into_function(function, constants);
      qualify_stmts(function.body, local_names, link.alias);
      if (function.name.find('.') == std::string::npos) {
        info.functions.push_back(function.name);
        function.name = link.alias + "." + function.name;
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
    for (parser::StructDecl& decl : structs) {
      program.structs.push_back(std::move(decl));
    }
    for (parser::EnumDecl& decl : enums) {
      program.enums.push_back(std::move(decl));
    }
    for (parser::VariantDecl& decl : variants) {
      program.variants.push_back(std::move(decl));
    }
    for (parser::TypeAlias& decl : aliases) {
      program.aliases.push_back(std::move(decl));
    }
    unqualify_program(program, link.alias, info.types);
    if (!constants.empty()) {
      inline_constants(program.statements, link.alias, constants);
      for (parser::Function& function : program.functions) {
        if (function.name.compare(0, link.alias.size() + 1, link.alias + ".") != 0) {
          inline_constants(function.body, link.alias, constants);
        }
      }
    }
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
  Lexer lexer(result.source_buffers.front());
  const LexResult lexed = lexer.tokenize();
  result.tokens = lexed.tokens;
  parser::Parser parser(lexed.tokens, lexed.diagnostics);
  parser::ParseResult parsed = parser.parse();
  result.diagnostics = std::move(parsed.diagnostics);
  result.program = std::move(parsed.program);
  result.links = result.program.links;
  // Top-level constants are visible inside the file's functions and methods.
  if (const std::vector<ConstantValue> constants = file_constants(result.program.statements); !constants.empty()) {
    for (parser::Function& function : result.program.functions) {
      inline_into_function(function, constants);
    }
  }
  ImportContext context{modules, stack, result.source_buffers, result.diagnostics, &result.modules, result.owned_text};
  (void)import_links(result.program, context, true);
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
  return result;
}

}  // namespace

AnalysisResult analyze_program(const std::string_view source, const ModuleLoader& modules) {
  std::vector<std::string> stack;
  return analyze_impl(source, modules, stack);
}

}  // namespace clpp
