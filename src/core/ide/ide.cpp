#include "clpp/ide.hpp"

#include "clpp/core/lexer/lexer.hpp"
#include "clpp/semantic.hpp"
#include "clpp/stdlib.hpp"
#include "core/compiler/analyze.hpp"

#include <algorithm>
#include <cctype>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace clpp::ide {

namespace {

struct Symbol {
  std::string name;
  std::string detail;
  Kind kind{Kind::Variable};
  std::uint32_t line{1};
  std::uint32_t column{1};
  std::uint32_t def_line{1};
  std::uint32_t def_column{1};
  std::uint32_t end_line{0xFFFFFFFF};
  std::uint32_t end_column{0xFFFFFFFF};
  std::string value;
};

struct Member {
  std::string owner;
  std::string name;
  std::string detail;
  Kind kind{Kind::Field};
};

[[nodiscard]] bool before(const std::uint32_t line, const std::uint32_t column, const std::uint32_t other_line,
                          const std::uint32_t other_column) {
  return line < other_line || (line == other_line && column < other_column);
}

[[nodiscard]] bool visible(const Symbol& symbol, const std::uint32_t line, const std::uint32_t column) {
  const bool after_start = symbol.line < line || (symbol.line == line && symbol.column <= column);
  return after_start && before(line, column, symbol.end_line, symbol.end_column);
}

[[nodiscard]] SourceLocation anchor(const parser::Stmt& stmt) {
  if (stmt.kind == parser::Stmt::Kind::Let || stmt.kind == parser::Stmt::Kind::Assign ||
      stmt.kind == parser::Stmt::Kind::SelfAssign) {
    return stmt.name_location;
  }
  return stmt.expr.location;
}

[[nodiscard]] std::string visible_detail(const std::vector<Symbol>& symbols, const std::string& name) {
  const Symbol* best = nullptr;
  for (const Symbol& symbol : symbols) {
    if (symbol.kind != Kind::Variable || symbol.name != name) {
      continue;
    }
    if (best == nullptr || symbol.line > best->line || (symbol.line == best->line && symbol.column > best->column)) {
      best = &symbol;
    }
  }
  return best == nullptr ? std::string{} : best->detail;
}

[[nodiscard]] std::string display_type(const parser::Stmt& stmt, const std::vector<Symbol>& symbols) {
  if (!stmt.declared_type.empty()) {
    return stmt.declared_type;
  }
  if (stmt.expr.kind == parser::Expr::Kind::String) {
    return "string";
  }
  if (stmt.expr.kind == parser::Expr::Kind::Number && !stmt.expr.enum_literal) {
    if (stmt.expr.bool_literal) {
      return "bool";
    }
    if (!stmt.expr.null_literal) {
      return stmt.expr.float_literal ? "float" : "int";
    }
  }
  if (stmt.expr.kind == parser::Expr::Kind::Name) {
    const std::string copied = visible_detail(symbols, stmt.expr.value);
    if (!copied.empty() && copied != "value") {
      return copied;
    }
  }
  if (stmt.expr.kind == parser::Expr::Kind::Construct && !stmt.expr.value.empty()) {
    return stmt.expr.value;
  }
  return "value";
}

void walk_stmts(const std::vector<parser::Stmt>& statements, const std::uint32_t end_line, const std::uint32_t end_column,
                std::vector<Symbol>& symbols, std::vector<std::string>& self_fields) {
  for (std::size_t index = 0; index < statements.size(); ++index) {
    const parser::Stmt& stmt = statements[index];
    std::uint32_t next_line = end_line;
    std::uint32_t next_column = end_column;
    if (index + 1 < statements.size()) {
      const SourceLocation next = anchor(statements[index + 1]);
      next_line = next.line;
      next_column = next.column;
    }
    if (stmt.kind == parser::Stmt::Kind::Let || stmt.kind == parser::Stmt::Kind::ForIn ||
        stmt.kind == parser::Stmt::Kind::Signal || stmt.kind == parser::Stmt::Kind::Using) {
      const bool function = stmt.kind == parser::Stmt::Kind::Using;
      const std::string detail = function ? "func" : stmt.kind == parser::Stmt::Kind::Signal ? "signal"
                                 : stmt.kind == parser::Stmt::Kind::ForIn ? "int"
                                                                         : display_type(stmt, symbols);
      const std::uint32_t symbol_end_line = stmt.kind == parser::Stmt::Kind::ForIn ? next_line : end_line;
      const std::uint32_t symbol_end_column = stmt.kind == parser::Stmt::Kind::ForIn ? next_column : end_column;
      Symbol symbol{stmt.name, detail, function ? Kind::Function : Kind::Variable, stmt.name_location.line,
                    stmt.name_location.column, stmt.name_location.line, stmt.name_location.column, symbol_end_line,
                    symbol_end_column, stmt.remembered};
      symbols.push_back(std::move(symbol));
    }
    if (stmt.kind == parser::Stmt::Kind::Assign && !stmt.remembered.empty()) {
      for (auto entry = symbols.rbegin(); entry != symbols.rend(); ++entry) {
        if (entry->kind == Kind::Variable && entry->name == stmt.name) {
          entry->value = stmt.remembered;
          break;
        }
      }
    }
    if (stmt.kind == parser::Stmt::Kind::SelfAssign) {
      self_fields.push_back(stmt.name);
    }
    if (stmt.kind == parser::Stmt::Kind::For) {
      walk_stmts(stmt.init, next_line, next_column, symbols, self_fields);
    }
    walk_stmts(stmt.then_body, next_line, next_column, symbols, self_fields);
    walk_stmts(stmt.else_body, next_line, next_column, symbols, self_fields);
    walk_stmts(stmt.step, next_line, next_column, symbols, self_fields);
    for (const parser::Stmt::MatchArm& arm : stmt.arms) {
      walk_stmts(arm.body, next_line, next_column, symbols, self_fields);
    }
  }
}

[[nodiscard]] bool starts_with(const std::string_view text, const std::string_view prefix) {
  return text.size() >= prefix.size() && text.substr(0, prefix.size()) == prefix;
}

[[nodiscard]] bool matches_prefix(const std::string_view text, const std::string_view prefix) {
  if (prefix.empty()) {
    return true;
  }
  std::size_t index = 0;
  for (const char character : text) {
    if (character == prefix[index]) {
      ++index;
      if (index == prefix.size()) {
        return true;
      }
    }
  }
  return false;
}

[[nodiscard]] bool suppress_completion(const std::string_view source, const std::uint32_t line, const std::uint32_t column) {
  std::size_t index = 0;
  std::uint32_t current_line = 1;
  std::uint32_t current_column = 1;
  bool line_comment = false;
  bool block_comment = false;
  bool quoted = false;
  char quote = 0;
  while (index < source.size() && (current_line < line || (current_line == line && current_column < column))) {
    const char character = source[index];
    if (line_comment) {
      if (character == '\n') {
        line_comment = false;
      }
    } else if (block_comment) {
      if (character == ']' && index + 2 < source.size() && source[index + 1] == '>' && source[index + 2] == '>') {
        index += 2;
        current_column += 2;
        block_comment = false;
      }
    } else if (quoted) {
      if (character == '\\' && index + 1 < source.size()) {
        ++index;
        ++current_column;
      } else if (character == quote) {
        quoted = false;
      }
    } else if (character == '<' && index + 1 < source.size() && source[index + 1] == '<') {
      if (index + 2 < source.size() && source[index + 2] == '[') {
        block_comment = true;
        index += 2;
        current_column += 2;
      } else {
        line_comment = true;
        ++index;
        ++current_column;
      }
    } else if (character == '"' || character == '\'') {
      quoted = true;
      quote = character;
    }
    if (character == '\n') {
      ++current_line;
      current_column = 1;
    } else {
      ++current_column;
    }
    ++index;
  }
  return line_comment || block_comment || quoted;
}

void add_unique(std::vector<Item>& items, Item item) {
  switch (item.kind) {
    case Kind::Field:
    case Kind::EnumMember:
      item.sort_text = "0";
      break;
    case Kind::Variable:
    case Kind::Constant:
      item.sort_text = "1";
      break;
    case Kind::Function:
    case Kind::Method:
      item.sort_text = "2";
      break;
    case Kind::Struct:
    case Kind::Enum:
    case Kind::Module:
    case Kind::Type:
      item.sort_text = "3";
      break;
    case Kind::Keyword:
      item.sort_text = "4";
      break;
    case Kind::Snippet:
      item.sort_text = "6";
      break;
  }
  for (const Item& existing : items) {
    if (existing.label == item.label && existing.kind == item.kind) {
      return;
    }
  }
  items.push_back(std::move(item));
}

// The token the cursor is "in": the one that ends at or contains the cursor. When the cursor sits
// between two tokens (`Math.|)` right after VS Code auto-closed a parenthesis), the token that ends
// at the cursor wins over the one that starts there; otherwise completion would look at `)`.
[[nodiscard]] const Token* token_at(const std::vector<Token>& tokens, const std::uint32_t line,
                                    const std::uint32_t column) {
  const Token* ending = nullptr;
  const Token* containing = nullptr;
  for (const Token& token : tokens) {
    if (token.type == TokenType::Eof || token.location.line != line) {
      continue;
    }
    const std::uint32_t start = token.location.column;
    const std::uint32_t end = start + static_cast<std::uint32_t>(token.lexeme.size());
    if (column == end && end > start) {
      ending = &token;
    } else if (column >= start && column < end) {
      containing = &token;
    }
  }
  if (ending != nullptr && (containing == nullptr || ending->type == TokenType::Identifier ||
                            ending->type == TokenType::Dot || ending->type == TokenType::At ||
                            ending->type == TokenType::Scope)) {
    return ending;
  }
  return containing != nullptr ? containing : ending;
}

[[nodiscard]] const Token* token_before(const std::vector<Token>& tokens, const Token& token) {
  const Token* previous = nullptr;
  for (const Token& candidate : tokens) {
    if (&candidate == &token) {
      return previous;
    }
    if (candidate.type != TokenType::Eof) {
      previous = &candidate;
    }
  }
  return nullptr;
}

// ---------------------------------------------------------------------------------------------
// Semantic helpers shared by completion, hover and go-to-definition.
// ---------------------------------------------------------------------------------------------

[[nodiscard]] std::string bare_type(const std::string& type) {
  const std::size_t generic = type.find('<');
  return generic == std::string::npos ? type : type.substr(0, generic);
}

[[nodiscard]] const parser::StructDecl* find_struct(const parser::Program& program, const std::string& name) {
  const std::string bare = bare_type(name);
  for (const parser::StructDecl& decl : program.structs) {
    if (decl.name == bare) {
      return &decl;
    }
  }
  return nullptr;
}

[[nodiscard]] const parser::EnumDecl* find_enum(const parser::Program& program, const std::string& name) {
  for (const parser::EnumDecl& decl : program.enums) {
    if (decl.name == name) {
      return &decl;
    }
  }
  return nullptr;
}

[[nodiscard]] const parser::Function* find_function(const parser::Program& program, const std::string& name) {
  for (const parser::Function& function : program.functions) {
    if (function.name == name) {
      return &function;
    }
  }
  return nullptr;
}

[[nodiscard]] const ModuleInfo* find_module(const AnalysisResult& analysis, const std::string& alias) {
  for (const ModuleInfo& module : analysis.modules) {
    if (module.alias == alias) {
      return &module;
    }
  }
  return nullptr;
}

// Struct plus all its bases, nearest first, each once (diamond-safe).
[[nodiscard]] std::vector<const parser::StructDecl*> lineage(const parser::Program& program, const std::string& name) {
  std::vector<const parser::StructDecl*> order;
  std::vector<std::string> pending{bare_type(name)};
  while (!pending.empty()) {
    const std::string current = pending.front();
    pending.erase(pending.begin());
    const parser::StructDecl* decl = find_struct(program, current);
    if (decl == nullptr || std::find(order.begin(), order.end(), decl) != order.end()) {
      continue;
    }
    order.push_back(decl);
    if (!decl->base.empty()) {
      pending.push_back(decl->base);
    }
    for (const std::string& base : decl->bases) {
      pending.push_back(base);
    }
  }
  return order;
}

[[nodiscard]] std::string params_text(const parser::Function& function) {
  std::string text;
  const bool method = function.name.find('.') != std::string::npos && !function.params.empty() &&
                      function.params.front() == "self";
  for (std::size_t index = method ? 1 : 0; index < function.params.size(); ++index) {
    if (!text.empty()) {
      text += ", ";
    }
    const std::string type = index < function.param_types.size() ? function.param_types[index] : std::string{};
    text += type.empty() ? function.params[index] : type + " " + function.params[index];
  }
  if (function.variadic) {
    text += text.empty() ? "..." : ", ...";
  }
  return text;
}

[[nodiscard]] std::string short_name(const std::string& name) {
  const std::size_t dot = name.rfind('.');
  return dot == std::string::npos ? name : name.substr(dot + 1);
}

[[nodiscard]] std::string signature_text(const parser::Function& function) {
  std::string text = function.is_async ? "async func " : "func ";
  text += short_name(function.name) + "(" + params_text(function) + ")";
  if (!function.return_type.empty() && function.return_type != "void") {
    text += " -> " + function.return_type;
  }
  return text;
}

[[nodiscard]] std::string code_block(const std::string& code) { return "```clpp\n" + code + "\n```"; }

// Where an imported entity lives: the `link` path of the module that declared it.
[[nodiscard]] const ModuleInfo* origin_of_function(const AnalysisResult& analysis, const std::string& qualified) {
  const std::size_t dot = qualified.find('.');
  if (dot == std::string::npos) {
    return nullptr;
  }
  const ModuleInfo* module = find_module(analysis, qualified.substr(0, dot));
  if (module == nullptr) {
    return nullptr;
  }
  const std::string bare = qualified.substr(dot + 1);
  return std::find(module->functions.begin(), module->functions.end(), bare) != module->functions.end() ? module
                                                                                                           : nullptr;
}

[[nodiscard]] const ModuleInfo* origin_of_type(const AnalysisResult& analysis, const std::string& type) {
  for (const ModuleInfo& module : analysis.modules) {
    if (std::find(module.types.begin(), module.types.end(), type) != module.types.end()) {
      return &module;
    }
  }
  return nullptr;
}

[[nodiscard]] std::string struct_summary(const parser::Program& program, const parser::StructDecl& decl) {
  std::string text = "struct " + decl.name;
  std::vector<std::string> bases = decl.bases;
  if (bases.empty() && !decl.base.empty()) {
    bases.push_back(decl.base);
  }
  for (std::size_t index = 0; index < bases.size(); ++index) {
    text += (index == 0 ? " : " : ", ") + bases[index];
  }
  text += " {\n";
  for (const parser::Field& field : decl.fields) {
    if (!field.owner.empty() && field.owner != decl.name) {
      continue;
    }
    text += "  " + std::string(field.is_private ? "private " : "") + (field.type_name.empty() ? "auto" : field.type_name) +
            " " + field.name + ";\n";
  }
  const std::string prefix = decl.name + ".";
  for (const parser::Function& function : program.functions) {
    if (function.name.compare(0, prefix.size(), prefix) == 0) {
      text += "  " + signature_text(function) + ";\n";
    }
  }
  text += "}";
  return text;
}

// Type of the value named `owner` at the cursor: a visible variable's type, or the name itself.
[[nodiscard]] std::string type_of_name(const std::string& owner, const std::vector<Symbol>& symbols,
                                       const std::uint32_t line, const std::uint32_t column) {
  const Symbol* best = nullptr;
  for (const Symbol& symbol : symbols) {
    if (symbol.kind != Kind::Variable || symbol.name != owner || !visible(symbol, line, column)) {
      continue;
    }
    if (best == nullptr || symbol.line > best->line || (symbol.line == best->line && symbol.column > best->column)) {
      best = &symbol;
    }
  }
  if (best != nullptr && best->detail != "value") {
    return best->detail;
  }
  return owner;
}

// Name of the struct whose method encloses the cursor (for `self.` / `@`).
[[nodiscard]] std::string enclosing_struct(const parser::Program& program, const std::vector<Token>& tokens,
                                           const std::uint32_t line, const std::uint32_t column) {
  std::string found;
  std::uint32_t found_line = 0;
  for (const parser::Function& function : program.functions) {
    const std::size_t dot = function.name.find('.');
    if (dot == std::string::npos || function.imported || function.params.empty() || function.params.front() != "self" ||
        function.name_location.line == 0 || function.name_location.line > line) {
      continue;
    }
    // Close brace of this method.
    bool seen = false;
    int depth = 0;
    SourceLocation close{0xFFFFFFFF, 0xFFFFFFFF};
    for (const Token& token : tokens) {
      if (!seen) {
        seen = token.location.line == function.name_location.line && token.location.column == function.name_location.column;
        continue;
      }
      if (token.type == TokenType::OpenBrace) {
        ++depth;
      } else if (token.type == TokenType::CloseBrace && --depth == 0) {
        close = token.location;
        break;
      }
    }
    if (!before(line, column, close.line, close.column + 1)) {
      continue;
    }
    if (function.name_location.line >= found_line) {
      found = function.name.substr(0, dot);
      found_line = function.name_location.line;
    }
  }
  return found;
}

void add_struct_members(const std::string& type, const std::string& prefix, const parser::Program& program,
                        const std::string& viewer, std::vector<Item>& items) {
  const std::vector<const parser::StructDecl*> chain = lineage(program, type);
  for (std::size_t level = 0; level < chain.size(); ++level) {
    const parser::StructDecl& decl = *chain[level];
    for (const parser::Field& field : decl.fields) {
      const std::string owner = field.owner.empty() ? decl.name : field.owner;
      if (field.is_private && viewer != owner) {
        continue;
      }
      if (!matches_prefix(field.name, prefix)) {
        continue;
      }
      Item item{field.name, field.type_name.empty() ? "auto" : field.type_name, Kind::Field};
      if (owner != chain.front()->name) {
        item.detail += "  (inherited from " + owner + ")";
      }
      item.documentation = code_block((field.type_name.empty() ? "auto" : field.type_name) + " " + owner + "." + field.name);
      add_unique(items, std::move(item));
    }
    const std::string methods = decl.name + ".";
    for (const parser::Function& function : program.functions) {
      if (function.name.compare(0, methods.size(), methods) != 0) {
        continue;
      }
      if (function.is_private && viewer != decl.name) {
        continue;
      }
      const std::string label = function.name.substr(methods.size());
      if (!matches_prefix(label, prefix)) {
        continue;
      }
      Item item{label, signature_text(function), Kind::Method};
      if (decl.name != chain.front()->name) {
        item.detail += "  (inherited from " + decl.name + ")";
      }
      item.documentation = code_block(decl.name + "." + signature_text(function).substr(5));
      item.insert_text = label + (params_text(function).empty() ? "()" : "($1)$0");
      item.snippet = !params_text(function).empty();
      add_unique(items, std::move(item));
    }
  }
}

void complete_members(const std::string& owner, const std::string& prefix, const AnalysisResult& analysis,
                      const std::vector<Symbol>& symbols, const std::vector<Token>& tokens, const std::uint32_t line,
                      const std::uint32_t column, std::vector<Item>& items) {
  const parser::Program& program = analysis.program;
  const std::string viewer = enclosing_struct(program, tokens, line, column);
  std::string type = owner == "self" && !viewer.empty() ? viewer : type_of_name(owner, symbols, line, column);
  const std::string bare = bare_type(type);
  if (bare == "Vector3" || bare == "Vector2" || bare == "Vector4") {
    for (const char* field : {"x", "y", "z", "w"}) {
      if (bare == "Vector2" && (field[0] == 'z' || field[0] == 'w')) {
        continue;
      }
      if (bare == "Vector3" && field[0] == 'w') {
        continue;
      }
      if (matches_prefix(field, prefix)) {
        add_unique(items, Item{field, "float · componente", Kind::Field});
      }
    }
  }
  add_struct_members(type, prefix, program, viewer, items);
  if (type != owner) {
    add_struct_members(owner, prefix, program, viewer, items);
  }
  if (const parser::EnumDecl* decl = find_enum(program, owner)) {
    for (std::size_t index = 0; index < decl->variants.size(); ++index) {
      const std::string& variant = decl->variants[index];
      if (matches_prefix(variant, prefix)) {
        Item item{variant, owner + " = " + std::to_string(index), Kind::EnumMember};
        add_unique(items, std::move(item));
      }
    }
  }
  const std::string qualified = owner + ".";
  for (const parser::Function& function : program.functions) {
    if (function.name.compare(0, qualified.size(), qualified) != 0 || find_struct(program, owner) != nullptr) {
      continue;
    }
    const std::string label = function.name.substr(qualified.size());
    if (label.find('.') != std::string::npos || !matches_prefix(label, prefix)) {
      continue;
    }
    Item item{label, signature_text(function), Kind::Function};
    item.documentation = code_block(signature_text(function));
    if (const ModuleInfo* module = origin_of_function(analysis, function.name)) {
      item.documentation += "\n\nFrom `" + module->path + "`";
    }
    item.insert_text = label + (params_text(function).empty() ? "()" : "($1)$0");
    item.snippet = !params_text(function).empty();
    add_unique(items, std::move(item));
  }
  if (const ModuleInfo* module = find_module(analysis, owner)) {
    for (const ModuleInfo::Constant& constant : module->constants) {
      if (!matches_prefix(constant.name, prefix)) {
        continue;
      }
      Item item{constant.name, constant.type + " = " + constant.display, Kind::Constant};
      item.documentation = code_block("const " + constant.name + " = " + constant.display + ";") + "\n\nFrom `" +
                           module->path + "`";
      add_unique(items, std::move(item));
    }
    for (const std::string& type_name : module->types) {
      if (!matches_prefix(type_name, prefix)) {
        continue;
      }
      if (const parser::StructDecl* decl = find_struct(program, type_name)) {
        Item item{type_name, "struct", Kind::Struct};
        item.documentation = code_block(struct_summary(program, *decl));
        add_unique(items, std::move(item));
      } else if (find_enum(program, type_name) != nullptr) {
        add_unique(items, Item{type_name, "enum", Kind::Enum});
      } else {
        add_unique(items, Item{type_name, "type", Kind::Struct});
      }
    }
  }
}

struct Snippet {
  const char* label;
  const char* detail;
  const char* body;
};

// Statement templates offered at the start of a statement (TypeScript/VS Code style snippets).
constexpr Snippet kSnippets[] = {
    {"func", "func name(params) { … }", "func ${1:name}(${2}) {\n\t$0\n}"},
    {"struct", "struct Name { … }", "struct ${1:Name} {\n\t${2:int} ${3:field};\n}"},
    {"enum", "enum Name { … }", "enum ${1:Name} { ${2:A}, ${3:B} }"},
    {"if", "if (cond) { … }", "if (${1:cond}) {\n\t$0\n}"},
    {"else", "else { … }", "else {\n\t$0\n}"},
    {"while", "while (cond) { … }", "while (${1:cond}) {\n\t$0\n}"},
    {"for", "for (let mut i = 0; i < n; i += 1) { … }",
     "for (let mut ${1:i} = 0; ${1:i} < ${2:n}; ${1:i} += 1) {\n\t$0\n}"},
    {"forin", "for (let x in items) { … }", "for (let ${1:item} in ${2:items}) {\n\t$0\n}"},
    {"match", "match (value) { … }", "match (${1:value}) {\n\t${2:0} ~> $0;\n\t_ ~> post(0);\n}"},
    {"switch", "switch (value) { case … }", "switch (${1:value}) {\n\tcase ${2:1}:\n\t\t$0\n\t\tbreak;\n\tdefault:\n\t\tbreak;\n}"},
    {"try", "try { … } catch (e) { … }", "try {\n\t$1\n} catch (${2:e}) {\n\t$0\n}"},
    {"link", "link @clpp.module as Alias;", "link @clpp.${1:axiom} as ${2:Axiom};"},
    {"linkfile", "link \"./file.clp\" as Alias;", "link \"./${1:file}.clp\" as ${2:Alias};"},
    {"post", "post(value);", "post($1);$0"},
    {"let", "let name = value;", "let ${1:name} = ${2:value};"},
    {"letmut", "let mut name = value;", "let mut ${1:name} = ${2:value};"},
};

}  // namespace

