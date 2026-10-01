#include "clpp/clir.hpp"

#include "core/parser/ast.hpp"

#include <charconv>
#include <cmath>
#include <sstream>
#include <string>

namespace clpp {

namespace {

void emit_expr(const parser::Expr& expr, std::string& out, std::string& error);

void emit_expr(const parser::Expr& expr, std::string& out, std::string& error) {
  if (!error.empty()) {
    return;
  }
  switch (expr.kind) {
    case parser::Expr::Kind::Number: {
      if (expr.bool_literal) {
        out += expr.number != 0 ? "true" : "false";
        break;
      }
      if (expr.null_literal) {
        out += "nil";
        break;
      }
      // Whole numbers as integers; everything else with the shortest text that reads back
      // exactly (the old stream output kept 6 digits: 3.14159265 became 3.14159).
      if (std::isfinite(expr.number) && expr.number == std::trunc(expr.number) &&
          std::fabs(expr.number) < 9007199254740992.0) {
        out += std::to_string(static_cast<long long>(expr.number));
      } else {
        char buffer[64];
        const std::to_chars_result result = std::to_chars(buffer, buffer + sizeof(buffer), expr.number);
        out.append(buffer, result.ptr);
      }
      break;
    }
    case parser::Expr::Kind::String:
      out.push_back('"');
      for (const char c : expr.value) {
        switch (c) {
          case '"':
            out += "\\\"";
            break;
          case '\\':
            out += "\\\\";
            break;
          case '\n':
            out += "\\n";
            break;
          case '\r':
            out += "\\r";
            break;
          case '\t':
            out += "\\t";
            break;
          default:
            out.push_back(c);
            break;
        }
      }
      out.push_back('"');
      break;
    case parser::Expr::Kind::Name:
      out += expr.value;
      break;
    case parser::Expr::Kind::Add:
    case parser::Expr::Kind::Sub:
    case parser::Expr::Kind::Mul:
    case parser::Expr::Kind::Div:
    case parser::Expr::Kind::Mod: {
      if (expr.left == nullptr || expr.right == nullptr) {
        error = "luau backend: incomplete expression";
        return;
      }
      // Every operand is parenthesized, so the tree's grouping survives regardless of Luau's
      // precedence table: (1 + 2) * 3 stays (1 + 2) * 3.
      const auto operand = [&](const parser::Expr& side) {
        const bool compound = side.left != nullptr && side.right != nullptr;
        if (compound) {
          out.push_back('(');
        }
        emit_expr(side, out, error);
        if (compound) {
          out.push_back(')');
        }
      };
      if (expr.kind == parser::Expr::Kind::Mod) {
        out += "math.fmod(";  // C++ remainder keeps the dividend's sign; Luau `%` floors
        emit_expr(*expr.left, out, error);
        out += ", ";
        emit_expr(*expr.right, out, error);
        out.push_back(')');
        break;
      }
      if (expr.kind == parser::Expr::Kind::Div && expr.int_specialized) {
        out += "(math.modf(";  // int / int truncates toward zero
        operand(*expr.left);
        out += " / ";
        operand(*expr.right);
        out += "))";
        break;
      }
      const char* op = expr.kind == parser::Expr::Kind::Add   ? " + "
                       : expr.kind == parser::Expr::Kind::Sub ? " - "
                       : expr.kind == parser::Expr::Kind::Mul ? " * "
                                                              : " / ";
      operand(*expr.left);
      out += op;
      operand(*expr.right);
      break;
    }
    case parser::Expr::Kind::Call: {
      if (expr.left == nullptr || expr.left->kind != parser::Expr::Kind::Name) {
        error = "luau backend: unsupported call";
        return;
      }
      out += expr.left->value;
      out.push_back('(');
      for (std::size_t index = 0; index < expr.args.size(); ++index) {
        if (index != 0) {
          out += ", ";
        }
        emit_expr(expr.args[index], out, error);
      }
      out.push_back(')');
      break;
    }
    default:
      error = "luau backend: unsupported expression";
      break;
  }
}

void emit_stmts(const std::vector<parser::Stmt>& statements, std::string& out, std::string& error);

void emit_stmt(const parser::Stmt& stmt, std::string& out, std::string& error) {
  if (!error.empty()) {
    return;
  }
  switch (stmt.kind) {
    case parser::Stmt::Kind::Let:
      out += "local ";
      out += stmt.name;
      out += " = ";
      emit_expr(stmt.expr, out, error);
      out.push_back('\n');
      break;
    case parser::Stmt::Kind::Post:
      out += "print(";
      emit_expr(stmt.expr, out, error);
      out += ")\n";
      break;
    case parser::Stmt::Kind::Return:
      out += "return ";
      emit_expr(stmt.expr, out, error);
      out.push_back('\n');
      break;
    case parser::Stmt::Kind::If:
      out += "if ";
      emit_expr(stmt.expr, out, error);
      out += " then\n";
      emit_stmts(stmt.then_body, out, error);
      if (stmt.has_else) {
        out += "else\n";
        emit_stmts(stmt.else_body, out, error);
      }
      out += "end\n";
      break;
    case parser::Stmt::Kind::While:
      out += "while ";
      emit_expr(stmt.expr, out, error);
      out += " do\n";
      emit_stmts(stmt.then_body, out, error);
      out += "end\n";
      break;
    case parser::Stmt::Kind::Expr:
      emit_expr(stmt.expr, out, error);
      out.push_back('\n');
      break;
    default:
      error = "luau backend: unsupported statement";
      break;
  }
}

void emit_stmts(const std::vector<parser::Stmt>& statements, std::string& out, std::string& error) {
  for (const parser::Stmt& stmt : statements) {
    emit_stmt(stmt, out, error);
  }
}

}  // namespace

LuauEmit clir_to_luau(const AnalysisResult& analysis) {
  LuauEmit emitted;
  if (!analysis.ok()) {
    emitted.error = "luau backend: analysis failed";
    return emitted;
  }
  const ClirEmitResult clir = emit_clir(analysis);
  if (!clir.ok()) {
    emitted.error = "luau backend: emit failed";
    return emitted;
  }
  const std::string rejected = check_backend(Backend::Luau, clir.chunk);
  if (!rejected.empty()) {
    emitted.error = rejected;
    return emitted;
  }
  for (const parser::Function& function : analysis.program.functions) {
    if (function.is_extern) {
      continue;
    }
    emitted.source += "function ";
    emitted.source += function.name;
    emitted.source.push_back('(');
    for (std::size_t index = 0; index < function.params.size(); ++index) {
      if (index != 0) {
        emitted.source += ", ";
      }
      emitted.source += function.params[index];
    }
    emitted.source += ")\n";
    emit_stmts(function.body, emitted.source, emitted.error);
    emitted.source += "end\n";
  }
  emit_stmts(analysis.program.statements, emitted.source, emitted.error);
  if (!emitted.error.empty()) {
    emitted.source.clear();
  }
  return emitted;
}

}  // namespace clpp
