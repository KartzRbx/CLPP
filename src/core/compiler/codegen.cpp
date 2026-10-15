#include "clpp/clir.hpp"
#include "clpp/compiler.hpp"

#include <algorithm>
#include <optional>

#include "core/compiler/analyze.hpp"
#include "core/compiler/opcode.hpp"
#include "core/parser/parser.hpp"

namespace clpp {

namespace {

SourceLocation emit_origin{};
thread_local const std::vector<std::uint16_t>* emit_boxed = nullptr;
thread_local std::size_t operand_opcode = 0;

void emit_op(BytecodeChunk& chunk, const compiler::Opcode op) {
  if (chunk.code.size() <= 0xFFFF) {
    SourceMapEntry entry;
    entry.pc = static_cast<std::uint16_t>(chunk.code.size());
    entry.location = emit_origin;
    chunk.source_map.push_back(entry);
  }
  operand_opcode = chunk.code.size();
  chunk.code.push_back(static_cast<std::uint8_t>(op));
}

void emit_u16(BytecodeChunk& chunk, const std::uint16_t value) {
  if (emit_boxed && chunk.code.size() == operand_opcode + 1 &&
      std::find(emit_boxed->begin(), emit_boxed->end(), value) != emit_boxed->end()) {
    auto& opcode = chunk.code[operand_opcode];
    if (opcode == static_cast<std::uint8_t>(compiler::Opcode::LoadLocal)) opcode = static_cast<std::uint8_t>(compiler::Opcode::LoadCell);
    else if (opcode == static_cast<std::uint8_t>(compiler::Opcode::StoreLocal)) opcode = static_cast<std::uint8_t>(compiler::Opcode::StoreCell);
    else if (opcode == static_cast<std::uint8_t>(compiler::Opcode::ClearLocal)) opcode = static_cast<std::uint8_t>(compiler::Opcode::ClearCell);
  }
  chunk.code.push_back(static_cast<std::uint8_t>(value & 0xFF));
  chunk.code.push_back(static_cast<std::uint8_t>(value >> 8));
}

[[nodiscard]] bool emit_const(BytecodeChunk& chunk, Value value, std::vector<Diagnostic>& diagnostics) {
  if (chunk.constants.size() > 0xFFFF) {
    diagnostics.push_back(Diagnostic{{}, {}, "too many constants"});
    return false;
  }

  const auto index = static_cast<std::uint16_t>(chunk.constants.size());
  chunk.constants.push_back(std::move(value));
  emit_op(chunk, compiler::Opcode::Const);
  emit_u16(chunk, index);
  return true;
}

[[nodiscard]] bool emit_self_field(BytecodeChunk& chunk, const std::string& name, const compiler::Opcode op,
                                    std::vector<Diagnostic>& diagnostics) {
  if (chunk.constants.size() > 0xFFFF) {
    diagnostics.push_back(Diagnostic{{}, {}, "too many constants"});
    return false;
  }
  std::uint16_t index = 0;
  bool found = false;
  for (std::size_t constant = 0; constant < chunk.constants.size(); ++constant) {
    if (chunk.constants[constant].is_string() && chunk.constants[constant].text == name) {
      index = static_cast<std::uint16_t>(constant);
      found = true;
      break;
    }
  }
  if (!found) {
    index = static_cast<std::uint16_t>(chunk.constants.size());
    chunk.constants.push_back(Value::string_of(name));
  }
  emit_op(chunk, op);
  emit_u16(chunk, index);
  return true;
}

[[nodiscard]] compiler::Opcode binary_opcode(const parser::Expr::Kind kind) {
  switch (kind) {
    case parser::Expr::Kind::Concat:
      return compiler::Opcode::Concat;
    case parser::Expr::Kind::Add:
      return compiler::Opcode::Add;
    case parser::Expr::Kind::Sub:
      return compiler::Opcode::Sub;
    case parser::Expr::Kind::Mul:
      return compiler::Opcode::Mul;
    case parser::Expr::Kind::Div:
      return compiler::Opcode::Div;
    case parser::Expr::Kind::Mod:
      return compiler::Opcode::Mod;
    case parser::Expr::Kind::Eq:
      return compiler::Opcode::Eq;
    case parser::Expr::Kind::NotEq:
      return compiler::Opcode::NotEq;
    case parser::Expr::Kind::Less:
      return compiler::Opcode::Less;
    case parser::Expr::Kind::LessEq:
      return compiler::Opcode::LessEq;
    case parser::Expr::Kind::Greater:
      return compiler::Opcode::Greater;
    case parser::Expr::Kind::GreaterEq:
      return compiler::Opcode::GreaterEq;
    case parser::Expr::Kind::BitAnd:
      return compiler::Opcode::BitAnd;
    case parser::Expr::Kind::BitOr:
      return compiler::Opcode::BitOr;
    case parser::Expr::Kind::BitXor:
      return compiler::Opcode::BitXor;
    case parser::Expr::Kind::Shl:
      return compiler::Opcode::Shl;
    case parser::Expr::Kind::Shr:
      return compiler::Opcode::Shr;
    case parser::Expr::Kind::Range:
      return compiler::Opcode::Range;
    case parser::Expr::Kind::String:
    case parser::Expr::Kind::Number:
    case parser::Expr::Kind::Name:
    case parser::Expr::Kind::And:
    case parser::Expr::Kind::Or:
    case parser::Expr::Kind::Not:
    case parser::Expr::Kind::Call:
    case parser::Expr::Kind::Member:
    case parser::Expr::Kind::Native:
    case parser::Expr::Kind::Construct:
    case parser::Expr::Kind::SelfField:
    case parser::Expr::Kind::Await:
    case parser::Expr::Kind::Spawn:
    case parser::Expr::Kind::Parallel:
    case parser::Expr::Kind::Index:
    case parser::Expr::Kind::PCall:
    case parser::Expr::Kind::MoveFrom:
    case parser::Expr::Kind::Join:
    case parser::Expr::Kind::Atomic:
    case parser::Expr::Kind::MakeList:
    case parser::Expr::Kind::Sort:
    case parser::Expr::Kind::Len:
    case parser::Expr::Kind::ListMut:
    case parser::Expr::Kind::Find:
    case parser::Expr::Kind::BitNot:
    case parser::Expr::Kind::Ternary:
    case parser::Expr::Kind::Update:
    case parser::Expr::Kind::Cast:
    case parser::Expr::Kind::Ascribe:
      return compiler::Opcode::Nop;
  }
  return compiler::Opcode::Nop;
}

[[nodiscard]] bool emit_expr(const parser::Expr& expr, BytecodeChunk& chunk,
                             std::vector<Diagnostic>& diagnostics);

[[nodiscard]] bool code_fits(const BytecodeChunk& chunk, std::vector<Diagnostic>& diagnostics) {
  if (chunk.code.size() > 0xFFFF) {
    diagnostics.push_back(Diagnostic{{}, {}, "program too large"});
    return false;
  }
  return true;
}

[[nodiscard]] std::size_t emit_jump(BytecodeChunk& chunk, const compiler::Opcode op) {
  emit_op(chunk, op);
  const std::size_t patch_at = chunk.code.size();
  emit_u16(chunk, 0);
  return patch_at;
}

[[nodiscard]] bool patch_jump(BytecodeChunk& chunk, const std::size_t patch_at,
                              std::vector<Diagnostic>& diagnostics) {
  if (!code_fits(chunk, diagnostics)) {
    return false;
  }
  const auto target = static_cast<std::uint16_t>(chunk.code.size());
  chunk.code[patch_at] = static_cast<std::uint8_t>(target & 0xFF);
  chunk.code[patch_at + 1] = static_cast<std::uint8_t>(target >> 8);
  return true;
}

[[nodiscard]] bool emit_value(const parser::Expr& expr, BytecodeChunk& chunk,
                             std::vector<Diagnostic>& diagnostics) {
  if (expr.location.line != 0) {
    emit_origin = expr.location;
  }
  if (expr.kind == parser::Expr::Kind::Ascribe) {
    return expr.left && emit_expr(*expr.left, chunk, diagnostics);
  }
  if (expr.kind == parser::Expr::Kind::Cast) {
    if (!expr.left || !emit_expr(*expr.left, chunk, diagnostics)) return false;
    emit_op(chunk, expr.runtime_op == 0 ? compiler::Opcode::ConvertNumber : compiler::Opcode::ConvertFloat);
    return true;
  }
  if (expr.kind == parser::Expr::Kind::String) {
    return emit_const(chunk, Value::string_of(expr.value), diagnostics);
  }
  if (expr.kind == parser::Expr::Kind::Number) {
    return emit_const(chunk, expr.null_literal ? Value::null_of() : expr.bool_literal ? Value::boolean_of(expr.number != 0) : expr.float_literal ? Value::number_of(expr.number) :
                      Value::integer_of(expr.exact_integer ? expr.integer : static_cast<std::int64_t>(expr.number)), diagnostics);
  }
  if (expr.kind == parser::Expr::Kind::SelfField) {
    return emit_self_field(chunk, expr.value, compiler::Opcode::GetSelf, diagnostics);
  }
  if (expr.kind == parser::Expr::Kind::Observe) {
    emit_op(chunk, expr.global ? compiler::Opcode::RawGlobal : compiler::Opcode::RawLoad); emit_u16(chunk, expr.slot);
    if (expr.args.empty() || !emit_expr(expr.args[0], chunk, diagnostics)) return false;
    emit_op(chunk, compiler::Opcode::Observe); return true;
  }
  if (expr.kind == parser::Expr::Kind::Event) {
    for (const auto& arg : expr.args) if (!emit_expr(arg, chunk, diagnostics)) return false;
    emit_op(chunk, compiler::Opcode::Event);
    emit_u16(chunk, static_cast<std::uint16_t>((expr.args.size() << 8) | expr.runtime_op)); return true;
  }
  if (expr.kind == parser::Expr::Kind::Closure) {
    if (expr.virtual_call && expr.left) {
      if (!emit_expr(*expr.left, chunk, diagnostics)) return false;
      emit_op(chunk, compiler::Opcode::MakeBoundMethod); emit_u16(chunk, expr.slot); return true;
    }
    for (const auto slot : expr.targets) { emit_op(chunk, compiler::Opcode::RawLoad); emit_u16(chunk, slot); }
    emit_op(chunk, compiler::Opcode::MakeStruct); emit_u16(chunk, static_cast<std::uint16_t>(expr.targets.size()));
    emit_op(chunk, compiler::Opcode::MakeClosure); emit_u16(chunk, expr.slot); return true;
  }
  if (expr.kind == parser::Expr::Kind::Invoke) {
    if (!expr.left || !emit_expr(*expr.left, chunk, diagnostics)) return false;
    for (const auto& arg : expr.args) if (!emit_expr(arg, chunk, diagnostics)) return false;
    emit_op(chunk, expr.thread_call ? compiler::Opcode::InvokeThread : compiler::Opcode::Invoke);
    emit_u16(chunk, static_cast<std::uint16_t>(expr.args.size())); return true;
  }
  if (expr.kind == parser::Expr::Kind::Name || expr.kind == parser::Expr::Kind::MoveFrom) {
    emit_op(chunk, expr.global ? compiler::Opcode::LoadGlobal : compiler::Opcode::LoadLocal);
    emit_u16(chunk, expr.slot);
    if (expr.kind == parser::Expr::Kind::MoveFrom) {
      emit_op(chunk, compiler::Opcode::ClearLocal);
      emit_u16(chunk, expr.slot);
    }
    return true;
  }
  if (expr.kind == parser::Expr::Kind::Parallel) {
    if (expr.args.size() > 0xFFFF) {
      diagnostics.push_back(Diagnostic{expr.location, expr.span, "wrong number of arguments"});
      return false;
    }
    for (const parser::Expr& arg : expr.args) {
      if (!emit_expr(arg, chunk, diagnostics)) {
        return false;
      }
    }
    emit_op(chunk, compiler::Opcode::Parallel);
    emit_u16(chunk, static_cast<std::uint16_t>(expr.args.size()));
    return true;
  }
  if (expr.kind == parser::Expr::Kind::PCall) {
    if (!code_fits(chunk, diagnostics) || expr.left == nullptr) {
      diagnostics.push_back(Diagnostic{expr.location, expr.span, "invalid expression"});
      return false;
    }
    emit_op(chunk, compiler::Opcode::Protect);
    const std::size_t handler = chunk.code.size();
    emit_u16(chunk, 0);
    if (!emit_expr(*expr.left, chunk, diagnostics)) {
      return false;
    }
    emit_op(chunk, compiler::Opcode::Pop);
    emit_op(chunk, compiler::Opcode::EndTry);
    if (!emit_const(chunk, Value::integer_of(1), diagnostics)) {
      return false;
    }
    const std::size_t skip_handler = emit_jump(chunk, compiler::Opcode::Jump);
    if (!patch_jump(chunk, handler, diagnostics)) {
      return false;
    }
    if (!emit_const(chunk, Value::integer_of(0), diagnostics)) {
      return false;
    }
    return patch_jump(chunk, skip_handler, diagnostics);
  }
  if (expr.kind == parser::Expr::Kind::Ternary) {
    if (expr.left == nullptr || expr.right == nullptr || expr.args.empty() || !emit_expr(*expr.left, chunk, diagnostics)) {
      diagnostics.push_back(Diagnostic{{}, {}, "invalid expression"});
      return false;
    }
    const std::size_t else_jump = emit_jump(chunk, compiler::Opcode::JumpIfFalse);
    if (!emit_expr(*expr.right, chunk, diagnostics)) {
      return false;
    }
    const std::size_t end_jump = emit_jump(chunk, compiler::Opcode::Jump);
    if (!patch_jump(chunk, else_jump, diagnostics) || !emit_expr(expr.args[0], chunk, diagnostics) ||
        !patch_jump(chunk, end_jump, diagnostics)) {
      return false;
    }
    return true;
  }
  if (expr.kind == parser::Expr::Kind::Update) {
    if (expr.left == nullptr || !emit_expr(*expr.left, chunk, diagnostics)) {
      return false;
    }
    if (!expr.postfix) {
      if (!emit_const(chunk, Value::integer_of(static_cast<std::int64_t>(expr.number)), diagnostics)) {
        return false;
      }
      emit_op(chunk, compiler::Opcode::Add);
      emit_op(chunk, compiler::Opcode::Dup);
    } else {
      emit_op(chunk, compiler::Opcode::Dup);
      if (!emit_const(chunk, Value::integer_of(static_cast<std::int64_t>(expr.number)), diagnostics)) {
        return false;
      }
      emit_op(chunk, compiler::Opcode::Add);
    }
    emit_op(chunk, expr.global ? compiler::Opcode::StoreGlobal : compiler::Opcode::StoreLocal);
    emit_u16(chunk, expr.slot);
    return true;
  }
  if (expr.kind == parser::Expr::Kind::BitNot) {
    if (expr.left == nullptr || !emit_expr(*expr.left, chunk, diagnostics)) {
      return false;
    }
    emit_op(chunk, compiler::Opcode::BitNot);
    return true;
  }
  if (expr.kind == parser::Expr::Kind::Index && expr.right != nullptr && expr.right->kind == parser::Expr::Kind::Range) {
    if (expr.left == nullptr || expr.right->left == nullptr || expr.right->right == nullptr ||
        !emit_expr(*expr.left, chunk, diagnostics) || !emit_expr(*expr.right->left, chunk, diagnostics) ||
        !emit_expr(*expr.right->right, chunk, diagnostics)) {
      diagnostics.push_back(Diagnostic{{}, {}, "invalid expression"});
      return false;
    }
    emit_op(chunk, compiler::Opcode::Slice);
    return true;
  }
  if (expr.kind == parser::Expr::Kind::Index) {
    if (expr.left == nullptr || expr.right == nullptr || !emit_expr(*expr.left, chunk, diagnostics) ||
        !emit_expr(*expr.right, chunk, diagnostics)) {
      diagnostics.push_back(Diagnostic{expr.location, expr.span, "invalid expression"});
      return false;
    }
    emit_op(chunk, compiler::Opcode::GetIndex);
    return true;
  }
  if (expr.kind == parser::Expr::Kind::Call && expr.signal_call) {
    if (expr.targets.empty()) {
      for (const parser::Expr& arg : expr.args) {
        if (!emit_expr(arg, chunk, diagnostics)) {
          return false;
        }
      }
      for (std::size_t index = 0; index < expr.args.size(); ++index) {
        emit_op(chunk, compiler::Opcode::Pop);
      }
      return emit_const(chunk, Value::integer_of(0), diagnostics);
    }
    for (std::size_t target = 0; target < expr.targets.size(); ++target) {
      for (const parser::Expr& arg : expr.args) {
        if (!emit_expr(arg, chunk, diagnostics)) {
          return false;
        }
      }
      emit_op(chunk, compiler::Opcode::Call);
      emit_u16(chunk, expr.targets[target]);
      if (target + 1 != expr.targets.size()) {
        emit_op(chunk, compiler::Opcode::Pop);
      }
    }
    return true;
  }
  if (expr.kind == parser::Expr::Kind::Call && (expr.runtime_op == 1 || expr.runtime_op == 4)) {
    emit_op(chunk, expr.runtime_op == 1 ? compiler::Opcode::CoCreate : compiler::Opcode::Defer);
    emit_u16(chunk, expr.slot);
    return true;
  }
  if (expr.kind == parser::Expr::Kind::Call) {
    for (const parser::Expr& arg : expr.args) {
      if (!emit_expr(arg, chunk, diagnostics)) {
        return false;
      }
    }
    if (expr.virtual_call) {
      emit_op(chunk, compiler::Opcode::VCall);
      emit_u16(chunk, expr.slot);
      emit_u16(chunk, static_cast<std::uint16_t>(expr.args.size()));
      return true;
    }
    if (expr.runtime_op == 2) {
      emit_op(chunk, compiler::Opcode::CoResume);
      return true;
    }
    if (expr.runtime_op == 3) {
      emit_op(chunk, compiler::Opcode::CoYield);
      return true;
    }
    if (expr.runtime_op == 5) {
      emit_op(chunk, compiler::Opcode::Actor);
      emit_u16(chunk, expr.slot);
      return true;
    }
    if (expr.extern_call) {
      emit_op(chunk, compiler::Opcode::CCall);
      emit_u16(chunk, expr.slot);
      return true;
    }
    if (expr.async_call) {
      emit_op(chunk, compiler::Opcode::Schedule);
      emit_u16(chunk, expr.slot);
      return true;
    }
    emit_op(chunk, expr.thread_call ? compiler::Opcode::SpawnThread : compiler::Opcode::Call);
    emit_u16(chunk, expr.slot);
    return true;
  }
  if (expr.kind == parser::Expr::Kind::Join) {
    if (expr.left == nullptr || !emit_expr(*expr.left, chunk, diagnostics)) {
      return false;
    }
    emit_op(chunk, compiler::Opcode::Join);
    return true;
  }
  if (expr.kind == parser::Expr::Kind::Atomic) {
    for (const parser::Expr& arg : expr.args) {
      if (!emit_expr(arg, chunk, diagnostics)) {
        return false;
      }
    }
    compiler::Opcode op = compiler::Opcode::FetchAdd;
    if (expr.slot == 2) {
      op = compiler::Opcode::AtomicLoad;
    } else if (expr.slot == 3) {
      op = compiler::Opcode::MutexNew;
    } else if (expr.slot == 4) {
      op = compiler::Opcode::Lock;
    } else if (expr.slot == 5) {
      op = compiler::Opcode::Unlock;
    }
    emit_op(chunk, op);
    return true;
  }
  if (expr.kind == parser::Expr::Kind::MakeList || expr.kind == parser::Expr::Kind::Construct) {
    for (const parser::Expr& arg : expr.args) {
      if (!emit_expr(arg, chunk, diagnostics)) {
        return false;
      }
    }
    emit_op(chunk, compiler::Opcode::MakeStruct);
    emit_u16(chunk, static_cast<std::uint16_t>(expr.args.size()));
    if (expr.kind == parser::Expr::Kind::MakeList && expr.slot == 65535) {
      emit_op(chunk, compiler::Opcode::Tag);
      emit_u16(chunk, 65535);
    }
    if (expr.kind == parser::Expr::Kind::Construct) {
      emit_op(chunk, compiler::Opcode::Tag);
      emit_u16(chunk, static_cast<std::uint16_t>(expr.slot + 1));
    }
    return true;
  }
  if (expr.kind == parser::Expr::Kind::ListMut) {
    if (expr.slot == 65535) {
      for (const auto& argument : expr.args) if (!emit_expr(argument, chunk, diagnostics)) return false;
      const int operation = static_cast<int>(expr.number);
      emit_op(chunk, operation == 0 ? compiler::Opcode::PushItem : operation == 1 ? compiler::Opcode::PopItem :
                     operation == 2 ? compiler::Opcode::InsertItem : compiler::Opcode::RemoveItem);
      if (operation == 0 || operation == 2) return emit_const(chunk, Value::integer_of(0), diagnostics);
      return true;
    }
    // push(xs, v) / pop(xs) / insert(xs, i, v) / remove(xs, i): change the variable in place.
    const int op = static_cast<int>(expr.number);
    for (std::size_t index = 1; index < expr.args.size(); ++index) {
      if (!emit_expr(expr.args[index], chunk, diagnostics)) {
        return false;
      }
    }
    const compiler::Opcode code = op == 0   ? compiler::Opcode::ListPush
                                  : op == 1 ? compiler::Opcode::ListPop
                                  : op == 2 ? compiler::Opcode::ListInsert
                                            : compiler::Opcode::ListRemove;
    emit_op(chunk, code);
    emit_u16(chunk, expr.slot);
    if (op == 0 || op == 2) {
      return emit_const(chunk, Value::integer_of(0), diagnostics);  // push/insert produce no value
    }
    return true;
  }
  if (expr.kind == parser::Expr::Kind::Len) {
    if (expr.args.size() != 1 || !emit_expr(expr.args[0], chunk, diagnostics)) {
      return false;
    }
    emit_op(chunk, compiler::Opcode::ListLen);
    return true;
  }
  if (expr.kind == parser::Expr::Kind::Sort || expr.kind == parser::Expr::Kind::Find) {
    for (const parser::Expr& arg : expr.args) {
      if (!emit_expr(arg, chunk, diagnostics)) {
        return false;
      }
    }
    emit_op(chunk, expr.kind == parser::Expr::Kind::Sort ? compiler::Opcode::ListSort : compiler::Opcode::ListFind);
    return true;
  }
  if (expr.kind == parser::Expr::Kind::Native) {
    for (const parser::Expr& arg : expr.args) {
      if (!emit_expr(arg, chunk, diagnostics)) {
        return false;
      }
    }
    if (expr.slot >= 2000) {  // host library (stdlib::host_native)
      const auto id = static_cast<std::uint16_t>(expr.slot - 2000);
      const auto arity = static_cast<std::uint16_t>(expr.args.size());
      emit_op(chunk, compiler::Opcode::Host);
      emit_u16(chunk, static_cast<std::uint16_t>((arity << 8) | id));
      return true;
    }
    if (expr.slot >= 1000) {
      const auto id = static_cast<std::uint16_t>(expr.slot - 1000);
      const auto arity = static_cast<std::uint16_t>(expr.args.size());
      emit_op(chunk, compiler::Opcode::Axiom);
      emit_u16(chunk, static_cast<std::uint16_t>((arity << 8) | id));
      return true;
    }
    switch (expr.slot) {
      case 0:
        emit_op(chunk, compiler::Opcode::MakeVector);
        chunk.code.push_back(3);
        break;
      case 7:
        emit_op(chunk, compiler::Opcode::MakeVector);
        chunk.code.push_back(2);
        break;
      case 8:
        emit_op(chunk, compiler::Opcode::MakeVector);
        chunk.code.push_back(4);
        break;
      case 1:
        emit_op(chunk, compiler::Opcode::MakeBuffer);
        break;
      case 2:
        emit_op(chunk, compiler::Opcode::BufferWrite);
        break;
      case 3:
        emit_op(chunk, compiler::Opcode::BufferSize);
        break;
      case 4:
        emit_op(chunk, compiler::Opcode::FileSize);
        break;
      case 5:
        emit_op(chunk, compiler::Opcode::Env);
        break;
      case 6:
        emit_op(chunk, compiler::Opcode::HttpHost);
        break;
      case 9:
      case 10:
      case 11:
      case 12:
      case 13:
      case 14:
      case 15:
      case 16:
      case 17:
      case 18:
      case 19:
      case 20:
      case 21:
      case 22:
      case 23: {
        const auto local = static_cast<std::uint16_t>(expr.slot - 9);
        const auto count = static_cast<std::uint16_t>(expr.args.size());
        emit_op(chunk, compiler::Opcode::Std);
        emit_u16(chunk, static_cast<std::uint16_t>((count << 8) | local));
        break;
      }
      default:
        diagnostics.push_back(Diagnostic{{}, {}, "unknown native"});
        return false;
    }
    return true;
  }
  if (expr.kind == parser::Expr::Kind::Member) {
    if (expr.slot > 255) {
      diagnostics.push_back(Diagnostic{expr.location, expr.span, "unknown field"});
      return false;
    }
    if (expr.left == nullptr || !emit_expr(*expr.left, chunk, diagnostics)) {
      return false;
    }
    emit_op(chunk, compiler::Opcode::GetField);
    chunk.code.push_back(static_cast<std::uint8_t>(expr.slot));
    return true;
  }
  if (expr.kind == parser::Expr::Kind::Spawn || expr.kind == parser::Expr::Kind::Await) {
    if (expr.left == nullptr || !emit_expr(*expr.left, chunk, diagnostics)) {
      diagnostics.push_back(Diagnostic{{}, {}, "invalid expression"});
      return false;
    }
    emit_op(chunk, expr.kind == parser::Expr::Kind::Spawn ? compiler::Opcode::Spawn : compiler::Opcode::Await);
    return true;
  }
  if (expr.kind == parser::Expr::Kind::Not) {
    if (expr.left == nullptr || !emit_expr(*expr.left, chunk, diagnostics)) {
      diagnostics.push_back(Diagnostic{{}, {}, "invalid expression"});
      return false;
    }
    emit_op(chunk, compiler::Opcode::Not);
    return true;
  }
  if (expr.kind == parser::Expr::Kind::And || expr.kind == parser::Expr::Kind::Or) {
    if (expr.left == nullptr || expr.right == nullptr || !emit_expr(*expr.left, chunk, diagnostics)) {
      diagnostics.push_back(Diagnostic{{}, {}, "invalid expression"});
      return false;
    }
    emit_op(chunk, compiler::Opcode::Dup);
    if (expr.kind == parser::Expr::Kind::Or) {
      emit_op(chunk, compiler::Opcode::Not);
    }
    const std::size_t jump = emit_jump(chunk, compiler::Opcode::JumpIfFalse);
    emit_op(chunk, compiler::Opcode::Pop);
    if (!emit_expr(*expr.right, chunk, diagnostics) || !patch_jump(chunk, jump, diagnostics)) {
      return false;
    }
    emit_op(chunk, compiler::Opcode::Not);
    emit_op(chunk, compiler::Opcode::Not);
    return true;
  }
  if (expr.left == nullptr || expr.right == nullptr) {
    diagnostics.push_back(Diagnostic{{}, {}, "invalid expression"});
    return false;
  }
  // A bool joined into text reads true/false (its runtime value is 1/0).
  const auto bool_as_text = [&]() {
    const std::size_t to_false = emit_jump(chunk, compiler::Opcode::JumpIfFalse);
    if (!emit_const(chunk, Value::string_of("true"), diagnostics)) {
      return false;
    }
    const std::size_t to_end = emit_jump(chunk, compiler::Opcode::Jump);
    return patch_jump(chunk, to_false, diagnostics) && emit_const(chunk, Value::string_of("false"), diagnostics) &&
           patch_jump(chunk, to_end, diagnostics);
  };
  if (!emit_expr(*expr.left, chunk, diagnostics) || (expr.bool_left && !bool_as_text()) ||
      !emit_expr(*expr.right, chunk, diagnostics) || (expr.bool_right && !bool_as_text())) {
    return false;
  }
  if (expr.kind == parser::Expr::Kind::Add && expr.int_specialized) {
    emit_op(chunk, compiler::Opcode::IntAdd);
  } else if (expr.kind == parser::Expr::Kind::Div && expr.int_specialized) {
    emit_op(chunk, compiler::Opcode::IntDiv);
  } else {
    emit_op(chunk, binary_opcode(expr.kind));
  }
  return true;
}

thread_local std::size_t emit_try_depth = 0;
struct TryScope {
  TryScope() { ++emit_try_depth; }
  ~TryScope() { --emit_try_depth; }
};
struct JumpTarget {
  std::vector<std::size_t>* jumps;
  std::size_t try_depth;
  JumpTarget(std::vector<std::size_t>* value) : jumps(value), try_depth(emit_try_depth) {}
  std::vector<std::size_t>* operator->() const { return jumps; }
};
thread_local std::vector<JumpTarget> break_stack;
thread_local std::vector<JumpTarget> continue_stack;  // loops only (a switch is not a target)
void leave_tries(BytecodeChunk& chunk, const std::size_t destination_depth) {
  for (std::size_t depth = emit_try_depth; depth > destination_depth; --depth) {
    emit_op(chunk, compiler::Opcode::EndTry);
  }
}
[[nodiscard]] bool emit_expr(const parser::Expr& expr, BytecodeChunk& chunk,
                             std::vector<Diagnostic>& diagnostics) {
  if (!emit_value(expr, chunk, diagnostics)) return false;
  if (expr.numeric_promotion) emit_op(chunk, expr.numeric_promotion == 2 ? compiler::Opcode::ConvertOptionalFloat : compiler::Opcode::ConvertFloat);
  return true;
}

[[nodiscard]] bool emit_stmt(const parser::Stmt& stmt, BytecodeChunk& chunk,
                             std::vector<Diagnostic>& diagnostics) {
  if (stmt.name_location.line != 0) {
    emit_origin = stmt.name_location;
  } else if (stmt.expr.location.line != 0) {
    emit_origin = stmt.expr.location;
  }
  if (stmt.kind == parser::Stmt::Kind::Break && stmt.is_continue) {
    if (continue_stack.empty()) {
      diagnostics.push_back(Diagnostic{stmt.name_location, stmt.name_span, "continue outside loop"});
      return false;
    }
    leave_tries(chunk, continue_stack.back().try_depth);
    continue_stack.back()->push_back(emit_jump(chunk, compiler::Opcode::Jump));
    return true;
  }
  if (stmt.kind == parser::Stmt::Kind::Break) {
    if (break_stack.empty()) {
      diagnostics.push_back(Diagnostic{stmt.name_location, stmt.name_span, "break outside loop"});
      return false;
    }
    leave_tries(chunk, break_stack.back().try_depth);
    break_stack.back()->push_back(emit_jump(chunk, compiler::Opcode::Jump));
    return true;
  }
  if (stmt.kind == parser::Stmt::Kind::Expr) {
    if (!emit_expr(stmt.expr, chunk, diagnostics)) {
      return false;
    }
    emit_op(chunk, compiler::Opcode::Pop);
    return true;
  }

  if (stmt.kind == parser::Stmt::Kind::PlaceAssign) {
    // p.a[i].b = v  ->  load p; walk down keeping each parent on the stack; set the last step;
    // then put each changed child back into its parent; store p.
    std::vector<const parser::Expr*> steps;
    for (const parser::Expr* at = &stmt.place; at->kind != parser::Expr::Kind::Name; at = at->left.get()) {
      if (at->left == nullptr || (at->kind != parser::Expr::Kind::Member && at->kind != parser::Expr::Kind::Index) ||
          (at->kind == parser::Expr::Kind::Index && at->right != nullptr && at->right->kind == parser::Expr::Kind::Range) ||
          (at->kind == parser::Expr::Kind::Member && at->slot > 255)) {
        diagnostics.push_back(Diagnostic{stmt.name_location, stmt.name_span, "cannot assign to this expression"});
        return false;
      }
      steps.push_back(at);
    }
    std::reverse(steps.begin(), steps.end());
    emit_op(chunk, stmt.module_global ? compiler::Opcode::LoadGlobal : compiler::Opcode::LoadLocal);
    emit_u16(chunk, stmt.slot);
    for (std::size_t index = 0; index + 1 < steps.size(); ++index) {
      const parser::Expr& step = *steps[index];
      if (step.kind == parser::Expr::Kind::Member) {
        emit_op(chunk, compiler::Opcode::Dup);
        emit_op(chunk, compiler::Opcode::GetField);
        chunk.code.push_back(static_cast<std::uint8_t>(step.slot));
      } else {
        if (!emit_expr(*step.right, chunk, diagnostics)) {
          return false;
        }
        emit_op(chunk, compiler::Opcode::Over);
        emit_op(chunk, compiler::Opcode::Over);
        emit_op(chunk, compiler::Opcode::GetIndex);
      }
    }
    const parser::Expr& last = *steps.back();
    if (last.kind == parser::Expr::Kind::Index && !emit_expr(*last.right, chunk, diagnostics)) {
      return false;
    }
    if (!emit_expr(stmt.expr, chunk, diagnostics)) {
      return false;
    }
    for (std::size_t index = steps.size(); index > 0; --index) {
      const parser::Expr& step = *steps[index - 1];
      if (step.kind == parser::Expr::Kind::Member) {
        emit_op(chunk, compiler::Opcode::SetField);
        chunk.code.push_back(static_cast<std::uint8_t>(step.slot));
      } else {
        emit_op(chunk, compiler::Opcode::SetIndex);
      }
    }
    emit_op(chunk, stmt.module_global ? compiler::Opcode::StoreGlobal : compiler::Opcode::StoreLocal);
    emit_u16(chunk, stmt.slot);
    return true;
  }

  if (stmt.kind == parser::Stmt::Kind::Return) {
    if (stmt.returns_value) {
      if (!emit_expr(stmt.expr, chunk, diagnostics)) {
        return false;
      }
    } else if (!emit_const(chunk, Value::integer_of(0), diagnostics)) {
      return false;
    }
    leave_tries(chunk, 0);
    emit_op(chunk, compiler::Opcode::Return);
    return true;
  }

  if (stmt.kind == parser::Stmt::Kind::Try) {
    emit_op(chunk, compiler::Opcode::Protect);
    const std::size_t handler = chunk.code.size();
    emit_u16(chunk, 0);
    {
      TryScope scope;
      for (const parser::Stmt& child : stmt.then_body) {
        if (!emit_stmt(child, chunk, diagnostics)) {
          return false;
        }
      }
    }
    emit_op(chunk, compiler::Opcode::EndTry);
    const std::size_t skip = emit_jump(chunk, compiler::Opcode::Jump);
    if (!patch_jump(chunk, handler, diagnostics)) {
      return false;
    }
    emit_op(chunk, compiler::Opcode::PushError);
    if (!stmt.name.empty()) {
      emit_op(chunk, compiler::Opcode::StoreLocal);
      emit_u16(chunk, stmt.slot);
    } else {
      emit_op(chunk, compiler::Opcode::Pop);
    }
    for (const parser::Stmt& child : stmt.else_body) {
      if (!emit_stmt(child, chunk, diagnostics)) {
        return false;
      }
    }
    return patch_jump(chunk, skip, diagnostics);
  }

  if (stmt.kind == parser::Stmt::Kind::If) {
    if (!emit_expr(stmt.expr, chunk, diagnostics)) return false;
    const std::size_t else_jump = emit_jump(chunk, compiler::Opcode::JumpIfFalse);
    for (const parser::Stmt& child : stmt.then_body) {
      if (!emit_stmt(child, chunk, diagnostics)) return false;
    }
    if (!stmt.has_else) return patch_jump(chunk, else_jump, diagnostics);
    const std::size_t end_jump = emit_jump(chunk, compiler::Opcode::Jump);
    if (!patch_jump(chunk, else_jump, diagnostics)) return false;
    for (const parser::Stmt& child : stmt.else_body) {
      if (!emit_stmt(child, chunk, diagnostics)) return false;
    }
    return patch_jump(chunk, end_jump, diagnostics);
  }

  if (stmt.kind == parser::Stmt::Kind::Match) {
    if (!emit_expr(stmt.expr, chunk, diagnostics)) {
      return false;
    }
    std::vector<std::size_t> switch_breaks;
    break_stack.push_back(&switch_breaks);
    std::vector<std::size_t> failed;
    std::vector<std::size_t> ends;
    const auto close_failed = [&]() {
      for (const std::size_t jump : failed) {
        if (!patch_jump(chunk, jump, diagnostics)) {
          return false;
        }
      }
      failed.clear();
      return true;
    };
    for (const parser::Stmt::MatchArm& arm : stmt.arms) {
      if (!close_failed()) {
        break_stack.pop_back();
        return false;
      }
      if (arm.tag_match) {
        emit_op(chunk, compiler::Opcode::Dup);
        emit_op(chunk, compiler::Opcode::GetField);
        chunk.code.push_back(0);
        if (!emit_expr(arm.pattern, chunk, diagnostics)) {
          break_stack.pop_back();
          return false;
        }
        emit_op(chunk, compiler::Opcode::Eq);
        failed.push_back(emit_jump(chunk, compiler::Opcode::JumpIfFalse));
        if (arm.has_payload) {
          emit_op(chunk, compiler::Opcode::Dup);
          emit_op(chunk, compiler::Opcode::GetField);
          chunk.code.push_back(1);
          emit_op(chunk, compiler::Opcode::StoreLocal);
          emit_u16(chunk, arm.bind_slot);
        }
        emit_op(chunk, compiler::Opcode::Pop);
        for (const parser::Stmt& child : arm.body) {
          if (!emit_stmt(child, chunk, diagnostics)) {
            break_stack.pop_back();
            return false;
          }
        }
        ends.push_back(emit_jump(chunk, compiler::Opcode::Jump));
        continue;
      }
      if (arm.wildcard) {
        emit_op(chunk, compiler::Opcode::Pop);
        for (const parser::Stmt& child : arm.body) {
          if (!emit_stmt(child, chunk, diagnostics)) {
            break_stack.pop_back();
            return false;
          }
        }
        ends.push_back(emit_jump(chunk, compiler::Opcode::Jump));
        continue;
      }
      emit_op(chunk, compiler::Opcode::Dup);
      if (!emit_expr(arm.pattern, chunk, diagnostics)) {
        break_stack.pop_back();
        return false;
      }
      emit_op(chunk, compiler::Opcode::Eq);
      failed.push_back(emit_jump(chunk, compiler::Opcode::JumpIfFalse));
      if (arm.has_guard) {
        if (!emit_expr(arm.guard, chunk, diagnostics)) {
          break_stack.pop_back();
          return false;
        }
        failed.push_back(emit_jump(chunk, compiler::Opcode::JumpIfFalse));
      }
      emit_op(chunk, compiler::Opcode::Pop);
      for (const parser::Stmt& child : arm.body) {
        if (!emit_stmt(child, chunk, diagnostics)) {
          break_stack.pop_back();
          return false;
        }
      }
      ends.push_back(emit_jump(chunk, compiler::Opcode::Jump));
    }
    if (!close_failed()) {
      break_stack.pop_back();
      return false;
    }
    emit_op(chunk, compiler::Opcode::Pop);
    for (const std::size_t jump : ends) {
      if (!patch_jump(chunk, jump, diagnostics)) {
        break_stack.pop_back();
        return false;
      }
    }
    for (const std::size_t jump : switch_breaks) {
      if (!patch_jump(chunk, jump, diagnostics)) {
        break_stack.pop_back();
        return false;
      }
    }
    break_stack.pop_back();
    return true;
  }

  if (stmt.kind == parser::Stmt::Kind::While) {
    if (!code_fits(chunk, diagnostics)) {
      return false;
    }
    const auto loop_start = static_cast<std::uint16_t>(chunk.code.size());
    std::optional<std::size_t> exit_jump;
    if (!stmt.post_condition) {
      if (!emit_expr(stmt.expr, chunk, diagnostics)) return false;
      exit_jump = emit_jump(chunk, compiler::Opcode::JumpIfFalse);
    }
    std::vector<std::size_t> breaks;
    break_stack.push_back(&breaks);
    std::vector<std::size_t> continues;
    continue_stack.push_back(&continues);
    bool body_ok = true;
    for (const parser::Stmt& child : stmt.then_body) {
      if (!emit_stmt(child, chunk, diagnostics)) {
        body_ok = false;
        break;
      }
    }
    break_stack.pop_back();
    continue_stack.pop_back();
    if (!body_ok) {
      return false;
    }
    for (const std::size_t jump : continues) {
      if (!patch_jump(chunk, jump, diagnostics)) {
        return false;
      }
    }
    if (stmt.post_condition) {
      if (!emit_expr(stmt.expr, chunk, diagnostics)) return false;
      exit_jump = emit_jump(chunk, compiler::Opcode::JumpIfFalse);
    }
    emit_op(chunk, compiler::Opcode::Jump);
    emit_u16(chunk, loop_start);
    if (!patch_jump(chunk, *exit_jump, diagnostics)) {
      return false;
    }
    for (const std::size_t jump : breaks) {
      if (!patch_jump(chunk, jump, diagnostics)) {
        return false;
      }
    }
    return true;
  }

  if (stmt.kind == parser::Stmt::Kind::Signal || stmt.kind == parser::Stmt::Kind::Connect ||
      stmt.kind == parser::Stmt::Kind::Using) {
    return true;
  }

  if (stmt.kind == parser::Stmt::Kind::ForIn && !stmt.numeric_range) {
    // Collection walk: items = source; limit = len(items); index = 0;
    // while (index < limit) { name = items[index]; body; index = index + 1; }
    // IterLen/IterAt also accept a number, so an untyped source still counts 0..n-1.
    if (!emit_expr(stmt.expr, chunk, diagnostics)) {
      return false;
    }
    emit_op(chunk, compiler::Opcode::Dup);
    emit_op(chunk, compiler::Opcode::StoreLocal);
    emit_u16(chunk, stmt.items_slot);
    emit_op(chunk, compiler::Opcode::IterLen);
    emit_op(chunk, compiler::Opcode::StoreLocal);
    emit_u16(chunk, stmt.limit_slot);
    if (!emit_const(chunk, Value::integer_of(0), diagnostics)) {
      return false;
    }
    emit_op(chunk, compiler::Opcode::StoreLocal);
    emit_u16(chunk, stmt.index_slot);
    if (!code_fits(chunk, diagnostics)) {
      return false;
    }
    const auto loop_start = static_cast<std::uint16_t>(chunk.code.size());
    emit_op(chunk, compiler::Opcode::LoadLocal);
    emit_u16(chunk, stmt.index_slot);
    emit_op(chunk, compiler::Opcode::LoadLocal);
    emit_u16(chunk, stmt.limit_slot);
    emit_op(chunk, compiler::Opcode::Less);
    const std::size_t exit_jump = emit_jump(chunk, compiler::Opcode::JumpIfFalse);
    emit_op(chunk, compiler::Opcode::LoadLocal);
    emit_u16(chunk, stmt.items_slot);
    emit_op(chunk, compiler::Opcode::LoadLocal);
    emit_u16(chunk, stmt.index_slot);
    emit_op(chunk, compiler::Opcode::IterAt);
    if (stmt.item_promotion != 0) {
      emit_op(chunk, stmt.item_promotion == 2 ? compiler::Opcode::ConvertOptionalFloat : compiler::Opcode::ConvertFloat);
    }
    if (emit_boxed && std::find(emit_boxed->begin(), emit_boxed->end(), stmt.slot) != emit_boxed->end()) {
      emit_op(chunk, compiler::Opcode::BoxLocal); emit_u16(chunk, stmt.slot);
    }
    emit_op(chunk, compiler::Opcode::StoreLocal);
    emit_u16(chunk, stmt.slot);
    std::vector<std::size_t> breaks;
    break_stack.push_back(&breaks);
    std::vector<std::size_t> continues;
    continue_stack.push_back(&continues);
    bool body_ok = true;
    for (const parser::Stmt& child : stmt.then_body) {
      if (!emit_stmt(child, chunk, diagnostics)) {
        body_ok = false;
        break;
      }
    }
    break_stack.pop_back();
    continue_stack.pop_back();
    for (const std::size_t jump : continues) {
      if (body_ok && !patch_jump(chunk, jump, diagnostics)) {
        return false;
      }
    }
    if (!body_ok || !emit_const(chunk, Value::integer_of(1), diagnostics)) {
      return false;
    }
    emit_op(chunk, compiler::Opcode::LoadLocal);
    emit_u16(chunk, stmt.index_slot);
    emit_op(chunk, compiler::Opcode::Add);
    emit_op(chunk, compiler::Opcode::StoreLocal);
    emit_u16(chunk, stmt.index_slot);
    emit_op(chunk, compiler::Opcode::Jump);
    emit_u16(chunk, loop_start);
    if (!patch_jump(chunk, exit_jump, diagnostics)) {
      return false;
    }
    for (const std::size_t jump : breaks) {
      if (!patch_jump(chunk, jump, diagnostics)) {
        return false;
      }
    }
    return true;
  }

  if (stmt.kind == parser::Stmt::Kind::ForIn) {
    if (!emit_expr(stmt.expr, chunk, diagnostics) || !emit_const(chunk, Value::integer_of(0), diagnostics)) {
      return false;
    }
    emit_op(chunk, compiler::Opcode::StoreLocal);
    emit_u16(chunk, stmt.index_slot);
    emit_op(chunk, compiler::Opcode::StoreLocal);
    emit_u16(chunk, stmt.limit_slot);
    if (!code_fits(chunk, diagnostics)) {
      return false;
    }
    const auto loop_start = static_cast<std::uint16_t>(chunk.code.size());
    emit_op(chunk, compiler::Opcode::LoadLocal);
    emit_u16(chunk, stmt.index_slot);
    emit_op(chunk, compiler::Opcode::LoadLocal);
    emit_u16(chunk, stmt.limit_slot);
    emit_op(chunk, compiler::Opcode::Less);
    const std::size_t exit_jump = emit_jump(chunk, compiler::Opcode::JumpIfFalse);
    emit_op(chunk, compiler::Opcode::LoadLocal); emit_u16(chunk, stmt.index_slot);
    if (emit_boxed && std::find(emit_boxed->begin(), emit_boxed->end(), stmt.slot) != emit_boxed->end()) {
      emit_op(chunk, compiler::Opcode::BoxLocal); emit_u16(chunk, stmt.slot);
    }
    emit_op(chunk, compiler::Opcode::StoreLocal); emit_u16(chunk, stmt.slot);
    std::vector<std::size_t> breaks;
    break_stack.push_back(&breaks);
    std::vector<std::size_t> continues;
    continue_stack.push_back(&continues);
    bool body_ok = true;
    for (const parser::Stmt& child : stmt.then_body) {
      if (!emit_stmt(child, chunk, diagnostics)) {
        body_ok = false;
        break;
      }
    }
    break_stack.pop_back();
    continue_stack.pop_back();
    for (const std::size_t jump : continues) {
      if (body_ok && !patch_jump(chunk, jump, diagnostics)) {
        return false;
      }
    }
    if (!body_ok || !emit_const(chunk, Value::integer_of(1), diagnostics)) {
      return false;
    }
    emit_op(chunk, compiler::Opcode::LoadLocal);
    emit_u16(chunk, stmt.index_slot);
    emit_op(chunk, compiler::Opcode::Add);
    emit_op(chunk, compiler::Opcode::StoreLocal);
    emit_u16(chunk, stmt.index_slot);
    emit_op(chunk, compiler::Opcode::Jump);
    emit_u16(chunk, loop_start);
    if (!patch_jump(chunk, exit_jump, diagnostics)) {
      return false;
    }
    for (const std::size_t jump : breaks) {
      if (!patch_jump(chunk, jump, diagnostics)) {
        return false;
      }
    }
    return true;
  }

  if (stmt.kind == parser::Stmt::Kind::For) {
    for (const parser::Stmt& init : stmt.init) {
      if (!emit_stmt(init, chunk, diagnostics)) {
        return false;
      }
    }
    if (!code_fits(chunk, diagnostics)) {
      return false;
    }
    const auto loop_start = static_cast<std::uint16_t>(chunk.code.size());
    if (!emit_expr(stmt.expr, chunk, diagnostics)) {
      return false;
    }
    const std::size_t exit_jump = emit_jump(chunk, compiler::Opcode::JumpIfFalse);
    std::vector<std::size_t> breaks;
    break_stack.push_back(&breaks);
    std::vector<std::size_t> continues;
    continue_stack.push_back(&continues);
    bool body_ok = true;
    for (const parser::Stmt& child : stmt.then_body) {
      if (!emit_stmt(child, chunk, diagnostics)) {
        body_ok = false;
        break;
      }
    }
    continue_stack.pop_back();
    for (const std::size_t jump : continues) {
      if (body_ok && !patch_jump(chunk, jump, diagnostics)) {
        return false;
      }
    }
    if (body_ok) {
      for (const parser::Stmt& child : stmt.step) {
        if (!emit_stmt(child, chunk, diagnostics)) {
          body_ok = false;
          break;
        }
      }
    }
    break_stack.pop_back();
    if (!body_ok) {
      return false;
    }
    emit_op(chunk, compiler::Opcode::Jump);
    emit_u16(chunk, loop_start);
    if (!patch_jump(chunk, exit_jump, diagnostics)) {
      return false;
    }
    for (const std::size_t jump : breaks) {
      if (!patch_jump(chunk, jump, diagnostics)) {
        return false;
      }
    }
    return true;
  }

  if (stmt.kind == parser::Stmt::Kind::SelfAssign) {
    if (!emit_expr(stmt.expr, chunk, diagnostics)) {
      return false;
    }
    return emit_self_field(chunk, stmt.name, compiler::Opcode::SetSelf, diagnostics);
  }

  if (!emit_expr(stmt.expr, chunk, diagnostics)) {
    return false;
  }
  if (stmt.atomic_cell) {
    emit_op(chunk, compiler::Opcode::AtomicNew);
  }
  if (stmt.kind == parser::Stmt::Kind::VariableDecl && !stmt.module_global && emit_boxed &&
      std::find(emit_boxed->begin(), emit_boxed->end(), stmt.slot) != emit_boxed->end()) {
    emit_op(chunk, compiler::Opcode::BoxLocal); emit_u16(chunk, stmt.slot);
  }
  if (stmt.kind == parser::Stmt::Kind::VariableDecl && stmt.immutable) emit_op(chunk, compiler::Opcode::Freeze);
  if (stmt.kind == parser::Stmt::Kind::VariableDecl || stmt.kind == parser::Stmt::Kind::Assign) {
    emit_op(chunk, stmt.module_global ? (stmt.kind == parser::Stmt::Kind::VariableDecl ?
        (stmt.immutable ? compiler::Opcode::InitConstGlobal : compiler::Opcode::InitGlobal) : compiler::Opcode::StoreGlobal) : compiler::Opcode::StoreLocal);
    emit_u16(chunk, stmt.slot);
    for (const std::uint16_t listener : stmt.listeners) {
      emit_op(chunk, stmt.module_global ? compiler::Opcode::LoadGlobal : compiler::Opcode::LoadLocal);
      emit_u16(chunk, stmt.slot);
      emit_op(chunk, compiler::Opcode::Call);
      emit_u16(chunk, listener);
      emit_op(chunk, compiler::Opcode::Pop);
    }
    return true;
  }
  if (stmt.kind == parser::Stmt::Kind::Post && stmt.print_as_bool) {
    // bool values print as true/false: JumpIfFalse -> "false", otherwise "true".
    const std::size_t to_false = emit_jump(chunk, compiler::Opcode::JumpIfFalse);
    if (!emit_const(chunk, Value::string_of("true"), diagnostics)) {
      return false;
    }
    const std::size_t to_end = emit_jump(chunk, compiler::Opcode::Jump);
    if (!patch_jump(chunk, to_false, diagnostics) || !emit_const(chunk, Value::string_of("false"), diagnostics) ||
        !patch_jump(chunk, to_end, diagnostics)) {
      return false;
    }
  }
  emit_op(chunk, stmt.channel == 1   ? compiler::Opcode::Warn
                 : stmt.channel == 2 ? compiler::Opcode::Report
                                     : compiler::Opcode::Post);
  return true;
}

[[nodiscard]] bool emit_program(const parser::Program& program, BytecodeChunk& chunk,
                                std::vector<Diagnostic>& diagnostics) {
  chunk.vtables.resize(program.structs.size());
  for (std::size_t index = 0; index < program.structs.size(); ++index) {
    chunk.vtables[index] = program.structs[index].vtable;
  }
  chunk.functions.resize(program.functions.size());
  for (const parser::Function& function : program.functions) {
    if (function.index == 65535 || function.is_extern) {
      if (function.is_extern && function.index != 65535) {
        chunk.functions[function.index].arity = static_cast<std::uint16_t>(function.params.size());
      }
      continue;
    }
    FunctionBytecode& code = chunk.functions[function.index];
    code.is_async = function.is_async;
    code.arity = static_cast<std::uint16_t>(function.params.size() + function.captures.size());
    code.local_count = function.local_count;
    if (!code_fits(chunk, diagnostics)) {
      return false;
    }
    code.entry = static_cast<std::uint16_t>(chunk.code.size());
    emit_boxed = nullptr;
    for (std::size_t index = 0; index < function.param_promotions.size(); ++index) {
      if (!function.param_promotions[index]) continue;
      emit_op(chunk, compiler::Opcode::LoadLocal); emit_u16(chunk, static_cast<std::uint16_t>(index));
      emit_op(chunk, function.param_promotions[index] == 2 ? compiler::Opcode::ConvertOptionalFloat : compiler::Opcode::ConvertFloat);
      emit_op(chunk, compiler::Opcode::StoreLocal); emit_u16(chunk, static_cast<std::uint16_t>(index));
    }
    for (const auto slot : function.boxed_slots) {
      if (slot >= function.params.size() && slot < function.params.size() + function.captures.size()) continue;
      emit_op(chunk, compiler::Opcode::BoxLocal); emit_u16(chunk, slot);
    }
    emit_boxed = &function.boxed_slots;
    for (const parser::Stmt& stmt : function.body) {
      if (!emit_stmt(stmt, chunk, diagnostics)) {
        return false;
      }
    }
    if (!emit_const(chunk, Value::integer_of(0), diagnostics)) {
      return false;
    }
    emit_op(chunk, compiler::Opcode::Return);
  }

  if (!code_fits(chunk, diagnostics)) {
    return false;
  }
  chunk.entry = static_cast<std::uint16_t>(chunk.code.size());
  chunk.local_count = program.local_count;
  emit_boxed = nullptr;
  if (!program.globals.empty()) { emit_op(chunk, compiler::Opcode::Globals); emit_u16(chunk, static_cast<std::uint16_t>(program.globals.size())); }
  for (const auto& initialize : program.functions) if (initialize.module_init) {
    emit_op(chunk, compiler::Opcode::Call); emit_u16(chunk, initialize.index); emit_op(chunk, compiler::Opcode::Pop);
  }
  for (const auto slot : program.boxed_slots) { emit_op(chunk, compiler::Opcode::BoxLocal); emit_u16(chunk, slot); }
  emit_boxed = &program.boxed_slots;
  for (const parser::Stmt& stmt : program.statements) {
    if (!emit_stmt(stmt, chunk, diagnostics)) {
      return false;
    }
  }
  emit_op(chunk, compiler::Opcode::Halt);
  emit_boxed = nullptr;
  return true;
}

}  // namespace

ClirEmitResult emit_clir(const AnalysisResult& analysis) {
  ClirEmitResult result;
  result.diagnostics = analysis.diagnostics;
  if (analysis.ok() && !emit_program(analysis.program, result.chunk, result.diagnostics)) {
    result.chunk = {};
  }
  if (!result.diagnostics.empty()) {
    auto text = std::make_shared<std::vector<std::string>>();
    text->reserve(result.diagnostics.size());
    for (const auto& diagnostic : result.diagnostics) text->emplace_back(diagnostic.span);
    for (std::size_t index = 0; index < result.diagnostics.size(); ++index)
      result.diagnostics[index].span = (*text)[index];
    result.diagnostic_text = std::move(text);
  }
  return result;
}

CompileResult Compiler::compile(const std::string_view source, const ModuleLoader& modules) const {
  const AnalysisResult analysis = analyze_program(source, modules);
  const ClirEmitResult emitted = emit_clir(analysis);
  CompileResult result;
  result.diagnostics = emitted.diagnostics;
  result.diagnostic_text = emitted.diagnostic_text;
  result.chunk = emitted.chunk;
  return result;
}

}  // namespace clpp