Info build_info_for(const AnalysisResult& analysis, const std::string_view source, const std::uint32_t line,
                const std::uint32_t column) {
  const parser::Program& program = analysis.program;
  const std::vector<Token>& tokens = analysis.tokens;
  Info info;
  info.diagnostics = analysis.diagnostics;
  // One problem, one squiggle: drop a second diagnostic at the same position (for example
  // "type mismatch" right after "undefined name").
  {
    std::vector<Diagnostic> unique;
    for (const Diagnostic& diagnostic : info.diagnostics) {
      bool repeated = false;
      for (const Diagnostic& kept : unique) {
        repeated = repeated || (kept.location.line == diagnostic.location.line &&
                                kept.location.column == diagnostic.location.column);
      }
      if (!repeated) {
        unique.push_back(diagnostic);
      }
    }
    info.diagnostics = std::move(unique);
  }

  std::vector<Symbol> symbols;
  std::vector<std::string> self_fields;
  for (std::size_t index = 0; index < program.functions.size(); ++index) {
    const parser::Function& function = program.functions[index];
    std::uint32_t end_line = 0xFFFFFFFF;
    std::uint32_t end_column = 0xFFFFFFFF;
    if (index + 1 < program.functions.size()) {
      end_line = program.functions[index + 1].name_location.line;
      end_column = program.functions[index + 1].name_location.column;
    }
    const bool module_function = origin_of_function(analysis, function.name) != nullptr;
    if (function.name.find('.') == std::string::npos) {
      symbols.push_back(Symbol{function.name, signature_text(function), Kind::Function, 1, 1,
                               function.name_location.line, function.name_location.column, 0xFFFFFFFF, 0xFFFFFFFF});
    } else if (module_function || find_struct(program, function.name.substr(0, function.name.rfind('.'))) == nullptr) {
      const std::string alias = function.name.substr(0, function.name.find('.'));
      bool known = false;
      for (const Symbol& symbol : symbols) {
        known = known || (symbol.kind == Kind::Module && symbol.name == alias);
      }
      if (!known) {
        const ModuleInfo* module = find_module(analysis, alias);
        symbols.push_back(Symbol{alias, module != nullptr ? "link " + module->path : "namespace", Kind::Module, 1, 1,
                                 module != nullptr ? module->location.line : function.name_location.line,
                                 module != nullptr ? module->location.column : function.name_location.column,
                                 0xFFFFFFFF, 0xFFFFFFFF});
      }
    }
    if (function.imported) {
      continue;  // body and parameters live in the module file (its own coordinates, not this file's)
    }
    walk_stmts(function.body, end_line, end_column, symbols, self_fields);
    for (std::size_t param = 0; param < function.params.size(); ++param) {
      const std::string detail = param < function.param_types.size() && !function.param_types[param].empty()
                                     ? function.param_types[param]
                                     : "value";
      symbols.push_back(Symbol{function.params[param], detail, Kind::Variable, function.param_locations[param].line,
                               function.param_locations[param].column, function.param_locations[param].line,
                               function.param_locations[param].column, end_line, end_column});
    }
  }
  for (const ModuleInfo& module : analysis.modules) {
    bool known = false;
    for (const Symbol& symbol : symbols) {
      known = known || (symbol.kind == Kind::Module && symbol.name == module.alias);
    }
    if (!known) {
      symbols.push_back(Symbol{module.alias, "link " + module.path, Kind::Module, 1, 1, module.location.line,
                               module.location.column, 0xFFFFFFFF, 0xFFFFFFFF});
    }
  }
  for (const parser::StructDecl& decl : program.structs) {
    symbols.push_back(Symbol{decl.name, "struct", Kind::Struct, 1, 1, decl.name_location.line, decl.name_location.column,
                             0xFFFFFFFF, 0xFFFFFFFF});
  }
  for (const parser::EnumDecl& decl : program.enums) {
    symbols.push_back(Symbol{decl.name, "enum", Kind::Enum, 1, 1, decl.name_location.line, decl.name_location.column,
                             0xFFFFFFFF, 0xFFFFFFFF});
  }
  for (const parser::TypeAlias& alias : program.aliases) {
    symbols.push_back(Symbol{alias.name, "type", Kind::Struct, 1, 1, 1, 1, 0xFFFFFFFF, 0xFFFFFFFF});
  }
  walk_stmts(program.statements, 0xFFFFFFFF, 0xFFFFFFFF, symbols, self_fields);

  const Token* current = token_at(tokens, line, column);
  std::string prefix;
  bool members = false;
  bool self = false;
  bool link_module = false;
  std::string owner;
  const auto previous_of = [&](const Token* token) { return token == nullptr ? nullptr : token_before(tokens, *token); };
  const Token* anchor = current;  // the token right before what is being typed
  // A reserved word being typed (`cons|`, `const|`) is a prefix too, like an identifier.
  const bool typing_word = current != nullptr && !current->lexeme.empty() &&
                    (std::isalpha(static_cast<unsigned char>(current->lexeme.front())) != 0 ||
                     current->lexeme.front() == '_') &&
                    keyword_doc(current->lexeme) != nullptr;
  if (current != nullptr && (current->type == TokenType::Identifier || typing_word)) {
    const std::uint32_t offset = column - current->location.column;
    prefix = std::string(current->lexeme.substr(0, std::min<std::size_t>(offset, current->lexeme.size())));
    anchor = previous_of(current);
  }
  if (anchor != nullptr && anchor->type == TokenType::Dot) {
    const Token* qualifier = previous_of(anchor);
    // `link @clpp.` / `link @clpp.li`
    const Token* at = previous_of(qualifier);
    const Token* keyword = previous_of(at);
    if (qualifier != nullptr && qualifier->lexeme == "clpp" && at != nullptr && at->type == TokenType::At &&
        keyword != nullptr && (keyword->type == TokenType::KwLink || keyword->type == TokenType::KwImport)) {
      link_module = true;
    } else if (qualifier != nullptr && qualifier->type == TokenType::Identifier) {
      members = true;
      owner = std::string(qualifier->lexeme);
      if (owner == "this" && at != nullptr && at->type == TokenType::At) {
        self = true;
        members = false;
      }
    } else if (qualifier != nullptr && qualifier->type == TokenType::CloseParen) {
      // `make().` — find the callee name before the matching '('
      int depth = 0;
      const Token* walk = qualifier;
      while (walk != nullptr) {
        if (walk->type == TokenType::CloseParen) {
          ++depth;
        } else if (walk->type == TokenType::OpenParen && --depth == 0) {
          break;
        }
        walk = previous_of(walk);
      }
      const Token* callee = previous_of(walk);
      if (callee != nullptr && callee->type == TokenType::Identifier) {
        const parser::Function* function = find_function(program, std::string(callee->lexeme));
        const Token* dot = previous_of(callee);
        const Token* base = previous_of(dot);
        if (function == nullptr && dot != nullptr && dot->type == TokenType::Dot && base != nullptr) {
          function = find_function(program, std::string(base->lexeme) + "." + std::string(callee->lexeme));
          if (function == nullptr) {
            const std::string base_type = type_of_name(std::string(base->lexeme), symbols, line, column);
            for (const parser::StructDecl* decl : lineage(program, base_type)) {
              function = function != nullptr ? function
                                             : find_function(program, decl->name + "." + std::string(callee->lexeme));
            }
          }
        }
        if (function != nullptr && !function->return_type.empty()) {
          members = true;
          owner = function->return_type;
        } else if (find_struct(program, std::string(callee->lexeme)) != nullptr) {
          members = true;
          owner = std::string(callee->lexeme);
        }
      }
    }
  } else if (anchor != nullptr && anchor->type == TokenType::At) {
    const Token* keyword = previous_of(anchor);
    if (keyword != nullptr && (keyword->type == TokenType::KwLink || keyword->type == TokenType::KwImport)) {
      link_module = true;  // `link @` → offer `clpp`
    } else {
      self = true;
    }
  } else if (anchor != nullptr && anchor->type == TokenType::Scope) {
    const Token* qualifier = previous_of(anchor);
    const Token* at = previous_of(qualifier);
    self = qualifier != nullptr && qualifier->lexeme == "this" && at != nullptr && at->type == TokenType::At;
  }
  // `link "./fi|` — the host (LSP) lists files; the compiler only says where.
  if (current != nullptr && (current->type == TokenType::StringLiteral || current->type == TokenType::Invalid)) {
    const Token* keyword = previous_of(current);
    if (keyword != nullptr && (keyword->type == TokenType::KwLink || keyword->type == TokenType::KwImport)) {
      info.link_path_completion = true;
      std::string_view text = current->lexeme;
      if (!text.empty() && (text.front() == '"' || text.front() == '\'')) {
        text.remove_prefix(1);
      }
      const std::uint32_t typed = column > current->location.column + 1 ? column - current->location.column - 1 : 0;
      info.link_path_prefix = std::string(text.substr(0, std::min<std::size_t>(typed, text.size())));
    }
  }

  if (link_module) {
    const bool after_at = anchor != nullptr && anchor->type == TokenType::At;
    if (after_at) {
      add_unique(info.completions, Item{"clpp", "standard library", Kind::Module});
    } else {
      for (const stdlib::ModuleEntry& entry : stdlib::module_catalog()) {
        const std::string name(entry.path.substr(6));  // after "@clpp."
        if (!matches_prefix(name, prefix)) {
          continue;
        }
        Item item{name, std::string(entry.description), Kind::Module};
        item.insert_text = name + " as " + std::string(entry.alias) + ";";
        item.documentation = code_block("link " + std::string(entry.path) + " as " + std::string(entry.alias) + ";") +
                             "\n\n" + std::string(entry.description);
        add_unique(info.completions, std::move(item));
      }
    }
  } else if (self) {
    const std::string viewer = enclosing_struct(program, tokens, line, column);
    if (matches_prefix("this", prefix)) {
      add_unique(info.completions, Item{"this", viewer.empty() ? "self" : viewer, Kind::Keyword});
    }
    if (!viewer.empty()) {
      add_struct_members(viewer, prefix, program, viewer, info.completions);
    }
    for (const std::string& field : self_fields) {
      if (matches_prefix(field, prefix)) {
        add_unique(info.completions, Item{field, "self", Kind::Field});
      }
    }
  } else if (members) {
    complete_members(owner, prefix, analysis, symbols, tokens, line, column, info.completions);
  } else if (!info.link_path_completion) {
    for (const Symbol& symbol : symbols) {
      if (!visible(symbol, line, column) || !matches_prefix(symbol.name, prefix)) {
        continue;
      }
      Item item{symbol.name, symbol.detail, symbol.kind};
      if (symbol.kind == Kind::Function) {
        if (const parser::Function* function = find_function(program, symbol.name)) {
          item.documentation = code_block(signature_text(*function));
          item.insert_text = symbol.name + (params_text(*function).empty() ? "()" : "($1)$0");
          item.snippet = !params_text(*function).empty();
        }
      } else if (symbol.kind == Kind::Struct) {
        if (const parser::StructDecl* decl = find_struct(program, symbol.name)) {
          item.documentation = code_block(struct_summary(program, *decl));
        }
      }
      add_unique(info.completions, std::move(item));
    }
    // Built-in functions that are not keywords (resolved by the binder by name).
    struct Builtin {
      std::string_view name;
      std::string_view signature;
      std::string_view doc;
    };
    static constexpr Builtin kBuiltins[] = {
        {"len", "len(collection) -> int", "Number of elements of a list, array or dictionary, or characters of a string."},
        {"list", "list(values...) -> list", "Builds a list from the values."},
        {"sort", "sort(list) -> list", "A new list with the numbers in ascending order."},
        {"find", "find(list, value) -> int", "Position of the value in the list, or `-1`."},
        {"push", "push(list, value)", "Appends the value to the end of the list variable."},
        {"pop", "pop(list)", "Removes the last element of the list variable and returns it."},
        {"insert", "insert(list, index, value)", "Inserts the value at the position, shifting the rest."},
        {"remove", "remove(list, index) / remove(dictionary, key)", "Removes and returns the element (or the key's value)."},
        {"args", "args() -> list", "Command-line arguments after the file name."},
        {"mutex", "mutex()", "Creates a lock for `lock()`/`unlock()`."},
        {"lock", "lock(mutex)", "Enters the region protected by the lock."},
        {"unlock", "unlock(mutex)", "Leaves the region protected by the lock."},
        {"fetch_add", "fetch_add(atomic, n)", "Adds `n` atomically and returns the previous value."},
        {"atomic_load", "atomic_load(atomic)", "Reads the current value of an `atomic`."},
        {"actor", "actor(function, value)", "Runs the function isolated (no files, network or global state) and returns its result."},
    };
    for (const Builtin& builtin : kBuiltins) {
      if (matches_prefix(builtin.name, prefix)) {
        Item item{std::string(builtin.name), std::string(builtin.signature), Kind::Function};
        item.documentation = code_block(std::string(builtin.signature)) + "\n\n" + std::string(builtin.doc);
        item.insert_text = std::string(builtin.name) + (builtin.name == "args" || builtin.name == "mutex" ? "()" : "($1)$0");
        item.snippet = item.insert_text.find('$') != std::string::npos;
        add_unique(info.completions, std::move(item));
      }
    }
    // Reserved words, each with what it really is: `int` a type, `post` a function, `true` a
    // constant, `match` a keyword. The editor shows a different icon and the description.
    for (const std::string_view keyword : keyword_names()) {
      if (!matches_prefix(keyword, prefix)) {
        continue;
      }
      const KeywordDoc* doc = keyword_doc(keyword);
      Item item{std::string(keyword), doc != nullptr ? std::string(doc->detail) : "reserved word",
                doc != nullptr ? doc->kind : Kind::Keyword};
      if (doc != nullptr) {
        item.documentation = std::string(doc->doc);
      }
      if (item.kind == Kind::Function) {
        item.insert_text = std::string(keyword) + "($1)$0";
        item.snippet = true;
      }
      add_unique(info.completions, std::move(item));
    }
    // Snippets only where a statement can start (after `;`, `{`, `}` or at the top of the file).
    const Token* start = anchor;
    const bool statement_start = start == nullptr || start->type == TokenType::Semicolon ||
                                 start->type == TokenType::OpenBrace || start->type == TokenType::CloseBrace;
    if (statement_start) {
      for (const Snippet& snippet : kSnippets) {
        if (!matches_prefix(snippet.label, prefix)) {
          continue;
        }
        Item item{snippet.label, snippet.detail, Kind::Snippet};
        item.sort_text = "6";
        item.insert_text = snippet.body;
        item.snippet = true;
        item.documentation = code_block(snippet.detail);
        info.completions.push_back(std::move(item));  // beside the plain keyword on purpose
      }
    }
  }

  // Hover and go to definition on the identifier under the cursor.
  const Token* word = token_at(tokens, line, column);
  if (word != nullptr && word->type != TokenType::Identifier) {
    // cursor right after a word, or on the first character: look at the identifier that contains it
    for (const Token& token : tokens) {
      if (token.type == TokenType::Identifier && token.location.line == line && column >= token.location.column &&
          column <= token.location.column + token.lexeme.size()) {
        word = &token;
      }
    }
  }
  if (word != nullptr && word->type == TokenType::Identifier) {
    const std::string name(word->lexeme);
    const Token* dot = previous_of(word);
    const Token* base = dot != nullptr && dot->type == TokenType::Dot ? previous_of(dot) : nullptr;
    if (base != nullptr && base->type == TokenType::Identifier) {
      // Member: module function/type, struct field/method, enum variant.
      const std::string base_name(base->lexeme);
      const std::string viewer = enclosing_struct(program, tokens, line, column);
      const std::string base_type =
          base_name == "self" && !viewer.empty() ? viewer : type_of_name(base_name, symbols, line, column);
      if (const parser::Function* function = find_function(program, base_name + "." + name);
          function != nullptr && find_struct(program, base_name) == nullptr) {
        info.has_hover = true;
        info.hover.text = base_name + "." + signature_text(*function).substr(5);
        info.hover_markdown = code_block(signature_text(*function));
        const ModuleInfo* module = origin_of_function(analysis, function->name);
        if (module != nullptr) {
          info.hover_markdown += "\n\nFrom `" + module->path + "` (via `" + module->alias + "`)";
          info.definition_path = module->path;
        }
        info.has_definition = true;
        info.definition = function->name_location;
        info.definition_length = static_cast<std::uint32_t>(short_name(function->name).size());
      } else if (const ModuleInfo* constant_module = find_module(analysis, base_name);
                 constant_module != nullptr &&
                 std::any_of(constant_module->constants.begin(), constant_module->constants.end(),
                             [&](const ModuleInfo::Constant& constant) { return constant.name == name; })) {
        for (const ModuleInfo::Constant& constant : constant_module->constants) {
          if (constant.name != name) {
            continue;
          }
          info.has_hover = true;
          info.hover.text = base_name + "." + name + ": " + constant.type + " = " + constant.display;
          info.hover_markdown = code_block("const " + name + ": " + constant.type + " = " + constant.display) +
                                "\n\nFrom `" + constant_module->path + "`";
          info.has_definition = constant.location.line != 0;
          info.definition = constant.location;
          info.definition_length = static_cast<std::uint32_t>(name.size());
          info.definition_path = constant_module->path;
        }
      } else if (find_module(analysis, base_name) != nullptr && find_struct(program, name) != nullptr) {
        const parser::StructDecl* decl = find_struct(program, name);
        info.has_hover = true;
        info.hover.text = "struct " + name;
        info.hover_markdown = code_block(struct_summary(program, *decl));
        info.has_definition = true;
        info.definition = decl->name_location;
        info.definition_length = static_cast<std::uint32_t>(name.size());
        info.definition_path = find_module(analysis, base_name)->path;
      } else {
        for (const parser::StructDecl* decl : lineage(program, base_type)) {
          if (info.has_hover) {
            break;
          }
          for (const parser::Field& field : decl->fields) {
            if (field.name != name) {
              continue;
            }
            const std::string owner = field.owner.empty() ? decl->name : field.owner;
            const std::string type = field.type_name.empty() ? "auto" : field.type_name;
            info.has_hover = true;
            info.hover.text = name + ": " + type;
            info.hover_markdown = code_block(type + " " + owner + "." + name);
            if (owner != bare_type(base_type)) {
              info.hover_markdown += "\n\nInherited from `" + owner + "`";
            }
            const parser::StructDecl* declaring = find_struct(program, owner);
            const parser::Field* original = &field;
            if (declaring != nullptr) {
              for (const parser::Field& candidate : declaring->fields) {
                if (candidate.name == name) {
                  original = &candidate;
                }
              }
            }
            info.has_definition = original->name_location.line != 0;
            info.definition = original->name_location;
            info.definition_length = static_cast<std::uint32_t>(name.size());
            if (const ModuleInfo* module = origin_of_type(analysis, owner)) {
              info.definition_path = module->path;
            }
            break;
          }
          if (info.has_hover) {
            break;
          }
          if (const parser::Function* method = find_function(program, decl->name + "." + name)) {
            info.has_hover = true;
            info.hover.text = decl->name + "." + signature_text(*method).substr(5);
            info.hover_markdown = code_block(decl->name + "." + signature_text(*method).substr(5));
            if (decl->name != bare_type(base_type)) {
              info.hover_markdown += "\n\nInherited from `" + decl->name + "`";
            }
            info.has_definition = true;
            info.definition = method->name_location;
            info.definition_length = static_cast<std::uint32_t>(name.size());
            if (const ModuleInfo* module = origin_of_type(analysis, decl->name)) {
              info.definition_path = module->path;
            }
          }
        }
        if (!info.has_hover) {
          if (const parser::EnumDecl* decl = find_enum(program, base_name)) {
            for (std::size_t index = 0; index < decl->variants.size(); ++index) {
              if (decl->variants[index] == name) {
                info.has_hover = true;
                info.hover.text = base_name + "." + name + " = " + std::to_string(index);
                info.hover_markdown = code_block("enum " + base_name + " { " + name + " }") + "\n\nValue: `" +
                                      std::to_string(index) + "`";
                if (index < decl->variant_locations.size()) {
                  info.has_definition = decl->variant_locations[index].line != 0;
                  info.definition = decl->variant_locations[index];
                  info.definition_length = static_cast<std::uint32_t>(name.size());
                }
                if (const ModuleInfo* module = origin_of_type(analysis, base_name)) {
                  info.definition_path = module->path;
                }
              }
            }
          }
        }
      }
    } else {
      const Symbol* best = nullptr;
      for (const Symbol& symbol : symbols) {
        if (symbol.name != name || !visible(symbol, line, column)) {
          continue;
        }
        if (best == nullptr || symbol.line > best->line || (symbol.line == best->line && symbol.column > best->column)) {
          best = &symbol;
        }
      }
      if (best != nullptr) {
        info.has_hover = true;
        info.hover.text = best->name + ": " + best->detail;
        if (!best->value.empty()) {
          info.hover.text += " = " + best->value;
        }
        switch (best->kind) {
          case Kind::Function:
            info.hover_markdown = code_block(best->detail);
            break;
          case Kind::Struct:
            if (const parser::StructDecl* decl = find_struct(program, best->name)) {
              info.hover_markdown = code_block(struct_summary(program, *decl));
              if (const ModuleInfo* module = origin_of_type(analysis, best->name)) {
                info.hover_markdown += "\n\nFrom `" + module->path + "`";
                info.definition_path = module->path;
              }
            } else {
              info.hover_markdown = code_block("type " + best->name);
            }
            break;
          case Kind::Enum:
            if (const parser::EnumDecl* decl = find_enum(program, best->name)) {
              std::string text = "enum " + best->name + " { ";
              for (std::size_t index = 0; index < decl->variants.size(); ++index) {
                text += (index == 0 ? "" : ", ") + decl->variants[index];
              }
              info.hover_markdown = code_block(text + " }");
              if (const ModuleInfo* module = origin_of_type(analysis, best->name)) {
                info.definition_path = module->path;
              }
            }
            break;
          case Kind::Module: {
            const ModuleInfo* module = find_module(analysis, best->name);
            std::string text = module != nullptr ? "link " + (module->path.front() == '@' ? module->path
                                                                                           : "\"" + module->path + "\"") +
                                                       " as " + module->alias + ";"
                                                 : "namespace " + best->name;
            if (module != nullptr) {
              for (const std::string& function : module->functions) {
                if (const parser::Function* found = find_function(program, module->alias + "." + function)) {
                  text += "\n  " + signature_text(*found);
                }
              }
              for (const std::string& type_name : module->types) {
                text += "\n  type " + type_name;
              }
            }
            info.hover_markdown = code_block(text);
            break;
          }
          default: {
            const std::string text = "let " + best->name + (best->detail == "value" ? "" : ": " + best->detail);
            info.hover_markdown = code_block(text);
            if (!best->value.empty()) {
              info.hover_markdown += "\n\nValue: `" + best->value + "`";
            }
            break;
          }
        }
        info.hover.location = SourceLocation{best->def_line, best->def_column};
        info.has_definition = true;
        info.definition = info.hover.location;
        info.definition_length = static_cast<std::uint32_t>(best->name.size());
        if (best->kind == Kind::Module) {
          info.definition_length = 4;  // `link`
        }
      }
    }
  }
  // Hover on a reserved word: what it is and an example.
  if (!info.has_hover && word != nullptr && word->type != TokenType::Identifier && word->type != TokenType::Eof) {
    if (const KeywordDoc* doc = keyword_doc(word->lexeme)) {
      info.has_hover = true;
      info.hover.text = std::string(doc->name) + ": " + std::string(doc->detail);
      info.hover_markdown =
          "**" + std::string(doc->name) + "** — " + std::string(doc->detail) + "\n\n" + std::string(doc->doc);
    }
  }
  info.symbols.reserve(symbols.size());
  for (const Symbol& symbol : symbols) {
    if (symbol.kind == Kind::Module || origin_of_type(analysis, symbol.name) != nullptr) {
      continue;  // outline shows what this file declares
    }
    info.symbols.push_back(SymbolInfo{symbol.name, symbol.detail, symbol.kind, SourceLocation{symbol.def_line, symbol.def_column}});
  }
  std::stable_sort(info.completions.begin(), info.completions.end(), [](const Item& left, const Item& right) {
    if (left.sort_text != right.sort_text) {
      return left.sort_text < right.sort_text;
    }
    return left.label < right.label;
  });

  const auto at_or_before = [&](const Token& token) {
    return token.location.line < line || (token.location.line == line && token.location.column <= column);
  };
  const Token* open = nullptr;
  int depth = 0;
  for (auto token = tokens.rbegin(); token != tokens.rend(); ++token) {
    if (token->type == TokenType::Eof || !at_or_before(*token)) {
      continue;
    }
    if (token->type == TokenType::CloseParen) {
      ++depth;
    } else if (token->type == TokenType::OpenParen) {
      if (depth == 0) {
        open = &*token;
        break;
      }
      --depth;
    }
  }
  if (open != nullptr) {
    const Token* name = token_before(tokens, *open);
    std::string callable = name == nullptr ? std::string{} : std::string(name->lexeme);
    const parser::Function* function = nullptr;
    if (name != nullptr) {
      const Token* dot = token_before(tokens, *name);
      const Token* owner_token = dot == nullptr ? nullptr : token_before(tokens, *dot);
      if (dot != nullptr && dot->type == TokenType::Dot && owner_token != nullptr &&
          owner_token->type == TokenType::Identifier) {
        const std::string owner_name(owner_token->lexeme);
        function = find_function(program, owner_name + "." + callable);
        if (function == nullptr) {
          for (const parser::StructDecl* decl : lineage(program, type_of_name(owner_name, symbols, line, column))) {
            function = function != nullptr ? function : find_function(program, decl->name + "." + callable);
          }
        }
      } else {
        function = find_function(program, callable);
      }
    }
    if (function != nullptr) {
      info.signature.label = short_name(function->name) + "(";
      int commas = 0;
      int nested = 0;
      bool inside = false;
      for (const Token& token : tokens) {
        if (&token == open) {
          inside = true;
          continue;
        }
        if (!inside || !at_or_before(token)) {
          if (inside && !at_or_before(token)) {
            break;
          }
          continue;
        }
        if (token.type == TokenType::OpenParen) {
          ++nested;
        } else if (token.type == TokenType::CloseParen) {
          if (nested == 0) {
            break;
          }
          --nested;
        } else if (token.type == TokenType::Comma && nested == 0) {
          ++commas;
        }
      }
      for (std::size_t index = 0; index < function->params.size(); ++index) {
        if (function->name.find('.') != std::string::npos && index == 0 && function->params[index] == "self") {
          continue;
        }
        const std::string type = index < function->param_types.size() ? function->param_types[index] : std::string{};
        const std::string piece = type.empty() ? function->params[index] : type + " " + function->params[index];
        if (!info.signature.parameters.empty()) {
          info.signature.label += ", ";
        }
        info.signature.label += piece;
        info.signature.parameters.push_back(piece);
      }
      info.signature.label.push_back(')');
      if (!function->return_type.empty() && function->return_type != "void") {
        info.signature.label += " -> " + function->return_type;
      }
      if (!info.signature.parameters.empty()) {
        info.signature.active = static_cast<std::uint32_t>(
            std::min(commas, static_cast<int>(info.signature.parameters.size()) - 1));
      }
      info.has_signature = true;
    }
  }
  if (suppress_completion(source, line, column) && !info.link_path_completion) {
    info.completions.clear();
    info.has_signature = false;
    info.signature = {};
  }
  return info;
}

