#include "clpp/semantic.hpp"

#include "core/parser/ast.hpp"

#include <string_view>

namespace clpp {

namespace {

void walk_expr(const parser::Expr& expr, const std::uint32_t scope, SemanticSnapshot& snapshot);
void walk_stmts(const std::vector<parser::Stmt>& statements, const std::uint32_t scope, SemanticSnapshot& snapshot);

void add_symbol(SemanticSnapshot& snapshot, const std::uint32_t scope, const std::uint16_t slot, const std::string& name,
                const SourceLocation location, const bool declaration) {
  if (name.empty() || location.line == 0) {
    return;
  }
  SemanticSymbol symbol;
  symbol.id = scope * 65536u + slot;
  symbol.scope = scope;
  symbol.slot = slot;
  symbol.name = name;
  symbol.location = location;
  symbol.declaration = declaration;
  snapshot.symbols.push_back(std::move(symbol));
}

void walk_expr(const parser::Expr& expr, const std::uint32_t scope, SemanticSnapshot& snapshot) {
  if (expr.kind == parser::Expr::Kind::Name || expr.kind == parser::Expr::Kind::MoveFrom) {
    add_symbol(snapshot, scope, expr.slot, expr.value, expr.location, false);
  }
  if (expr.left != nullptr) {
    walk_expr(*expr.left, scope, snapshot);
  }
  if (expr.right != nullptr) {
    walk_expr(*expr.right, scope, snapshot);
  }
  for (const parser::Expr& arg : expr.args) {
    walk_expr(arg, scope, snapshot);
  }
}

void walk_stmts(const std::vector<parser::Stmt>& statements, const std::uint32_t scope, SemanticSnapshot& snapshot) {
  for (const parser::Stmt& stmt : statements) {
    if (stmt.kind == parser::Stmt::Kind::Let || stmt.kind == parser::Stmt::Kind::For ||
        stmt.kind == parser::Stmt::Kind::ForIn) {
      add_symbol(snapshot, scope, stmt.slot, stmt.name, stmt.name_location, true);
    }
    if (stmt.kind == parser::Stmt::Kind::Assign && !stmt.name.empty()) {
      add_symbol(snapshot, scope, stmt.slot, stmt.name, stmt.name_location, false);  // `score = …`
    }
    walk_expr(stmt.expr, scope, snapshot);
    walk_expr(stmt.place, scope, snapshot);  // p.hp = …: `p` is a reference
    walk_expr(stmt.type_expr, scope, snapshot);
    walk_stmts(stmt.init, scope, snapshot);
    walk_stmts(stmt.then_body, scope, snapshot);
    walk_stmts(stmt.else_body, scope, snapshot);
    walk_stmts(stmt.step, scope, snapshot);
    for (const parser::Stmt::MatchArm& arm : stmt.arms) {
      if (!arm.bind_name.empty()) {
        add_symbol(snapshot, scope, arm.bind_slot, arm.bind_name, arm.pattern.location, true);
      }
      walk_expr(arm.pattern, scope, snapshot);
      walk_expr(arm.guard, scope, snapshot);
      walk_stmts(arm.body, scope, snapshot);
    }
  }
}

[[nodiscard]] bool covers(const SemanticSymbol& symbol, const std::uint32_t line, const std::uint32_t column) {
  if (symbol.location.line != line) {
    return false;
  }
  const std::uint32_t start = symbol.location.column;
  const std::uint32_t end = start + static_cast<std::uint32_t>(symbol.name.size());
  return column >= start && column <= end;
}

}  // namespace

SemanticSnapshot build_snapshot(const AnalysisResult& analysis) {
  SemanticSnapshot snapshot;
  walk_stmts(analysis.program.statements, 0, snapshot);
  for (const parser::Function& function : analysis.program.functions) {
    const std::uint32_t scope = static_cast<std::uint32_t>(function.index) + 1u;
    for (std::size_t param = 0; param < function.params.size() && param < function.param_locations.size(); ++param) {
      add_symbol(snapshot, scope, static_cast<std::uint16_t>(param), function.params[param],
                 function.param_locations[param], true);
    }
    walk_stmts(function.body, scope, snapshot);
  }
  return snapshot;
}

int utf16_units(const std::string_view text, const std::size_t byte_count) {
  int units = 0;
  std::size_t index = 0;
  while (index < byte_count && index < text.size()) {
    const auto lead = static_cast<unsigned char>(text[index]);
    std::size_t bytes = 1;
    int width = 1;
    if (lead >= 0xF0) {
      bytes = 4;
      width = 2;
    } else if (lead >= 0xE0) {
      bytes = 3;
    } else if (lead >= 0xC0) {
      bytes = 2;
    }
    if (index + bytes > text.size()) {
      break;
    }
    index += bytes;
    units += width;
  }
  return units;
}

int byte_column_from_utf16(const std::string_view text, const int lsp_line, const int lsp_character) {
  int line = 0;
  std::size_t index = 0;
  while (index < text.size() && line < lsp_line) {
    if (text[index++] == '\n') {
      ++line;
    }
  }
  int units = 0;
  int column = 0;
  while (index < text.size() && text[index] != '\n' && units < lsp_character) {
    const auto lead = static_cast<unsigned char>(text[index]);
    std::size_t bytes = 1;
    int width = 1;
    if (lead >= 0xF0) {
      bytes = 4;
      width = 2;
    } else if (lead >= 0xE0) {
      bytes = 3;
    } else if (lead >= 0xC0) {
      bytes = 2;
    }
    if (index + bytes > text.size()) {
      break;
    }
    index += bytes;
    column += static_cast<int>(bytes);
    units += width;
  }
  return column;
}

int lsp_character_at(const std::string_view text, const int line1, const int column1) {
  if (line1 <= 0) {
    return 0;
  }
  int line = 1;
  std::size_t index = 0;
  while (index < text.size() && line < line1) {
    if (text[index++] == '\n') {
      ++line;
    }
  }
  const std::size_t bytes = column1 <= 1 ? 0 : static_cast<std::size_t>(column1 - 1);
  return utf16_units(text.substr(index), bytes);
}

std::vector<SemanticSymbol> references_at(const SemanticSnapshot& snapshot, const std::uint32_t line,
                                          const std::uint32_t column) {
  const SemanticSymbol* hit = nullptr;
  for (const SemanticSymbol& symbol : snapshot.symbols) {
    if (covers(symbol, line, column)) {
      hit = &symbol;
      break;
    }
  }
  std::vector<SemanticSymbol> matches;
  if (hit == nullptr) {
    return matches;
  }
  for (const SemanticSymbol& symbol : snapshot.symbols) {
    if (symbol.id == hit->id) {
      matches.push_back(symbol);
    }
  }
  return matches;
}

}  // namespace clpp