struct Session::State {
  std::string source;
  AnalysisResult analysis;
  bool initialized{false};
};

Session::Session() : state(std::make_unique<State>()) {}

Session::~Session() = default;

Session::Session(Session&&) noexcept = default;

Session& Session::operator=(Session&&) noexcept = default;

Info inspect(const std::string_view source, const std::uint32_t line, const std::uint32_t column,
             const ModuleLoader& modules) {
  AnalysisResult analysis = analyze_program(source, modules);
  return build_info_for(analysis, analysis.source_buffers.front(), line, column);
}

namespace {

// ---------------------------------------------------------------------------------------------
// Semantic tokens.
//
// Two passes over the analyzed file:
//   1. Declarations of *this* file (structs, fields, enums, functions, parameters, variables,
//      `link` aliases) become tokens with the `declaration` modifier and are remembered as
//      bindings. Declarations merged from a `link`ed module are skipped: their locations are
//      coordinates in the module's text, and painting them here colored random slices of this
//      file (the "am" of `amount` shown as a parameter). Every declaration must also sit exactly
//      on an identifier token of this file with the same text, so a stale or synthetic location
//      (the implicit `self`, anonymous functions) can never produce a partial token.
//   2. Every remaining identifier is classified by context: member after `.`, `@field`, module
//      alias, struct/enum name, binding in scope, built-in function.
// ---------------------------------------------------------------------------------------------

struct Binding {
  std::uint32_t line{1};
  std::uint32_t column{1};
  std::uint32_t end_line{0xFFFFFFFF};
  std::uint32_t end_column{0xFFFFFFFF};
  std::uint32_t token_type{kSemanticVariable};
  std::uint32_t modifiers{0};
  bool local{false};
  bool hoisted{false};  // functions: callable before the line that declares them
};

[[nodiscard]] constexpr std::uint64_t position_key(const std::uint32_t line, const std::uint32_t column) {
  return (static_cast<std::uint64_t>(line) << 32U) | column;
}

[[nodiscard]] bool earlier(const std::uint32_t line, const std::uint32_t column, const std::uint32_t other_line,
                           const std::uint32_t other_column) {
  return line < other_line || (line == other_line && column < other_column);
}

class Highlighter {
 public:
  explicit Highlighter(const std::vector<Token>& tokens) : m_tokens(tokens) {
    for (std::size_t index = 0; index < tokens.size(); ++index) {
      if (tokens[index].type != TokenType::Eof) {
        m_index.emplace(position_key(tokens[index].location.line, tokens[index].location.column), index);
      }
    }
  }

  // A declaration of this file. Returns false when `location` is not an identifier token that
  // spells `name` (imported or synthetic declarations), so nothing is painted.
  bool declare(const std::string& name, const SourceLocation location, const std::uint32_t token_type,
               const std::uint32_t modifiers, const bool local, const bool hoisted, const SourceLocation end) {
    const Token* token = identifier_at(location, name);
    if (token == nullptr || !mark(location)) {
      return false;
    }
    m_result.push_back(SemanticToken{location.line, location.column, static_cast<std::uint32_t>(token->lexeme.size()),
                                     token_type, modifiers | kSemanticDeclaration});
    m_bindings[name].push_back(
        Binding{location.line, location.column, end.line, end.column, token_type, modifiers, local, hoisted});
    return true;
  }

  // A token that is not a declaration (a use, a member, a type keyword).
  void paint(const Token& token, const std::uint32_t token_type, const std::uint32_t modifiers) {
    if (mark(token.location)) {
      m_result.push_back(SemanticToken{token.location.line, token.location.column,
                                       static_cast<std::uint32_t>(token.lexeme.size()), token_type, modifiers});
    }
  }

  [[nodiscard]] bool taken(const SourceLocation location) const {
    return m_taken.count(position_key(location.line, location.column)) != 0;
  }

  // The innermost binding of `name` visible at `location`: a local of the enclosing function
  // first (the latest one declared before the use, so shadowing works), then a global.
  [[nodiscard]] const Binding* lookup(const std::string& name, const SourceLocation location) const {
    const auto found = m_bindings.find(name);
    if (found == m_bindings.end()) {
      return nullptr;
    }
    const Binding* best = nullptr;
    int best_rank = -1;
    for (const Binding& binding : found->second) {
      const bool started = binding.hoisted || earlier(binding.line, binding.column, location.line, location.column);
      if (!started) {
        continue;
      }
      if (binding.local && !earlier(location.line, location.column, binding.end_line, binding.end_column + 1)) {
        continue;
      }
      const int rank = binding.local ? 2 : binding.hoisted ? 0 : 1;
      if (rank > best_rank ||
          (rank == best_rank && best != nullptr && earlier(best->line, best->column, binding.line, binding.column))) {
        best = &binding;
        best_rank = rank;
      }
    }
    return best;
  }

  [[nodiscard]] std::vector<SemanticToken> take() {
    std::sort(m_result.begin(), m_result.end(), [](const SemanticToken& left, const SemanticToken& right) {
      return left.line != right.line ? left.line < right.line : left.column < right.column;
    });
    return std::move(m_result);
  }

 private:
  [[nodiscard]] const Token* identifier_at(const SourceLocation location, const std::string& name) const {
    if (location.line == 0 || name.empty()) {
      return nullptr;
    }
    const auto found = m_index.find(position_key(location.line, location.column));
    if (found == m_index.end()) {
      return nullptr;
    }
    const Token& token = m_tokens[found->second];
    return token.type == TokenType::Identifier && token.lexeme == name ? &token : nullptr;
  }

  bool mark(const SourceLocation location) { return m_taken.insert(position_key(location.line, location.column)).second; }

  const std::vector<Token>& m_tokens;
  std::unordered_map<std::uint64_t, std::size_t> m_index;
  std::unordered_set<std::uint64_t> m_taken;
  std::unordered_map<std::string, std::vector<Binding>> m_bindings;
  std::vector<SemanticToken> m_result;
};

[[nodiscard]] SourceLocation function_close(const std::vector<Token>& tokens, const SourceLocation name) {
  bool seen = false;
  int depth = 0;
  for (const Token& token : tokens) {
    if (!seen) {
      if (token.location.line == name.line && token.location.column == name.column) {
        seen = true;
      }
      continue;
    }
    if (token.type == TokenType::OpenBrace) {
      ++depth;
    } else if (token.type == TokenType::CloseBrace) {
      --depth;
      if (depth == 0) {
        return token.location;
      }
    }
  }
  return SourceLocation{0xFFFFFFFF, 0xFFFFFFFF};
}

// First identifier token spelling `name` at or after `from`, on the same line (match arm bindings
// only know where their pattern starts: `Some(price)` binds `price`, not `Some`).
[[nodiscard]] SourceLocation name_after(const std::vector<Token>& tokens, const SourceLocation from,
                                        const std::string& name) {
  for (const Token& token : tokens) {
    if (token.type == TokenType::Identifier && token.lexeme == name && token.location.line == from.line &&
        token.location.column >= from.column) {
      return token.location;
    }
  }
  return SourceLocation{0, 0};
}

void declare_statements(const std::vector<parser::Stmt>& statements, const std::vector<Token>& tokens,
                        Highlighter& highlighter, const bool local, const SourceLocation end) {
  for (const parser::Stmt& stmt : statements) {
    if (stmt.kind == parser::Stmt::Kind::Let || stmt.kind == parser::Stmt::Kind::ForIn || stmt.if_let ||
        stmt.kind == parser::Stmt::Kind::Signal) {
      const std::uint32_t modifiers = stmt.kind == parser::Stmt::Kind::Let && stmt.immutable ? kSemanticReadonly : 0;
      (void)highlighter.declare(stmt.name, stmt.name_location, kSemanticVariable, modifiers, local, false, end);
    }
    declare_statements(stmt.then_body, tokens, highlighter, local, end);
    declare_statements(stmt.else_body, tokens, highlighter, local, end);
    declare_statements(stmt.init, tokens, highlighter, local, end);
    declare_statements(stmt.step, tokens, highlighter, local, end);
    for (const parser::Stmt::MatchArm& arm : stmt.arms) {
      if (!arm.bind_name.empty()) {
        (void)highlighter.declare(arm.bind_name, name_after(tokens, arm.pattern.location, arm.bind_name),
                                  kSemanticVariable, 0, local, false, end);
      }
      declare_statements(arm.body, tokens, highlighter, local, end);
    }
  }
}

[[nodiscard]] bool is_type_token(const TokenType type) {
  switch (type) {
    case TokenType::KwVoid:
    case TokenType::KwInt:
    case TokenType::KwFloat:
    case TokenType::KwDouble:
    case TokenType::KwString:
    case TokenType::KwBool:
    case TokenType::KwAuto:
    case TokenType::KwBuffer:
    case TokenType::KwVector2:
    case TokenType::KwVector3:
    case TokenType::KwVector4:
    case TokenType::KwTask:
      return true;
    default:
      return false;
  }
}

// Reserved words that are called like functions: `post(x)`, `pcall(e)`, `join(t)`.
[[nodiscard]] bool is_builtin_call_token(const TokenType type) {
  switch (type) {
    case TokenType::KwPost:
    case TokenType::KwWarn:
    case TokenType::KwReport:
    case TokenType::KwCout:
    case TokenType::KwPcall:
    case TokenType::KwJoin:
    case TokenType::KwMove:
      return true;
    default:
      return false;
  }
}

// Built-in functions that are ordinary names (resolved by the binder).
[[nodiscard]] bool is_builtin_function(const std::string_view name) {
  static constexpr std::string_view kNames[] = {"len",   "list",   "sort",      "find",        "push",
                                                 "pop",   "insert", "remove",    "args",        "mutex",
                                                 "lock",  "unlock", "fetch_add", "atomic_load", "actor",
                                                 "array", "dictionary", "Some", "Ok", "Err"};
  return std::find(std::begin(kNames), std::end(kNames), name) != std::end(kNames);
}

// `clpp` and `axiom` in `link @clpp.axiom as Axiom;`
[[nodiscard]] bool in_link_path(const std::vector<Token>& tokens, const std::size_t index) {
  std::size_t cursor = index;
  while (cursor > 0) {
    const TokenType type = tokens[cursor - 1].type;
    if (type == TokenType::KwLink || type == TokenType::KwImport || type == TokenType::KwFrom) {
      return true;
    }
    if (type != TokenType::Dot && type != TokenType::At && type != TokenType::Identifier) {
      return false;
    }
    --cursor;
  }
  return false;
}

[[nodiscard]] bool is_synthetic_function(const std::string& name) {
  return name.rfind("anon", 0) == 0 || name.find("operator") != std::string::npos;
}

}  // namespace

const std::vector<std::string_view>& semantic_token_types() {
  static const std::vector<std::string_view> types = {"type",     "struct",   "enum",     "enumMember",
                                                      "parameter", "variable", "property", "function",
                                                      "keyword",  "namespace", "method",  "typeParameter"};
  return types;
}

const std::vector<std::string_view>& semantic_token_modifiers() {
  static const std::vector<std::string_view> modifiers = {"declaration", "readonly", "async", "defaultLibrary",
                                                          "static"};
  return modifiers;
}

std::vector<SemanticToken> semantic_tokens(const std::string_view source, const ModuleLoader& modules) {
  const AnalysisResult analysis = analyze_program(source, modules);
  const parser::Program& program = analysis.program;
  const std::vector<Token>& tokens = analysis.tokens;
  Highlighter highlighter(tokens);
  constexpr SourceLocation kForever{0xFFFFFFFF, 0xFFFFFFFF};

  // --- 1. declarations of this file -------------------------------------------------------
  std::unordered_set<std::string> type_params;
  for (const parser::LinkDecl& link : analysis.links) {
    (void)highlighter.declare(link.alias, link.alias_location, kSemanticNamespace, 0, false, true, kForever);
  }
  for (const parser::StructDecl& decl : program.structs) {
    if (decl.imported) {
      continue;
    }
    (void)highlighter.declare(decl.name, decl.name_location, kSemanticStruct, 0, false, true, kForever);
    for (const parser::Field& field : decl.fields) {
      if (field.owner.empty() || field.owner == decl.name) {
        (void)highlighter.declare(field.name, field.name_location, kSemanticProperty, 0, false, true, kForever);
      }
    }
    type_params.insert(decl.type_params.begin(), decl.type_params.end());
  }
  for (const parser::EnumDecl& decl : program.enums) {
    if (decl.imported) {
      continue;
    }
    (void)highlighter.declare(decl.name, decl.name_location, kSemanticEnum, 0, false, true, kForever);
    for (std::size_t index = 0; index < decl.variants.size() && index < decl.variant_locations.size(); ++index) {
      (void)highlighter.declare(decl.variants[index], decl.variant_locations[index], kSemanticEnumMember, 0, false,
                                true, kForever);
    }
  }
  declare_statements(program.statements, tokens, highlighter, false, kForever);
  for (const parser::Function& function : program.functions) {
    if (function.imported) {
      continue;
    }
    const SourceLocation end = function_close(tokens, function.name_location);
    const std::size_t dot = function.name.rfind('.');
    const bool method = dot != std::string::npos && !function.params.empty() && function.params.front() == "self";
    if (!is_synthetic_function(function.name)) {
      const std::string bare = dot == std::string::npos ? function.name : function.name.substr(dot + 1);
      // Methods are reached through `obj.` / `@`, so only plain functions become a name binding.
      const std::uint32_t modifiers = function.is_async ? kSemanticAsync : 0;
      if (method || dot != std::string::npos) {
        (void)highlighter.declare(bare, function.name_location, method ? kSemanticMethod : kSemanticFunction,
                                  modifiers | (method ? 0 : kSemanticStatic), false, false, kForever);
      } else {
        (void)highlighter.declare(bare, function.name_location, kSemanticFunction, modifiers, false, true, kForever);
      }
    }
    for (std::size_t index = method ? 1 : 0; index < function.params.size() && index < function.param_locations.size();
         ++index) {
      (void)highlighter.declare(function.params[index], function.param_locations[index], kSemanticParameter, 0, true,
                                false, end);
    }
    type_params.insert(function.type_params.begin(), function.type_params.end());
    declare_statements(function.body, tokens, highlighter, true, end);
  }

  std::unordered_map<std::string, const ModuleInfo*> modules_by_alias;
  for (const ModuleInfo& module : analysis.modules) {
    modules_by_alias.emplace(module.alias, &module);
  }
  std::unordered_map<std::string, std::uint32_t> type_names;  // struct, enum, variant and alias names
  for (const parser::StructDecl& decl : program.structs) {
    type_names.emplace(decl.name, kSemanticStruct);
  }
  for (const parser::EnumDecl& decl : program.enums) {
    type_names.emplace(decl.name, kSemanticEnum);
  }
  for (const parser::VariantDecl& decl : program.variants) {
    type_names.emplace(decl.name, kSemanticType);
  }
  for (const parser::TypeAlias& alias : program.aliases) {
    type_names.emplace(alias.name, kSemanticType);
  }
  for (const std::string_view generic : {"array", "dictionary", "Option", "Result"}) {
    type_names.emplace(std::string(generic), kSemanticType);
  }
  const auto library_type = [&](const std::string& name) {
    for (const ModuleInfo& module : analysis.modules) {
      if (!module.path.empty() && module.path.front() == '@' &&
          std::find(module.types.begin(), module.types.end(), name) != module.types.end()) {
        return true;
      }
    }
    return false;
  };
  const auto enum_has = [&](const std::string& owner, const std::string& variant) {
    for (const parser::EnumDecl& decl : program.enums) {
      if (decl.name == owner) {
        return std::find(decl.variants.begin(), decl.variants.end(), variant) != decl.variants.end();
      }
    }
    return false;
  };

  // --- 2. every other token -----------------------------------------------------------------
  for (std::size_t index = 0; index < tokens.size(); ++index) {
    const Token& token = tokens[index];
    if (token.type == TokenType::Eof || token.lexeme.empty() || highlighter.taken(token.location)) {
      continue;
    }
    const Token* next = index + 1 < tokens.size() ? &tokens[index + 1] : nullptr;
    const bool call = next != nullptr && next->type == TokenType::OpenParen;
    if (is_type_token(token.type)) {
      highlighter.paint(token, kSemanticType, kSemanticDefaultLibrary);
      continue;
    }
    if (is_builtin_call_token(token.type) && call) {
      highlighter.paint(token, kSemanticFunction, kSemanticDefaultLibrary);
      continue;
    }
    if (token.type != TokenType::Identifier) {
      continue;
    }
    const std::string name(token.lexeme);
    const Token* previous = index > 0 ? &tokens[index - 1] : nullptr;

    // `link @clpp.axiom`: the pieces of a module path
    if (in_link_path(tokens, index)) {
      highlighter.paint(token, kSemanticNamespace, kSemanticDefaultLibrary);
      continue;
    }
    // `owner.name`
    if (previous != nullptr && previous->type == TokenType::Dot) {
      const Token* owner = index > 1 ? &tokens[index - 2] : nullptr;
      const std::string owner_name = owner != nullptr && owner->type == TokenType::Identifier ? std::string(owner->lexeme)
                                                                                               : std::string{};
      if (const auto module = modules_by_alias.find(owner_name); module != modules_by_alias.end()) {
        const ModuleInfo& info = *module->second;
        const std::uint32_t library = !info.path.empty() && info.path.front() == '@' ? kSemanticDefaultLibrary : 0;
        if (std::find(info.functions.begin(), info.functions.end(), name) != info.functions.end()) {
          highlighter.paint(token, kSemanticFunction, library);
        } else if (const auto type = type_names.find(name); type != type_names.end()) {
          highlighter.paint(token, type->second, library);
        } else if (std::any_of(info.constants.begin(), info.constants.end(),
                               [&](const ModuleInfo::Constant& constant) { return constant.name == name; })) {
          highlighter.paint(token, kSemanticVariable, library | kSemanticReadonly | kSemanticStatic);
        } else {
          highlighter.paint(token, call ? kSemanticFunction : kSemanticProperty, library);
        }
        continue;
      }
      if (!owner_name.empty() && enum_has(owner_name, name)) {
        highlighter.paint(token, kSemanticEnumMember, 0);
        continue;
      }
      if (owner_name == "coroutine" || owner_name == "task") {
        highlighter.paint(token, kSemanticFunction, kSemanticDefaultLibrary);
        continue;
      }
      highlighter.paint(token, call ? kSemanticMethod : kSemanticProperty, 0);
      continue;
    }
    // `@field`, `@method()`, `@this`
    if (previous != nullptr && previous->type == TokenType::At) {
      if (name != "this") {
        highlighter.paint(token, call ? kSemanticMethod : kSemanticProperty, 0);
      }
      continue;
    }
    // `buffer::create(...)`, `@this::field`
    if (previous != nullptr && previous->type == TokenType::Scope) {
      highlighter.paint(token, call ? kSemanticFunction : kSemanticProperty, call ? kSemanticDefaultLibrary : 0);
      continue;
    }
    if (name == "self") {
      highlighter.paint(token, kSemanticParameter, kSemanticReadonly);
      continue;
    }
    if (const Binding* binding = highlighter.lookup(name, token.location)) {
      highlighter.paint(token, binding->token_type, binding->modifiers);
      continue;
    }
    if (modules_by_alias.count(name) != 0) {
      highlighter.paint(token, kSemanticNamespace, 0);
      continue;
    }
    if (const auto type = type_names.find(name); type != type_names.end()) {
      highlighter.paint(token, type->second, library_type(name) ? kSemanticDefaultLibrary : 0);
      continue;
    }
    if (type_params.count(name) != 0) {
      highlighter.paint(token, kSemanticTypeParameter, 0);
      continue;
    }
    if (is_builtin_function(name) && (call || (next != nullptr && next->type == TokenType::Less))) {
      highlighter.paint(token, kSemanticFunction, kSemanticDefaultLibrary);
      continue;
    }
    if (name == "coroutine") {
      highlighter.paint(token, kSemanticNamespace, kSemanticDefaultLibrary);
    }
  }
  return highlighter.take();
}

namespace {

void append_encoded(std::vector<std::uint32_t>& data, std::uint32_t& previous_line, std::uint32_t& previous_column,
                    const std::uint32_t line, const std::uint32_t column, const std::uint32_t length,
                    const SemanticToken& token) {
  const std::uint32_t delta_line = line - previous_line;
  const std::uint32_t delta_column = delta_line == 0 ? column - previous_column : column;
  data.push_back(delta_line);
  data.push_back(delta_column);
  data.push_back(length);
  data.push_back(token.token_type);
  data.push_back(token.modifiers);
  previous_line = line;
  previous_column = column;
}

[[nodiscard]] std::vector<SemanticToken> ordered_tokens(const std::vector<SemanticToken>& tokens) {
  std::vector<SemanticToken> ordered = tokens;
  std::sort(ordered.begin(), ordered.end(), [](const SemanticToken& left, const SemanticToken& right) {
    return left.line != right.line ? left.line < right.line : left.column < right.column;
  });
  return ordered;
}

}  // namespace

std::vector<std::uint32_t> encode_semantic_tokens(const std::vector<SemanticToken>& tokens) {
  std::vector<std::uint32_t> data;
  std::uint32_t previous_line = 0;
  std::uint32_t previous_column = 0;
  for (const SemanticToken& token : ordered_tokens(tokens)) {
    if (token.line == 0 || token.length == 0) {
      continue;
    }
    append_encoded(data, previous_line, previous_column, token.line - 1, token.column == 0 ? 0 : token.column - 1,
                   token.length, token);
  }
  return data;
}

std::vector<std::uint32_t> encode_semantic_tokens(const std::vector<SemanticToken>& tokens, const std::string_view source) {
  std::vector<std::size_t> line_starts{0};
  for (std::size_t index = 0; index < source.size(); ++index) {
    if (source[index] == '\n') {
      line_starts.push_back(index + 1);
    }
  }
  // Byte column (1-based) on a line -> UTF-16 units from the start of that line.
  const auto utf16 = [&](const std::uint32_t line, const std::uint32_t column) -> std::uint32_t {
    if (line == 0 || line > line_starts.size() || column <= 1) {
      return 0;
    }
    const std::size_t start = line_starts[line - 1];
    return static_cast<std::uint32_t>(utf16_units(source.substr(start), static_cast<std::size_t>(column - 1)));
  };
  std::vector<std::uint32_t> data;
  std::uint32_t previous_line = 0;
  std::uint32_t previous_column = 0;
  for (const SemanticToken& token : ordered_tokens(tokens)) {
    if (token.line == 0 || token.length == 0) {
      continue;
    }
    const std::uint32_t start = utf16(token.line, token.column);
    const std::uint32_t end = utf16(token.line, token.column + token.length);
    if (end <= start) {
      continue;
    }
    append_encoded(data, previous_line, previous_column, token.line - 1, start, end - start, token);
  }
  return data;
}

Info Session::open(const std::string_view source, const std::uint32_t line, const std::uint32_t column,
                   const ModuleLoader& modules) {
  if (!state->initialized || state->source != source) {
    state->source.assign(source.begin(), source.end());
    state->analysis = analyze_program(state->source, modules);
    state->initialized = true;
  }
  return build_info_for(state->analysis, state->analysis.source_buffers.front(), line, column);
}

namespace {

[[nodiscard]] std::size_t source_offset(const std::string_view source, const std::uint32_t line, const std::uint32_t column) {
  std::size_t index = 0;
  std::uint32_t current_line = 1;
  std::uint32_t current_column = 1;
  while (index < source.size() && (current_line < line || (current_line == line && current_column < column))) {
    if (source[index] == '\n') {
      ++current_line;
      current_column = 1;
    } else {
      ++current_column;
    }
    ++index;
  }
  return index;
}

[[nodiscard]] bool space_before(const TokenType previous, const TokenType current) {
  if (current == TokenType::CloseParen || current == TokenType::CloseBracket || current == TokenType::Semicolon ||
      current == TokenType::Comma || current == TokenType::Dot || current == TokenType::Scope ||
      current == TokenType::OpenBracket) {
    return false;
  }
  if (current == TokenType::OpenParen) {
    return previous != TokenType::Identifier && previous != TokenType::KwPost && previous != TokenType::KwCout &&
           previous != TokenType::KwWarn && previous != TokenType::KwReport && previous != TokenType::KwPcall &&
           previous != TokenType::KwSpawn && previous != TokenType::KwNew && previous != TokenType::KwParallel &&
           previous != TokenType::KwAwait;
  }
  return previous != TokenType::OpenParen && previous != TokenType::OpenBracket && previous != TokenType::Dot &&
         previous != TokenType::Scope && previous != TokenType::At;
}

void append_comments(std::string& formatted, const std::string_view trivia, int& indent, bool& line_start) {
  for (std::size_t index = 0; index < trivia.size(); ++index) {
    if (trivia[index] != '<' || index + 1 >= trivia.size() || trivia[index + 1] != '<') {
      continue;
    }
    std::size_t end = trivia.size();
    if (index + 2 < trivia.size() && trivia[index + 2] == '[') {
      const std::size_t close = trivia.find("]>>", index);
      end = close == std::string_view::npos ? trivia.size() : close + 3;
    } else {
      const std::size_t newline = trivia.find('\n', index);
      end = newline == std::string_view::npos ? trivia.size() : newline;
    }
    if (!formatted.empty() && formatted.back() != '\n') {
      formatted.push_back('\n');
    }
    formatted.append(static_cast<std::size_t>(indent) * 2, ' ');
    formatted.append(trivia.substr(index, end - index));
    formatted.push_back('\n');
    line_start = true;
    if (end == 0) {
      break;
    }
    index = end - 1;
  }
}

}  // namespace

std::string format_source(const std::string_view source) {
  const LexResult lexed = Lexer(source).tokenize();
  std::string formatted;
  int indent = 0;
  bool line_start = true;
  std::size_t cursor = 0;
  const auto write_indent = [&]() {
    formatted.append(static_cast<std::size_t>(indent) * 2, ' ');
    line_start = false;
  };
  for (std::size_t index = 0; index < lexed.tokens.size(); ++index) {
    const Token& token = lexed.tokens[index];
    if (token.type == TokenType::Eof) {
      break;
    }
    const std::size_t start = source_offset(source, token.location.line, token.location.column);
    if (start >= cursor && start <= source.size()) {
      append_comments(formatted, source.substr(cursor, start - cursor), indent, line_start);
      cursor = start + token.lexeme.size();
    }
    const TokenType next = index + 1 < lexed.tokens.size() ? lexed.tokens[index + 1].type : TokenType::Eof;
    if (token.type == TokenType::CloseBrace) {
      if (!formatted.empty() && formatted.back() != '\n') {
        formatted.push_back('\n');
      }
      indent = std::max(0, indent - 1);
      write_indent();
      formatted.append(token.lexeme);
      if (next == TokenType::KwElse) {
        line_start = false;
      } else {
        formatted.push_back('\n');
        line_start = true;
      }
      continue;
    }
    if (line_start) {
      write_indent();
    } else if (index > 0) {
      const TokenType previous = lexed.tokens[index - 1].type;
      if (space_before(previous, token.type)) {
        formatted.push_back(' ');
      }
    }
    formatted.append(token.lexeme);
    if (token.type == TokenType::OpenBrace) {
      indent += 1;
      formatted.push_back('\n');
      line_start = true;
    } else if (token.type == TokenType::Semicolon) {
      formatted.push_back('\n');
      line_start = true;
    } else {
      line_start = false;
    }
  }
  if (cursor < source.size()) {
    append_comments(formatted, source.substr(cursor), indent, line_start);
  }
  if (!formatted.empty() && formatted.back() != '\n') {
    formatted.push_back('\n');
  }
  return formatted;
}

}  // namespace clpp::ide
